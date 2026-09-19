#include "save_manager.hpp"

#include "aurora/card.h"
#include "borealis/io.hpp"
#include "borealis/version.h"
#include "dusk/main.h"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/svc/save.hpp"
#include "dusk/utilities.hpp"
#include "fmt/format.h"
#include "helpers/bits.hpp"
#include "m_Do/m_Do_MemCard.h"
#include "miniz.h"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <ranges>
#include <set>
#include <span>

namespace dusk::save_manager {
namespace {

constexpr size_t kGciHeaderSize = 0x40;
constexpr size_t kCardBlockSize = 0x2000;
constexpr size_t kMinRawSize = 512 * 1024;
constexpr size_t kMaxRawSize = 16 * 1024 * 1024;
constexpr size_t kMaxArchiveEntryCount = 130;
constexpr size_t kMaxModFileCount = kMaxArchiveEntryCount - 2;
constexpr size_t kMaxMetadataSize = 1024 * 1024;
constexpr size_t kMaxModFileSize = 1024 * 1024;
constexpr size_t kMaxArtifactSize = 160 * 1024 * 1024;
constexpr int kSaveFormatVersion = 1;

std::atomic_uint64_t s_tempSequence = 0;

Result success(std::string message = {}) {
    return {.ok = true, .message = std::move(message)};
}

Result failure(std::string message) {
    return {.ok = false, .message = std::move(message)};
}

std::string fixed_string(const uint8_t* value, size_t size) {
    const auto* end = static_cast<const uint8_t*>(std::memchr(value, '\0', size));
    return std::string{reinterpret_cast<const char*>(value),
        reinterpret_cast<const char*>(end != nullptr ? end : value + size)};
}

bool valid_raw(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kMinRawSize || bytes.size() > kMaxRawSize ||
        (bytes.size() & (bytes.size() - 1)) != 0 || bytes.size() < 512)
    {
        return false;
    }

    uint16_t checksum = 0;
    uint16_t inverse = 0;
    for (size_t i = 0; i < 0xfe; ++i) {
        const uint16_t word = read_bits<uint16_t>(bytes.data() + i * 2);
        checksum = static_cast<uint16_t>(checksum + word);
        inverse = static_cast<uint16_t>(inverse + static_cast<uint16_t>(word ^ 0xffff));
    }
    if (checksum == 0xffff) {
        checksum = 0;
    }
    if (inverse == 0xffff) {
        inverse = 0;
    }
    if (checksum != read_bits<uint16_t>(bytes.data() + 0x1fc) ||
        inverse != read_bits<uint16_t>(bytes.data() + 0x1fe))
    {
        return false;
    }

    const uint16_t sizeMb = read_bits<uint16_t>(bytes.data() + 0x22);
    return sizeMb >= 4 && sizeMb <= 128 && (sizeMb & (sizeMb - 1)) == 0 &&
           static_cast<size_t>(sizeMb) * 16 * kCardBlockSize == bytes.size();
}

ValueResult<std::vector<uint8_t>> read_location(std::string_view location, size_t maxSize) {
    auto opened = borealis::io::open(location);
    if (opened.status != borealis::io::Status::Ok || !opened.file) {
        return {failure(opened.message.empty() ? "The selected file could not be opened." :
                                                 opened.message),
            {}};
    }
    const uint64_t size = opened.file.size();
    if (size > maxSize || size > std::numeric_limits<size_t>::max()) {
        return {failure("The selected file is too large."), {}};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    if (!bytes.empty() && opened.file.read(bytes.data(), bytes.size()) != bytes.size()) {
        return {failure(opened.file.error().empty() ? "The selected file could not be read." :
                                                      opened.file.error()),
            {}};
    }
    return {success(), std::move(bytes)};
}

bool write_bytes(
    const std::filesystem::path& path, std::span<const uint8_t> bytes, std::string& error) {
    try {
        if (!path.parent_path().empty()) {
            std::filesystem::create_directories(path.parent_path());
        }
        std::ofstream out{path, std::ios::binary | std::ios::trunc};
        if (!out.is_open()) {
            error = "Unable to open the destination file.";
            return false;
        }
        out.write(reinterpret_cast<const char*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        out.close();
        if (!out.good()) {
            error = "Unable to write the destination file.";
            return false;
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::filesystem::path temporary_path(std::string_view extension) {
    std::filesystem::path directory = CachePath;
    if (directory.empty()) {
        std::error_code ec;
        directory = std::filesystem::temp_directory_path(ec);
        if (ec) {
            directory = ".";
        }
    }
    directory /= "save-manager";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    return directory / fmt::format("artifact-{}{}", s_tempSequence.fetch_add(1), extension);
}

std::string utc_timestamp(bool filename) {
    const std::time_t now = std::time(nullptr);
    std::tm value{};
#if defined(_WIN32)
    gmtime_s(&value, &now);
#else
    gmtime_r(&now, &value);
#endif
    std::array<char, 32> buffer{};
    std::strftime(
        buffer.data(), buffer.size(), filename ? "%Y%m%dT%H%M%SZ" : "%Y-%m-%dT%H:%M:%SZ", &value);
    return buffer.data();
}

std::filesystem::path gci_path(const Storage& storage, const SaveIdentity& identity) {
    return storage.path /
           borealis::io::fs_path_from_utf8(
               card_file_stem(identity.maker, identity.game, identity.saveName) + ".gci");
}

std::filesystem::path backup_directory(const Storage& storage) {
    return (storage.kind == StorageKind::GciDirectory ? storage.path : storage.path.parent_path()) /
           "backups";
}

ValueResult<std::map<std::string, std::vector<uint8_t>>> read_mod_files(
    const Storage& storage, const SaveIdentity& identity) {
    std::map<std::string, std::vector<uint8_t>> files;
    const auto directory = save_sidecar_directory(
        storage.path, storage.kind, identity.maker, identity.game, identity.saveName);
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        if (ec) {
            return {failure(fmt::format("Unable to inspect mod save data: {}", ec.message())), {}};
        }
        return {success(), {}};
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }
            const std::string id = borealis::io::fs_path_to_string(entry.path().stem());
            if (!utils::is_valid_mod_id(id)) {
                continue;
            }
            const auto location = borealis::io::fs_path_to_string(entry.path());
            auto read = read_location(location, kMaxModFileSize);
            if (!read) {
                return {read.result, {}};
            }
            files.emplace(id, std::move(read.value));
            if (files.size() > kMaxModFileCount) {
                return {failure("There are too many mod save data files to archive."), {}};
            }
        }
    } catch (const std::exception& exception) {
        return {failure(fmt::format("Unable to read mod save data: {}", exception.what())), {}};
    }
    return {success(), std::move(files)};
}

std::string mod_version(std::string_view id) {
    for (const auto& mod : mods::ModLoader::instance().mods()) {
        if (mod.metadata.id == id) {
            return mod.metadata.version;
        }
    }
    return {};
}

Result pack_dusksave(const std::filesystem::path& destination, const std::vector<uint8_t>& gci,
    const std::map<std::string, std::vector<uint8_t>>& modFiles) {
    auto parsed = parse_gci(gci);
    if (!parsed) {
        return parsed.result;
    }

    nlohmann::json mods = nlohmann::json::array();
    for (const auto& entry : modFiles) {
        mods.push_back({{"id", entry.first}, {"version", mod_version(entry.first)}});
    }
    const nlohmann::json metadata{
        {"version", kSaveFormatVersion},
        {"dusklight", BOREALIS_APP_VERSION},
        {"created", utc_timestamp(false)},
        {"mods", std::move(mods)},
    };
    const std::string metadataText = metadata.dump(2);

    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        return failure("Unable to initialize the save archive.");
    }
    const std::unique_ptr<mz_zip_archive, decltype(&mz_zip_writer_end)> zipGuard{
        &zip, &mz_zip_writer_end};
    if (!mz_zip_writer_add_mem(
            &zip, "save.json", metadataText.data(), metadataText.size(), MZ_BEST_COMPRESSION) ||
        !mz_zip_writer_add_mem(&zip, "save.gci", gci.data(), gci.size(), MZ_BEST_COMPRESSION))
    {
        return failure("Unable to add the save to the archive.");
    }
    for (const auto& [id, data] : modFiles) {
        const std::string name = fmt::format("mods/{}.json", id);
        if (!mz_zip_writer_add_mem(
                &zip, name.c_str(), data.data(), data.size(), MZ_BEST_COMPRESSION))
        {
            return failure("Unable to add mod data to the save archive.");
        }
    }

    void* archiveData = nullptr;
    size_t archiveSize = 0;
    if (!mz_zip_writer_finalize_heap_archive(&zip, &archiveData, &archiveSize)) {
        return failure("Unable to finish the save archive.");
    }
    std::string error;
    const bool written = write_bytes(
        destination, std::span{static_cast<const uint8_t*>(archiveData), archiveSize}, error);
    mz_free(archiveData);
    return written ? success() : failure(std::move(error));
}

ValueResult<std::vector<uint8_t>> extract_zip_entry(
    mz_zip_archive& zip, mz_uint index, size_t maxSize) {
    mz_zip_archive_file_stat stat{};
    if (!mz_zip_reader_file_stat(&zip, index, &stat) || stat.m_uncomp_size > maxSize) {
        return {failure("The save archive contains an invalid or oversized entry."), {}};
    }
    std::vector<uint8_t> bytes(static_cast<size_t>(stat.m_uncomp_size));
    if (!mz_zip_reader_extract_to_mem(&zip, index, bytes.data(), bytes.size(), 0)) {
        return {failure("The save archive could not be read."), {}};
    }
    return {success(), std::move(bytes)};
}

ValueResult<Artifact> read_dusksave(std::vector<uint8_t> bytes, std::string sourceName) {
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        return {failure("The selected ZIP is not a Dusklight save."), {}};
    }
    const std::unique_ptr<mz_zip_archive, decltype(&mz_zip_reader_end)> zipGuard{
        &zip, &mz_zip_reader_end};
    const mz_uint entryCount = mz_zip_reader_get_num_files(&zip);
    if (entryCount > kMaxArchiveEntryCount) {
        return {failure("The save archive contains too many entries."), {}};
    }

    int metadataIndex = -1;
    int gciIndex = -1;
    size_t metadataCount = 0;
    size_t gciCount = 0;
    for (mz_uint i = 0; i < entryCount; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat) || mz_zip_reader_is_file_a_directory(&zip, i))
        {
            continue;
        }
        const std::string_view name{stat.m_filename};
        if (name == "save.json") {
            metadataIndex = static_cast<int>(i);
            ++metadataCount;
        } else if (name == "save.gci") {
            gciIndex = static_cast<int>(i);
            ++gciCount;
        }
    }
    if (metadataCount != 1 || gciCount != 1) {
        return {failure("The selected ZIP is not a Dusklight save."), {}};
    }

    auto metadataBytes =
        extract_zip_entry(zip, static_cast<mz_uint>(metadataIndex), kMaxMetadataSize);
    auto gciBytes =
        extract_zip_entry(zip, static_cast<mz_uint>(gciIndex), kMaxRawSize + kGciHeaderSize);
    if (!metadataBytes || !gciBytes) {
        const Result result = !metadataBytes ? metadataBytes.result : gciBytes.result;
        return {result, {}};
    }

    Artifact artifact{
        .kind = ArtifactKind::DuskSave,
        .gci = std::move(gciBytes.value),
        .sourceName = std::move(sourceName),
    };
    auto parsedGci = parse_gci(artifact.gci);
    if (!parsedGci) {
        return {parsedGci.result, {}};
    }
    artifact.header = std::move(parsedGci.value);

    try {
        const auto metadata =
            nlohmann::json::parse(metadataBytes.value.begin(), metadataBytes.value.end());
        if (!metadata.is_object() || metadata.value("version", 0) != kSaveFormatVersion ||
            !metadata.contains("dusklight") || !metadata["dusklight"].is_string() ||
            !metadata.contains("created") || !metadata["created"].is_string() ||
            !metadata.contains("mods") || !metadata["mods"].is_array())
        {
            return {failure("The Dusklight save uses an unsupported format version."), {}};
        }
        std::set<std::string> declaredIds;
        for (const auto& mod : metadata["mods"]) {
            if (!mod.is_object()) {
                return {failure("The Dusklight save metadata is invalid."), {}};
            }
            const std::string id = mod.value("id", "");
            if (!utils::is_valid_mod_id(id) || !declaredIds.insert(id).second ||
                !mod.contains("version") || !mod["version"].is_string())
            {
                return {failure("The Dusklight save metadata is invalid."), {}};
            }
            artifact.declaredMods.push_back(
                {.id = id, .version = mod["version"].get<std::string>()});
        }
    } catch (const std::exception&) {
        return {failure("The Dusklight save metadata is invalid."), {}};
    }

    for (mz_uint i = 0; i < entryCount; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat) || mz_zip_reader_is_file_a_directory(&zip, i))
        {
            continue;
        }
        const std::string_view name{stat.m_filename};
        if (!name.starts_with("mods/")) {
            continue;
        }
        const std::string_view child = name.substr(5);
        if (!utils::is_safe_path_component(child) || !child.ends_with(".json")) {
            return {failure("The save archive contains an unsafe mod data path."), {}};
        }
        const std::string id{child.substr(0, child.size() - 5)};
        if (!utils::is_valid_mod_id(id) || artifact.modFiles.contains(id)) {
            return {failure("The save archive contains an invalid mod data entry."), {}};
        }
        auto data = extract_zip_entry(zip, i, kMaxModFileSize);
        if (!data) {
            const Result result = data.result;
            return {result, {}};
        }
        artifact.modFiles.emplace(id, std::move(data.value));
    }
    if (artifact.modFiles.size() != artifact.declaredMods.size() ||
        std::ranges::any_of(artifact.declaredMods,
            [&artifact](const ModFileInfo& mod) { return !artifact.modFiles.contains(mod.id); }))
    {
        return {failure("The save archive metadata does not match its mod data."), {}};
    }
    return {success(), std::move(artifact)};
}

ValueResult<std::vector<SaveIdentity>> list_card_saves(
    const Storage& storage, std::string_view game, std::string_view maker) {
    std::set<std::string> names;
    try {
        if (!std::filesystem::exists(storage.path)) {
            return {success(), {}};
        }
        if (storage.kind == StorageKind::RawImage) {
            if (!aurora_card_raw_list(
                    borealis::io::fs_path_to_string(storage.path).c_str(),
                    std::string{game}.c_str(), std::string{maker}.c_str(),
                    [](const char* name, void* userData) {
                        static_cast<std::set<std::string>*>(userData)->insert(name);
                    },
                    &names))
            {
                return {failure("The memory card image could not be read."), {}};
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator{storage.path}) {
                if (!entry.is_regular_file() || entry.path().extension() != ".gci") {
                    continue;
                }
                auto read = read_location(
                    borealis::io::fs_path_to_string(entry.path()), kMaxRawSize + kGciHeaderSize);
                auto parsed = read ? parse_gci(read.value) : ValueResult<GciHeader>{};
                if (parsed && parsed.value.game == game && parsed.value.maker == maker &&
                    borealis::io::fs_path_to_string(entry.path().filename()) ==
                        card_file_stem(maker, game, parsed.value.saveName) + ".gci")
                {
                    names.insert(parsed.value.saveName);
                }
            }
        }
    } catch (const std::exception& exception) {
        return {failure(fmt::format("Unable to list saves: {}", exception.what())), {}};
    }
    std::vector<SaveIdentity> identities;
    for (const auto& name : names) {
        if (utils::is_valid_save_name(name)) {
            identities.push_back({
                .maker = std::string{maker},
                .game = std::string{game},
                .saveName = name,
            });
        }
    }
    return {success(), std::move(identities)};
}

std::string_view backup_save_name(
    std::string_view name, std::string_view game, std::string_view maker) {
    const std::string prefix = card_file_stem(maker, game, "");
    if (!name.starts_with(prefix) || !name.ends_with(".dusksave")) {
        return {};
    }
    name.remove_prefix(prefix.size());
    name.remove_suffix(std::string_view{".dusksave"}.size());
    const auto digits = [](std::string_view value) {
        return !value.empty() &&
               std::ranges::all_of(value, [](char ch) { return ch >= '0' && ch <= '9'; });
    };
    if (!name.ends_with('Z')) {
        const auto separator = name.rfind('-');
        if (separator == std::string_view::npos || !digits(name.substr(separator + 1))) {
            return {};
        }
        name = name.substr(0, separator);
    }
    if (name.size() < 18) {
        return {};
    }
    const auto timestamp = name.substr(name.size() - 16);
    if (name[name.size() - 17] != '-' || timestamp[8] != 'T' || timestamp[15] != 'Z' ||
        !digits(timestamp.substr(0, 8)) || !digits(timestamp.substr(9, 6)))
    {
        return {};
    }
    const auto saveName = name.substr(0, name.size() - 17);
    return utils::is_valid_save_name(saveName) ? saveName : std::string_view{};
}

ValueResult<std::vector<uint8_t>> read_current_gci(
    const Storage& storage, const SaveIdentity& identity) {
    if (storage.kind == StorageKind::RawImage) {
        const std::string imagePath = borealis::io::fs_path_to_string(storage.path);
        const size_t required = aurora_card_raw_extract(imagePath.c_str(), identity.game.c_str(),
            identity.maker.c_str(), identity.saveName.c_str(), nullptr, 0);
        if (required == 0) {
            return {success(), {}};
        }
        std::vector<uint8_t> gci(required);
        if (aurora_card_raw_extract(imagePath.c_str(), identity.game.c_str(),
                identity.maker.c_str(), identity.saveName.c_str(), gci.data(),
                gci.size()) != required)
        {
            return {failure("The save could not be extracted from the card image."), {}};
        }
        return {success(), std::move(gci)};
    }

    std::error_code ec;
    if (!std::filesystem::exists(storage.path, ec)) {
        return ec ?
                   ValueResult<std::vector<uint8_t>>{
                       failure(fmt::format("Unable to inspect the save folder: {}", ec.message())),
                       {}} :
                   ValueResult<std::vector<uint8_t>>{success(), {}};
    }
    try {
        const auto canonical = gci_path(storage, identity);
        for (const auto& entry : std::filesystem::directory_iterator{storage.path}) {
            if (entry.path().filename() != canonical.filename()) {
                continue;
            }
            if (!entry.is_regular_file()) {
                return {failure("The expected save path is not a regular file."), {}};
            }
            auto read = read_location(
                borealis::io::fs_path_to_string(entry.path()), kMaxRawSize + kGciHeaderSize);
            if (!read) {
                return read;
            }
            auto parsed = parse_gci(read.value);
            if (!parsed) {
                return {parsed.result, {}};
            }
            if (parsed.value.game != identity.game || parsed.value.maker != identity.maker ||
                parsed.value.saveName != identity.saveName)
            {
                return {failure("The save filename does not match the save data it contains."), {}};
            }
            return read;
        }
        if (std::filesystem::exists(std::filesystem::symlink_status(canonical))) {
            return {
                failure("The expected save path is occupied. Check its filename and letter case."),
                {}};
        }
    } catch (const std::exception& exception) {
        return {
            failure(fmt::format("Unable to inspect the save folder: {}", exception.what())), {}};
    }
    return {success(), {}};
}

Result replace_sidecars(const Storage& storage, const SaveIdentity& identity,
    const std::map<std::string, std::vector<uint8_t>>& files) {
    const auto destination = save_sidecar_directory(
        storage.path, storage.kind, identity.maker, identity.game, identity.saveName);
    const auto suffix = fmt::format(".replace-{}", s_tempSequence.fetch_add(1));
    std::filesystem::path staging{destination};
    staging += suffix;
    std::filesystem::path previous{destination};
    previous += suffix + ".old";
    std::error_code ec;
    std::filesystem::remove_all(staging, ec);
    std::filesystem::remove_all(previous, ec);

    try {
        if (!files.empty()) {
            std::filesystem::create_directories(staging);
            for (const auto& [id, data] : files) {
                std::string error;
                if (!write_bytes(staging / (id + ".json"), data, error)) {
                    std::filesystem::remove_all(staging, ec);
                    return failure(fmt::format("Unable to stage mod save data: {}", error));
                }
            }
        }
        if (std::filesystem::exists(destination)) {
            std::filesystem::rename(destination, previous);
        }
        if (!files.empty()) {
            try {
                std::filesystem::rename(staging, destination);
            } catch (...) {
                if (std::filesystem::exists(previous)) {
                    std::filesystem::rename(previous, destination);
                }
                throw;
            }
        }
        std::filesystem::remove_all(previous, ec);
        return success();
    } catch (const std::exception& exception) {
        std::filesystem::remove_all(staging, ec);
        return failure(fmt::format("Unable to replace mod save data: {}", exception.what()));
    }
}

Result ensure_write_allowed(const Storage& storage) {
    if (storage.mounted && !mDoMemCd_isCardCommNone()) {
        return failure("The memory card is busy. Try again after returning to the main menu.");
    }
    return success();
}

Result finish_write(const Storage& storage, std::span<const SaveIdentity> identities) {
    const bool remounted = !storage.mounted || aurora_card_remount(storage.channel);
    for (const auto& identity : identities) {
        mods::svc::invalidate_save(identity.saveName);
    }
    if (!remounted) {
        return failure("The save changed on disk, but the memory card could not be remounted.");
    }
    return success();
}

Result finish_write(const Storage& storage, const SaveIdentity& identity) {
    return finish_write(storage, std::span{&identity, size_t{1}});
}

Result write_file_atomic(const std::filesystem::path& destination, std::span<const uint8_t> bytes) {
    auto temporary = destination;
    temporary += ".tmp";
    std::string error;
    if (!write_bytes(temporary, bytes, error) ||
        !borealis::io::atomic_replace(temporary, destination, error))
    {
        std::error_code ec;
        std::filesystem::remove(temporary, ec);
        return failure(fmt::format("The save could not be replaced: {}", error));
    }
    return success();
}

Result write_gci(
    const Storage& storage, const SaveIdentity& identity, const std::vector<uint8_t>& gci) {
    if (storage.kind == StorageKind::RawImage) {
        const std::string path = borealis::io::fs_path_to_string(storage.path);
        if (!aurora_card_raw_insert(path.c_str(), gci.data(), gci.size(), true)) {
            return failure("The save could not be written to the card image.");
        }
        return success();
    }
    return write_file_atomic(gci_path(storage, identity), gci);
}

ValueResult<std::filesystem::path> backup_existing_save(
    const Storage& storage, const SaveIdentity& identity) {
    auto current = read_current_gci(storage, identity);
    if (!current) {
        return {current.result, {}};
    }
    if (current.value.empty()) {
        return {success(), {}};
    }
    auto modFiles = read_mod_files(storage, identity);
    if (!modFiles) {
        return {modFiles.result, {}};
    }

    std::filesystem::path destination;
    try {
        const auto directory = backup_directory(storage);
        std::filesystem::create_directories(directory);
        const std::string prefix =
            card_file_stem(identity.maker, identity.game, identity.saveName) + "-" +
            utc_timestamp(true);
        destination = directory / (prefix + ".dusksave");
        for (int suffix = 1; std::filesystem::exists(destination); ++suffix) {
            destination = directory / fmt::format("{}-{}.dusksave", prefix, suffix);
        }
    } catch (const std::exception& exception) {
        return {failure(fmt::format("Backup failed: {}", exception.what())), {}};
    }
    std::filesystem::path temporary{destination};
    temporary += ".tmp";
    const Result packed = pack_dusksave(temporary, current.value, modFiles.value);
    if (!packed) {
        std::error_code ec;
        std::filesystem::remove(temporary, ec);
        return {failure(fmt::format("Backup failed: {}", packed.message)), {}};
    }
    std::string replaceError;
    if (!borealis::io::atomic_replace(temporary, destination, replaceError)) {
        std::error_code ec;
        std::filesystem::remove(temporary, ec);
        return {failure(fmt::format("Backup failed: {}", replaceError)), {}};
    }

    auto backups = list_backups(storage, identity);
    if (backups) {
        // Always keep this recovery copy, even if older backups have future timestamps.
        size_t retained = 1;
        for (const auto& backup : backups.value) {
            if (backup.path != destination && retained++ >= kDefaultBackupRetention) {
                std::error_code ec;
                std::filesystem::remove(backup.path, ec);
            }
        }
    }
    return {success(), std::move(destination)};
}

Result apply_artifact(const Storage& storage, const SaveIdentity& identity,
    const Artifact& artifact, ModDataAction modDataAction, bool backupCurrentSave) {
    if (const Result allowed = ensure_write_allowed(storage); !allowed) {
        return allowed;
    }

    if (artifact.kind != ArtifactKind::Gci && artifact.kind != ArtifactKind::DuskSave) {
        return failure("The selected artifact is not an individual save.");
    }
    auto parsed = parse_gci(artifact.gci);
    if (!parsed) {
        return parsed.result;
    }
    if (parsed.value.game != identity.game || parsed.value.maker != identity.maker) {
        return failure(fmt::format("This save is for {}-{}, but the configured disc uses {}-{}.",
            parsed.value.maker, parsed.value.game, identity.maker, identity.game));
    }
    if (!utils::is_valid_save_name(parsed.value.saveName) ||
        parsed.value.saveName != identity.saveName)
    {
        return failure("The imported save name does not match its destination.");
    }
    if (backupCurrentSave) {
        if (auto backup = backup_existing_save(storage, identity); !backup) {
            return backup.result;
        }
    }
    if (const Result written = write_gci(storage, identity, artifact.gci); !written) {
        return written;
    }

    if (artifact.kind == ArtifactKind::DuskSave) {
        modDataAction = ModDataAction::Replace;
    }
    if (modDataAction != ModDataAction::Keep) {
        const std::map<std::string, std::vector<uint8_t>> noModFiles;
        const auto& files =
            modDataAction == ModDataAction::Replace ? artifact.modFiles : noModFiles;
        if (const Result sidecars = replace_sidecars(storage, identity, files); !sidecars) {
            finish_write(storage, identity);
            return sidecars;
        }
    }
    return finish_write(storage, identity);
}

}  // namespace

std::optional<SaveIdentity> identity_for_disc(const iso::DiscInfo& info, std::string saveName) {
    if (info.gameId.size() != 6 || !utils::is_valid_save_name(saveName)) {
        return std::nullopt;
    }
    return SaveIdentity{
        .maker = info.gameId.substr(4),
        .game = info.gameId.substr(0, 4),
        .saveName = std::move(saveName),
    };
}

std::string card_file_stem(
    std::string_view maker, std::string_view game, std::string_view saveName) {
    std::array<char, 64> filename{};
    const size_t required =
        aurora_card_gci_filename(std::string{game}.c_str(), std::string{maker}.c_str(),
            std::string{saveName}.c_str(), filename.data(), filename.size());
    if (required == 0 || required > filename.size()) {
        return {};
    }
    return {filename.data(), required - 5};
}

std::filesystem::path save_sidecar_directory(const std::filesystem::path& backingPath,
    StorageKind kind, std::string_view maker, std::string_view game, std::string_view saveName) {
    const auto stem = card_file_stem(maker, game, saveName);
    if (kind == StorageKind::GciDirectory) {
        return backingPath / (stem + ".mods");
    }
    std::filesystem::path root{backingPath};
    root += ".mods";
    return root / stem;
}

ValueResult<Storage> resolve_storage(
    std::string_view game, StorageKind preferredKind, int channel) {
    if (game.size() != 4 || channel < 0 || channel > 1) {
        return {failure("The memory card location could not be resolved."), {}};
    }
    Storage storage{.kind = preferredKind, .channel = channel};
    const auto activeType = aurora_card_get_type(channel);
    if (activeType != AURORA_CARD_UNAVAILABLE) {
        storage.kind = activeType == AURORA_CARD_GCI_DIRECTORY ? StorageKind::GciDirectory :
                                                                 StorageKind::RawImage;
        storage.mounted = true;
    }
    const std::string gameName{game};
    const auto cardType = storage.kind == StorageKind::GciDirectory ? AURORA_CARD_GCI_DIRECTORY :
                                                                      AURORA_CARD_RAW_IMAGE;
    const size_t required = aurora_card_get_path(gameName.c_str(), cardType, channel, nullptr, 0);
    if (required == 0) {
        return {failure("The memory card location could not be resolved."), {}};
    }
    std::vector<char> path(required);
    const size_t copied =
        aurora_card_get_path(gameName.c_str(), cardType, channel, path.data(), path.size());
    if (copied != required) {
        return {failure("The memory card location could not be resolved."), {}};
    }
    storage.path = borealis::io::fs_path_from_utf8(path.data());
    return {success(), std::move(storage)};
}

ValueResult<std::vector<SaveIdentity>> list_saves(
    const Storage& storage, std::string_view game, std::string_view maker) {
    auto saves = list_card_saves(storage, game, maker);
    if (!saves) {
        return saves;
    }
    std::set<std::string> names;
    for (const auto& identity : saves.value) {
        names.insert(identity.saveName);
    }
    const std::string prefix = card_file_stem(maker, game, "");
    const auto add_name = [&](std::string_view name) {
        if (utils::is_valid_save_name(name) && names.emplace(name).second) {
            saves.value.push_back({.maker = std::string{maker},
                .game = std::string{game},
                .saveName = std::string{name}});
        }
    };
    try {
        auto modRoot = storage.path;
        const bool gciDirectory = storage.kind == StorageKind::GciDirectory;
        if (!gciDirectory) {
            modRoot += ".mods";
        }
        if (std::filesystem::exists(modRoot)) {
            for (const auto& entry : std::filesystem::directory_iterator{modRoot}) {
                const std::string name = borealis::io::fs_path_to_string(entry.path().filename());
                if (!entry.is_directory() || !name.starts_with(prefix) ||
                    (gciDirectory && !name.ends_with(".mods")))
                {
                    continue;
                }
                const auto saveName = std::string_view{name}.substr(
                    prefix.size(), name.size() - prefix.size() - (gciDirectory ? 5 : 0));
                if (!utils::is_valid_save_name(saveName)) {
                    continue;
                }
                for (const auto& mod : std::filesystem::directory_iterator{entry.path()}) {
                    if (mod.is_regular_file() && mod.path().extension() == ".json" &&
                        utils::is_valid_mod_id(borealis::io::fs_path_to_string(mod.path().stem())))
                    {
                        add_name(saveName);
                        break;
                    }
                }
            }
        }
        const auto backups = backup_directory(storage);
        if (std::filesystem::exists(backups)) {
            for (const auto& entry : std::filesystem::directory_iterator{backups}) {
                if (entry.is_regular_file()) {
                    const std::string name =
                        borealis::io::fs_path_to_string(entry.path().filename());
                    add_name(backup_save_name(name, game, maker));
                }
            }
        }
    } catch (const std::exception& exception) {
        return {
            failure(fmt::format("Unable to list save data and backups: {}", exception.what())), {}};
    }
    std::ranges::sort(saves.value, {}, &SaveIdentity::saveName);
    return saves;
}

ValueResult<GciHeader> parse_gci(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < kGciHeaderSize || (bytes.size() - kGciHeaderSize) % kCardBlockSize != 0) {
        return {failure("The selected file is not a valid GCI save."), {}};
    }
    const uint16_t blockCount = read_bits<uint16_t>(bytes.data() + 0x38);
    if (blockCount == 0 || blockCount != (bytes.size() - kGciHeaderSize) / kCardBlockSize) {
        return {failure("The selected file has an invalid GCI header."), {}};
    }
    const std::string saveName = fixed_string(bytes.data() + 8, 32);
    const std::string game = fixed_string(bytes.data(), 4);
    const std::string maker = fixed_string(bytes.data() + 4, 2);
    if (saveName.empty() || game.size() != 4 || maker.size() != 2) {
        return {failure("The selected file has an invalid GCI filename."), {}};
    }
    return {
        success(),
        GciHeader{
            .maker = maker,
            .game = game,
            .saveName = saveName,
            .modifiedTime = read_bits<uint32_t>(bytes.data() + 0x28),
            .blockCount = blockCount,
        },
    };
}

ValueResult<Artifact> read_artifact(std::string_view location) {
    auto read = read_location(location, kMaxArtifactSize);
    if (!read) {
        return {read.result, {}};
    }
    const std::string sourceName = borealis::io::display_name(location);
    if (read.value.size() >= 4 && read.value[0] == 'P' && read.value[1] == 'K' &&
        read.value[2] == 3 && read.value[3] == 4)
    {
        return read_dusksave(std::move(read.value), sourceName);
    }
    if (auto parsed = parse_gci(read.value)) {
        return {
            success(),
            Artifact{
                .kind = ArtifactKind::Gci,
                .header = std::move(parsed.value),
                .gci = std::move(read.value),
                .sourceName = sourceName,
            },
        };
    }
    if (valid_raw(read.value)) {
        return {
            success(),
            Artifact{
                .kind = ArtifactKind::Raw,
                .raw = std::move(read.value),
                .sourceName = sourceName,
            },
        };
    }
    return {failure("The selected file is not a GCI, raw card image, or Dusklight save."), {}};
}

ValueResult<std::vector<Artifact>> extract_raw_saves(
    const Artifact& artifact, std::string_view game, std::string_view maker) {
    if (artifact.kind != ArtifactKind::Raw) {
        return {failure("The selected artifact is not a raw card image."), {}};
    }
    const auto staged = temporary_path(".raw");
    const auto removeStaged = [](const std::filesystem::path* path) {
        std::error_code ec;
        std::filesystem::remove(*path, ec);
    };
    const std::unique_ptr<const std::filesystem::path, decltype(removeStaged)> stagedGuard{
        &staged, removeStaged};
    if (std::string error; !write_bytes(staged, artifact.raw, error)) {
        return {failure(std::move(error)), {}};
    }
    const Storage storage{
        .kind = StorageKind::RawImage,
        .path = staged,
    };
    auto identities = list_card_saves(storage, game, maker);
    if (!identities) {
        return {identities.result, {}};
    }
    std::vector<Artifact> extracted;
    for (const auto& identity : identities.value) {
        auto gci = read_current_gci(storage, identity);
        if (!gci) {
            return {gci.result, {}};
        }
        auto parsed = parse_gci(gci.value);
        if (!parsed || parsed.value.game != identity.game || parsed.value.maker != identity.maker ||
            parsed.value.saveName != identity.saveName)
        {
            return {failure("The selected card image contains an invalid save entry."), {}};
        }
        extracted.push_back({
            .kind = ArtifactKind::Gci,
            .header = std::move(parsed.value),
            .gci = std::move(gci.value),
            .sourceName = artifact.sourceName,
        });
    }
    return {success(), std::move(extracted)};
}

ValueResult<SaveInfo> inspect_save(const Storage& storage, const SaveIdentity& identity) {
    SaveInfo info;
    auto gci = read_current_gci(storage, identity);
    if (!gci) {
        return {gci.result, {}};
    }
    if (!gci.value.empty()) {
        auto parsed = parse_gci(gci.value);
        if (!parsed) {
            return {parsed.result, {}};
        }
        info.present = true;
        info.size = gci.value.size();
        info.modifiedTime = parsed.value.modifiedTime;
    }
    auto mods = read_mod_files(storage, identity);
    if (!mods) {
        return {mods.result, {}};
    }
    for (const auto& [id, data] : mods.value) {
        info.mods.push_back({
            .id = id,
            .version = mod_version(id),
            .size = data.size(),
        });
    }
    return {success(), std::move(info)};
}

ValueResult<ExportArtifact> build_export(
    const Storage& storage, const SaveIdentity& identity, bool includeModData) {
    auto gci = read_current_gci(storage, identity);
    if (!gci) {
        return {gci.result, {}};
    }
    if (gci.value.empty()) {
        return {failure("There is no save file to export."), {}};
    }
    const std::string extension = includeModData ? ".dusksave" : ".gci";
    ExportArtifact artifact{
        .path = temporary_path(extension),
        .suggestedName =
            card_file_stem(identity.maker, identity.game, identity.saveName) + extension,
        .temporary = true,
    };
    Result result;
    if (includeModData) {
        auto mods = read_mod_files(storage, identity);
        if (!mods) {
            return {mods.result, {}};
        }
        result = pack_dusksave(artifact.path, gci.value, mods.value);
    } else {
        std::string error;
        result =
            write_bytes(artifact.path, gci.value, error) ? success() : failure(std::move(error));
    }
    if (!result) {
        std::error_code ec;
        std::filesystem::remove(artifact.path, ec);
        return {result, {}};
    }
    return {success(), std::move(artifact)};
}

ValueResult<ExportArtifact> raw_card_export(const Storage& storage) {
    std::error_code ec;
    if (storage.kind != StorageKind::RawImage ||
        !std::filesystem::is_regular_file(storage.path, ec))
    {
        return {failure("There is no raw card image to export."), {}};
    }
    return {
        success(),
        ExportArtifact{
            .path = storage.path,
            .suggestedName = borealis::io::fs_path_to_string(storage.path.filename()),
        },
    };
}

Result import_artifact(const Storage& storage, const SaveIdentity& identity,
    const Artifact& artifact, ModDataAction modDataAction) {
    return apply_artifact(storage, identity, artifact, modDataAction, true);
}

Result import_raw_image(const Storage& storage, std::string_view game, std::string_view maker,
    const Artifact& artifact, ModDataAction modDataAction) {
    if (storage.kind != StorageKind::RawImage || artifact.kind != ArtifactKind::Raw) {
        return failure("The raw card image import target is invalid.");
    }
    if (const Result allowed = ensure_write_allowed(storage); !allowed) {
        return allowed;
    }
    if (!valid_raw(artifact.raw)) {
        return failure("The selected artifact is not a valid raw card image.");
    }
    auto identities = list_saves(storage, game, maker);
    if (!identities) {
        return identities.result;
    }
    for (const auto& identity : identities.value) {
        auto backup = backup_existing_save(storage, identity);
        if (!backup) {
            return backup.result;
        }
    }

    if (const Result written = write_file_atomic(storage.path, artifact.raw); !written) {
        return written;
    }

    Result sidecarResult = success();
    if (modDataAction == ModDataAction::Clear) {
        for (const auto& identity : identities.value) {
            if (const Result cleared = replace_sidecars(storage, identity, {}); !cleared) {
                sidecarResult = cleared;
                break;
            }
        }
    }
    const Result finished = finish_write(storage, identities.value);
    if (!sidecarResult) {
        return sidecarResult;
    }
    return finished;
}

Result delete_save(const Storage& storage, const SaveIdentity& identity) {
    if (const Result allowed = ensure_write_allowed(storage); !allowed) {
        return allowed;
    }
    auto current = read_current_gci(storage, identity);
    if (!current) {
        return current.result;
    }
    if (current.value.empty()) {
        return failure("There is no save file to delete.");
    }
    auto backup = backup_existing_save(storage, identity);
    if (!backup) {
        return backup.result;
    }

    if (storage.kind == StorageKind::RawImage) {
        const std::string path = borealis::io::fs_path_to_string(storage.path);
        if (!aurora_card_raw_delete(path.c_str(), identity.game.c_str(), identity.maker.c_str(),
                identity.saveName.c_str()))
        {
            return failure("The save could not be deleted from the card image.");
        }
    } else {
        try {
            if (!std::filesystem::remove(gci_path(storage, identity))) {
                return failure("The save file could not be deleted.");
            }
        } catch (const std::exception& exception) {
            return failure(fmt::format("The save could not be deleted: {}", exception.what()));
        }
    }
    if (const Result sidecars = replace_sidecars(storage, identity, {}); !sidecars) {
        finish_write(storage, identity);
        return sidecars;
    }
    return finish_write(storage, identity);
}

Result delete_mod_data(
    const Storage& storage, const SaveIdentity& identity, std::string_view modId) {
    if (!utils::is_valid_mod_id(modId)) {
        return failure("The mod ID is invalid.");
    }
    if (const Result allowed = ensure_write_allowed(storage); !allowed) {
        return allowed;
    }
    if (auto backup = backup_existing_save(storage, identity); !backup) {
        return backup.result;
    }
    const auto directory = save_sidecar_directory(
        storage.path, storage.kind, identity.maker, identity.game, identity.saveName);
    std::error_code ec;
    const bool removed = std::filesystem::remove(directory / (std::string{modId} + ".json"), ec);
    if (ec) {
        return failure(fmt::format("The mod data could not be deleted: {}", ec.message()));
    }
    if (!removed) {
        return failure("The mod data no longer exists.");
    }
    mods::svc::invalidate_save(identity.saveName);
    return success();
}

Result create_backup(const Storage& storage, const SaveIdentity& identity) {
    if (const Result allowed = ensure_write_allowed(storage); !allowed) {
        return allowed;
    }
    auto backup = backup_existing_save(storage, identity);
    if (!backup) {
        return backup.result;
    }
    return backup.value.empty() ? failure("There is no save file to back up.") : success();
}

ValueResult<std::vector<BackupInfo>> list_backups(
    const Storage& storage, const SaveIdentity& identity) {
    std::vector<BackupInfo> backups;
    const auto directory = backup_directory(storage);
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) {
        return ec ?
                   ValueResult<std::vector<BackupInfo>>{
                       failure(fmt::format("Unable to inspect backups: {}", ec.message())), {}} :
                   ValueResult<std::vector<BackupInfo>>{success(), {}};
    }
    try {
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            const std::string name = borealis::io::fs_path_to_string(entry.path().filename());
            if (backup_save_name(name, identity.game, identity.maker) != identity.saveName) {
                continue;
            }
            if (entry.is_regular_file()) {
                backups.push_back({
                    .path = entry.path(),
                    .name = name,
                    .modified = entry.last_write_time(),
                });
            }
        }
    } catch (const std::exception& exception) {
        return {failure(fmt::format("Unable to inspect backups: {}", exception.what())), {}};
    }
    std::ranges::sort(backups, std::greater{}, &BackupInfo::modified);
    return {success(), std::move(backups)};
}

Result restore_backup(
    const Storage& storage, const SaveIdentity& identity, const std::filesystem::path& path) {
    auto artifact = read_artifact(borealis::io::fs_path_to_string(path));
    if (!artifact) {
        return artifact.result;
    }
    if (artifact.value.kind != ArtifactKind::DuskSave) {
        return failure("The selected backup is not a Dusklight save.");
    }
    return apply_artifact(storage, identity, artifact.value, ModDataAction::Replace, false);
}

Result delete_backup(const Storage& storage, const std::filesystem::path& path) {
    if (path.parent_path() != backup_directory(storage) || path.extension() != ".dusksave") {
        return failure("The backup path is invalid.");
    }
    std::error_code ec;
    if (!std::filesystem::remove(path, ec)) {
        return failure(ec ? fmt::format("The backup could not be deleted: {}", ec.message()) :
                            "The backup no longer exists.");
    }
    return success();
}

std::string format_gc_time(uint32_t value) {
    constexpr std::time_t kGcEpoch = 946684800;
    const std::time_t time = kGcEpoch + value;
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    std::array<char, 64> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M", &local);
    return buffer.data();
}

void remove_temporary_export(const ExportArtifact& artifact) {
    if (!artifact.temporary) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(artifact.path, ec);
}

}  // namespace dusk::save_manager

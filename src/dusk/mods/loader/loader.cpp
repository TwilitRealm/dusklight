#include "loader.hpp"

#include "../manifest.hpp"
#include "depgraph.hpp"
#include "native_module.hpp"
#if DUSK_HAS_PREPATCH
#include "prepatch.hpp"
#endif

#include "dusk/config.hpp"
#include "dusk/data.hpp"
#include "dusk/io.hpp"
#include "dusk/logging.h"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/log_buffer.hpp"
#include "dusk/mods/path.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/svc/config.hpp"
#include "dusk/mods/svc/hook.hpp"
#include "dusk/mods/svc/registry.hpp"
#include "dusk/ui/mod_texture_provider.hpp"
#include "dusk/ui/mods_window.hpp"
#include "dusk/ui/ui.hpp"

#include <borealis/io.hpp>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

using namespace std::string_literals;
using namespace std::string_view_literals;
namespace fs = std::filesystem;

#if defined(_WIN32)
#if defined(_M_ARM64)
static constexpr std::string_view k_nativePlatform = "windows-arm64"sv;
#elif defined(_M_X64)
static constexpr std::string_view k_nativePlatform = "windows-amd64"sv;
#elif defined(_M_IX86)
static constexpr std::string_view k_nativePlatform = "windows-x86"sv;
#else
static constexpr std::string_view k_nativePlatform = ""sv;
#endif
static constexpr std::string_view k_nativeLibName = "mod.dll"sv;
#elif defined(__ANDROID__)
#if defined(__aarch64__)
static constexpr std::string_view k_nativePlatform = "android-aarch64"sv;
#elif defined(__x86_64__)
static constexpr std::string_view k_nativePlatform = "android-x86_64"sv;
#else
static constexpr std::string_view k_nativePlatform = ""sv;
#endif
static constexpr std::string_view k_nativeLibName = "mod.so"sv;
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS
static constexpr std::string_view k_nativePlatform = "ios-arm64"sv;
#elif TARGET_OS_TV
static constexpr std::string_view k_nativePlatform = "tvos-arm64"sv;
#elif defined(__aarch64__)
static constexpr std::string_view k_nativePlatform = "macos-arm64"sv;
#elif defined(__x86_64__)
static constexpr std::string_view k_nativePlatform = "macos-x86_64"sv;
#else
static constexpr std::string_view k_nativePlatform = ""sv;
#endif
static constexpr std::string_view k_nativeLibName = "mod.so"sv;
#elif defined(__linux__)
#if defined(__aarch64__)
static constexpr std::string_view k_nativePlatform = "linux-aarch64"sv;
#elif defined(__x86_64__)
static constexpr std::string_view k_nativePlatform = "linux-x86_64"sv;
#elif defined(__i386__)
static constexpr std::string_view k_nativePlatform = "linux-x86"sv;
#else
static constexpr std::string_view k_nativePlatform = ""sv;
#endif
static constexpr std::string_view k_nativeLibName = "mod.so"sv;
#else
static constexpr std::string_view k_nativePlatform = ""sv;
static constexpr std::string_view k_nativeLibName = ""sv;
#endif

namespace dusk::mods {
namespace {
constexpr borealis::Log Log{"dusk::mods::loader"};
ModLoader g_modLoader;
constexpr std::string_view k_nativeLibDir = "lib/"sv;

class DirectoryRollback {
public:
    ~DirectoryRollback() {
        if (!mPath.empty()) {
            std::error_code ec;
            fs::remove_all(mPath, ec);
        }
    }

    void set_path(fs::path path) { mPath = std::move(path); }
    void release() { mPath.clear(); }

private:
    fs::path mPath;
};

std::unique_ptr<ModBundle> load_bundle(const fs::path& modPath, bool fromDir) {
    if (fromDir) {
        return std::make_unique<ModBundleDisk>(modPath);
    } else {
        return std::make_unique<ModBundleZip>(modPath);
    }
}

struct NativeRuntimeLocation {
    std::string entry;
    std::vector<std::string> runtimeEntries;
    bool anyLibs = false;
};

struct NativeLocateFailure {
    NativeModStatus status;
    std::string logMessage;
};

using NativeLocateResult = std::variant<NativeRuntimeLocation, NativeLocateFailure>;

bool has_native_library_extension(std::string_view name) {
    const auto endsWith = [name](std::string_view extension) {
        if (name.size() < extension.size()) {
            return false;
        }
        const auto suffix = name.substr(name.size() - extension.size());
        return std::ranges::equal(suffix, extension, [](char lhs, char rhs) {
            const auto lower = [](char value) {
                return value >= 'A' && value <= 'Z' ? static_cast<char>(value + ('a' - 'A')) :
                                                      value;
            };
            return lower(lhs) == lower(rhs);
        });
    };
    return endsWith(".dll"sv) || endsWith(".so"sv) || endsWith(".dylib"sv);
}

NativeLocateResult locate_native_runtime(ModBundle& bundle) {
    NativeRuntimeLocation result;
    const std::string platformPrefix = fmt::format("{}{}/", k_nativeLibDir, k_nativePlatform);
    const std::string nativeEntry = platformPrefix + std::string{k_nativeLibName};
    for (const auto& name : bundle.getFileNames()) {
        if (name.find('/') == std::string::npos && has_native_library_extension(name)) {
            return NativeLocateFailure{
                NativeModStatus::InvalidBundle,
                fmt::format(
                    "native library '{}' found at the root (natives go in /lib/{{platform}})",
                    name),
            };
        }
        if (!name.starts_with(k_nativeLibDir)) {
            continue;
        }

        const std::string_view libPath{
            name.data() + k_nativeLibDir.size(), name.size() - k_nativeLibDir.size()};
        const auto platformEnd = libPath.find('/');
        if (platformEnd != std::string_view::npos) {
            const auto entryName = libPath.substr(platformEnd + 1);
            if (entryName.find('/') == std::string_view::npos &&
                (entryName == "mod.dll"sv || entryName == "mod.so"sv))
            {
                result.anyLibs = true;
            }
        }

        if (!k_nativePlatform.empty() && name.starts_with(platformPrefix)) {
            const std::string_view relativeName{
                name.data() + platformPrefix.size(), name.size() - platformPrefix.size()};
            if (!is_safe_resource_path(relativeName)) {
                continue;
            }
            result.runtimeEntries.push_back(name);
        }
        if (name == nativeEntry) {
            result.entry = name;
        }
    }
    std::ranges::sort(result.runtimeEntries);
    result.runtimeEntries.erase(
        std::unique(result.runtimeEntries.begin(), result.runtimeEntries.end()),
        result.runtimeEntries.end());
    return result;
}

void complete_operation(const std::shared_ptr<ModOperation>& operation, const bool success = true,
    std::string message = {}) {
    if (operation == nullptr) {
        return;
    }
    operation->state = success ? ModOperation::State::Succeeded : ModOperation::State::Failed;
    operation->message = std::move(message);
}

LoadedMod::FileIdentity file_identity(const fs::path& path) {
    std::error_code error;
    const bool isDirectory = fs::is_directory(path, error);
    if (error) {
        return {};
    }
    const auto size = isDirectory ? 0 : fs::file_size(path, error);
    if (error) {
        return {};
    }
    const auto modified = fs::last_write_time(path, error);
    if (error) {
        return {};
    }
    return {.size = size, .modified = modified, .valid = true};
}

}  // namespace

ModLoader& ModLoader::instance() {
    return g_modLoader;
}

class InvalidModDataException : public std::runtime_error {
public:
    explicit InvalidModDataException(const std::string& msg) : runtime_error(msg) {}
    explicit InvalidModDataException(const char* msg) : runtime_error(msg) {}
};

static void validate_mod_id(std::string_view const str) {
    if (str.empty()) {
        throw InvalidModDataException("Missing ID value in mod metadata!");
    }

    bool lastWasPeriod = false;
    for (auto const chr : str) {
        if (chr == '.') {
            if (lastWasPeriod) {
                throw InvalidModDataException("Cannot have two consecutive periods in mod ID!");
            }
            lastWasPeriod = true;
            continue;
        }

        lastWasPeriod = false;

        if (chr == '_')
            continue;

        if (chr >= '0' && chr <= '9')
            continue;

        if (chr >= 'a' && chr <= 'z')
            continue;

        if (chr >= 'A' && chr <= 'Z')
            continue;

        throw InvalidModDataException(
            fmt::format("Invalid character '{}' in mod ID. Valid characters are period, "
                        "underscore, and alphanumerics.",
                chr));
    }
}

static bool bundle_has_file(ModBundle& bundle, const std::string& path) {
    try {
        bundle.getFileSize(path);
        return true;
    } catch (const std::runtime_error&) {
        return false;
    }
}

static std::string resolve_image_path(ModBundle& bundle, const std::string& modId,
    std::string_view key, const std::string& manifestPath, const std::string& defaultPath) {
    if (!manifestPath.empty()) {
        if (!is_safe_resource_path(manifestPath)) {
            log::write(
                modId, LOG_LEVEL_WARN, "invalid {} path '{}' in mod.json", key, manifestPath);
        } else if (!bundle_has_file(bundle, manifestPath)) {
            log::write(
                modId, LOG_LEVEL_WARN, "{} path '{}' not found in bundle", key, manifestPath);
        } else {
            return manifestPath;
        }
    }
    if (bundle_has_file(bundle, defaultPath)) {
        return defaultPath;
    }
    return {};
}

struct LoadedManifest {
    ModMetadata metadata;
    std::optional<DelegatedModRuntime> runtime;
};

static uint16_t parse_runtime_version_component(std::string_view text, std::string_view fieldName) {
    uint32_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || error != std::errc{} || end != text.data() + text.size() ||
        value > UINT16_MAX)
    {
        throw InvalidModDataException(fmt::format("Invalid {} in runtime version pin", fieldName));
    }
    return static_cast<uint16_t>(value);
}

static std::optional<DelegatedModRuntime> parse_runtime(const nlohmann::json& manifest) {
    const auto field = manifest.find("runtime");
    if (field == manifest.end()) {
        return std::nullopt;
    }
    if (!field->is_string()) {
        throw InvalidModDataException("runtime must be a string");
    }

    const std::string pin = field->get<std::string>();
    const auto at = pin.rfind('@');
    if (at == std::string::npos || at == 0 || at + 1 == pin.size() || pin.find('@') != at ||
        at >= MOD_META_SERVICE_ID_SIZE)
    {
        throw InvalidModDataException(
            "runtime must be a service id followed by @major or @major.minor");
    }

    const std::string_view version{pin.data() + at + 1, pin.size() - at - 1};
    const auto dot = version.find('.');
    if (dot != std::string_view::npos && version.find('.', dot + 1) != std::string_view::npos) {
        throw InvalidModDataException("runtime version pin has too many components");
    }

    DelegatedModRuntime result;
    result.id = pin.substr(0, at);
    result.major = parse_runtime_version_component(
        dot == std::string_view::npos ? version : version.substr(0, dot), "major version");
    if (dot != std::string_view::npos) {
        result.minMinor = parse_runtime_version_component(version.substr(dot + 1), "minor version");
    }
    return result;
}

static LoadedManifest load_manifest(const std::filesystem::path& modPath, ModBundle& bundle) {
    const auto metaJson = bundle.readFile("mod.json");
    auto j = nlohmann::json::parse(metaJson);

    std::string metaId = j.value("id", "");
    std::string metaName = j.value("name", "");
    std::string metaVersion = j.value("version", "");
    std::string metaAuthor = j.value("author", "");
    std::string metaDescription = j.value("description", "");
    std::string metaIcon = j.value("icon", "");
    std::string metaBanner = j.value("banner", "");

    validate_mod_id(metaId);

    if (metaName.empty()) {
        metaName = borealis::io::fs_path_to_string(modPath.stem());
    }
    if (metaVersion.empty()) {
        metaVersion = "?"s;
    }
    if (metaAuthor.empty()) {
        metaAuthor = "unknown"s;
    }

    std::string iconPath = resolve_image_path(bundle, metaId, "icon", metaIcon, "res/icon.png"s);
    std::string bannerPath =
        resolve_image_path(bundle, metaId, "banner", metaBanner, "res/banner.png"s);

    return LoadedManifest{
        .metadata =
            {
                std::move(metaId),
                std::move(metaName),
                std::move(metaVersion),
                std::move(metaAuthor),
                std::move(metaDescription),
                std::move(iconPath),
                std::move(bannerPath),
            },
        .runtime = parse_runtime(j),
    };
}

bool inspect_mod_bundle(
    const fs::path& path, ModMetadata& metadata, std::string& error, bool* hasNative) noexcept {
    try {
        auto bundle = load_bundle(path, false);
        metadata = load_manifest(path, *bundle).metadata;
        if (hasNative != nullptr) {
            *hasNative = std::ranges::any_of(bundle->getFileNames(),
                [](const auto& name) { return has_native_library_extension(name); });
        }
        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
    } catch (...) {
        error = "Unknown bundle validation error";
    }
    return false;
}

// True if the first `capacity` bytes of `str` contain a NUL.
static bool terminated_within(const char* str, size_t capacity) {
    return std::memchr(str, '\0', capacity) != nullptr;
}

static bool parse_meta(NativeMod& native, LoadedMod& mod) {
    const ModMeta* meta = native.meta;
    if (meta->struct_size < sizeof(ModMeta)) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "mod_meta descriptor has invalid size {}",
            meta->struct_size);
        mod.nativeStatus = NativeModStatus::InvalidMetadata;
        return false;
    }
    const auto* cursor = static_cast<const uint8_t*>(meta->records_begin);
    const auto* end = static_cast<const uint8_t*>(meta->records_end);
    if (cursor == nullptr || end == nullptr || cursor > end ||
        (reinterpret_cast<uintptr_t>(cursor) & 7) != 0)
    {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "mod_meta section bounds are invalid");
        mod.nativeStatus = NativeModStatus::InvalidMetadata;
        return false;
    }

    ModMetaParsed parsed;
    size_t headerCount = 0;
    const auto invalid = [&](std::string_view why) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "invalid metadata record at offset {}: {}",
            cursor - static_cast<const uint8_t*>(meta->records_begin), why);
        mod.nativeStatus = NativeModStatus::InvalidMetadata;
        return false;
    };

    while (cursor < end) {
        if (end - cursor < 8) {
            return invalid("trailing bytes");
        }
        uint64_t first = 0;
        std::memcpy(&first, cursor, sizeof(first));
        if (first == 0) {  // linker padding / bounds sentinel
            cursor += 8;
            continue;
        }

        const auto* rec = reinterpret_cast<const ModMetaRecord*>(cursor);
        const size_t size = rec->size;
        if (size < 8 || size % 8 != 0 || size > static_cast<size_t>(end - cursor)) {
            return invalid("bad record size");
        }

        switch (rec->kind) {
        case MOD_META_PAD:
            break;
        case MOD_META_HEADER: {
            if (size < sizeof(ModMetaHeader)) {
                return invalid("truncated header record");
            }
            const auto* header = reinterpret_cast<const ModMetaHeader*>(rec);
            ++headerCount;
            parsed.abiVersion = header->abi_version;
            break;
        }
        case MOD_META_IMPORT: {
            if (size < sizeof(ModMetaImport)) {
                return invalid("truncated import record");
            }
            auto* record = reinterpret_cast<ModMetaImport*>(const_cast<uint8_t*>(cursor));
            if (!terminated_within(record->service_id.chars, sizeof(record->service_id.chars))) {
                return invalid("unterminated import service id");
            }
            parsed.imports.push_back(record);
            break;
        }
        case MOD_META_EXPORT: {
            if (size < sizeof(ModMetaExport)) {
                return invalid("truncated export record");
            }
            auto* record = reinterpret_cast<ModMetaExport*>(const_cast<uint8_t*>(cursor));
            if (!terminated_within(record->service_id.chars, sizeof(record->service_id.chars))) {
                return invalid("unterminated export service id");
            }
            parsed.exports.push_back(record);
            break;
        }
        case MOD_META_HOOK_FN: {
            if (size < sizeof(ModMetaHookFn)) {
                return invalid("truncated hook record");
            }
            parsed.hookFns.push_back(
                reinterpret_cast<ModMetaHookFn*>(const_cast<uint8_t*>(cursor)));
            break;
        }
        case MOD_META_HOOK_MEM: {
            if (size <= sizeof(ModMetaHookMem)) {
                return invalid("truncated hook record");
            }
            auto* record = reinterpret_cast<ModMetaHookMem*>(const_cast<uint8_t*>(cursor));
            const char* strings = reinterpret_cast<const char*>(cursor) + sizeof(ModMetaHookMem);
            const size_t capacity = size - sizeof(ModMetaHookMem);
            if (!terminated_within(strings, capacity)) {
                return invalid("unterminated hook vtable symbol");
            }
            const size_t vtableLen = std::char_traits<char>::length(strings);
            if (!terminated_within(strings + vtableLen + 1, capacity - vtableLen - 1)) {
                return invalid("unterminated hook display name");
            }
            parsed.hookMems.push_back(record);
            break;
        }
        case MOD_META_HOOK_MEM_EXT: {
            if (size <= sizeof(ModMetaHookMemExt)) {
                return invalid("truncated extended hook record");
            }
            auto* record = reinterpret_cast<ModMetaHookMemExt*>(const_cast<uint8_t*>(cursor));
            if (record->pmf_size <= MOD_META_HOOK_MEM_CAPACITY ||
                record->pmf_size > MOD_META_HOOK_MEM_EXT_CAPACITY || record->materialize == nullptr)
            {
                return invalid("bad extended hook member-pointer size");
            }
            const char* strings = reinterpret_cast<const char*>(cursor) + sizeof(ModMetaHookMemExt);
            const size_t capacity = size - sizeof(ModMetaHookMemExt);
            if (!terminated_within(strings, capacity)) {
                return invalid("unterminated extended hook vtable symbol");
            }
            const size_t vtableLen = std::char_traits<char>::length(strings);
            if (!terminated_within(strings + vtableLen + 1, capacity - vtableLen - 1)) {
                return invalid("unterminated extended hook display name");
            }
            parsed.hookMemExts.push_back(record);
            break;
        }
        case MOD_META_HOOK_NAME: {
            if (size <= sizeof(ModMetaHookName)) {
                return invalid("truncated hook record");
            }
            auto* record = reinterpret_cast<ModMetaHookName*>(const_cast<uint8_t*>(cursor));
            const char* name = reinterpret_cast<const char*>(cursor) + sizeof(ModMetaHookName);
            if (!terminated_within(name, size - sizeof(ModMetaHookName))) {
                return invalid("unterminated hook symbol name");
            }
            parsed.hookNames.push_back(record);
            break;
        }
        default:
            // Additive record kinds may appear within a format version; skip them.
            log::write(mod.metadata.id, LOG_LEVEL_DEBUG, "skipping unknown metadata record kind {}",
                rec->kind);
            break;
        }
        cursor += size;
    }

    if (headerCount != 1) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "expected 1 metadata header record, found {}",
            headerCount);
        mod.nativeStatus = NativeModStatus::InvalidMetadata;
        return false;
    }
    if (parsed.abiVersion != MOD_ABI_VERSION) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "expects ABI v{} but engine is v{}, skipping",
            parsed.abiVersion, MOD_ABI_VERSION);
        mod.nativeStatus = NativeModStatus::ApiVersionMismatch;
        return false;
    }

    native.parsed = std::move(parsed);
    return true;
}

static std::string lifecycle_error_message(
    const char* fnName, const ModResult result, const ModError& error) {
    if (error.message[0] != '\0') {
        return error.message;
    }
    return fmt::format("{} failed with result {}", fnName, static_cast<int>(result));
}

static std::string native_status_message(const NativeModStatus status) {
    switch (status) {
    case NativeModStatus::BuildDisabled:
        return "Code mods are disabled on this Dusklight build";
    case NativeModStatus::ModMissingPlatform:
        return fmt::format("Mod not supported on this platform ({})", k_nativePlatform);
    case NativeModStatus::ApiVersionMismatch:
        // TODO: differentiate whether mod or Dusklight is out of date
        return "Mod ABI version mismatch";
    case NativeModStatus::MissingExport:
        return "Missing required mod API exports";
    case NativeModStatus::InvalidMetadata:
        return "Invalid mod metadata records";
    case NativeModStatus::InvalidBundle:
        return "Invalid mod bundle layout (old mod?)";
    case NativeModStatus::Unknown:
        return "Unknown mod load failure";
    case NativeModStatus::None:
    case NativeModStatus::Loaded:
        break;
    }
    return "native mod failed to load";
}

fs::path ModLoader::external_native_lib_path(const LoadedMod& mod) const {
    if (k_nativeLibName.empty()) {
        return {};
    }
    const auto& libDir = m_searchDirs[mod.searchDirIndex].nativeLibDir;
    if (libDir.empty()) {
        return {};
    }
    const auto filename = fmt::format("{}{}", mod.metadata.id,
        borealis::io::fs_path_to_string(fs::path{k_nativeLibName}.extension()));
    fs::path path = libDir / fs::path{filename};
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) {
        return {};
    }
    return path;
}

void ModLoader::load_native(
    LoadedMod& mod, const std::string& dllEntry, const std::vector<std::string>& runtimeEntries) {
    if (!EnableCodeMods) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "Code mods are not available in this build");
        mod.nativeStatus = NativeModStatus::BuildDisabled;
        return;
    }

    const fs::path cacheDir = m_cacheDir / mod.metadata.id;
    const fs::path scratchDir = cacheDir / "data";
    std::error_code ec;
    fs::create_directories(scratchDir, ec);
    if (ec) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to create mod directory {}: {}",
            data::abbreviated_path_string(scratchDir), ec.message());
        return;
    }
    mod.dir = fs::absolute(scratchDir);
    mod.dirUtf8 = borealis::io::fs_path_to_string(mod.dir);

    fs::path libPath;
    fs::path runtimeDir;
    DirectoryRollback runtimeDirRollback;
    if (mod.nativeInPlace) {
        if (!dllEntry.empty()) {
            libPath = mod.modPath / dllEntry;
        } else if (auto external = external_native_lib_path(mod); !external.empty()) {
            libPath = std::move(external);
        } else {
            log::write(mod.metadata.id, LOG_LEVEL_ERROR,
                "no native library named {} found; skipping", k_nativeLibName);
            mod.nativeStatus = NativeModStatus::ModMissingPlatform;
            return;
        }
        runtimeDir = libPath.parent_path();
    } else {
        if (dllEntry.empty()) {
            log::write(mod.metadata.id, LOG_LEVEL_ERROR,
                "no native library named {} found; skipping", k_nativeLibName);
            mod.nativeStatus = NativeModStatus::ModMissingPlatform;
            return;
        }

        // Every generation gets a new directory. The main module and all of its runtime
        // libraries therefore have fresh paths and can coexist with a previous generation
        // that is still unwinding after a reload.
        runtimeDir = cacheDir / fmt::format("g{}", ++mod.cacheGeneration);
        runtimeDirRollback.set_path(runtimeDir);
        fs::create_directories(runtimeDir, ec);
        if (ec) {
            log::write(mod.metadata.id, LOG_LEVEL_ERROR,
                "failed to create native runtime directory {}: {}",
                data::abbreviated_path_string(runtimeDir), ec.message());
            return;
        }

        const std::string platformPrefix = fmt::format("{}{}/", k_nativeLibDir, k_nativePlatform);
        for (const auto& entry : runtimeEntries) {
            if (!entry.starts_with(platformPrefix)) {
                continue;
            }
            const std::string_view relativeName{
                entry.data() + platformPrefix.size(), entry.size() - platformPrefix.size()};
            if (!is_safe_resource_path(relativeName)) {
                log::write(mod.metadata.id, LOG_LEVEL_ERROR,
                    "unsafe native runtime path '{}'; skipping", entry);
                return;
            }

            const fs::path outputPath = runtimeDir / fs::path{relativeName};
            fs::create_directories(outputPath.parent_path(), ec);
            if (ec) {
                log::write(mod.metadata.id, LOG_LEVEL_ERROR,
                    "failed to create directory for {}: {}", entry, ec.message());
                return;
            }

            std::vector<u8> data;
            try {
                data = mod.bundle->readFile(entry);
            } catch (const std::exception& e) {
                log::write(
                    mod.metadata.id, LOG_LEVEL_ERROR, "failed to extract {}: {}", entry, e.what());
                return;
            }

            std::ofstream out(outputPath, std::ios::binary | std::ios::out);
            if (!out) {
                log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to write {}", entry);
                return;
            }
            out.write(reinterpret_cast<const char*>(data.data()),
                static_cast<std::streamsize>(data.size()));
            if (!out) {
                log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to write {}", entry);
                return;
            }
        }

        libPath = runtimeDir / fs::path{dllEntry}.filename();
    }

    auto nativeMod = std::make_unique<NativeMod>();
    try {
        nativeMod->handle = std::make_unique<loader::NativeModule>(libPath);
    } catch (const std::runtime_error& e) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to open {}: {}",
            data::abbreviated_path_string(libPath), e.what());
        return;
    }

    nativeMod->meta = nativeMod->handle->LookupSymbol<const ModMeta*>("mod_meta");
    nativeMod->contextSymbol = nativeMod->handle->LookupSymbol<ModContext**>("mod_ctx");
    nativeMod->fn_initialize = nativeMod->handle->LookupSymbol<ModInitializeFn>("mod_initialize");
    nativeMod->fn_update = nativeMod->handle->LookupSymbol<ModUpdateFn>("mod_update");
    nativeMod->fn_shutdown = nativeMod->handle->LookupSymbol<ModShutdownFn>("mod_shutdown");

    if (!nativeMod->meta || !nativeMod->contextSymbol || !nativeMod->fn_initialize ||
        !nativeMod->fn_update || !nativeMod->fn_shutdown)
    {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR,
            "{} missing required mod API exports; skipping",
            data::abbreviated_path_string(libPath));
        mod.nativeStatus = NativeModStatus::MissingExport;
        return;
    }

    if (!parse_meta(*nativeMod, mod)) {
        return;
    }

    if (nativeMod->contextSymbol == nullptr) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "missing required mod_ctx export");
        mod.nativeStatus = NativeModStatus::MissingExport;
        return;
    }
    *nativeMod->contextSymbol = mod.context.get();

    mod.nativePath = fs::absolute(libPath);
    mod.nativeDir = fs::absolute(runtimeDir);
    mod.nativeDirUtf8 = borealis::io::fs_path_to_string(mod.nativeDir);
    mod.native = std::move(nativeMod);
    mod.nativeStatus = NativeModStatus::Loaded;
    runtimeDirRollback.release();
}

bool ModLoader::load_native_if_present(LoadedMod& mod) {
    const auto result = locate_native_runtime(*mod.bundle);
    if (const auto* failure = std::get_if<NativeLocateFailure>(&result)) {
        mod.nativeStatus = failure->status;
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "{}", failure->logMessage);
        fail_mod(mod, MOD_ERROR, native_status_message(failure->status));
        return false;
    }

    const auto& native = std::get<NativeRuntimeLocation>(result);
    if (mod.runtime.has_value() &&
        (native.anyLibs || (mod.nativeInPlace && !external_native_lib_path(mod).empty())))
    {
        mod.nativeStatus = NativeModStatus::InvalidBundle;
        fail_mod(mod, MOD_CONFLICT, "A mod cannot declare both runtime and native code");
        return false;
    }
    if (!native.anyLibs && !(mod.nativeInPlace && !external_native_lib_path(mod).empty())) {
        mod.nativeStatus = NativeModStatus::None;
        return true;
    }

    mod.nativeStatus = NativeModStatus::Unknown;
    load_native(mod, native.entry, native.runtimeEntries);
    if (mod.nativeStatus != NativeModStatus::Loaded) {
        fail_mod(mod, MOD_ERROR, native_status_message(mod.nativeStatus));
        return false;
    }
    return true;
}

void ModLoader::unload_native(LoadedMod& mod) {
    if (!mod.native || mod.nativeInPlace) {
        return;
    }
    // Deferred dlclose: this mod's code may still be on the stack below the current tick
    m_retiredNatives.push_back({std::move(mod.native), std::move(mod.nativeDir)});
    mod.nativePath.clear();
    mod.nativeDir.clear();
    mod.nativeDirUtf8.clear();
}

void ModLoader::drain_retired_natives() {
    for (auto& retired : m_retiredNatives) {
        retired.native.reset();
        if (!retired.directory.empty()) {
            std::error_code ec;
            fs::remove_all(retired.directory, ec);
        }
    }
    m_retiredNatives.clear();
}

static ModManifestInfo build_manifest_info(const ModMetaParsed& parsed) {
    ModManifestInfo info;
    info.imports.reserve(parsed.imports.size());
    for (const auto* record : parsed.imports) {
        if (!svc::valid_service_id(record->service_id.chars)) {
            continue;
        }
        info.imports.push_back({
            .id = record->service_id.chars,
            .major = record->major_version,
            .minMinor = record->min_minor_version,
            .required = (record->rec.flags & SERVICE_IMPORT_OPTIONAL) == 0,
        });
    }
    info.exports.reserve(parsed.exports.size());
    for (const auto* record : parsed.exports) {
        if (!svc::valid_service_id(record->service_id.chars)) {
            continue;
        }
        info.exports.push_back({
            .id = record->service_id.chars,
            .major = record->major_version,
        });
    }
    return info;
}

std::string escape_mod_id_for_config(std::string_view const id) {
    std::string buf;

    // Simple escaping. All characters in mod IDs literal, except for '.' and '_'.
    // '.' -> '_', '_' -> '__'
    for (char const chr : id) {
        if (chr == '.') {
            buf.push_back('_');
        } else if (chr == '_') {
            buf.push_back('_');
            buf.push_back('_');
        } else {
            buf.push_back(chr);
        }
    }

    return buf;
}

static std::string mod_enabled_cvar_name(std::string_view const id) {
    return fmt::format("mod.{}.enabled", escape_mod_id_for_config(id));
}

static bool required_deps_active(const LoadedMod& mod) {
    return std::ranges::all_of(mod.dependencies,
        [](const ModDependencyEdge& edge) { return !edge.required || edge.mod->active; });
}

// A deferred export that was not published by the end of the provider's initialization can
// never resolve, which is almost certainly a bug in the provider.
static void warn_unpublished_deferred_exports(const LoadedMod& mod) {
    if (!mod.active || !mod.native) {
        return;
    }

    for (const auto* serviceExport : mod.native->parsed.exports) {
        if ((serviceExport->rec.flags & SERVICE_EXPORT_DEFERRED) == 0) {
            continue;
        }
        const auto* record =
            svc::find_service_record(serviceExport->service_id.chars, serviceExport->major_version);
        if (record != nullptr && record->service == nullptr) {
            log::write(mod.metadata.id, LOG_LEVEL_WARN,
                "declared deferred service '{}@{}' but never published it during initialization",
                serviceExport->service_id.chars, serviceExport->major_version);
        }
    }
}

LoadedMod* ModLoader::try_load_mod(const fs::path& modPath, bool fromDir, uint32_t searchDirIndex,
    std::unique_ptr<ModBundle> bundle) {
    if (bundle == nullptr) {
        try {
            bundle = load_bundle(modPath, fromDir);
        } catch (const std::exception& e) {
            Log.error(
                "Failed to open {} bundle: {}", data::abbreviated_path_string(modPath), e.what());
            return nullptr;
        }
    }

    LoadedManifest manifest;
    try {
        manifest = load_manifest(modPath, *bundle);
    } catch (const std::exception& e) {
        Log.error("bad mod.json in {}: {}", data::abbreviated_path_string(modPath), e.what());
        return nullptr;
    }

    if (const auto* existing = find_mod(manifest.metadata.id)) {
        if (existing->searchDirIndex < searchDirIndex) {
            log::write(manifest.metadata.id, LOG_LEVEL_INFO,
                "{} shadowed by higher-priority duplicate {}",
                data::abbreviated_path_string(modPath),
                data::abbreviated_path_string(existing->modPath));
        } else {
            log::write(manifest.metadata.id, LOG_LEVEL_ERROR, "duplicate mod id, not loading {}",
                data::abbreviated_path_string(modPath));
        }
        return nullptr;
    }

    const auto& inserted = m_mods.emplace_back(std::make_unique<LoadedMod>());
    auto& mod = *inserted;
    mod.active = true;
    mod.modPath = fs::absolute(modPath);
    mod.searchDirIndex = searchDirIndex;
    mod.nativeInPlace = m_searchDirs[searchDirIndex].inPlaceNative && fromDir;
    mod.fileIdentity = file_identity(modPath);
    mod.metadata = std::move(manifest.metadata);
    mod.runtime = std::move(manifest.runtime);
    mod.bundle = std::move(bundle);
    mod.context = std::make_unique<ModContext>();
    mod.context->mod = &mod;
    mod.cvarIsEnabled =
        std::make_unique<ConfigVar<bool>>(mod_enabled_cvar_name(mod.metadata.id), true);
    if (mod.runtime.has_value()) {
        const auto& runtime = *mod.runtime;
        mod.manifestInfo.imports.push_back({runtime.id, runtime.major, runtime.minMinor, true});

        std::error_code ec;
        mod.dir = fs::absolute(m_cacheDir / mod.metadata.id / "data", ec);
        if (!ec) {
            fs::create_directories(mod.dir, ec);
        }
        if (ec) {
            fail_mod(mod, MOD_ERROR,
                fmt::format("Failed to create script scratch directory: {}", ec.message()));
        } else {
            mod.dirUtf8 = borealis::io::fs_path_to_string(mod.dir);
        }
    }
    if (load_native_if_present(mod) && mod.native) {
        mod.manifestInfo = build_manifest_info(mod.native->parsed);
    }

    log::write(mod.metadata.id, LOG_LEVEL_INFO, "found '{}' v{} by {} ({})", mod.metadata.name,
        mod.metadata.version, mod.metadata.author, data::abbreviated_path_string(modPath));
    return &mod;
}

bool ModLoader::activate_mod(LoadedMod& mod) {
    log::write(mod.metadata.id, LOG_LEVEL_INFO, "activating mod");
    mod.active = true;

    // Asset-only mods have no lifecycle beyond their overlay files.
    if (!mod.native && !mod.runtime.has_value()) {
        mod.enabledApplied = true;
        return true;
    }

    if (mod.native && !mod.servicesRegistered) {
        if (!register_static_service_exports(mod)) {
            log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to register service exports");
            deactivate_mod(mod);
            return false;
        }
        mod.servicesRegistered = true;
    }

    if (mod.native && !resolve_service_imports(mod)) {
        log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to resolve service imports");
        deactivate_mod(mod);
        return false;
    }

    if (mod.native) {
        svc::hook_resolve_mod_records(mod);
        *mod.native->contextSymbol = mod.context.get();
    } else {
        auto& runtime = *mod.runtime;
        const auto* record = svc::find_service(runtime.id.c_str(), runtime.major, runtime.minMinor);
        if (record == nullptr) {
            fail_mod(mod, MOD_UNAVAILABLE,
                describe_missing_import(runtime.id.c_str(), runtime.major, runtime.minMinor));
            deactivate_mod(mod);
            return false;
        }

        const auto* service = static_cast<const ModRuntimeService*>(record->service);
        constexpr size_t kRequiredSize = offsetof(ModRuntimeService, deactivate) +
                                         sizeof(((ModRuntimeService*)nullptr)->deactivate);
        if (record->provider == nullptr || service == nullptr ||
            service->header.struct_size < kRequiredSize || service->activate == nullptr ||
            service->update == nullptr || service->deactivate == nullptr)
        {
            fail_mod(mod, MOD_UNAVAILABLE,
                fmt::format("Runtime service {}@{} has an invalid lifecycle contract", runtime.id,
                    runtime.major));
            deactivate_mod(mod);
            return false;
        }
        runtime.service = service;
        runtime.providerContext = record->provider->context.get();
    }

    const char* initializeName = mod.native ? "mod_initialize" : "runtime activate";
    log::write(mod.metadata.id, LOG_LEVEL_TRACE, "calling {}", initializeName);
    try {
        ModError error = MOD_ERROR_INIT;
        const auto result = mod.native ?
                                mod.native->fn_initialize(&error) :
                                mod.runtime->service->activate(
                                    mod.runtime->providerContext, mod.context.get(), &error);
        if (result == MOD_OK && !mod.loadFailed) {
            mod.initialized = true;
            log::write(mod.metadata.id, LOG_LEVEL_TRACE, "{} succeeded", initializeName);
        } else if (result != MOD_OK && !mod.loadFailed) {
            fail_mod(mod, result, lifecycle_error_message(initializeName, result, error));
        }
    } catch (const std::exception& e) {
        fail_mod(mod, MOD_ERROR, fmt::format("Exception in {}: {}", initializeName, e.what()));
    } catch (...) {
        fail_mod(mod, MOD_ERROR, fmt::format("Unknown exception in {}", initializeName));
    }

    warn_unpublished_deferred_exports(mod);

    if (!mod.active) {
        // Failed initialization may have left hooks or other service state behind
        deactivate_mod(mod);
        return false;
    }

    mod.enabledApplied = true;
    return true;
}

void ModLoader::deactivate_mod(LoadedMod& mod) {
    svc::modules_mod_deactivating(mod);
    if (mod.initialized && ((mod.native && mod.native->fn_shutdown) ||
                               (mod.runtime.has_value() && mod.runtime->service != nullptr)))
    {
        const char* shutdownName = mod.native ? "mod_shutdown" : "runtime deactivate";
        log::write(mod.metadata.id, LOG_LEVEL_TRACE, "calling {}", shutdownName);
        try {
            ModError error = MOD_ERROR_INIT;
            const auto result = mod.native ?
                                    mod.native->fn_shutdown(&error) :
                                    mod.runtime->service->deactivate(
                                        mod.runtime->providerContext, mod.context.get(), &error);
            if (result == MOD_OK) {
                log::write(mod.metadata.id, LOG_LEVEL_TRACE, "{} succeeded", shutdownName);
            } else {
                log::write(mod.metadata.id, LOG_LEVEL_ERROR, "{} failed: {}", shutdownName,
                    lifecycle_error_message(shutdownName, result, error));
            }
        } catch (const std::exception& exception) {
            log::write(
                mod.metadata.id, LOG_LEVEL_ERROR, "{} threw: {}", shutdownName, exception.what());
        } catch (...) {
            log::write(
                mod.metadata.id, LOG_LEVEL_ERROR, "{} threw an unknown exception", shutdownName);
        }
    }
    mod.initialized = false;
    if (mod.runtime.has_value()) {
        mod.runtime->service = nullptr;
        mod.runtime->providerContext = nullptr;
    }

    if (mod.servicesRegistered) {
        svc::remove_services_for_provider(mod);
        mod.servicesRegistered = false;
    }
    svc::modules_mod_detached(mod);
    unload_native(mod);

    mod.active = false;
    mod.enabledApplied = false;
}

void ModLoader::init() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    manifest::initialize();
#if DUSK_HAS_PREPATCH
    prepatch::initialize();
#endif

    if (m_searchDirs.empty()) {
        Log.warn("no mod search directories configured; mod loading skipped");
        return;
    }

    if (m_cacheDir.empty()) {
        m_cacheDir = m_searchDirs.front().path / ".cache";
    }

    std::error_code ec;

    // Stale libs from previous sessions (see load_native).
    fs::remove_all(m_cacheDir, ec);

    // A Windows update can be interrupted between moving the live archive aside and publishing
    // its replacement. Recover that narrow crash window before scanning the user directory.
    if (fs::is_directory(m_searchDirs.front().path, ec)) {
        for (const auto& entry : fs::directory_iterator(m_searchDirs.front().path, ec)) {
            const auto path = entry.path();
            if (!entry.is_regular_file() || path.extension() != ".old" ||
                path.stem().extension() != ".dusk")
            {
                continue;
            }
            auto primary = path;
            primary.replace_extension();
            if (fs::exists(primary, ec)) {
                fs::remove(path, ec);
            } else {
                fs::rename(path, primary, ec);
            }
            if (ec) {
                Log.warn("failed to recover stale mod archive '{}': {}",
                    data::abbreviated_path_string(path), ec.message());
                ec.clear();
            }
        }
    }

    for (size_t dirIndex = 0; dirIndex < m_searchDirs.size(); ++dirIndex) {
        const auto& searchDir = m_searchDirs[dirIndex];

        // --mods can point the user dir at the bundled dir; don't scan the same dir twice.
        bool alreadyScanned = false;
        for (size_t earlier = 0; earlier < dirIndex && !alreadyScanned; ++earlier) {
            alreadyScanned = fs::equivalent(m_searchDirs[earlier].path, searchDir.path, ec);
        }
        if (alreadyScanned) {
            continue;
        }

        if (!fs::is_directory(searchDir.path)) {
            if (dirIndex == 0) {
                Log.info(
                    "mods directory '{}' not found", data::abbreviated_path_string(searchDir.path));
            } else {
                Log.debug(
                    "mods directory '{}' not found", data::abbreviated_path_string(searchDir.path));
            }
            continue;
        }

        std::vector<fs::directory_entry> entries;
        for (auto& e : fs::directory_iterator(searchDir.path, ec)) {
            if (e.is_directory() && fs::exists(e.path() / "mod.json")) {
                entries.push_back(e);
            } else if (e.is_regular_file() && e.path().extension() == ".dusk") {
                entries.push_back(e);
            }
        }
        std::sort(entries.begin(), entries.end(),
            [](const fs::directory_entry& a, const fs::directory_entry& b) {
                return a.path().filename() < b.path().filename();
            });

        for (auto& entry : entries) {
            (void)try_load_mod(entry.path(), entry.is_directory(), static_cast<uint32_t>(dirIndex));
        }
    }

    if (m_mods.empty()) {
        init_services();
        Log.info("no mods found");
        svc::modules_lifecycle_applied();
        m_startupComplete = true;
        return;
    }

    std::stable_sort(m_mods.begin(), m_mods.end(),
        [](const auto& a, const auto& b) { return a->searchDirIndex > b->searchDirIndex; });

    Log.info("initializing {} mod(s)...", m_mods.size());
    for (auto& mod : mods()) {
        mod.enabledSubscription = Register(*mod.cvarIsEnabled,
            [this, &mod](const bool&, const bool&) { on_enabled_changed(mod); });
    }

    init_services();

    // Providers must initialize (and publish deferred services) before their importers, so
    // imports are resolved per mod, interleaved with initialization, in dependency order.
    loader::sort_mods(m_mods);

    // Decide the startup lifecycle state before publishing exports. Config-disabled mods and
    // mods blocked by required providers keep dependency edges but must not resolve or provide
    // services until they can actually initialize.
    for (auto& mod : mods()) {
        if (!mod.cvarIsEnabled->getValue()) {
            log::write(mod.metadata.id, LOG_LEVEL_INFO, "disabled by config");
            mod.active = false;
            mod.suspendedByProvider = false;
            continue;
        }
        if (!mod.loadFailed && !required_deps_active(mod)) {
            log::write(
                mod.metadata.id, LOG_LEVEL_INFO, "suspended: a required provider is disabled");
            mod.active = false;
            mod.suspendedByProvider = true;
            continue;
        }
        mod.suspendedByProvider = false;
    }

    for (auto& mod : mods()) {
        if (!mod.active || !mod.native) {
            continue;
        }
        if (register_static_service_exports(mod)) {
            mod.servicesRegistered = true;
        } else {
            log::write(mod.metadata.id, LOG_LEVEL_ERROR, "failed to register service exports");
            deactivate_mod(mod);
        }
    }

    for (auto& mod : mods()) {
        if (mod.active) {
            activate_mod(mod);
        }
    }

    svc::modules_lifecycle_applied();

    auto active = std::ranges::count_if(mods(), [](const LoadedMod& m) { return m.active; });
    Log.info("{}/{} mod(s) active", active, m_mods.size());

    m_startupComplete = true;
}

LoadedMod* ModLoader::find_mod(std::string_view id) {
    for (auto& mod : m_mods) {
        if (mod->metadata.id == id) {
            return mod.get();
        }
    }
    return nullptr;
}

const LoadedMod* ModLoader::find_mod(std::string_view id) const {
    for (const auto& mod : m_mods) {
        if (mod->metadata.id == id) {
            return mod.get();
        }
    }
    return nullptr;
}

void ModLoader::request_enable(std::string_view id) {
    if (auto* mod = find_mod(id)) {
        mod->cvarIsEnabled->setValue(true);
    }
}

void ModLoader::request_disable(std::string_view id) {
    if (auto* mod = find_mod(id)) {
        mod->cvarIsEnabled->setValue(false);
    }
}

ModOperationHandle ModLoader::request_reload(std::string_view id) {
    auto operation = std::make_shared<ModOperation>();
    if (find_mod(id) != nullptr) {
        m_pendingRequests.push_back(ReloadRequest{
            .modId = std::string{id},
            .operation = operation,
        });
    } else {
        complete_operation(operation, false, "The mod is no longer installed");
    }
    return operation;
}

ModOperationHandle ModLoader::request_install(fs::path path) {
    auto operation = std::make_shared<ModOperation>();
    m_pendingRequests.push_back(InstallRequest{
        .stagedPath = std::move(path),
        .operation = operation,
    });
    return operation;
}

ModOperationHandle ModLoader::request_uninstall(std::string_view id) {
    auto operation = std::make_shared<ModOperation>();
    if (find_mod(id) != nullptr) {
        m_pendingRequests.push_back(UninstallRequest{
            .modId = std::string{id},
            .operation = operation,
        });
    } else {
        complete_operation(operation);
    }
    return operation;
}

ModOperationHandle ModLoader::request_reactivate(std::string_view id) {
    auto operation = std::make_shared<ModOperation>();
    m_pendingRequests.push_back(LifecycleRequest{
        .modId = std::string{id},
        .action = LifecycleAction::Reactivate,
        .operation = operation,
    });
    return operation;
}

fs::path ModLoader::user_mods_dir() const {
    return m_searchDirs.empty() ? fs::path{} : m_searchDirs.front().path;
}

bool ModLoader::can_uninstall(const LoadedMod& mod) const {
    return mod.searchDirIndex == 0 && mod.modPath.extension() == ".dusk";
}

void ModLoader::notify_mod_failure(LoadedMod& mod, bool firstFailure) {
    if (firstFailure) {
        m_pendingFailures.push_back(mod.metadata.name);
    }
    // Startup failures are handled inline by activate_mod
    if (!m_startupComplete) {
        return;
    }
    m_pendingRequests.push_back(LifecycleRequest{
        .modId = mod.metadata.id,
        .action = LifecycleAction::Disable,
    });
}

void ModLoader::flush_toasts() {
    if (m_pendingFailures.empty()) {
        return;
    }

    const auto names = std::exchange(m_pendingFailures, {});

    // Skip displaying toasts if the mods window is currently open
    if (const auto* window = dynamic_cast<const ui::ModsWindow*>(ui::top_document())) {
        if (window->visible()) {
            return;
        }
    }

    ui::Toast toast{.type = "warning", .duration = std::chrono::seconds{5}};
    if (names.size() == 1) {
        toast.title = "Mod failed";
        toast.content =
            fmt::format("<div><b>{}</b> failed and was disabled.</div><div>Check Mods for "
                        "more information.</div>",
                ui::escape(names.front()));
    } else {
        toast.title = "Mods failed";
        toast.content = fmt::format("<div><b>{} mods</b> failed and were disabled.</div><div>Check "
                                    "Mods for more information.</div>",
            names.size());
    }
    ui::push_toast(std::move(toast));
}

std::vector<LoadedMod*> ModLoader::collect_lifecycle_set(LoadedMod& target) const {
    std::vector included{&target};
    std::vector pending{&target};
    while (!pending.empty()) {
        auto* current = pending.back();
        pending.pop_back();
        for (const auto& edge : current->dependents) {
            auto* dependent = edge.mod;
            if (!dependent->active && !dependent->suspendedByProvider) {
                continue;
            }
            if (std::ranges::find(included, dependent) != included.end()) {
                continue;
            }
            included.push_back(dependent);
            pending.push_back(dependent);
        }
    }

    std::vector<LoadedMod*> ordered;
    ordered.reserve(included.size());
    for (auto& mod : mods()) {
        if (std::ranges::find(included, &mod) != included.end()) {
            ordered.push_back(&mod);
        }
    }
    return ordered;
}

bool ModLoader::ensure_native_loaded(LoadedMod& mod) {
    if (mod.native || mod.nativeStatus == NativeModStatus::None) {
        return true;
    }
    return load_native_if_present(mod);
}

bool ModLoader::reload_bundle(LoadedMod& mod) {
    log::write(mod.metadata.id, LOG_LEVEL_INFO, "reloading from {}",
        data::abbreviated_path_string(mod.modPath));

    std::shared_ptr<ModBundle> newBundle;
    LoadedManifest newManifest;
    try {
        std::error_code ec;
        newBundle = load_bundle(mod.modPath, fs::is_directory(mod.modPath, ec));
        newManifest = load_manifest(mod.modPath, *newBundle);
    } catch (const std::exception& e) {
        fail_mod(mod, MOD_ERROR, fmt::format("Reload failed: {}", e.what()));
        return false;
    }

    if (newManifest.metadata.id != mod.metadata.id) {
        fail_mod(mod, MOD_CONFLICT,
            fmt::format(
                "Mod ID changed on reload ('{}'); restart required", newManifest.metadata.id));
        return false;
    }

    mod.metadata = std::move(newManifest.metadata);
    mod.runtime = std::move(newManifest.runtime);
    // In-flight readers of the old bundle keep it alive through their shared_ptr.
    mod.bundle = std::move(newBundle);
    mod.fileIdentity = file_identity(mod.modPath);
    mod.loadFailed = false;
    mod.failureReason.clear();

    ModManifestInfo newInfo;
    if (mod.runtime.has_value()) {
        const auto& runtime = *mod.runtime;
        newInfo.imports.push_back({runtime.id, runtime.major, runtime.minMinor, true});
    }
    if (!load_native_if_present(mod)) {
        return false;
    }
    if (mod.native) {
        newInfo = build_manifest_info(mod.native->parsed);
    } else {
        ++mod.cacheGeneration;
    }

    if (newInfo != mod.manifestInfo) {
        // The reload changes the mod's imports/exports; rebuild the dependency graph so edges,
        // init/tick/shutdown order and cascade sets reflect the new manifest.
        log::write(mod.metadata.id, LOG_LEVEL_INFO,
            "changed its service imports/exports; rebuilding mod dependency graph");
        mod.manifestInfo = std::move(newInfo);
        loader::sort_mods(m_mods);
    }

    return true;
}

void ModLoader::resume_lifecycle_set(const std::vector<LoadedMod*>& affected) {
    // Publish every candidate's static exports before any initialize, so optional cycles and
    // provider changes use the same ordering rules as startup.
    for (auto* mod : affected) {
        if (mod->active || mod->loadFailed || !mod->cvarIsEnabled->getValue()) {
            continue;
        }
        if (!ensure_native_loaded(*mod)) {
            continue;
        }
        if (mod->native && !mod->servicesRegistered) {
            if (register_static_service_exports(*mod)) {
                mod->servicesRegistered = true;
            } else {
                log::write(mod->metadata.id, LOG_LEVEL_ERROR, "failed to register service exports");
                deactivate_mod(*mod);
            }
        }
    }

    for (auto* mod : affected) {
        if (mod->active || mod->loadFailed || !mod->cvarIsEnabled->getValue()) {
            continue;
        }
        if (!required_deps_active(*mod)) {
            mod->suspendedByProvider = true;
            log::write(
                mod->metadata.id, LOG_LEVEL_INFO, "suspended: a required provider is disabled");
            continue;
        }
        mod->suspendedByProvider = false;
        activate_mod(*mod);
    }

    for (auto* mod : affected) {
        if (!mod->active && mod->servicesRegistered) {
            svc::remove_services_for_provider(*mod);
            mod->servicesRegistered = false;
        }
    }
}

void ModLoader::apply_lifecycle_change(LoadedMod& target, const bool reload) {
    auto affected = collect_lifecycle_set(target);

    // Dependents first (reverse init order), like shutdown.
    for (auto* mod : affected | std::views::reverse) {
        const bool needsTeardown =
            mod->active ||
            (mod == &target && (mod->initialized || (reload && mod->native != nullptr)));
        if (!needsTeardown) {
            continue;
        }
        const bool wasActive = mod->active;
        log::write(mod->metadata.id, LOG_LEVEL_INFO, "deactivating mod");
        deactivate_mod(*mod);
        if (mod != &target && wasActive) {
            // Provisional; cleared below if the mod comes straight back up.
            mod->suspendedByProvider = true;
        }
    }

    if (reload) {
        // On failure the target is failed and stays down; dependents get resume attempts below
        // and suspend against the failed provider where required.
        reload_bundle(target);

        // The reload may have rebuilt the dependency graph and reordered m_mods; refresh the
        // set's iteration order so reactivation still runs providers first.
        std::vector<LoadedMod*> reordered;
        reordered.reserve(affected.size());
        for (auto& mod : mods()) {
            if (std::ranges::find(affected, &mod) != affected.end()) {
                reordered.push_back(&mod);
            }
        }
        affected = std::move(reordered);
    }

    resume_lifecycle_set(affected);
}

void ModLoader::on_enabled_changed(LoadedMod& mod) {
    svc::config_mark_dirty();
    if (mod.loadFailed) {
        if (!mod.cvarIsEnabled->getValue()) {
            mod.loadFailed = false;
            mod.failureReason.clear();
        }
        return;
    }
    if (mod.suspendedByProvider) {
        if (!mod.cvarIsEnabled->getValue()) {
            // The user disabled a suspended mod; stop waiting for its providers.
            mod.suspendedByProvider = false;
        }
        return;
    }
    m_pendingRequests.push_back(LifecycleRequest{
        .modId = mod.metadata.id,
        .action =
            mod.cvarIsEnabled->getValue() ? LifecycleAction::Enable : LifecycleAction::Disable,
    });
}

void ModLoader::forget_mod(LoadedMod& mod) {
    auto affected = collect_lifecycle_set(mod);
    std::vector<LoadedMod*> blocked;
    std::vector<LoadedMod*> pending{&mod};
    while (!pending.empty()) {
        auto* provider = pending.back();
        pending.pop_back();
        for (const auto& edge : provider->dependents) {
            if (!edge.required || edge.mod == nullptr ||
                std::ranges::find(blocked, edge.mod) != blocked.end())
            {
                continue;
            }
            blocked.push_back(edge.mod);
            pending.push_back(edge.mod);
        }
    }

    for (auto* affectedMod : affected | std::views::reverse) {
        const bool wasActive = affectedMod->active;
        if (affectedMod->active || affectedMod->initialized || affectedMod->native != nullptr) {
            log::write(affectedMod->metadata.id, LOG_LEVEL_INFO, "deactivating mod");
            deactivate_mod(*affectedMod);
        }
        if (affectedMod != &mod && wasActive) {
            affectedMod->suspendedByProvider = true;
        }
    }

    const std::string modId = mod.metadata.id;
    auto* removedMod = &mod;
    if (mod.enabledSubscription != 0) {
        config::unsubscribe(mod.enabledSubscription);
        mod.enabledSubscription = 0;
    }
    unregister(*mod.cvarIsEnabled);

    std::vector<LoadedMod*> remaining;
    remaining.reserve(affected.size());
    for (auto* affectedMod : affected) {
        if (affectedMod != &mod) {
            remaining.push_back(affectedMod);
        }
    }
    std::erase_if(
        m_mods, [removedMod](const auto& candidate) { return candidate.get() == removedMod; });
    loader::sort_mods(m_mods);

    std::vector<LoadedMod*> resumable;
    for (auto* affectedMod : remaining) {
        const bool needsRemovedProvider = std::ranges::find(blocked, affectedMod) != blocked.end();
        affectedMod->suspendedByProvider = needsRemovedProvider;
        if (needsRemovedProvider) {
            log::write(affectedMod->metadata.id, LOG_LEVEL_INFO,
                "suspended: required provider '{}' was removed", modId);
        } else {
            resumable.push_back(affectedMod);
        }
    }
    resume_lifecycle_set(resumable);

    ++m_generation;
    log::write(modId, LOG_LEVEL_INFO, "forgot removed package");
}

ModLoader::OperationResult ModLoader::load_runtime_mod(const fs::path& requestedPath) {
    const auto path = fs::absolute(requestedPath).lexically_normal();
    std::error_code error;
    const auto status = fs::status(path, error);
    if (error || !fs::exists(status)) {
        return {
            .success = false,
            .message = error ? fmt::format("Could not inspect the package: {}", error.message()) :
                               "The package was not found",
        };
    }

    const bool fromDir = fs::is_directory(status);
    std::unique_ptr<ModBundle> bundle;
    std::optional<ModMetadata> metadata;
    try {
        bundle = load_bundle(path, fromDir);
        metadata = load_manifest(path, *bundle).metadata;
    } catch (const std::exception& exception) {
        return {
            .success = false,
            .message = fmt::format("Invalid mod package: {}", exception.what()),
        };
    }
    if (const auto* duplicate = find_mod(metadata->id)) {
        return {
            .success = false,
            .message = fmt::format("A mod with this ID is already loaded from {}",
                data::abbreviated_path_string(duplicate->modPath)),
        };
    }
    auto* mod = try_load_mod(path, fromDir, 0, std::move(bundle));
    if (mod == nullptr) {
        return {
            .success = false,
            .message = "The mod could not be loaded",
        };
    }

    mod->enabledSubscription = Register(
        *mod->cvarIsEnabled, [this, mod](const bool&, const bool&) { on_enabled_changed(*mod); });
    loader::sort_mods(m_mods);
    if (!mod->cvarIsEnabled->getValue()) {
        mod->active = false;
        mod->suspendedByProvider = false;
        log::write(mod->metadata.id, LOG_LEVEL_INFO, "installed disabled by config");
    } else if (!mod->loadFailed) {
        mod->active = false;
        apply_lifecycle_change(*mod, false);
    }
    ++m_generation;
    log::write(mod->metadata.id, LOG_LEVEL_INFO, "installed at runtime");
    return runtime_result(*mod);
}

ModLoader::OperationResult ModLoader::reload_runtime_mod(LoadedMod& mod) {
    if (mod.nativeInPlace) {
        return {
            .success = false,
            .message = "Built-in mods cannot be updated in-game",
            .mod = &mod,
        };
    }
    apply_lifecycle_change(mod, true);
    ++m_generation;
    return runtime_result(mod);
}

ModLoader::OperationResult ModLoader::runtime_result(LoadedMod& mod) {
    if (mod.loadFailed) {
        return {
            .success = false,
            .message = mod.failureReason.empty() ? "The mod failed to activate" : mod.failureReason,
            .mod = &mod,
        };
    }
    if (mod.cvarIsEnabled->getValue() && !mod.active) {
        return {
            .success = false,
            .message = "A required provider is unavailable",
            .mod = &mod,
        };
    }
    if (!mod.cvarIsEnabled->getValue()) {
        return {
            .message = "Installed, disabled by config",
            .mod = &mod,
        };
    }
    return {.mod = &mod};
}

ModLoader::OperationResult ModLoader::install_staged(const fs::path& requestedPath) {
    if (m_searchDirs.empty()) {
        return {
            .success = false,
            .message = "No writable mods directory is configured",
        };
    }
    std::error_code error;
    const auto userDir = fs::weakly_canonical(m_searchDirs.front().path, error);
    if (error) {
        return {
            .success = false,
            .message =
                fmt::format("Could not resolve the user mods directory: {}", error.message()),
        };
    }
    const auto stagingDir = userDir / ".staging";
    const auto path = fs::weakly_canonical(requestedPath, error);
    const bool stagedName = path.extension() == ".part" && path.stem().extension() == ".dusk";
    if (error || path.parent_path() != stagingDir || !stagedName ||
        !fs::is_regular_file(path, error))
    {
        Log.error("refusing staged install from {}", data::abbreviated_path_string(requestedPath));
        return {
            .success = false,
            .message = "The package is not in the mod staging directory",
        };
    }

    ModMetadata metadata;
    std::string validationError;
    if (!inspect_mod_bundle(path, metadata, validationError)) {
        return {
            .success = false,
            .message = fmt::format("Invalid mod package: {}", validationError),
        };
    }

    fs::path destination;
    auto* installed = find_mod(metadata.id);
    if (installed != nullptr) {
        if (!can_uninstall(*installed)) {
            return {
                .success = false,
                .message = "This bundled mod cannot be updated in-game",
            };
        }
        destination = installed->modPath;
    } else {
        destination = userDir / fmt::format("{}.dusk", safe_filename(metadata.id));
    }

    std::string replaceError;
#ifdef _WIN32
    fs::path aside = destination;
    aside += ".old";
    const bool hadDestination = fs::exists(destination, error);
    if (hadDestination) {
        fs::remove(aside, error);
        error.clear();
        fs::rename(destination, aside, error);
        if (error) {
            return {
                .success = false,
                .message =
                    fmt::format("Failed to prepare the installed package: {}", error.message()),
            };
        }
    }
    if (!borealis::io::atomic_replace(path, destination, replaceError)) {
        if (hadDestination) {
            std::error_code restoreError;
            fs::rename(aside, destination, restoreError);
        }
        return {
            .success = false,
            .message = std::move(replaceError),
        };
    }
    auto result =
        installed != nullptr ? reload_runtime_mod(*installed) : load_runtime_mod(destination);
    if (hadDestination) {
        fs::remove(aside, error);
    }
    return result;
#else
    if (!borealis::io::atomic_replace(path, destination, replaceError)) {
        return {
            .success = false,
            .message = std::move(replaceError),
        };
    }
    return installed != nullptr ? reload_runtime_mod(*installed) : load_runtime_mod(destination);
#endif
}

void ModLoader::apply_pending_requests() {
    // Images retired by the previous tick have had a full frame to unwind off the stack.
    drain_retired_natives();

    if (m_pendingRequests.empty()) {
        return;
    }

    // Package mutations retain request order. Enable/disable/reactivate can still coalesce per mod.
    const auto requests = std::exchange(m_pendingRequests, {});
    std::vector<LifecycleRequest> coalesced;
    for (const auto& request : requests) {
        if (const auto* install = std::get_if<InstallRequest>(&request)) {
            auto result = install_staged(install->stagedPath);
            if (result.success && result.mod != nullptr) {
                const auto& metadata = result.mod->metadata;
                const std::string iconRml =
                    metadata.iconPath.empty() ?
                        std::string{} :
                        fmt::format(R"(<img src="{}" />)",
                            ui::escape(ui::mod_image_source(*result.mod, metadata.iconPath)));
                ui::push_toast({
                    .type = "mod-installed",
                    .title = "Mod installed",
                    .content = fmt::format(
                        R"(<row><mod-icon>{}</mod-icon><mod-info><mod-name><b>{}</b><small class="version">v{}</small></mod-name><small>{}</small></mod-info></row>)",
                        iconRml, ui::escape(metadata.name), ui::escape(metadata.version),
                        ui::escape(metadata.author)),
                    .duration = std::chrono::seconds{4},
                });
            } else if (!result.success && result.mod == nullptr) {
                ui::push_toast({
                    .type = "warning",
                    .title = "Mod install failed",
                    .content =
                        result.message.empty() ? "The loader rejected the package" : result.message,
                    .duration = std::chrono::seconds{6},
                });
            }
            if (!result.success && !m_searchDirs.empty()) {
                // request_install owns only files under the configured staging directory.
                std::error_code error;
                const auto userDir = fs::weakly_canonical(m_searchDirs.front().path, error);
                if (!error) {
                    const auto stagedPath = fs::weakly_canonical(install->stagedPath, error);
                    if (!error && stagedPath.parent_path() == userDir / ".staging") {
                        fs::remove(stagedPath, error);
                    }
                }
            }
            complete_operation(install->operation, result.success, std::move(result.message));
            continue;
        }
        if (const auto* reload = std::get_if<ReloadRequest>(&request)) {
            auto* mod = find_mod(reload->modId);
            if (mod == nullptr) {
                complete_operation(reload->operation, false, "The mod is no longer installed");
                continue;
            }
            auto result = reload_runtime_mod(*mod);
            complete_operation(reload->operation, result.success, std::move(result.message));
            continue;
        }
        if (const auto* uninstall = std::get_if<UninstallRequest>(&request)) {
            auto* mod = find_mod(uninstall->modId);
            if (mod == nullptr) {
                complete_operation(uninstall->operation);
                continue;
            }
            if (!can_uninstall(*mod)) {
                complete_operation(
                    uninstall->operation, false, "The mod is part of this Dusklight installation");
                continue;
            }

            const auto removedName = mod->metadata.name;
            const auto removedId = mod->metadata.id;
            std::error_code error;
            if (!fs::remove(mod->modPath, error)) {
                complete_operation(uninstall->operation, false,
                    error ? error.message() : "The package was not found");
                continue;
            }
            forget_mod(*mod);
            queue::remove_by_mod_id(removedId);
            complete_operation(uninstall->operation);
            ui::push_toast({
                .title = "Mod uninstalled",
                .content = removedName,
                .duration = std::chrono::seconds{2},
            });
            continue;
        }

        const auto& lifecycle = std::get<LifecycleRequest>(request);
        const auto existing =
            std::ranges::find(coalesced, lifecycle.modId, &LifecycleRequest::modId);
        if (existing != coalesced.end()) {
            complete_operation(
                existing->operation, false, "Superseded by a newer lifecycle request");
            *existing = lifecycle;
        } else {
            coalesced.push_back(lifecycle);
        }
    }

    for (const auto& request : coalesced) {
        auto* mod = find_mod(request.modId);
        if (mod == nullptr) {
            Log.warn("lifecycle request for unknown mod '{}'", request.modId);
            complete_operation(request.operation, false, "The mod is no longer installed");
            continue;
        }
        if (request.action == LifecycleAction::Enable && mod->enabledApplied) {
            continue;
        }
        if (request.action == LifecycleAction::Disable && !mod->enabledApplied && !mod->active) {
            continue;
        }

        if (request.action == LifecycleAction::Reactivate) {
            mod->loadFailed = false;
            mod->failureReason.clear();
            mod->suspendedByProvider = false;
            if (!mod->cvarIsEnabled->getValue()) {
                mod->cvarIsEnabled->setValue(true);
            }
        }
        apply_lifecycle_change(*mod, false);

        if (request.action == LifecycleAction::Reactivate) {
            std::string error;
            if (mod->loadFailed) {
                error =
                    mod->failureReason.empty() ? "The mod failed to activate" : mod->failureReason;
            } else if (mod->cvarIsEnabled->getValue() && !mod->active) {
                error = "A required provider is unavailable";
            }
            complete_operation(request.operation, error.empty(), std::move(error));
        }
    }

    svc::modules_lifecycle_applied();

    auto active = std::ranges::count_if(mods(), [](const LoadedMod& m) { return m.active; });
    Log.info("{}/{} mod(s) active", active, m_mods.size());
}

void ModLoader::tick() {
    svc::modules_frame_begin();
    apply_pending_requests();

    for (auto& mod : mods()) {
        if (!mod.active || (!mod.native && !mod.runtime.has_value())) {
            continue;
        }
        try {
            ModError error = MOD_ERROR_INIT;
            const bool delegated = !mod.native;
            const auto result = delegated ?
                                    mod.runtime->service->update(
                                        mod.runtime->providerContext, mod.context.get(), &error) :
                                    mod.native->fn_update(&error);
            if (result != MOD_OK) {
                fail_mod(mod, result,
                    lifecycle_error_message(
                        delegated ? "runtime update" : "mod_update", result, error));
            }
        } catch (const std::exception& e) {
            fail_mod(mod, MOD_ERROR, fmt::format("Exception in mod update: {}", e.what()));
        } catch (...) {
            fail_mod(mod, MOD_ERROR, "Unknown exception in mod_update");
        }
    }

    svc::modules_frame_end();
    flush_toasts();
}

void ModLoader::shutdown() {
    // Reverse initialization order, so importers shut down before their service providers.
    for (auto& mod : mods() | std::views::reverse) {
        deactivate_mod(mod);
        if (mod.enabledSubscription != 0) {
            config::unsubscribe(mod.enabledSubscription);
            mod.enabledSubscription = 0;
        }
        unregister(*mod.cvarIsEnabled);
    }

    m_mods.clear();
    drain_retired_natives();
    svc::modules_shutdown();
    Log.info("all mods unloaded");
}

}  // namespace dusk::mods

#include "save.hpp"

#include "internal.hpp"
#include "item.hpp"
#include "registry.hpp"

#include "dusk/main.h"
#include "dusk/mods/loader/loader.hpp"
#include "dusk/save_manager.hpp"
#include "dusk/utilities.hpp"
#include "dusk/version.hpp"
#include "m_Do/m_Do_MemCard.h"
#include "mods/svc/save.h"

#include <aurora/card.h>
#include <borealis/io.hpp>
#include <borealis/log.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::mods::svc {
namespace {

borealis::Log Log{"dusk::mods::save"};

constexpr uint32_t kSlotCount = 3;
constexpr int kLegacySidecarVersion = 1;
constexpr int kModSidecarVersion = 1;
constexpr const char* kLegacySidecarName = "mod_saves.json";
constexpr const char* kDefaultSaveName = "gczelda2";
constexpr s32 kCardChannel = 0;
constexpr size_t kMaxBlobNameLength = 256;

using BlobMap = std::map<std::string, std::vector<uint8_t>>;

struct SlotStore {
    std::map<std::string, BlobMap> mods;
};

struct SaveStore {
    std::array<SlotStore, kSlotCount> slots;
    std::set<std::string> dirtyMods;
    bool loaded = false;
};

struct SaveObserverRecord {
    uint64_t handle = 0;
    LoadedMod* mod = nullptr;
    SaveEventFn onNewSave = nullptr;
    SaveEventFn onLoaded = nullptr;
    SaveEventFn onWritten = nullptr;
    void* userData = nullptr;
};

std::map<std::string, SaveStore> s_saves;
int32_t s_currentSlot = -1;
bool s_legacyMigrationChecked = false;
std::vector<SaveObserverRecord> s_observers;
uint64_t s_nextHandle = 1;

std::filesystem::path legacy_sidecar_path() {
    return ConfigPath / kLegacySidecarName;
}

std::optional<save_manager::Storage> card_storage() {
    const auto cardType = aurora_card_get_type(kCardChannel);
    if (cardType == AURORA_CARD_UNAVAILABLE) {
        return std::nullopt;
    }
    const auto& diskId = version::getDiskID();
    const std::string_view game{diskId.gameName, sizeof(diskId.gameName)};
    const auto kind = cardType == AURORA_CARD_GCI_DIRECTORY ?
                          save_manager::StorageKind::GciDirectory :
                          save_manager::StorageKind::RawImage;
    auto storage = save_manager::resolve_storage(game, kind, kCardChannel);
    if (!storage) {
        return std::nullopt;
    }
    return std::move(storage.value);
}

std::filesystem::path save_sidecar_directory(
    const save_manager::Storage& storage, std::string_view saveName) {
    const auto& diskId = version::getDiskID();
    const std::string_view maker{diskId.company, sizeof(diskId.company)};
    const std::string_view game{diskId.gameName, sizeof(diskId.gameName)};
    return save_manager::save_sidecar_directory(storage.path, storage.kind, maker, game, saveName);
}

std::optional<std::filesystem::path> mod_sidecar_path(
    std::string_view saveName, std::string_view modId) {
    const auto storage = card_storage();
    if (!storage) {
        return std::nullopt;
    }
    return save_sidecar_directory(*storage, saveName) / (std::string{modId} + ".json");
}

bool write_mod_sidecar(
    const std::filesystem::path& path, const std::string& modId, const SaveStore& store) {
    nlohmann::json slots = nlohmann::json::array();
    bool hasBlobs = false;
    for (const auto& slot : store.slots) {
        nlohmann::json blobsJson = nlohmann::json::object();
        const auto modIt = slot.mods.find(modId);
        if (modIt != slot.mods.end()) {
            for (const auto& [name, bytes] : modIt->second) {
                blobsJson[name] = utils::base64_encode(bytes);
                hasBlobs = true;
            }
        }
        slots.push_back(nlohmann::json{{"blobs", std::move(blobsJson)}});
    }

    if (!hasBlobs) {
        std::error_code ec;
        std::filesystem::remove(path, ec);
        if (ec) {
            Log.error("failed to remove mod save sidecar '{}': {}",
                borealis::io::fs_path_to_string(path), ec.message());
            return false;
        }
        return true;
    }

    const nlohmann::json json{
        {"version", kModSidecarVersion},
        {"slots", std::move(slots)},
    };
    std::filesystem::path tempPath{path};
    tempPath += ".tmp";
    try {
        std::filesystem::create_directories(path.parent_path());
        {
            std::ofstream out{tempPath, std::ios::trunc};
            out << json.dump(2);
            out.close();
            if (!out.good()) {
                throw std::runtime_error{"write failed"};
            }
        }
        std::string error;
        if (!borealis::io::atomic_replace(tempPath, path, error)) {
            throw std::runtime_error{error};
        }
        return true;
    } catch (const std::exception& e) {
        Log.error("failed to write mod save sidecar '{}': {}",
            borealis::io::fs_path_to_string(path), e.what());
        std::error_code ec;
        std::filesystem::remove(tempPath, ec);
        return false;
    }
}

bool write_mod_sidecar(
    const std::string& saveName, const std::string& modId, const SaveStore& store) {
    if (!utils::is_valid_save_name(saveName) || !utils::is_valid_mod_id(modId)) {
        Log.error("refusing to write mod save sidecar with invalid path components '{}/{}'",
            saveName, modId);
        return false;
    }

    const auto path = mod_sidecar_path(saveName, modId);
    if (!path) {
        Log.error("CARD backing storage is unavailable for mod save sidecars");
        return false;
    }
    return write_mod_sidecar(*path, modId, store);
}

void load_mod_sidecar(const std::filesystem::path& path, const std::string& saveName,
    const std::string& modId, SaveStore& store) {
    try {
        std::ifstream in{path};
        if (!in.is_open()) {
            throw std::runtime_error{"open failed"};
        }
        const auto json = nlohmann::json::parse(in);
        if (json.value("version", 0) != kModSidecarVersion) {
            Log.warn("mod save sidecar '{}/{}' has unknown version {}; ignoring it", saveName,
                modId, json.value("version", 0));
            return;
        }
        const auto& slots = json.at("slots");
        for (uint32_t slot = 0; slot < kSlotCount && slot < slots.size(); ++slot) {
            const auto blobsJson = slots[slot].value("blobs", nlohmann::json::object());
            BlobMap blobs;
            size_t totalSize = 0;
            for (const auto& [name, encoded] : blobsJson.items()) {
                if (!utils::is_valid_name(name, kMaxBlobNameLength) || !encoded.is_string()) {
                    Log.warn("mod save sidecar: invalid blob '{}' in {}/{} slot {}; dropped", name,
                        saveName, modId, slot);
                    continue;
                }
                std::vector<uint8_t> bytes;
                if (!utils::base64_decode(encoded.get<std::string>(), bytes)) {
                    Log.warn("mod save sidecar: bad blob '{}' in {}/{} slot {}; dropped", name,
                        saveName, modId, slot);
                    continue;
                }
                if (totalSize + bytes.size() > SAVE_BLOB_BUDGET_BYTES) {
                    Log.warn("mod save sidecar: blobs exceed the {}-byte budget in {}/{} slot {}; "
                             "blob '{}' dropped",
                        SAVE_BLOB_BUDGET_BYTES, saveName, modId, slot, name);
                    continue;
                }
                totalSize += bytes.size();
                blobs[name] = std::move(bytes);
            }
            if (!blobs.empty()) {
                store.slots[slot].mods[modId] = std::move(blobs);
            }
        }
    } catch (const std::exception& e) {
        Log.error("failed to read mod save sidecar '{}': {}", borealis::io::fs_path_to_string(path),
            e.what());
    }
}

SaveStore& load_save_store(const std::string& saveName) {
    auto& store = s_saves[saveName];
    if (store.loaded) {
        return store;
    }
    const auto storage = card_storage();
    if (!storage) {
        return store;
    }

    const auto& diskId = version::getDiskID();
    if (const auto migrated = migrate_legacy_sidecar(*storage,
            {diskId.company, sizeof(diskId.company)}, {diskId.gameName, sizeof(diskId.gameName)});
        !migrated)
    {
        Log.error("{}", migrated.message);
        return store;
    }
    store.loaded = true;

    const auto directory = save_sidecar_directory(*storage, saveName);
    if (std::error_code ec; !std::filesystem::exists(directory, ec)) {
        if (ec) {
            Log.error("failed to inspect mod save directory '{}': {}",
                borealis::io::fs_path_to_string(directory), ec.message());
        }
        return store;
    }

    try {
        for (const auto& entry : std::filesystem::directory_iterator{directory}) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") {
                continue;
            }
            const auto modId = borealis::io::fs_path_to_string(entry.path().stem());
            if (!utils::is_valid_mod_id(modId)) {
                Log.warn("ignoring mod save sidecar with invalid mod ID '{}'", modId);
                continue;
            }
            load_mod_sidecar(entry.path(), saveName, modId, store);
        }
    } catch (const std::exception& e) {
        Log.error("failed to enumerate mod save directory '{}': {}",
            borealis::io::fs_path_to_string(directory), e.what());
    }
    return store;
}

std::optional<std::string> current_save_name() {
    const char* fileName = mDoMemCd_GetFileName();
    std::string saveName = fileName != nullptr ? fileName : "";
    if (!utils::is_valid_save_name(saveName)) {
        Log.error("CARD save file name '{}' is invalid for mod save storage", saveName);
        return std::nullopt;
    }
    return saveName;
}

void flush_save_sidecars(const std::string& saveName, SaveStore& store) {
    const auto dirtyMods = store.dirtyMods;
    for (const auto& modId : dirtyMods) {
        if (write_mod_sidecar(saveName, modId, store)) {
            store.dirtyMods.erase(modId);
        }
    }
}

void mark_slot_dirty(SaveStore& save, const SlotStore& slot) {
    for (const auto& entry : slot.mods) {
        save.dirtyMods.insert(entry.first);
    }
}

void notify(uint32_t slot, SaveEventFn SaveObserverRecord::* which, const char* what) {
    const auto observers = s_observers;
    for (const auto& observer : observers) {
        if (!observer.mod->active || observer.*which == nullptr) {
            continue;
        }
        try {
            (observer.*which)(observer.mod->context.get(), slot, observer.userData);
        } catch (const std::exception& e) {
            fail_mod(*observer.mod, MOD_ERROR,
                fmt::format("exception in {} save callback: {}", what, e.what()));
        } catch (...) {
            fail_mod(*observer.mod, MOD_ERROR,
                fmt::format("unknown exception in {} save callback", what));
        }
    }
}

}  // namespace

save_manager::Result migrate_legacy_sidecar(
    const save_manager::Storage& storage, std::string_view maker, std::string_view game) {
    if (s_legacyMigrationChecked) {
        return {.ok = true};
    }

    const auto path = legacy_sidecar_path();
    try {
        if (!std::filesystem::exists(path)) {
            s_legacyMigrationChecked = true;
            return {.ok = true};
        }
        std::ifstream in{path};
        if (!in.is_open()) {
            throw std::runtime_error{"Unable to open the legacy mod save file."};
        }
        const auto json = nlohmann::json::parse(in);
        in.close();
        if (json.value("version", 0) != kLegacySidecarVersion) {
            throw std::runtime_error{
                fmt::format("Unsupported legacy mod save version {}.", json.value("version", 0))};
        }

        SaveStore legacyStore;
        std::set<std::string> modIds;
        const auto& slots = json.at("slots");
        for (uint32_t slot = 0; slot < kSlotCount && slot < slots.size(); ++slot) {
            const auto& slotJson = slots[slot];
            const auto modsJson = slotJson.value("mods", nlohmann::json::object());
            for (const auto& [modId, blobs] : modsJson.items()) {
                if (!utils::is_valid_mod_id(modId)) {
                    Log.warn("legacy mod save sidecar has invalid mod ID '{}'; dropped", modId);
                    continue;
                }
                for (const auto& [name, encoded] : blobs.items()) {
                    if (!utils::is_valid_name(name, kMaxBlobNameLength) || !encoded.is_string()) {
                        Log.warn(
                            "legacy mod save sidecar: invalid blob '{}/{}' in slot {}; dropped",
                            modId, name, slot);
                        continue;
                    }
                    std::vector<uint8_t> bytes;
                    if (!utils::base64_decode(encoded.get<std::string>(), bytes)) {
                        Log.warn("legacy mod save sidecar: bad blob '{}/{}' in slot {}; dropped",
                            modId, name, slot);
                        continue;
                    }
                    legacyStore.slots[slot].mods[modId][name] = std::move(bytes);
                    modIds.insert(modId);
                }
            }
        }

        for (const auto& modId : modIds) {
            const std::string_view saveName =
                modId == "dev.twilitrealm.randomizer" ? "randomizer" : kDefaultSaveName;
            const auto destination = save_manager::save_sidecar_directory(
                                         storage.path, storage.kind, maker, game, saveName) /
                                     (modId + ".json");
            if (std::filesystem::exists(destination)) {
                if (!std::filesystem::is_regular_file(destination)) {
                    throw std::runtime_error{
                        fmt::format("The mod save destination is not a file: {}",
                            borealis::io::fs_path_to_string(destination))};
                }
                continue;
            }
            if (!write_mod_sidecar(destination, modId, legacyStore)) {
                throw std::runtime_error{fmt::format("Unable to write migrated mod data: {}",
                    borealis::io::fs_path_to_string(destination))};
            }
        }

        std::filesystem::path backupPath{path};
        backupPath += ".bak";
        std::string error;
        if (!borealis::io::atomic_replace(path, backupPath, error)) {
            throw std::runtime_error{
                fmt::format("Unable to preserve the legacy mod save file: {}", error)};
        }
        s_legacyMigrationChecked = true;
        Log.info(
            "migrated legacy mod save data to '{}'", borealis::io::fs_path_to_string(storage.path));
        return {.ok = true};
    } catch (const std::exception& e) {
        return {.message = fmt::format("Legacy mod save migration failed: {}", e.what())};
    }
}

void save_slot_new(uint32_t slot) {
    if (slot >= kSlotCount) {
        return;
    }
    s_currentSlot = static_cast<int32_t>(slot);
    item_gives_clear();
    if (const auto saveName = current_save_name()) {
        auto& save = load_save_store(*saveName);
        auto& mods = save.slots[slot].mods;
        mark_slot_dirty(save, save.slots[slot]);
        mods.clear();
        Log.info("new save in {}/{}; mod blob store cleared", *saveName, slot);
    }
    notify(slot, &SaveObserverRecord::onNewSave, "new-save");
}

void save_slot_loaded(uint32_t slot) {
    if (slot >= kSlotCount) {
        return;
    }
    if (const auto saveName = current_save_name()) {
        load_save_store(*saveName);
    }
    s_currentSlot = static_cast<int32_t>(slot);
    item_gives_clear();
    notify(slot, &SaveObserverRecord::onLoaded, "save-loaded");
}

void save_slot_written(uint32_t slot) {
    if (slot >= kSlotCount) {
        return;
    }
    s_currentSlot = static_cast<int32_t>(slot);
    notify(slot, &SaveObserverRecord::onWritten, "save-written");
    if (const auto saveName = current_save_name()) {
        auto& store = load_save_store(*saveName);
        flush_save_sidecars(*saveName, store);
    }
}

void save_slot_copied(uint32_t fromSlot, uint32_t toSlot) {
    if (fromSlot >= kSlotCount || toSlot >= kSlotCount || fromSlot == toSlot) {
        return;
    }
    const auto saveName = current_save_name();
    if (!saveName) {
        return;
    }
    auto& save = load_save_store(*saveName);
    auto& fromMods = save.slots[fromSlot].mods;
    auto& toMods = save.slots[toSlot].mods;
    mark_slot_dirty(save, save.slots[fromSlot]);
    mark_slot_dirty(save, save.slots[toSlot]);
    toMods = fromMods;
    flush_save_sidecars(*saveName, save);
    Log.info("mod save data copied in {} with slot {} -> {}", *saveName, fromSlot, toSlot);
}

void save_slot_erased(uint32_t slot) {
    if (slot >= kSlotCount) {
        return;
    }
    const auto saveName = current_save_name();
    if (!saveName) {
        return;
    }
    auto& save = load_save_store(*saveName);
    auto& mods = save.slots[slot].mods;
    mark_slot_dirty(save, save.slots[slot]);
    mods.clear();
    flush_save_sidecars(*saveName, save);
    Log.info("mod save data erased in {} with slot {}", *saveName, slot);
}

void save_no_slot() {
    s_currentSlot = -1;
    item_gives_clear();
}

void invalidate_save(std::string_view saveName) {
    s_saves.erase(std::string{saveName});
}

namespace {

struct CurrentBlobs {
    SaveStore* store = nullptr;
    BlobMap* blobs = nullptr;
};

CurrentBlobs current_blobs(const LoadedMod& mod, bool create) {
    if (s_currentSlot < 0) {
        return {};
    }
    const auto saveName = current_save_name();
    if (!saveName) {
        return {};
    }
    auto& store = load_save_store(*saveName);
    if (!store.loaded) {
        return {};
    }
    auto& mods = store.slots[s_currentSlot].mods;
    if (!create) {
        const auto it = mods.find(mod.metadata.id);
        return {&store, it != mods.end() ? &it->second : nullptr};
    }
    return {&store, &mods[mod.metadata.id]};
}

ModResult save_set_blob(ModContext* context, const char* name, const void* data, size_t size) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || !utils::is_valid_name(name, kMaxBlobNameLength) ||
        (data == nullptr && size != 0) || size > SAVE_BLOB_BUDGET_BYTES)
    {
        return MOD_INVALID_ARGUMENT;
    }
    const auto [store, blobs] = current_blobs(*mod, true);
    if (blobs == nullptr) {
        return MOD_UNAVAILABLE;
    }
    size_t total = size;
    for (const auto& [blobName, bytes] : *blobs) {
        if (blobName != name) {
            total += bytes.size();
        }
    }
    if (total > SAVE_BLOB_BUDGET_BYTES) {
        Log.error("[{}] save blob '{}' rejected: {} bytes would exceed the {}-byte budget",
            mod->metadata.id, name, total, SAVE_BLOB_BUDGET_BYTES);
        return MOD_UNAVAILABLE;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    std::vector<uint8_t> blob;
    if (size != 0) {
        blob.assign(bytes, bytes + size);
    }
    (*blobs)[name] = std::move(blob);
    store->dirtyMods.insert(mod->metadata.id);
    return MOD_OK;
}

ModResult save_get_blob(ModContext* context, const char* name, void* buf, size_t* inoutSize) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || !utils::is_valid_name(name, kMaxBlobNameLength) || inoutSize == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }
    const auto [store, blobs] = current_blobs(*mod, false);
    if (blobs == nullptr) {
        return MOD_UNAVAILABLE;
    }
    const auto it = blobs->find(name);
    if (it == blobs->end()) {
        return MOD_UNAVAILABLE;
    }
    if (buf == nullptr) {
        *inoutSize = it->second.size();
        return MOD_OK;
    }
    if (*inoutSize < it->second.size()) {
        return MOD_INVALID_ARGUMENT;
    }
    std::memcpy(buf, it->second.data(), it->second.size());
    *inoutSize = it->second.size();
    return MOD_OK;
}

ModResult save_delete_blob(ModContext* context, const char* name) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || !utils::is_valid_name(name, kMaxBlobNameLength)) {
        return MOD_INVALID_ARGUMENT;
    }
    const auto [store, blobs] = current_blobs(*mod, false);
    if (blobs == nullptr) {
        return MOD_UNAVAILABLE;
    }
    if (blobs->erase(name) == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    store->dirtyMods.insert(mod->metadata.id);
    return MOD_OK;
}

ModResult save_observe(ModContext* context, SaveEventFn onNewSave, SaveEventFn onLoaded,
    SaveEventFn onWritten, void* userData, SaveObserverHandle* outHandle) {
    if (outHandle != nullptr) {
        *outHandle = 0;
    }
    auto* mod = mod_from_context(context);
    if (mod == nullptr || (onNewSave == nullptr && onLoaded == nullptr && onWritten == nullptr)) {
        return MOD_INVALID_ARGUMENT;
    }
    auto& observer = s_observers.emplace_back();
    observer.handle = s_nextHandle++;
    observer.mod = mod;
    observer.onNewSave = onNewSave;
    observer.onLoaded = onLoaded;
    observer.onWritten = onWritten;
    observer.userData = userData;
    if (outHandle != nullptr) {
        *outHandle = observer.handle;
    }
    return MOD_OK;
}

ModResult save_unobserve(ModContext* context, SaveObserverHandle handle) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || handle == 0) {
        return MOD_INVALID_ARGUMENT;
    }
    const auto removed = std::erase_if(s_observers,
        [&](const auto& observer) { return observer.handle == handle && observer.mod == mod; });
    return removed != 0 ? MOD_OK : MOD_INVALID_ARGUMENT;
}

ModResult save_peek_blob(
    ModContext* context, uint32_t slot, const char* name, void* buf, size_t* inoutSize) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr || !utils::is_valid_name(name, kMaxBlobNameLength) || inoutSize == nullptr ||
        slot >= kSlotCount)
    {
        return MOD_INVALID_ARGUMENT;
    }
    const auto saveName = current_save_name();
    if (!saveName) {
        return MOD_UNAVAILABLE;
    }
    const auto& store = load_save_store(*saveName);
    if (!store.loaded) {
        return MOD_UNAVAILABLE;
    }
    const auto& mods = store.slots[slot].mods;
    const auto modIt = mods.find(mod->metadata.id);
    if (modIt == mods.end()) {
        return MOD_UNAVAILABLE;
    }
    const auto it = modIt->second.find(name);
    if (it == modIt->second.end()) {
        return MOD_UNAVAILABLE;
    }
    if (buf == nullptr) {
        *inoutSize = it->second.size();
        return MOD_OK;
    }
    if (*inoutSize < it->second.size()) {
        return MOD_INVALID_ARGUMENT;
    }
    std::memcpy(buf, it->second.data(), it->second.size());
    *inoutSize = it->second.size();
    return MOD_OK;
}

void save_remove_mod(LoadedMod& mod) {
    std::erase_if(s_observers, [&](const auto& observer) { return observer.mod == &mod; });
}

constexpr SaveService s_saveService{
    .header = SERVICE_HEADER(SaveService, SAVE_SERVICE_MAJOR, SAVE_SERVICE_MINOR),
    .set_blob = SERVICE_FUNCTION(save_set_blob),
    .get_blob = SERVICE_FUNCTION(save_get_blob),
    .delete_blob = SERVICE_FUNCTION(save_delete_blob),
    .observe_saves = SERVICE_FUNCTION(save_observe),
    .unobserve_saves = SERVICE_FUNCTION(save_unobserve),
    .peek_blob = SERVICE_FUNCTION(save_peek_blob),
};

}  // namespace

constinit const ServiceModule g_saveModule{
    .id = SAVE_SERVICE_ID,
    .majorVersion = SAVE_SERVICE_MAJOR,
    .minorVersion = SAVE_SERVICE_MINOR,
    .service = &s_saveService,
    .modDetached = save_remove_mod,
};

}  // namespace dusk::mods::svc

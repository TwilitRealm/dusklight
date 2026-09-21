#pragma once

#include "dusk/iso_validate.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::save_manager {

constexpr int kDefaultBackupRetention = 10;

enum class StorageKind {
    GciDirectory,
    RawImage,
};

enum class ArtifactKind {
    Unknown,
    Gci,
    Raw,
    DuskSave,
};

enum class ModDataAction {
    Replace,
    Clear,
    Keep,
};

enum class DiscCompatibility {
    Exact,
    RegionChange,
    PlatformChange,
    Incompatible,
};

struct Result {
    bool ok = false;
    std::string message;

    explicit operator bool() const { return ok; }
};

template <typename T>
struct ValueResult {
    Result result;
    T value{};

    explicit operator bool() const { return result.ok; }
};

struct SaveIdentity {
    std::string maker;
    std::string game;
    std::string saveName;
};

struct GciHeader {
    std::string maker;
    std::string game;
    std::string saveName;
    uint32_t modifiedTime = 0;
    uint16_t blockCount = 0;
};

struct Storage {
    StorageKind kind = StorageKind::GciDirectory;
    std::filesystem::path path;
    int channel = 0;
    bool mounted = false;
};

struct ModFileInfo {
    std::string id;
    std::string version;
    uintmax_t size = 0;
};

struct SaveInfo {
    bool present = false;
    uintmax_t size = 0;
    uint32_t modifiedTime = 0;
    std::vector<ModFileInfo> mods;
};

struct Artifact {
    ArtifactKind kind = ArtifactKind::Unknown;
    GciHeader header;
    std::vector<uint8_t> gci;
    std::vector<uint8_t> raw;
    std::map<std::string, std::vector<uint8_t>> modFiles;
    std::vector<ModFileInfo> declaredMods;
    std::string sourceName;
};

struct ExportArtifact {
    std::filesystem::path path;
    std::string suggestedName;
    bool temporary = false;
};

struct BackupInfo {
    std::filesystem::path path;
    std::string name;
    std::filesystem::file_time_type modified;
};

std::optional<SaveIdentity> identity_for_disc(const iso::DiscInfo& info, std::string saveName);

std::string card_file_stem(
    std::string_view maker, std::string_view game, std::string_view saveName);
std::filesystem::path save_sidecar_directory(const std::filesystem::path& backingPath,
    StorageKind kind, std::string_view maker, std::string_view game, std::string_view saveName);

ValueResult<Storage> resolve_storage(
    std::string_view game, StorageKind preferredKind, int channel = 0);
ValueResult<std::vector<SaveIdentity>> list_saves(
    const Storage& storage, std::string_view game, std::string_view maker);
ValueResult<GciHeader> parse_gci(const std::vector<uint8_t>& bytes);
DiscCompatibility disc_compatibility(const GciHeader& header, const SaveIdentity& identity);
ValueResult<Artifact> read_artifact(std::string_view location);
ValueResult<std::vector<Artifact>> extract_raw_saves(
    const Artifact& artifact, std::string_view game, std::string_view maker);

ValueResult<SaveInfo> inspect_save(const Storage& storage, const SaveIdentity& identity);
ValueResult<ExportArtifact> build_export(
    const Storage& storage, const SaveIdentity& identity, bool includeModData);
ValueResult<ExportArtifact> raw_card_export(const Storage& storage);

Result import_artifact(const Storage& storage, const SaveIdentity& identity,
    const Artifact& artifact, ModDataAction modDataAction);
Result import_raw_image(const Storage& storage, std::string_view game, std::string_view maker,
    const Artifact& artifact, ModDataAction modDataAction);
Result delete_save(const Storage& storage, const SaveIdentity& identity);
Result delete_mod_data(
    const Storage& storage, const SaveIdentity& identity, std::string_view modId);

Result create_backup(const Storage& storage, const SaveIdentity& identity);
ValueResult<std::vector<BackupInfo>> list_backups(
    const Storage& storage, const SaveIdentity& identity);
Result restore_backup(
    const Storage& storage, const SaveIdentity& identity, const std::filesystem::path& path);
Result delete_backup(const Storage& storage, const std::filesystem::path& path);

std::string format_gc_time(uint32_t value);
void remove_temporary_export(const ExportArtifact& artifact);

}  // namespace dusk::save_manager

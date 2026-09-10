#include "natives.hpp"

#include "loader.hpp"
#include "native_module.hpp"

#include "dusk/data.hpp"
#include "dusk/mods/log_buffer.hpp"
#include "dusk/mods/svc/registry.hpp"

#include <borealis/io.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <variant>

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

namespace {

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

// True if the first `capacity` bytes of `str` contain a NUL.
bool terminated_within(const char* str, size_t capacity) {
    return std::memchr(str, '\0', capacity) != nullptr;
}

bool parse_meta(NativeMod& native, LoadedMod& mod) {
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

std::string native_status_message(const NativeModStatus status) {
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

}  // namespace

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
    if (!mod.native) {
        return;
    }
    // Deferred dlclose: this mod's code may still be on the stack below the current tick
    m_retiredNatives.push_back(
        {std::move(mod.native), mod.nativeInPlace ? fs::path{} : std::move(mod.nativeDir)});
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

bool ModLoader::ensure_native_loaded(LoadedMod& mod) {
    if (mod.native || mod.nativeStatus == NativeModStatus::None) {
        return true;
    }
    return load_native_if_present(mod);
}

ModManifestInfo build_manifest_info(const ModMetaParsed& parsed) {
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

}  // namespace dusk::mods

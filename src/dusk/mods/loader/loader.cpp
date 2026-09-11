#include "loader.hpp"

#include "depgraph.hpp"
#include "manifest.hpp"
#include "native_module.hpp"
#include "natives.hpp"
#include "packages.hpp"
#if DUSK_HAS_PREPATCH
#include "prepatch.hpp"
#endif

#include "dusk/config.hpp"
#include "dusk/data.hpp"
#include "dusk/logging.h"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/log_buffer.hpp"
#include "dusk/mods/manifest.hpp"
#include "dusk/mods/path.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/svc/config.hpp"
#include "dusk/mods/svc/hook.hpp"
#include "dusk/mods/svc/registry.hpp"
#include "dusk/ui/mod_texture_provider.hpp"
#include "dusk/ui/mods_window.hpp"
#include "dusk/ui/ui.hpp"

#include <borealis/io.hpp>
#include <borealis/update.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace fs = std::filesystem;

namespace dusk::mods {
namespace {
constexpr borealis::Log Log{"dusk::mods::loader"};
ModLoader g_modLoader;

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

static std::string lifecycle_error_message(
    const char* fnName, const ModResult result, const ModError& error) {
    if (error.message[0] != '\0') {
        return error.message;
    }
    return fmt::format("{} failed with result {}", fnName, static_cast<int>(result));
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
    mod.fromDirectory = fromDir;
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

    const auto packages = scan_packages(m_searchDirs);
    for (const auto& package : packages) {
        const auto* selected = select_package(packages, package.metadata.id);
        if (selected != &package) {
            log::write(package.metadata.id, LOG_LEVEL_INFO, "{} v{} shadowed by {} v{}",
                data::abbreviated_path_string(package.path), package.metadata.version,
                data::abbreviated_path_string(selected->path), selected->metadata.version);
            continue;
        }
        if (auto* mod = try_load_mod(package.path, package.fromDirectory, package.searchDirIndex)) {
            record_package_sources(*mod, packages);
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
    return mod.hasUserPackage;
}

bool ModLoader::can_update(const LoadedMod& mod) const {
    return mod.searchDirIndex != 0 || (!mod.fromDirectory && can_uninstall(mod));
}

void ModLoader::notify_mod_failure(LoadedMod& mod, bool firstFailure) {
    if (firstFailure) {
        m_pendingFailures.push_back(mod.metadata.name);
    }
    // Startup failures are handled inline by activate_mod
    if (!m_startupComplete) {
        return;
    }
    m_pendingRequests.emplace_back(LifecycleRequest{
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

void ModLoader::apply_lifecycle_change(
    LoadedMod& target, const bool reload, const PackageCandidate* replacement) {
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

    if (replacement != nullptr) {
        target.modPath = replacement->path;
        target.searchDirIndex = replacement->searchDirIndex;
        target.fromDirectory = replacement->fromDirectory;
        target.nativeInPlace =
            replacement->fromDirectory && m_searchDirs[replacement->searchDirIndex].inPlaceNative;
        std::stable_sort(m_mods.begin(), m_mods.end(),
            [](const auto& a, const auto& b) { return a->searchDirIndex > b->searchDirIndex; });
        loader::sort_mods(m_mods);
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

ModLoader::OperationResult ModLoader::reload_runtime_mod(
    LoadedMod& mod, const PackageCandidate* replacement) {
    if (mod.nativeInPlace && replacement == nullptr) {
        return {
            .success = false,
            .message = "An in-place native library cannot be reloaded",
            .mod = &mod,
        };
    }
    apply_lifecycle_change(mod, true, replacement);
    ++m_generation;
    return runtime_result(mod);
}

ModLoader::OperationResult ModLoader::uninstall_runtime_mod(LoadedMod& mod) {
    std::vector<PackageCandidate> packages;
    try {
        packages = scan_packages(m_searchDirs);
    } catch (const std::exception& exception) {
        return {.success = false, .message = exception.what()};
    }
    record_package_sources(mod, packages);
    if (!can_uninstall(mod)) {
        return {.success = false, .message = "No installed package to remove"};
    }

    std::string removalError;
    std::erase_if(packages, [&](const auto& package) {
        if (package.metadata.id != mod.metadata.id || package.searchDirIndex != 0 ||
            package.fromDirectory || package.symlink)
        {
            return false;
        }
        std::error_code error;
        fs::remove(package.path, error);
        if (error) {
            removalError = fmt::format("Could not remove {}: {}",
                data::abbreviated_path_string(package.path), error.message());
        }
        return !error;
    });

    OperationResult result;
    if (const auto* selected = select_package(packages, mod.metadata.id)) {
        if (selected->path != mod.modPath) {
            result = reload_runtime_mod(mod, selected);
        } else {
            result.mod = &mod;
            ++m_generation;
        }
        record_package_sources(mod, packages);
    } else {
        forget_mod(mod);
    }
    if (!removalError.empty()) {
        result.success = false;
        result.message = std::move(removalError);
    }
    return result;
}

ModLoader::OperationResult ModLoader::runtime_result(LoadedMod& mod) {
    if (mod.loadFailed) {
        return {
            .success = false,
            .message = mod.failureReason.empty() ? "Mod failed to activate" : mod.failureReason,
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

    const auto destination = userDir / fmt::format("{}.dusk", safe_filename(metadata.id));
    auto* installed = find_mod(metadata.id);
    if (installed != nullptr && !can_update(*installed)) {
        return {
            .success = false,
            .message = "Cannot install mod over a development directory",
        };
    }

    for (const auto& mod : mods()) {
        if (mod.metadata.id != metadata.id && fs::equivalent(mod.modPath, destination, error)) {
            return {
                .success = false,
                .message = "The destination filename belongs to a different mod",
            };
        }
    }
    error.clear();

    if (!borealis::update::parse_version(metadata.version)) {
        return {.success = false, .message = "The package version is invalid"};
    }
    std::vector<PackageCandidate> packages;
    try {
        packages = scan_packages(m_searchDirs);
    } catch (const std::exception& exception) {
        return {.success = false, .message = exception.what()};
    }
    if (const auto* selected = select_package(packages, metadata.id);
        selected && compare_package_versions(metadata.version, selected->metadata.version) < 0)
    {
        return {
            .success = false,
            .message = fmt::format(
                "A newer version ({}) is already installed", selected->metadata.version),
        };
    }

    const auto packageResult = install_package(path, destination, metadata.id);
    if (!packageResult.replaced) {
        return {.success = false, .message = packageResult.error};
    }
    std::erase_if(packages, [&](const auto& package) {
        if (package.metadata.id != metadata.id || package.searchDirIndex != 0 ||
            package.fromDirectory)
        {
            return false;
        }
        std::error_code statusError;
        return fs::equivalent(package.path, destination, statusError) ||
               !fs::exists(package.path, statusError);
    });
    packages.push_back({.path = destination, .metadata = metadata});
    const auto* selected = select_package(packages, metadata.id);
    auto result = installed != nullptr ? reload_runtime_mod(*installed, selected) :
                                         load_runtime_mod(selected->path);
    if (result.mod != nullptr) {
        record_package_sources(*result.mod, packages);
    }

    if (result.success && !packageResult.error.empty()) {
        result.success = false;
        result.message = packageResult.error;
    }
    return result;
}

void ModLoader::apply_pending_requests() {
    // Images retired by the previous tick have had a full frame to unwind off the stack.
    drain_retired_natives();

    if (m_pendingRequests.empty()) {
        return;
    }

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
            const auto removedName = mod->metadata.name;
            const auto removedId = mod->metadata.id;
            auto result = uninstall_runtime_mod(*mod);
            if (result.success) {
                queue::remove_by_mod_id(removedId);
                ui::push_toast({
                    .title = result.mod != nullptr ? "User update removed" : "Mod uninstalled",
                    .content = removedName,
                    .duration = std::chrono::seconds{2},
                });
            }
            complete_operation(uninstall->operation, result.success, std::move(result.message));
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

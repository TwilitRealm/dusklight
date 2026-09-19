#include "updates.hpp"

#include "catalog.hpp"
#include "dusk/mod_loader.hpp"
#include "dusk/mods/queue.hpp"
#include "dusk/mods/svc/registry.hpp"
#include "dusk/settings.h"

#include <borealis/log.hpp>
#include <borealis/update.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace dusk::mods::updates {
namespace {

using Clock = std::chrono::steady_clock;
constexpr borealis::Log Log{"dusk::mods::updates"};
constexpr size_t batchSize = 64;

struct Store {
    State state = State::Idle;
    bool initialized = false;
    bool automatic = false;
    bool requested = false;
    bool tracking = false;
    bool fresh = false;
    uint64_t generation = 0;
    uint64_t loaderGeneration = 0;
    uint64_t serviceGeneration = 0;
    UpdateEnvironment environment;
    UpdateEnvironment requestEnvironment;
    borealis::Task<catalog::UpdateFetchResult> task;
    std::vector<std::string> targets;
    size_t nextTarget = 0;
    std::vector<ModUpdate> pending;
    std::vector<Entry> entries;
    std::string error;
    int retries = 0;
    Clock::time_point checkAt{};
};

Store store;

bool hosted_id(std::string_view id) {
    if (id.empty() || id.size() > 128 || id.front() == '.' || id.back() == '.' ||
        id.find("..") != id.npos || id.find('.') == id.npos)
    {
        return false;
    }
    return std::ranges::all_of(id, [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
    });
}

UpdatePrecondition precondition(const Entry& entry) {
    return {
        .modId = entry.result.id,
        .installedVersion = entry.result.installedVersion,
        .targetVersion = entry.result.target->version,
        .compatibility = entry.result.target->compatibility,
    };
}

void refresh_entries() {
    for (auto& entry : store.entries) {
        std::string reason;
        std::string queueKey;
        if (const auto queued = queue::find_by_mod_id(entry.result.id);
            queued && !queue::is_completed(queued->state))
        {
            queueKey = queued->id;
        }
        const auto* local = ModLoader::instance().find_mod(entry.result.id);
        if (local == nullptr || local->metadata.version != entry.result.installedVersion) {
            reason = "The installed version changed.";
        } else if (!store.fresh) {
            reason = "Check for updates again.";
        } else if (entry.result.target) {
            if (!ModLoader::instance().can_update(*local)) {
                reason = "Development directories cannot be updated in-game.";
            } else {
                reason = validate(store.environment, precondition(entry));
            }
        } else if (!entry.result.blockers.empty()) {
            reason = entry.result.blockers.front();
        }
        const bool actionable = entry.result.target && reason.empty() && queueKey.empty();
        if (entry.reason != reason || entry.queueKey != queueKey || entry.actionable != actionable)
        {
            entry.reason = std::move(reason);
            entry.queueKey = std::move(queueKey);
            entry.actionable = actionable;
            ++store.generation;
        }
    }
}

void start_batch() {
    const auto end = std::min(store.nextTarget + batchSize, store.targets.size());
    std::vector<std::string> targets{
        store.targets.begin() + store.nextTarget, store.targets.begin() + end};
    store.nextTarget = end;
    store.task = catalog::fetch_updates(store.requestEnvironment, std::move(targets));
}

void begin_check() {
    store.requested = false;
    store.requestEnvironment = store.environment;
    store.targets.clear();
    store.pending.clear();
    store.nextTarget = 0;
    for (const auto& mod : store.environment.mods) {
        if (hosted_id(mod.id) && borealis::update::parse_version(mod.version)) {
            store.targets.push_back(mod.id);
        }
    }
    store.error.clear();
    store.state = State::Checking;
    ++store.generation;
    if (store.environment.mods.size() > 512 || store.environment.services.size() > 4096) {
        store.state = State::Failed;
        store.error = "The installed mod inventory exceeds the catalog limit.";
    } else if (store.targets.empty()) {
        store.entries.clear();
        store.state = State::Ready;
        store.fresh = true;
    } else {
        start_batch();
    }
}

UpdateEnvironment capture_environment() {
    UpdateEnvironment result;
    result.platform = catalog::platform();
    result.abi = MOD_ABI_VERSION;
    result.services = svc::list_services();
    for (const auto& mod : ModLoader::instance().mods()) {
        InstalledPackage installed{
            .id = mod.metadata.id, .version = mod.metadata.version, .enabled = mod.is_enabled()};
        for (const auto& service : mod.manifestInfo.imports) {
            if (service.required) {
                installed.imports.push_back({service.id, service.major, service.minMinor, false});
            }
        }
        if (mod.runtime) {
            installed.imports.push_back(
                {mod.runtime->id, mod.runtime->major, mod.runtime->minMinor, false});
        }
        std::ranges::sort(installed.imports, {}, [](const auto& service) {
            return std::tie(service.id, service.major, service.minMinor);
        });
        // Runtime pins can also be present in the native manifest's required imports.
        std::vector<ServiceImport> imports;
        for (const auto& service : installed.imports) {
            if (!imports.empty() && imports.back().id == service.id &&
                imports.back().major == service.major)
            {
                imports.back().minMinor = std::max(imports.back().minMinor, service.minMinor);
            } else {
                imports.push_back(service);
            }
        }
        installed.imports = std::move(imports);
        result.mods.push_back(std::move(installed));
    }
    std::ranges::sort(result.mods, {}, &InstalledPackage::id);
    return result;
}

}  // namespace

std::string validate(const UpdateEnvironment& environment, const UpdatePrecondition& update) {
    const auto mod = std::ranges::find(environment.mods, update.modId, &InstalledPackage::id);
    if (mod == environment.mods.end()) {
        return "The mod is no longer installed.";
    }
    if (mod->version != update.installedVersion) {
        return "The installed version changed. Check for updates again.";
    }
    const auto installedVersion = borealis::update::parse_version(mod->version);
    const auto targetVersion = borealis::update::parse_version(update.targetVersion);
    if (!installedVersion || !targetVersion) {
        return "The mod version cannot be compared.";
    }
    if (borealis::update::compare_version(*targetVersion, *installedVersion) <= 0) {
        return "This version is already installed or older.";
    }
    const auto& candidate = update.compatibility;
    if (candidate.containsNativeCode) {
        if (!catalog::supports_native_installs()) {
            return "Native mod updates are not supported on this device.";
        }
        if (environment.platform.empty() || std::ranges::find(candidate.platforms,
                                                environment.platform) == candidate.platforms.end())
        {
            return "This release does not support this device.";
        }
        if (candidate.abi != environment.abi) {
            return "This release requires a different Dusklight ABI.";
        }
    }

    std::unordered_map<std::string, ServiceExport> services;
    for (const auto& service : environment.services) {
        if (service.providerId != update.modId) {
            services.emplace(service_key(service.id, service.major), service);
        }
    }
    if (mod->enabled) {
        for (auto service : candidate.exports) {
            service.providerId = update.modId;
            if (!services.emplace(service_key(service.id, service.major), service).second) {
                return fmt::format(
                    "Another provider already supplies {}@{}.", service.id, service.major);
            }
        }
    }
    const auto available = [&](const std::string& id, uint16_t major, uint16_t minor) {
        const auto service = services.find(service_key(id, major));
        return service != services.end() && service->second.minor >= minor;
    };
    for (const auto& service : environment.services) {
        if (service.providerId == update.modId &&
            !available(service.id, service.major, service.minor))
        {
            return fmt::format("This release removes or lowers {}@{}.{}.", service.id,
                service.major, service.minor);
        }
    }
    for (const auto& required : candidate.imports) {
        if (!required.optional && !available(required.id, required.major, required.minMinor)) {
            return fmt::format(
                "Requires {}@{}.{} or newer.", required.id, required.major, required.minMinor);
        }
    }

    std::unordered_map<std::string, std::vector<std::string>> graph;
    for (const auto& installed : environment.mods) {
        if (!installed.enabled) {
            continue;
        }
        const auto& imports = installed.id == update.modId ? candidate.imports : installed.imports;
        for (const auto& required : imports) {
            const auto service = services.find(service_key(required.id, required.major));
            if (!required.optional && service != services.end() &&
                !service->second.providerId.empty() && service->second.providerId != installed.id)
            {
                graph[installed.id].push_back(service->second.providerId);
            }
        }
    }
    std::unordered_set<std::string> visited;
    const std::function<bool(const std::string&)> reaches_target = [&](const std::string& id) {
        if (id == update.modId) {
            return true;
        }
        if (!visited.insert(id).second) {
            return false;
        }
        const auto edges = graph.find(id);
        return edges != graph.end() && std::ranges::any_of(edges->second, reaches_target);
    };
    if (std::ranges::any_of(graph[update.modId], reaches_target)) {
        return "This release introduces a required dependency cycle.";
    }
    return {};
}

std::string validate(const UpdatePrecondition& value) {
    const auto* mod = ModLoader::instance().find_mod(value.modId);
    if (mod != nullptr && !ModLoader::instance().can_update(*mod)) {
        return "Development directories cannot be updated in-game.";
    }
    return validate(capture_environment(), value);
}

void update() {
    auto& loader = ModLoader::instance();
    if (!loader.initialized()) {
        return;
    }
    if (!borealis::http::available()) {
        if (store.state != State::Unavailable) {
            store.state = State::Unavailable;
            ++store.generation;
        }
        return;
    }
    const bool automatic = getSettings().backend.checkForModUpdates.getValue();
    if (automatic && !store.automatic) {
        store.requested = true;
        store.checkAt = Clock::now() + std::chrono::seconds{1};
    } else if (!automatic && !store.tracking) {
        store.requested = false;
    }
    store.automatic = automatic;
    bool changed = !store.initialized || store.loaderGeneration != loader.generation() ||
                   store.serviceGeneration != svc::services_generation();
    if (!changed) {
        for (const auto& mod : store.environment.mods) {
            const auto* current = loader.find_mod(mod.id);
            if (current == nullptr || current->is_enabled() != mod.enabled) {
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        auto environment = capture_environment();
        if (!store.initialized || environment != store.environment) {
            store.environment = std::move(environment);
            store.fresh = false;
            if (store.automatic || store.tracking) {
                store.requested = true;
                store.checkAt = Clock::now() + std::chrono::seconds{1};
            }
            ++store.generation;
        }
        store.initialized = true;
        store.loaderGeneration = loader.generation();
        store.serviceGeneration = svc::services_generation();
    }
    if (store.task && store.task.ready()) {
        auto result = store.task.try_take();
        store.task = {};
        if (store.requestEnvironment != store.environment) {
            store.requested = store.automatic || store.tracking;
            store.checkAt = Clock::now() + std::chrono::seconds{1};
        } else if (result && result->updates) {
            store.pending.insert(store.pending.end(),
                std::make_move_iterator(result->updates->begin()),
                std::make_move_iterator(result->updates->end()));
            if (store.nextTarget < store.targets.size()) {
                start_batch();
            } else {
                store.entries.clear();
                for (auto& entry : store.pending) {
                    store.entries.push_back({.result = std::move(entry)});
                }
                store.pending.clear();
                store.fresh = true;
                store.state = State::Ready;
                store.retries = 0;
                store.requested = false;
                ++store.generation;
                Log.info("Checked {} installed mods for updates", store.entries.size());
            }
        } else {
            store.state = State::Failed;
            store.error = result ? result->error : "The update check did not return a result.";
            if (result && result->retryable && store.retries < 3 &&
                (store.automatic || store.tracking))
            {
                store.requested = true;
                store.checkAt = Clock::now() + std::chrono::seconds{std::max(
                                                   result->retryAfter, 5 << store.retries++)};
            }
            Log.warn("Mod update check failed: {}", store.error);
            ++store.generation;
        }
    }
    if (!store.task && store.requested && Clock::now() >= store.checkAt) {
        begin_check();
    }
    refresh_entries();
}

void shutdown() noexcept {
    store = {};
}

void request_check() {
    store.tracking = true;
    if (store.task) {
        return;
    }
    store.requested = true;
    store.retries = 0;
    store.checkAt = Clock::now();
}

State state() noexcept {
    return store.state;
}

uint64_t generation() noexcept {
    return store.generation;
}

const std::vector<Entry>& entries() noexcept {
    return store.entries;
}

const Entry* find(std::string_view id) {
    const auto entry = std::ranges::find(
        store.entries, id, [](const Entry& entry) { return std::string_view{entry.result.id}; });
    return entry == store.entries.end() ? nullptr : &*entry;
}

size_t actionable_count() noexcept {
    return std::ranges::count_if(store.entries, &Entry::actionable);
}

uint64_t download_size() noexcept {
    uint64_t total = 0;
    for (const auto& entry : store.entries) {
        if (entry.actionable) {
            total += entry.result.target->download.size;
        }
    }
    return total;
}

std::string status_text() {
    if (!store.initialized) {
        return "Waiting for mods to initialize.";
    }
    if (store.state == State::Unavailable) {
        return "Online mod updates are unavailable.";
    }
    if (store.state == State::Checking) {
        return "Checking for mod updates…";
    }
    if (store.state == State::Idle) {
        return store.requested ? "Checking for mod updates…" : "Automatic update checks are off.";
    }
    if (!store.error.empty()) {
        return fmt::format("Could not check: {}", store.error);
    }
    if (!store.fresh) {
        return store.requested || store.task ? "Installed mods changed. Checking again…" :
                                               "Installed mods changed. Check again.";
    }
    if (const auto count = actionable_count()) {
        return fmt::format("{} update{} available", count, count == 1 ? "" : "s");
    }
    if (std::ranges::any_of(
            store.entries, [](const Entry& entry) { return !entry.queueKey.empty(); }))
    {
        return "No other updates available.";
    }
    if (std::ranges::any_of(store.entries,
            [](const Entry& entry) {
                return entry.result.target || !entry.result.blockers.empty();
            }))
    {
        return "No updates ready to install.";
    }
    return "Your mods are up to date.";
}

EnqueueResult enqueue_update(std::string_view id) {
    const auto* entry = find(id);
    if (!entry || !entry->result.target) {
        return {.skipped = 1, .error = "Check for updates first."};
    }
    if (const auto existing = queue::find_by_mod_id(id);
        existing && !queue::is_terminal(existing->state))
    {
        return {.skipped = 1, .queueKey = existing->id};
    }
    if (!store.fresh) {
        return {.skipped = 1, .error = "Check for updates again."};
    }
    auto expected = precondition(*entry);
    if (auto error = validate(expected); !error.empty()) {
        return {.skipped = 1, .error = std::move(error)};
    }
    const auto* local = ModLoader::instance().find_mod(id);
    const auto& target = *entry->result.target;
    std::string key;
    if (!queue::enqueue(
            {
                .id = std::string{id},
                .name = local->metadata.name,
                .version = target.version,
                .source = target.download,
                .update = std::move(expected),
            },
            &key))
    {
        return {.skipped = 1, .error = "Could not enqueue this update."};
    }
    refresh_entries();
    return {.accepted = 1, .queueKey = std::move(key)};
}

EnqueueResult enqueue_all() {
    std::vector<std::string> ids;
    for (const auto& entry : store.entries) {
        if (entry.actionable) {
            ids.push_back(entry.result.id);
        }
    }
    EnqueueResult result;
    for (const auto& id : ids) {
        auto item = enqueue_update(id);
        result.accepted += item.accepted;
        result.skipped += item.skipped;
        if (!item.error.empty()) {
            result.error = std::move(item.error);
        }
    }
    return result;
}

}  // namespace dusk::mods::updates

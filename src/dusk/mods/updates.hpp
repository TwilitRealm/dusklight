#pragma once

#include "services.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::mods {

struct Download {
    std::string url;
    std::string sha256;
    uint64_t size = 0;
};

struct InstalledPackage {
    std::string id;
    std::string version;
    bool enabled = false;
    std::vector<ServiceImport> imports;
    bool operator==(const InstalledPackage&) const = default;
};

struct UpdateEnvironment {
    std::string platform;
    uint32_t abi = 0;
    std::vector<ServiceExport> services;
    std::vector<InstalledPackage> mods;
    bool operator==(const UpdateEnvironment&) const = default;
};

struct UpdateCompatibility {
    bool containsNativeCode = false;
    std::vector<std::string> platforms;
    std::optional<uint32_t> abi;
    std::vector<ServiceImport> imports;
    std::vector<ServiceExport> exports;
};

struct UpdatePrecondition {
    std::string modId;
    std::string installedVersion;
    std::string targetVersion;
    UpdateCompatibility compatibility;
};

struct UpdateTarget {
    std::string version;
    Download download;
    UpdateCompatibility compatibility;
    std::string changelogHtml;
};

struct ModUpdate {
    std::string id;
    std::string installedVersion;
    bool published = false;
    bool yankedInstalled = false;
    std::string latestVersion;
    std::vector<std::string> blockers;
    std::optional<UpdateTarget> target;
};

}  // namespace dusk::mods

namespace dusk::mods::updates {

enum class State { Idle, Checking, Ready, Failed, Unavailable };

struct Entry {
    ModUpdate result;
    std::string reason;
    std::string queueKey;
    bool actionable = false;
};

struct EnqueueResult {
    size_t accepted = 0;
    size_t skipped = 0;
    std::string error;
    std::string queueKey;
};

void update();
void shutdown() noexcept;
void request_check();
State state() noexcept;
std::string status_text();
uint64_t generation() noexcept;
const std::vector<Entry>& entries() noexcept;
const Entry* find(std::string_view id);
size_t actionable_count() noexcept;
uint64_t download_size() noexcept;
EnqueueResult enqueue_update(std::string_view id);
EnqueueResult enqueue_all();

/** Returns the reason an update cannot be applied, or an empty string. */
std::string validate(const UpdateEnvironment& environment, const UpdatePrecondition& update);

/** Rechecks an update against the current loader and callable services. */
std::string validate(const UpdatePrecondition& precondition);

}  // namespace dusk::mods::updates

#pragma once

#include "dusk/mod_loader.hpp"

#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::mods {

struct PackageCandidate {
    std::filesystem::path path;
    ModMetadata metadata;
    uint32_t searchDirIndex = 0;
    bool fromDirectory = false;
    bool symlink = false;
};

std::vector<PackageCandidate> scan_packages(std::span<const ModSearchDir> searchDirs);
int compare_package_versions(std::string_view lhs, std::string_view rhs);
const PackageCandidate* select_package(
    std::span<const PackageCandidate> packages, std::string_view modId);
void record_package_sources(LoadedMod& mod, std::span<const PackageCandidate> packages);

std::unique_ptr<ModBundle> load_bundle(const std::filesystem::path& modPath, bool fromDir);

struct PackageInstallResult {
    bool replaced = false;
    std::string error;
};

PackageInstallResult install_package(const std::filesystem::path& path,
    const std::filesystem::path& destination, std::string_view modId);

}  // namespace dusk::mods

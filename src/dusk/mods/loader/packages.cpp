#include "packages.hpp"

#include "loader.hpp"
#include "manifest.hpp"

#include "dusk/data.hpp"

#include <borealis/io.hpp>
#include <borealis/log.hpp>
#include <borealis/update.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace dusk::mods {
namespace {

constexpr borealis::Log Log{"dusk::mods::loader"};

}  // namespace

std::unique_ptr<ModBundle> load_bundle(const fs::path& modPath, bool fromDir) {
    if (fromDir) {
        return std::make_unique<ModBundleDisk>(modPath);
    } else {
        return std::make_unique<ModBundleZip>(modPath);
    }
}

int compare_package_versions(std::string_view lhs, std::string_view rhs) {
    const auto left = borealis::update::parse_version(lhs);
    const auto right = borealis::update::parse_version(rhs);
    if (left && right) {
        return borealis::update::compare_version(*left, *right);
    }
    return static_cast<int>(left.has_value()) - static_cast<int>(right.has_value());
}

std::vector<PackageCandidate> scan_packages(std::span<const ModSearchDir> searchDirs) {
    std::vector<PackageCandidate> packages;
    for (size_t dirIndex = 0; dirIndex < searchDirs.size(); ++dirIndex) {
        const auto& searchDir = searchDirs[dirIndex];
        std::error_code error;
        bool alreadyScanned = false;
        for (size_t earlier = 0; earlier < dirIndex && !alreadyScanned; ++earlier) {
            alreadyScanned = fs::equivalent(searchDirs[earlier].path, searchDir.path, error);
        }
        if (alreadyScanned || !fs::is_directory(searchDir.path)) {
            continue;
        }
        std::vector<fs::directory_entry> entries;
        for (const auto& entry : fs::directory_iterator{searchDir.path}) {
            if ((entry.is_directory() && fs::exists(entry.path() / "mod.json")) ||
                (entry.is_regular_file() && entry.path().extension() == ".dusk"))
            {
                entries.push_back(entry);
            }
        }
        std::ranges::sort(entries, {}, &fs::directory_entry::path);
        for (const auto& entry : entries) {
            try {
                const bool fromDirectory = entry.is_directory();
                auto bundle = load_bundle(entry.path(), fromDirectory);
                packages.push_back({
                    .path = fs::absolute(entry.path()),
                    .metadata = load_manifest(entry.path(), *bundle).metadata,
                    .searchDirIndex = static_cast<uint32_t>(dirIndex),
                    .fromDirectory = fromDirectory,
                    .symlink = entry.is_symlink(),
                });
            } catch (const std::exception& exception) {
                Log.error("bad mod package {}: {}",
                    data::abbreviated_path_string(entry.path()), exception.what());
            }
        }
    }
    return packages;
}

const PackageCandidate* select_package(std::span<const PackageCandidate> packages,
    std::string_view modId) {
    const PackageCandidate* selected = nullptr;
    for (const auto& package : packages) {
        if (package.metadata.id != modId) {
            continue;
        }
        const int order = selected ?
            compare_package_versions(package.metadata.version, selected->metadata.version) : 1;
        if (order > 0 || (order == 0 &&
            (package.searchDirIndex < selected->searchDirIndex ||
                (package.searchDirIndex == selected->searchDirIndex && package.path < selected->path))))
        {
            selected = &package;
        }
    }
    return selected;
}

void record_package_sources(LoadedMod& mod, std::span<const PackageCandidate> packages) {
    mod.hasUserPackage = false;
    mod.hasBundledCopy = false;
    for (const auto& package : packages) {
        if (package.metadata.id != mod.metadata.id) {
            continue;
        }
        if (package.searchDirIndex != 0) {
            mod.hasBundledCopy = true;
        } else if (!package.fromDirectory && !package.symlink) {
            mod.hasUserPackage = true;
        }
    }
}

PackageInstallResult install_package(const fs::path& path, const fs::path& destination,
    std::string_view modId) {
    const auto userDir = destination.parent_path();
    std::error_code error;
    std::string validationError;
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error == std::errc::no_such_file_or_directory) {
        error.clear();
    }
    if (error) {
        return {.replaced = false,
            .error = fmt::format("Could not inspect the destination: {}", error.message())};
    }
    if (fs::exists(destinationStatus)) {
        ModMetadata existingMetadata;
        if (!fs::is_regular_file(destinationStatus) ||
            !inspect_mod_bundle(destination, existingMetadata, validationError) ||
            existingMetadata.id != modId)
        {
            return {
                .replaced = false,
                .error = "The destination filename is already used by another package",
            };
        }
    }
    std::vector<fs::path> duplicates;
    for (fs::directory_iterator entry{userDir, error}, end; !error && entry != end;
        entry.increment(error))
    {
        const auto& candidate = entry->path();
        if (candidate.extension() != ".dusk") {
            continue;
        }
        const bool regularFile = entry->is_regular_file(error);
        if (error) {
            break;
        }
        if (!regularFile) {
            continue;
        }
        ModMetadata candidateMetadata;
        if (inspect_mod_bundle(candidate, candidateMetadata, validationError) &&
            candidateMetadata.id == modId)
        {
            duplicates.push_back(candidate);
        }
    }
    if (error) {
        return {.replaced = false,
            .error = fmt::format("Could not scan the mods directory: {}", error.message())};
    }

    std::string replaceError;
    if (!borealis::io::atomic_replace(path, destination, replaceError)) {
        return {
            .replaced = false,
            .error = std::move(replaceError),
        };
    }

    std::string cleanupError;
    for (const auto& duplicate : duplicates) {
        // Replacing a file on a case-insensitive filesystem can preserve its old spelling.
        const bool destinationAlias =
            fs::equivalent(duplicate, destination, error) && !fs::is_symlink(duplicate, error);
        error.clear();
        if (destinationAlias) {
            if (duplicate.filename() == destination.filename()) {
                continue;
            }
            fs::rename(duplicate, destination, error);
        } else {
            fs::remove(duplicate, error);
        }
        if (error) {
            cleanupError = fmt::format("Installed, but could not consolidate '{}': {}",
                data::abbreviated_path_string(duplicate), error.message());
            Log.warn("{}", cleanupError);
        }
    }
    return {.replaced = true, .error = std::move(cleanupError)};
}

}  // namespace dusk::mods

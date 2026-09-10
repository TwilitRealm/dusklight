#pragma once

#include "dusk/mod_loader.hpp"
#include "modal.hpp"

#include <borealis/task.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace dusk::ui {

struct DropPackage {
    std::filesystem::path path;
    mods::ModMetadata metadata;
    uint64_t size = 0;
    std::string error;
    std::string status;
    bool hasNative = false;
    bool valid = false;
};

std::vector<DropPackage> inspect_drop_packages(
    const std::vector<std::filesystem::path>& paths, borealis::TaskContext& context);

class DropInstallModal final : public Modal {
public:
    explicit DropInstallModal(std::vector<DropPackage> packages);

private:
    struct PreparedTag {};
    DropInstallModal(std::vector<DropPackage> packages, PreparedTag);
    void install();

    std::vector<DropPackage> mPackages;
};

}  // namespace dusk::ui

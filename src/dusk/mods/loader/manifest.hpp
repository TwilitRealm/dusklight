#pragma once

#include "dusk/mod_loader.hpp"

namespace dusk::mods {

struct LoadedManifest {
    ModMetadata metadata;
    std::optional<DelegatedModRuntime> runtime;
};

LoadedManifest load_manifest(const std::filesystem::path& modPath, ModBundle& bundle);

}  // namespace dusk::mods

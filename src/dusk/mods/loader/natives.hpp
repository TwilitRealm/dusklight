#pragma once

#include <string_view>

namespace dusk::mods {

struct ModManifestInfo;
struct ModMetaParsed;

bool has_native_library_extension(std::string_view name);
ModManifestInfo build_manifest_info(const ModMetaParsed& parsed);

}  // namespace dusk::mods

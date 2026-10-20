#pragma once

#if _WIN32

#include <span>
#include <filesystem>

namespace dusk::mods::loader::amsi {

/**
 * Check the provided native data with AMSI. Returns true if it detected malware.
 */
bool check_for_malware(std::filesystem::path const& targetPath, std::span<u8 const> data);

}

#endif

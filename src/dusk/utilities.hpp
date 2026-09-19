#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef crc32
// miniz defines crc32 as an alias.
#undef crc32
#endif

namespace dusk::utils {

std::string base64_encode(const std::vector<uint8_t>& data);
bool base64_decode(const std::string& text, std::vector<uint8_t>& out);
uint32_t crc32(const void* data, size_t size);

struct PaneCache {
    uint64_t tag;
    float origTransX;
    float origTransY;
    bool cached;
};

// Returns the string before the first NUL, or nullopt if the buffer has no terminator.
std::optional<std::string_view> bounded_string(const char* value, size_t capacity);

// Names must be nonempty, contain no NULs, and fit within maxBytes.
bool is_valid_name(std::string_view name, size_t maxBytes = std::string_view::npos);
bool is_valid_name(const char* name, size_t maxBytes = std::string_view::npos);

// Nonempty [a-z0-9_] segments separated by periods.
bool is_valid_mod_id(std::string_view id);
// 1-31 bytes matching [A-Za-z0-9._-], excluding "." and "..".
bool is_valid_save_name(std::string_view name);
// 1-64 bytes matching [A-Za-z0-9_-].
bool is_valid_config_name(std::string_view name);

// A single path component without separators, colons, NULs, ".", or "..".
bool is_safe_path_component(std::string_view name);
// A relative path with no empty, ".", or ".." components.
bool is_safe_resource_path(std::string_view path);
bool is_valid_disc_path(std::string_view path);
// Replaces characters that don't match [A-Za-z0-9._-] with underscores.
std::string safe_filename(std::string_view value);

}  // namespace dusk::utils

#include "utilities.hpp"

#include <algorithm>
#include <array>
#include <cstring>

namespace dusk::utils {
namespace {

constexpr char kBase64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::array<uint32_t, 256> generate_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t ch = i;
        for (size_t j = 0; j < 8; ++j) {
            ch = (ch & 1) != 0 ? 0xEDB88320 ^ ch >> 1 : ch >> 1;
        }
        table[i] = ch;
    }
    return table;
}

constexpr std::array<uint32_t, 256> kCrc32Table = generate_crc32_table();

bool is_ascii_lowercase_alphanumeric(char value) {
    return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
}

bool is_ascii_alphanumeric(char value) {
    return is_ascii_lowercase_alphanumeric(value) || (value >= 'A' && value <= 'Z');
}

bool is_filename_character(char value) {
    return is_ascii_alphanumeric(value) || value == '.' || value == '_' || value == '-';
}

}  // namespace

std::string base64_encode(const std::vector<uint8_t>& data) {
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        const uint32_t rest = data.size() - i;
        uint32_t chunk = data[i] << 16;
        if (rest > 1) {
            chunk |= data[i + 1] << 8;
        }
        if (rest > 2) {
            chunk |= data[i + 2];
        }
        out.push_back(kBase64Chars[chunk >> 18 & 0x3F]);
        out.push_back(kBase64Chars[chunk >> 12 & 0x3F]);
        out.push_back(rest > 1 ? kBase64Chars[chunk >> 6 & 0x3F] : '=');
        out.push_back(rest > 2 ? kBase64Chars[chunk & 0x3F] : '=');
    }
    return out;
}

bool base64_decode(const std::string& text, std::vector<uint8_t>& out) {
    if (text.size() % 4 != 0) {
        return false;
    }
    static const auto lookup = [] {
        std::array<int8_t, 256> table;
        table.fill(-1);
        for (int i = 0; i < 64; ++i) {
            table[static_cast<uint8_t>(kBase64Chars[i])] = static_cast<int8_t>(i);
        }
        return table;
    }();
    out.clear();
    out.reserve(text.size() / 4 * 3);
    for (size_t i = 0; i < text.size(); i += 4) {
        uint32_t chunk = 0;
        int pads = 0;
        for (size_t j = 0; j < 4; ++j) {
            const char c = text[i + j];
            if (c == '=' && i + 4 == text.size() && j >= 2) {
                ++pads;
                chunk <<= 6;
                continue;
            }
            const int8_t value = lookup[static_cast<uint8_t>(c)];
            if (value < 0 || pads != 0) {
                return false;
            }
            chunk = chunk << 6 | static_cast<uint32_t>(value);
        }
        out.push_back(chunk >> 16 & 0xFF);
        if (pads < 2) {
            out.push_back(chunk >> 8 & 0xFF);
        }
        if (pads < 1) {
            out.push_back(chunk & 0xFF);
        }
    }
    return true;
}

uint32_t crc32(const void* data, size_t size) {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = ~0u;
    for (size_t i = 0; i < size; ++i) {
        crc = crc >> 8 ^ kCrc32Table[static_cast<uint8_t>(crc ^ bytes[i])];
    }
    return ~crc;
}

std::optional<std::string_view> bounded_string(const char* value, size_t capacity) {
    if (value == nullptr || capacity == 0) {
        return std::nullopt;
    }
    const auto* end = static_cast<const char*>(std::memchr(value, '\0', capacity));
    if (end == nullptr) {
        return std::nullopt;
    }
    return std::string_view{value, static_cast<size_t>(end - value)};
}

bool is_valid_name(std::string_view name, size_t maxBytes) {
    return !name.empty() && name.size() <= maxBytes && name.find('\0') == std::string_view::npos;
}

bool is_valid_name(const char* name, size_t maxBytes) {
    return name != nullptr && is_valid_name(std::string_view{name}, maxBytes);
}

bool is_valid_mod_id(std::string_view id) {
    if (!is_valid_name(id) || id.front() == '.' || id.back() == '.' ||
        id.find("..") != std::string_view::npos)
    {
        return false;
    }
    return std::ranges::all_of(id, [](char value) {
        return is_ascii_lowercase_alphanumeric(value) || value == '_' || value == '.';
    });
}

bool is_valid_save_name(std::string_view name) {
    return is_valid_name(name, 31) && name != "." && name != ".." &&
           std::ranges::all_of(name, is_filename_character);
}

bool is_valid_config_name(std::string_view name) {
    return is_valid_name(name, 64) && std::ranges::all_of(name, [](char value) {
        return is_ascii_alphanumeric(value) || value == '_' || value == '-';
    });
}

bool is_safe_path_component(std::string_view name) {
    return is_valid_name(name) && name != "." && name != ".." &&
           name.find_first_of("/\\:") == std::string_view::npos;
}

bool is_safe_resource_path(std::string_view path) {
    while (true) {
        const auto separator = path.find_first_of("/\\");
        if (!is_safe_path_component(path.substr(0, separator))) {
            return false;
        }
        if (separator == std::string_view::npos) {
            return true;
        }
        path.remove_prefix(separator + 1);
    }
}

bool is_valid_disc_path(std::string_view path) {
    return path.starts_with('/') && is_safe_resource_path(path.substr(1));
}

std::string safe_filename(std::string_view value) {
    std::string result{value};
    std::ranges::replace_if(
        result, [](char character) { return !is_filename_character(character); }, '_');
    return result;
}

}  // namespace dusk::utils

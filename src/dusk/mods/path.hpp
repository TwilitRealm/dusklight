#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace dusk::mods {

inline std::string safe_filename(std::string_view value) {
    std::string result{value};
    std::ranges::replace_if(
        result,
        [](char character) {
            return !((character >= 'a' && character <= 'z') ||
                     (character >= 'A' && character <= 'Z') ||
                     (character >= '0' && character <= '9') || character == '.' ||
                     character == '_' || character == '-');
        },
        '_');
    return result;
}

}  // namespace dusk::mods

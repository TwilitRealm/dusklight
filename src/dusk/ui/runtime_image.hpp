#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace dusk::ui {

struct DecodedImage {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
};

std::optional<DecodedImage> decode_png(std::span<const uint8_t> data, std::string_view source);

}  // namespace dusk::ui

#include "runtime_image.hpp"

#include <borealis/log.hpp>
#include <png.h>

#include <cstddef>
#include <memory>

namespace dusk::ui {
namespace {

constexpr borealis::Log Log{"dusk::ui"};
constexpr uint32_t kMaxImageDimension = 4096;

}  // namespace

std::optional<DecodedImage> decode_png(std::span<const uint8_t> data, std::string_view source) {
    png_image png{};
    png.version = PNG_IMAGE_VERSION;
    const std::unique_ptr<png_image, decltype(&png_image_free)> cleanup{&png, png_image_free};
    if (!png_image_begin_read_from_memory(&png, data.data(), data.size())) {
        Log.warn("Failed to read image header '{}': {}", source, png.message);
        return std::nullopt;
    }
    if (png.width == 0 || png.height == 0 || png.width > kMaxImageDimension ||
        png.height > kMaxImageDimension)
    {
        Log.warn("Image '{}' has unsupported dimensions {}x{}", source, png.width, png.height);
        return std::nullopt;
    }

    png.format = PNG_FORMAT_RGBA;
    DecodedImage image{
        .width = png.width,
        .height = png.height,
    };
    image.pixels.resize(PNG_IMAGE_SIZE(png));
    if (!png_image_finish_read(&png, nullptr, image.pixels.data(), 0, nullptr)) {
        Log.warn("Failed to decode image '{}': {}", source, png.message);
        return std::nullopt;
    }
    for (size_t offset = 0; offset < image.pixels.size(); offset += 4) {
        const uint8_t alpha = image.pixels[offset + 3];
        for (size_t channel = 0; channel < 3; ++channel) {
            image.pixels[offset + channel] = static_cast<uint8_t>(
                (static_cast<uint32_t>(image.pixels[offset + channel]) * alpha) / 255);
        }
    }
    return image;
}

}  // namespace dusk::ui

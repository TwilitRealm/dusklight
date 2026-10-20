#pragma once

#include <span>

#include "image.h"
#include "mods/expected.hpp"

namespace mods::image {

/**
 * RAII wrapper around an @c svc_image handle.
 *
 * @see ImageService
 */
struct Image {
    ImageHandle handle = nullptr;

    constexpr Image() noexcept = default;

    constexpr explicit Image(ImageHandle handle) noexcept : handle(handle) {}

    Image(Image&& other) noexcept { *this = std::move(other); }

    Image& operator=(Image&& other) noexcept {
        unregister(handle);

        handle = other.handle;
        other.handle = nullptr;

        return *this;
    }

    ~Image() { unregister(handle); }

    [[nodiscard]] ModExpected<ImageInfo> get_info() const {
        if (!svc_image) {
            return std::unexpected(MOD_UNAVAILABLE);
        }

        ImageInfo info IMAGE_INFO_INIT;
        auto result = svc_image->image_info(mod_ctx, handle, &info);
        if (result != MOD_OK) {
            return std::unexpected(result);
        }

        return info;
    }

    [[nodiscard]] ModExpected<void> decode_rgba8(std::span<unsigned char> data) {
        if (!svc_image) {
            return std::unexpected(MOD_UNAVAILABLE);
        }

        auto result = svc_image->image_decode_rgba8(mod_ctx, handle, data.data(), data.size());
        if (result != MOD_OK) {
            return std::unexpected(result);
        }

        return {};
    }

private:
    static void unregister(ImageHandle handle) {
        if (svc_image && handle) {
            svc_image->image_close(mod_ctx, handle);
        }
    }
};

[[nodiscard]] inline ModExpected<Image> open(
    std::span<unsigned char const> data, ImageFormat format = IMAGE_FORMAT_ANY) {
    if (!svc_image) {
        return std::unexpected(MOD_UNAVAILABLE);
    }

    ImageHandle handle;
    auto result = svc_image->image_open(mod_ctx, data.data(), data.size(), format, &handle);
    if (result != MOD_OK) {
        return std::unexpected(result);
    }

    return Image(handle);
}

}  // namespace mods::image

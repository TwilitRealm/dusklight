#pragma once

#include "dusk/mod_loader.hpp"
#include "mods/svc/image.h"

namespace dusk::mods::svc::image {

constexpr size_t kPixelRgbaWidth = IMAGE_PIXEL_RGBA8_BYTES;

class ImageHandleImpl {
public:
    ImageFormat format;
    uint32_t width = 0;
    uint32_t height = 0;
    bool hasRead = false;

    virtual ~ImageHandleImpl() = default;
    virtual ModResult image_decode_rgba8(std::span<u8> dest) = 0;

protected:
    explicit ImageHandleImpl(ImageFormat format) : format(format) {

    }
};

using ImageLoadFunction = ModResult (*)(
    LoadedMod const& mod, ImageHandleImpl*& slot, std::span<u8 const> fileData);

ModResult open_png(LoadedMod const& mod, ImageHandleImpl*& slot, std::span<u8 const> fileData);

}  // namespace dusk::mods::svc::image

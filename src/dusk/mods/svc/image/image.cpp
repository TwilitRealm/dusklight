#include "image.hpp"

#include "borealis/aurora_log.h"
#include "dusk/mods/loader/loader.hpp"
#include "dusk/mods/svc/registry.hpp"

namespace dusk::mods::svc::image {

namespace {

borealis::Log Log("dusk::mods::svc::image");

constexpr std::tuple<ImageFormat, ImageLoadFunction> kOpenFunctions[] = {
    {IMAGE_FORMAT_PNG, open_png},
};

ModResult impl_image_open(
    ModContext* ctx, void const* buf, size_t length, ImageFormat format, ImageHandle* out_handle) {
    auto* const mod = mod_from_context(ctx);
    if (mod == nullptr || buf == nullptr || length == 0 || out_handle == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }

    std::span data{static_cast<u8 const*>(buf), length};

    *out_handle = nullptr;
    ImageHandleImpl* impl_handle = nullptr;

    for (auto [format_mask, open_fn] : kOpenFunctions) {
        if (format != IMAGE_FORMAT_ANY && (format & format_mask) == 0) {
            continue;
        }

        auto const result = open_fn(*mod, impl_handle, data);
        if (result == MOD_OK) {
            assert(impl_handle);
            assert(impl_handle->width > 0 && impl_handle->height > 0);
            assert(impl_handle->format != IMAGE_FORMAT_ANY);

            *out_handle = impl_handle;
            assert(*out_handle);
            return MOD_OK;
        }

        if (result == MOD_UNSUPPORTED) {
            // Try next loader, not this format.
            continue;
        }

        // Individual loader is expected to log an error in this case.
        return result;
    }

    Log.error("image_open failed: no format recognized");
    return MOD_UNSUPPORTED;
}

ModResult impl_image_close(ModContext* ctx, ImageHandle handle) {
    auto* const mod = mod_from_context(ctx);
    if (mod == nullptr || handle == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }

    auto const cast_ptr = static_cast<ImageHandleImpl*>(handle);
    delete cast_ptr;

    return MOD_OK;
}

ModResult impl_image_info(ModContext* ctx, ImageHandle handle, ImageInfo* out_info) {
    auto* const mod = mod_from_context(ctx);
    if (mod == nullptr || handle == nullptr || out_info == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }

    if (out_info->struct_size != sizeof(ImageInfo)) {
        return MOD_INVALID_ARGUMENT;
    }

    auto const cast_ptr = static_cast<ImageHandleImpl*>(handle);
    out_info->width = cast_ptr->width;
    out_info->height = cast_ptr->height;
    out_info->format = cast_ptr->format;

    return MOD_OK;
}

ModResult impl_image_decode_rgba8(
    ModContext* ctx, ImageHandle handle, unsigned char* buf, size_t length) {
    auto* const mod = mod_from_context(ctx);
    if (mod == nullptr || handle == nullptr || buf == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }

    auto const cast_ptr = static_cast<ImageHandleImpl*>(handle);

    auto expected_length = cast_ptr->width * cast_ptr->height * kPixelRgbaWidth;
    if (length < expected_length) {
        Log.error("image_decode_rgba8: output buffer too small");
        return MOD_INVALID_ARGUMENT;
    }

    if (cast_ptr->hasRead) {
        Log.error("image_decode_rgba8: image has already been decoded");
        return MOD_INVALID_ARGUMENT;
    }

    cast_ptr->hasRead = true;

    std::span data(buf, length);

    return cast_ptr->image_decode_rgba8(data);
}

constexpr ImageService s_imageService{
    .header = SERVICE_HEADER(ImageService, IMAGE_SERVICE_MAJOR, IMAGE_SERVICE_MINOR),
    .image_open = impl_image_open,
    .image_close = impl_image_close,
    .image_info = impl_image_info,
    .image_decode_rgba8 = impl_image_decode_rgba8,
};

}  // namespace

}  // namespace dusk::mods::svc::image

namespace dusk::mods::svc {

ServiceModule const g_imageModule{
    .id = IMAGE_SERVICE_ID,
    .majorVersion = IMAGE_SERVICE_MAJOR,
    .minorVersion = IMAGE_SERVICE_MINOR,
    .service = &image::s_imageService,
};

}
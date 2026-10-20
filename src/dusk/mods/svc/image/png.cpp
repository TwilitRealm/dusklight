#include <png.h>

#include "borealis/log.hpp"
#include "image.hpp"

namespace dusk::mods::svc::image {

namespace {

borealis::Log Log("dusk::mods::svc::image::png");

struct PngStructs {
    png_structp pStruct = nullptr;
    png_infop pInfo = nullptr;

    ~PngStructs() { png_destroy_read_struct(&pStruct, &pInfo, nullptr); }
};


class ImageHandlePng final : public ImageHandleImpl {
public:
    PngStructs png;
    std::span<u8 const> bytes;
    size_t read_pos = 0;

    ImageHandlePng() : ImageHandleImpl(IMAGE_FORMAT_PNG) {
    }

    ~ImageHandlePng() override = default;
    ModResult image_decode_rgba8(std::span<u8> dest) override {
        // I'm scared of putting any locals after that setjmp.
        std::vector<png_bytep> rowPointers { height };

        if (setjmp(png_jmpbuf(png.pStruct))) {
            Log.error("libpng encountered an error");
            return MOD_ERROR;
        }

        // Always read as RGBA8.
        png_set_gray_to_rgb(png.pStruct);
        png_set_filler(png.pStruct, 0xFF, PNG_FILLER_AFTER);
        png_set_expand(png.pStruct);
        png_set_strip_16(png.pStruct);

        png_read_update_info(png.pStruct, png.pInfo);
        auto rowBytes = png_get_rowbytes(png.pStruct, png.pInfo);
        if (rowBytes != kPixelRgbaWidth * width) {
            // Wrong size? Huh.
            Log.error("PNG size mismatch");
            return MOD_ERROR;
        }

        for (u32 i = 0; i < height; i++) {
            rowPointers[i] = &dest[i * rowBytes];
        }

        png_read_image(png.pStruct, rowPointers.data());
        png_read_end(png.pStruct, nullptr);

        return MOD_OK;
    }
};

void readPngData(png_structp png, png_bytep data, const size_t length) {
    auto* cursor = static_cast<ImageHandlePng*>(png_get_io_ptr(png));
    if (length > cursor->bytes.size() - cursor->read_pos) {
        png_error(png, "unexpected end of data");
    }

    std::memcpy(data, cursor->bytes.data() + cursor->read_pos, length);
    cursor->read_pos += length;
}

}

ModResult open_png(LoadedMod const&, ImageHandleImpl*& slot, std::span<u8 const> fileData) {
    constexpr size_t kPngSignatureSize = 8;  // No constant for this in libpng?
    if (fileData.size() <= kPngSignatureSize) {
        return MOD_UNSUPPORTED;
    }

    // Check whether it's (probably) a PNG.
    if (png_sig_cmp(fileData.data(), 0, kPngSignatureSize)) {
        return MOD_UNSUPPORTED;
    }

    auto handle = std::make_unique<ImageHandlePng>();
    handle->bytes = fileData;
    handle->png.pStruct = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (!handle->png.pStruct) {
        Log.error("png_create_read_struct failed");
        return MOD_ERROR;
    }

    handle->png.pInfo = png_create_info_struct(handle->png.pStruct);
    if (!handle->png.pInfo) {
        Log.error("png_create_info_struct failed");
        return MOD_ERROR;
    }

    if (setjmp(png_jmpbuf(handle->png.pStruct))) {
        Log.error("libpng encountered an error");
        return MOD_ERROR;
    }

    png_set_read_fn(handle->png.pStruct, handle.get(), readPngData);
    png_read_info(handle->png.pStruct, handle->png.pInfo);

    int bit_depth, color_type, interlace_type, compression_type, filter_type;
    if (!png_get_IHDR(handle->png.pStruct, handle->png.pInfo, &handle->width, &handle->height, &bit_depth, &color_type,
            &interlace_type, &compression_type, &filter_type))
    {
        Log.error("libpng unable to read IHDR");
        return MOD_ERROR;
    }

    slot = handle.release();
    return MOD_OK;
}

}  // namespace
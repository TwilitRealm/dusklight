#pragma once

#include <mods/api.h>

#define IMAGE_SERVICE_ID "dev.twilitrealm.dusklight.image"
#define IMAGE_SERVICE_MAJOR 1u
#define IMAGE_SERVICE_MINOR 0u

/*
 * svc_image: a simple service for decoding image data to raw bytes.
 *
 * Supported image decoders use common libraries and are suitable for untrusted input.
 * Note that typical image loading safeguards still apply
 * (e.g. checking size before allocating a huge image buffer).
 */

/**
 * Bitflags for image formats supported by @c svc_image.
 *
 * By default, opening an image will automatically try all supported image formats,
 * detecting based on file contents.
 * By specifying these flags, the set of formats tested can be limited.
 */
typedef enum ImageFormat {
    /**
     * No image format specified. All image formats will be tried in @c image_open
     */
    IMAGE_FORMAT_ANY = 0,

    /**
     * The PNG image format.
     */
    IMAGE_FORMAT_PNG = 1 << 0,
} ImageFormat;

/**
 * Basic information of an image that can be gleaned without doing a full decode.
 */
typedef struct ImageInfo {
    /**
     * Must always be initialized to @c sizeof(ImageInfo)
     */
    size_t struct_size;

    /**
     * Image format which this image was decoded as.
     */
    ImageFormat format;

    /**
     * Width of the image, in pixels.
     */
    uint32_t width;

    /**
     * Height of the image, in pixels.
     */
    uint32_t height;
} ImageInfo;

#define IMAGE_INFO_INIT {sizeof(ImageInfo), IMAGE_FORMAT_ANY, 0u, 0u};

/**
 * Bytes per pixel for RGBA8.
 */
#define IMAGE_PIXEL_RGBA8_BYTES 4

typedef void* ImageHandle;

typedef struct ImageService {
    ServiceHeader header;

    /**
     * Open an in-memory image file.
     *
     * This merely parses image metadata, it does *not* yet decode pixel data.
     * The provided buffer *must* be kept alive until the image is closed (@c image_close).
     *
     * @param buf Pointer to buffer containing image data.
     * @param length Length of @c buf
     * @param format Allowed formats to try when decoding image.
     *      Leave at @c IMAGE_FORMAT_ANY to to try every supported format.
     * @param out_handle Receives a handle representing the opened image,
     *      used with further API calls.
     */
    ModResult (*image_open)(
        ModContext* ctx,
        void const* buf,
        size_t length,
        ImageFormat format,
        ImageHandle* out_handle);

    /**
     * Close an image previously opened with @c image_open
     */
    ModResult (*image_close)(
        ModContext* ctx,
        ImageHandle handle);

    /**
     * Fetch basic information about an opened image.
     *
     * @see ImageInfo
     */
    ModResult (*image_info)(
        ModContext* ctx,
        ImageHandle handle,
        ImageInfo* out_info);

    /**
     * Decode an opened image to RGBA8 pixel data.
     *
     * This function may only be called once.
     *
     * The decoded pixel data is tightly packed, each row of pixels having no padding.
     * The provided buffer will receive the image data and must be at least
     * <tt>width * height * IMAGE_PIXEL_RGBA8_BYTES</tt> large.
     *
     * @param handle Handle to open image.
     * @param buf Buffer to receive pixel data.
     * @param length Size of @c buf
     */
    ModResult (*image_decode_rgba8)(
        ModContext* ctx,
        ImageHandle handle,
        unsigned char* buf,
        size_t length);
} ImageService;

MOD_DECLARE_SERVICE(ImageService, svc_image, IMAGE_SERVICE_ID, IMAGE_SERVICE_MAJOR, IMAGE_SERVICE_MINOR);

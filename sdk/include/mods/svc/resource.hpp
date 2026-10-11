#pragma once

#include <cassert>
#include <expected>
#include <span>

#include <mods/svc/resource.h>

#include "mods/expected.hpp"

/*
 * C++ wrapper for svc_resource
 */

namespace mods::resource {

/**
 * RAII wrapper for buffers returned by the resource service.
 */
struct LoadedResource {
    ResourceBuffer buffer{.struct_size = sizeof(buffer)};

    explicit LoadedResource(ResourceBuffer buffer) noexcept : buffer(buffer) {
        assert(sizeof(buffer) == buffer.struct_size);
    }
    LoadedResource(LoadedResource const&) = delete;
    LoadedResource(LoadedResource&& other) noexcept {
        buffer = other.buffer;
        other.buffer = {};
    }

    [[nodiscard]] constexpr std::span<uint8_t const> span() const noexcept {
        auto base = static_cast<uint8_t const*>(buffer.data);
        return {base, base + buffer.size};
    }

    ~LoadedResource() {
        if (svc_resource && buffer.data) {
            assert(sizeof(buffer) == buffer.struct_size);

            svc_resource->free(mod_ctx, &buffer);
        }
    }
};

/**
 * Load a file. @c relative_path is resolved against the bundle's
 * <tt>res/</tt> directory. Absolute paths and ".." are rejected. @c MOD_UNAVAILABLE if the file
 * does not exist. An empty file loads as data == NULL with size 0.
 */
[[nodiscard]] inline ModExpected<LoadedResource> load(char const* relative_path) {
    if (!svc_resource) {
        return std::unexpected(MOD_UNAVAILABLE);
    }

    ResourceBuffer buffer{.struct_size = sizeof(buffer)};
    auto const result = svc_resource->load(mod_ctx, relative_path, &buffer);
    if (result != MOD_OK) {
        return std::unexpected(result);
    }

    return LoadedResource(buffer);
}

/**
 * Load a file. @c relative_path is resolved against the bundle's
 * <tt>res/</tt> directory. Absolute paths and ".." are rejected. @c MOD_UNAVAILABLE if the file
 * does not exist. An empty file loads as data == NULL with size 0.
 */
[[nodiscard]] inline ModExpected<LoadedResource> load(std::string const& relative_path) {
    return load(relative_path.c_str());
}

[[nodiscard]] inline bool file_exists(char const* relative_path) {
    return svc_resource->file_exists(mod_ctx, relative_path);
}

[[nodiscard]] inline bool directory_exists(char const* relative_path) {
    return svc_resource->directory_exists(mod_ctx, relative_path);
}

}  // namespace mods::resource

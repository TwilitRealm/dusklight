#include "registry.hpp"

#include <borealis/log.hpp>
#include "dusk/mods/loader/loader.hpp"
#include "dusk/utilities.hpp"
#include "mods/svc/resource.h"

#include <fmt/format.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>

namespace dusk::mods::svc {
namespace {

constexpr borealis::Log Log{"dusk::mods::resource"};

// Allocations by owning mod, so buffers still live when a mod detaches can be freed.
std::unordered_map<void*, const LoadedMod*> s_buffers;

void resource_remove_mod(LoadedMod& mod) {
    size_t reclaimed = 0;
    std::erase_if(s_buffers, [&](const auto& entry) {
        if (entry.second != &mod) {
            return false;
        }
        std::free(entry.first);
        ++reclaimed;
        return true;
    });
    if (reclaimed != 0) {
        Log.warn("[{}] reclaimed {} resource buffer(s) that were never freed", mod.metadata.id,
            reclaimed);
    }
}

std::string prefix_path(const char* relativePath) {
    return fmt::format("res/{}", relativePath);
}

ModResult resource_load(ModContext* context, const char* relativePath, ResourceBuffer* outBuffer) {
    if (outBuffer == nullptr || outBuffer->struct_size < sizeof(ResourceBuffer)) {
        return MOD_INVALID_ARGUMENT;
    }
    outBuffer->data = nullptr;
    outBuffer->size = 0;
    auto* mod = mod_from_context(context);
    if (mod == nullptr || relativePath == nullptr || !utils::is_safe_resource_path(relativePath)) {
        return MOD_INVALID_ARGUMENT;
    }

    const auto entry = prefix_path(relativePath);
    std::vector<u8> data;
    try {
        data = mod->bundle->readFile(entry);
    } catch (const std::runtime_error& e) {
        Log.error("[{}] resource load '{}' failed: {}", mod->metadata.id, entry, e.what());
        return MOD_UNAVAILABLE;
    }

    if (!data.empty()) {
        void* copy = std::malloc(data.size());
        if (copy == nullptr) {
            return MOD_ERROR;
        }
        std::memcpy(copy, data.data(), data.size());
        s_buffers.emplace(copy, mod);
        outBuffer->data = copy;
        outBuffer->size = data.size();
    }
    return MOD_OK;
}

void resource_free(ModContext* context, ResourceBuffer* buffer) {
    if (buffer == nullptr || buffer->struct_size < sizeof(ResourceBuffer) ||
        buffer->data == nullptr)
    {
        return;
    }
    auto* mod = mod_from_context(context);
    const auto it = s_buffers.find(buffer->data);
    if (it == s_buffers.end()) {
        Log.error("[{}] resource free: not a live loaded buffer", mod_id_from_context(context));
        return;
    }
    if (mod == nullptr || it->second != mod) {
        Log.error("[{}] resource free: buffer is owned by '{}'", mod_id_from_context(context),
            it->second != nullptr ? it->second->metadata.id : "unknown");
        return;
    }
    s_buffers.erase(it);
    std::free(buffer->data);
    buffer->data = nullptr;
    buffer->size = 0;
}

bool file_exists(ModContext* ctx, char const* relative_path) {
    auto* mod = mod_from_context(ctx);
    if (mod == nullptr || relative_path == nullptr || !utils::is_safe_resource_path(relative_path))
    {
        return MOD_INVALID_ARGUMENT;
    }

    return mod->bundle->file_exists(prefix_path(relative_path));
}

bool directory_exists(ModContext* ctx, char const* relative_path) {
    auto* mod = mod_from_context(ctx);
    if (mod == nullptr || relative_path == nullptr || !utils::is_safe_resource_path(relative_path))
    {
        return MOD_INVALID_ARGUMENT;
    }

    return mod->bundle->directory_exists(prefix_path(relative_path));
}

constexpr ResourceService s_resourceService{
    .header = SERVICE_HEADER(ResourceService, RESOURCE_SERVICE_MAJOR, RESOURCE_SERVICE_MINOR),
    .load = resource_load,
    .free = resource_free,
    .file_exists = file_exists,
    .directory_exists = directory_exists,
};

}  // namespace

constinit const ServiceModule g_resourceModule{
    .id = RESOURCE_SERVICE_ID,
    .majorVersion = RESOURCE_SERVICE_MAJOR,
    .minorVersion = RESOURCE_SERVICE_MINOR,
    .service = &s_resourceService,
    .modDetached = resource_remove_mod,
};

}  // namespace dusk::mods::svc

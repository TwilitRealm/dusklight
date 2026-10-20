#include "mods/svc/imgui.h"

#include <absl/container/flat_hash_map.h>

#include "../../imgui.hpp"
#include "borealis/log.hpp"
#include "dusk/mods/loader/loader.hpp"
#include "registry.hpp"

namespace dusk::mods::svc {
namespace {

constexpr borealis::Log Log{"dusk::mods::imgui"};

struct ModDatum {
    std::array<std::pair<ImGuiCallback, void*>, IMGUI_CALLBACK_MAX> callbacks;
};

absl::flat_hash_map<LoadedMod*, ModDatum> modData;

ModResult set_callback(
    ModContext* context, ImguiCallbackPoint point, ImGuiCallback callback, void* user_data) {
    auto* mod = mod_from_context(context);
    if (mod == nullptr) {
        return MOD_INVALID_ARGUMENT;
    }

    if (!(point >= IMGUI_CALLBACK_FRAME && point < IMGUI_CALLBACK_MAX)) {
        Log.error("[{}] invalid callback point", mod->metadata.id);
        return MOD_INVALID_ARGUMENT;
    }

    if (!callback && user_data) {
        Log.error("[{}] cannot set userdata when clearing callback", mod->metadata.id);
        return MOD_INVALID_ARGUMENT;
    }

    auto& modDatum = modData[mod];

    modDatum.callbacks.at(point) = {callback, user_data};

    return MOD_OK;
}

void mod_detached(LoadedMod& mod) {
    modData.erase(&mod);
}

constexpr ImguiService s_imguiService{
    .header = SERVICE_HEADER(IMGUI_SERVICE_ID, IMGUI_SERVICE_MAJOR, IMGUI_SERVICE_MINOR),
    .set_callback = set_callback,
};

}  // namespace

constinit ServiceModule const g_imguiModule{
    .id = IMGUI_SERVICE_ID,
    .majorVersion = IMGUI_SERVICE_MAJOR,
    .minorVersion = IMGUI_SERVICE_MINOR,
    .service = &s_imguiService,
    .modDetached = mod_detached,
};

}  // namespace dusk::mods::svc

namespace dusk::mods {

void imgui_run_point(ImguiCallbackPoint const point) {
    for (auto& [ctx, datum] : svc::modData) {
        auto [cb, userData] = datum.callbacks.at(point);

        if (!cb) {
            continue;
        }

        cb(ctx->context.get(), userData);
    }
}

}  // namespace dusk::mods

#include "services.hpp"

namespace luau_runtime::services {

namespace {

using namespace std::string_view_literals;

constexpr std::pair<std::string_view, lua_CFunction> kModules[] = {
    { "dusklight.log"sv, open_log },
    { "dusklight.host"sv, open_host },
    { "dusklight.resource"sv, open_resource },
    { "dusklight.overlay"sv, open_overlay },
    { "dusklight.texture"sv, open_texture },
    { "dusklight.config"sv, open_config },
    { "dusklight.ui"sv, open_ui },
    { "dusklight.audio_res"sv, open_audio_res },
};

}

ModuleOpenFn module_factory(std::string_view name) {
    for (auto [ modName, ptr ] : kModules) {
        if (name == modName) {
            return ptr;
        }
    }

    return nullptr;
}

}

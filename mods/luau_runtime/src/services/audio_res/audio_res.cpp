#include "../../lua_bridge_helpers.hpp"
#include "../../runtime.hpp"
#include "../services.hpp"
#include "LuaBridge/LuaBridge.h"

#include "mods/svc/audio_res.h"

#include "bst.hpp"
#include "wsys.hpp"

namespace luau_runtime::services {
namespace {

constexpr auto kServiceName = "AudioResService";

}

int open_audio_res(lua_State* state) {
    using namespace audio_res;

    if (svc_audio_res == nullptr) {
        service_unavailable(state, kServiceName);
    }

    lua_newtable(state);

    luabridge::getNamespaceFromStack(state)
        // WSYS
        .addVariable("DEFAULT_KEY", AUDIO_RES_DEFAULT_KEY)
        .addFunction("default_wave_info", default_wave_info)
        .addFunction("replace_wave", replace_wave)
        .addFunction("add_wave", add_wave)
        .beginClass<LuaAudioWaveHandle>("AudioWaveHandle")
        .addFunction("unregister", &LuaAudioWaveHandle::unregister)
        .endClass()
        // BST
        .addFunction("default_effect_info", default_effect_info)
        .addFunction("replace_sound_table_effect", replace_sound_table_effect)
        .addFunction("add_sound_table_effect", add_sound_table_effect)
        .addVariable("STREAM_MAX_CHILDREN", STREAM_MAX_CHILDREN)
        .addFunction("default_stream_info", default_stream_info)
        .addFunction("replace_sound_table_stream", replace_sound_table_stream)
        .addFunction("add_sound_table_stream", add_sound_table_stream)
        .beginClass<LuaSoundTableHandle>("AudioSoundTableHandle")
        .addFunction("unregister", &LuaSoundTableHandle::unregister)
        .endClass();

    lua_setreadonly(state, -1, true);
    return 1;
}

}  // namespace luau_runtime::services

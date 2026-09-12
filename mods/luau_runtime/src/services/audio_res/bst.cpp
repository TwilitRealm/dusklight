#include "bst.hpp"

namespace luau_runtime::services::audio_res {

LuaSoundTableHandle::LuaSoundTableHandle(AudioSoundTableHandle handle) : BridgeScriptHandle(handle) { }

void LuaSoundTableHandle::unregister_impl(lua_State*, Vm& vm) {
    svc_audio_res->remove_sound_table(vm.subject, handle);
}

AudioSoundTableEffectInfo const& default_effect_info() {
    return *svc_audio_res->default_effect_info;
}

LuaSoundTableHandle replace_sound_table_effect(SoundEffectCategory category_id, uint16_t effect_id,
    std::optional<AudioSoundTableEffectInfo> info, lua_State* state, Vm& vm) {
    AudioSoundTableEffectInfo const* effect_info = nullptr;
    if (info.has_value()) {
        effect_info = &*info;
    }

    AudioSoundTableHandle handle;

    check_result(
        state,
        svc_audio_res->replace_sound_table_effect(vm.subject, category_id, effect_id, effect_info, &handle),
        "audio_res.replace_sound_table_effect");

    return LuaSoundTableHandle(handle);
}

std::tuple<uint16_t, LuaSoundTableHandle> add_sound_table_effect(
    SoundEffectCategory category_id,
    std::optional<AudioSoundTableEffectInfo> info,
    lua_State* state,
    Vm& vm) {
    AudioSoundTableEffectInfo const* effect_info = nullptr;
    if (info.has_value()) {
        effect_info = &*info;
    }

    uint16_t out_effect_id;
    AudioSoundTableHandle handle;

    check_result(
        state,
        svc_audio_res->add_sound_table_effect(vm.subject, category_id, effect_info, &handle, &out_effect_id),
        "audio_res.add_sound_table_effect");

    return { out_effect_id, LuaSoundTableHandle(handle) };
}

AudioSoundTableStreamInfo const& default_stream_info() {
    return *svc_audio_res->default_stream_info;
}

LuaSoundTableHandle replace_sound_table_stream(uint16_t stream_id, char const* file_path,
    std::optional<AudioSoundTableStreamInfo> info, lua_State* state, Vm& vm) {
    AudioSoundTableStreamInfo const* stream_info = nullptr;
    if (info.has_value()) {
        stream_info = &*info;
    }

    AudioSoundTableHandle handle;

    check_result(
        state,
        svc_audio_res->replace_sound_table_stream(vm.subject, stream_id, file_path, stream_info, &handle),
        "audio_res.replace_sound_table_stream");

    return LuaSoundTableHandle(handle);
}

std::tuple<uint16_t, LuaSoundTableHandle> add_sound_table_stream(
    char const* file_path,
    std::optional<AudioSoundTableStreamInfo> info,
    lua_State* state,
    Vm& vm) {
    AudioSoundTableStreamInfo const* stream_info = nullptr;
    if (info.has_value()) {
        stream_info = &*info;
    }

    uint16_t out_stream_id;
    AudioSoundTableHandle handle;

    check_result(
        state,
        svc_audio_res->add_sound_table_stream(vm.subject, file_path, stream_info, &handle, &out_stream_id),
        "audio_res.add_sound_table_stream");

    return { out_stream_id, LuaSoundTableHandle(handle) };
}


}  // namespace luau_runtime::services::audio_res

namespace luabridge {
namespace {

using namespace std::string_view_literals;

constexpr luau_runtime::EnumName<SoundEffectCategory> kSoundEffectCategoryNames[] = {
    // clang-format off
    { SE_CATEGORY_SYSTEM_SE,    "system_se"sv },
    { SE_CATEGORY_PLAYER_VOICE, "player_voice"sv },
    { SE_CATEGORY_PLAYER_SE,    "player_se"sv },
    { SE_CATEGORY_FOOTNOTE_SE,  "footnote_se"sv },
    { SE_CATEGORY_COLLISION_SE, "collision_se"sv },
    { SE_CATEGORY_CHARA_VOICE,  "chara_voice"sv },
    { SE_CATEGORY_CHARA_SE,     "chara_se"sv },
    { SE_CATEGORY_ENEMY_SE,     "enemy_se"sv },
    { SE_CATEGORY_OBJECT_SE,    "object_se"sv },
    { SE_CATEGORY_ENV_SE,       "env_se"sv },
    // clang-format on
};

constexpr luau_runtime::EnumName<StreamPan> kStreamPanNames[] = {
    // clang-format off
    { STREAM_PAN_CENTER, "center"sv },
    { STREAM_PAN_LEFT,   "left"sv },
    { STREAM_PAN_RIGHT,  "right"sv },
    // clang-format on
};

}  // namespace

DEFINE_STRING_ENUM(SoundEffectCategory, kSoundEffectCategoryNames);
DEFINE_STRING_ENUM(StreamPan, kStreamPanNames);

TypeResult<AudioSoundTableEffectInfo> Stack<AudioSoundTableEffectInfo>::get(
    lua_State* L, int index) {

    auto const ref = LuaRef::fromStack(L, index);

    return AudioSoundTableEffectInfo {
        .priority = ref["priority"],
        .volume = ref["volume"],
        .pitch = ref["pitch"],
        .always_max_priority = ref["always_max_priority"],
        .ignore_distance_volume = ref["ignore_distance_volume"],
        .ignore_distance_fx_mix = ref["ignore_distance_fx_mix"],
        .ignore_pan = ref["ignore_pan"],
        .ignore_dolby = ref["ignore_dolby"],
        .random_volume = ref["random_volume"],
        .random_pitch = ref["random_pitch"],
        .doppler_power = ref["doppler_power"],
        .volume_dist_class = ref["volume_dist_class"],
        .clamp_min_volume = ref["clamp_min_volume"],
        .cull_at_max_distance = ref["cull_at_max_distance"],
    };
}

Result Stack<AudioSoundTableEffectInfo>::push(
    lua_State* L, const AudioSoundTableEffectInfo& value) {

    LuaRef const ref = newTable(L);

    ref["priority"] = value.priority;
    ref["volume"] = value.volume;
    ref["pitch"] = value.pitch;
    ref["always_max_priority"] = value.always_max_priority;
    ref["ignore_distance_volume"] = value.ignore_distance_volume;
    ref["ignore_distance_fx_mix"] = value.ignore_distance_fx_mix;
    ref["ignore_pan"] = value.ignore_pan;
    ref["ignore_dolby"] = value.ignore_dolby;
    ref["random_volume"] = value.random_volume;
    ref["random_pitch"] = value.random_pitch;
    ref["doppler_power"] = value.doppler_power;
    ref["volume_dist_class"] = value.volume_dist_class;
    ref["clamp_min_volume"] = value.clamp_min_volume;
    ref["cull_at_max_distance"] = value.cull_at_max_distance;

    ref.push();

    return {};
}

TypeResult<AudioSoundTableStreamInfo> Stack<AudioSoundTableStreamInfo>::get(
    lua_State* L, int index) {

    auto const ref = LuaRef::fromStack(L, index);

    std::array<std::optional<StreamPan>, STREAM_MAX_CHILDREN> const pan_parameters = ref["pan_parameters"];

    auto info = AudioSoundTableStreamInfo {
        .priority = ref["priority"],
        .volume = ref["volume"],
        .stop_on_scene_change = ref["stop_on_scene_change"],
    };

    for (int i = 0; i < STREAM_MAX_CHILDREN; i++) {
        info.pan_parameters[i] = pan_parameters[i].value_or(STREAM_PAN_CENTER);
    }

    return info;
}

Result Stack<AudioSoundTableStreamInfo>::push(
    lua_State* L, const AudioSoundTableStreamInfo& value) {

    LuaRef const ref = newTable(L);

    ref["priority"] = value.priority;
    ref["volume"] = value.volume;
    ref["pan_parameters"] = value.pan_parameters;
    ref["stop_on_scene_change"] = value.stop_on_scene_change;

    ref.push();

    return {};
}

}  // namespace luabridge

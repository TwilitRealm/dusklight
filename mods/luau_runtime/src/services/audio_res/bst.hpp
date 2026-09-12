#pragma once

#include "../../lua_bridge_helpers.hpp"
#include "../../lua_helpers.hpp"
#include "mods/svc/audio_res.h"

namespace luau_runtime::services::audio_res {

class LuaSoundTableHandle final : public BridgeScriptHandle {
protected:
    void unregister_impl(lua_State*, Vm& vm) override;

public:
    explicit LuaSoundTableHandle(AudioSoundTableHandle handle);
};

AudioSoundTableEffectInfo const& default_effect_info();

LuaSoundTableHandle replace_sound_table_effect(SoundEffectCategory category_id, uint16_t effect_id,
    std::optional<AudioSoundTableEffectInfo> info, lua_State* state, Vm& vm);

std::tuple<uint16_t, LuaSoundTableHandle> add_sound_table_effect(SoundEffectCategory category_id,
    std::optional<AudioSoundTableEffectInfo> info, lua_State* state, Vm& vm);

AudioSoundTableStreamInfo const& default_stream_info();

LuaSoundTableHandle replace_sound_table_stream(uint16_t stream_id, char const* file_path,
    std::optional<AudioSoundTableStreamInfo> info, lua_State* state, Vm& vm);

std::tuple<uint16_t, LuaSoundTableHandle> add_sound_table_stream(
    char const* file_path, std::optional<AudioSoundTableStreamInfo> info, lua_State* state, Vm& vm);

}  // namespace luau_runtime::services::audio_res

namespace luabridge {

template <>
struct Stack<AudioSoundTableEffectInfo> {
    [[nodiscard]] static Result push(lua_State* L, const AudioSoundTableEffectInfo& value);
    [[nodiscard]] static TypeResult<AudioSoundTableEffectInfo> get(lua_State* L, int index);
};

template <>
struct Stack<AudioSoundTableStreamInfo> {
    [[nodiscard]] static Result push(lua_State* L, const AudioSoundTableStreamInfo& value);
    [[nodiscard]] static TypeResult<AudioSoundTableStreamInfo> get(lua_State* L, int index);
};

DECLARE_STRING_ENUM(SoundEffectCategory);
DECLARE_STRING_ENUM(StreamPan);

}  // namespace luabridge

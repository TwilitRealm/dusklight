#pragma once

#include "LuaBridge/LuaBridge.h"
#include "lua.h"
#include "lualib.h"

#include "../../lua_helpers.hpp"
#include "../../runtime.hpp"
#include "mods/svc/audio_res.h"

#include <memory>

namespace luau_runtime::services::audio_res {

struct OwnedWaveInfo {
    AudioWaveInfo info;
    std::unique_ptr<AudioRawWave> raw_wave;
};

class LuaAudioWaveHandle final : public BridgeScriptHandle {
protected:
    void unregister_impl(lua_State*, Vm& vm) override;

public:
    explicit LuaAudioWaveHandle(AudioWaveHandle handle);
};

AudioWaveInfo const& default_wave_info();
LuaAudioWaveHandle replace_wave(AudioWaveBank bank, uint16_t wave_id, char const* file,
    std::optional<OwnedWaveInfo> info, lua_State* state, Vm& vm);
std::tuple<uint16_t, LuaAudioWaveHandle> add_wave(AudioWaveBank bank, char const* file,
    std::optional<OwnedWaveInfo> info, lua_State* state, Vm& vm);

}  // namespace luau_runtime::services::audio_res

namespace luabridge {

template <>
struct Stack<AudioWaveInfo> {
    [[nodiscard]] static Result push(lua_State* L, const AudioWaveInfo& value);
};

template <>
struct Stack<AudioRawWave> {
    [[nodiscard]] static Result push(lua_State* L, const AudioRawWave& value);
    [[nodiscard]] static TypeResult<AudioRawWave> get(lua_State* L, int index);
};

template <>
struct Stack<luau_runtime::services::audio_res::OwnedWaveInfo> {
    [[nodiscard]] static Result push(
        lua_State* L, const luau_runtime::services::audio_res::OwnedWaveInfo& value);
    [[nodiscard]] static TypeResult<luau_runtime::services::audio_res::OwnedWaveInfo> get(
        lua_State* L, int index);
};

DECLARE_STRING_ENUM(AudioWaveFormat);
DECLARE_STRING_ENUM(AudioWaveBank);

}  // namespace luabridge

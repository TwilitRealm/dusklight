#include "wsys.hpp"

namespace luau_runtime::services::audio_res {

LuaAudioWaveHandle::LuaAudioWaveHandle(AudioWaveHandle handle) : BridgeScriptHandle(handle) { }

void LuaAudioWaveHandle::unregister_impl(lua_State*, Vm& vm) {
    svc_audio_res->remove_wave(vm.subject, handle);
}

AudioWaveInfo const& default_wave_info() {
    return *svc_audio_res->default_wave_info;
}

LuaAudioWaveHandle replace_wave(AudioWaveBank bank, uint16_t wave_id, char const* file, std::optional<OwnedWaveInfo> info, lua_State* state, Vm& vm) {
    AudioWaveInfo const* wave_info = nullptr;
    if (info.has_value()) {
        wave_info = &info->info;
    }

    AudioWaveHandle handle;

    check_result(
        state,
        svc_audio_res->replace_wave(vm.subject, bank, wave_id, file, wave_info, &handle),
        "audio_res.replace_wave");

    return LuaAudioWaveHandle(handle);
}

std::tuple<uint16_t, LuaAudioWaveHandle> add_wave(AudioWaveBank bank, char const* file, std::optional<OwnedWaveInfo> info, lua_State* state, Vm& vm) {
    AudioWaveInfo const* wave_info = nullptr;
    if (info.has_value()) {
        wave_info = &info->info;
    }

    AudioWaveHandle handle;
    uint16_t out_wave_id;

    check_result(
        state,
        svc_audio_res->add_wave(vm.subject, bank, file, wave_info, &handle, &out_wave_id),
        "audio_res.replace_wave");

    return {out_wave_id, LuaAudioWaveHandle(handle)};
}

}  // namespace luau_runtime::services::audio_res

namespace luabridge {

Result Stack<AudioWaveInfo>::push(lua_State* L, const AudioWaveInfo& value) {
    LuaRef const ref = newTable(L);

    ref["base_key"] = value.base_key;
    ref["loop"] = value.loop;
    ref["loop_start_sample"] = value.loop_start_sample;
    ref["loop_end_sample"] = value.loop_end_sample;
    if (value.raw_wave) {
        ref["raw_wave"] = *value.raw_wave;
    }
    ref.push();

    return {};
}

Result Stack<AudioRawWave>::push(lua_State* L, const AudioRawWave& value) {
    LuaRef const ref = newTable(L);

    ref["format"] = value.format;
    ref["sample_rate"] = value.sample_rate;
    ref["sample_value_last"] = value.sample_value_last;
    ref["sample_value_penult"] = value.sample_value_penult;

    ref.push();

    return {};
}

TypeResult<AudioRawWave> Stack<AudioRawWave>::get(lua_State* L, int index) {
    auto const ref = LuaRef::fromStack(L, index);

    return AudioRawWave {
        .format = ref["format"],
        .sample_rate = ref["sample_rate"],
        .sample_value_last = opt_or<int16_t>(ref["sample_value_last"], 0),
        .sample_value_penult = opt_or<int16_t>(ref["sample_value_penult"], 0),
    };
}

using namespace luau_runtime::services::audio_res;

Result Stack<OwnedWaveInfo>::push(lua_State* L, const OwnedWaveInfo& value) {
    return Stack<AudioWaveInfo>::push(L, value.info);
}

TypeResult<OwnedWaveInfo> Stack<OwnedWaveInfo>::get(lua_State* L, int index) {
    auto const ref = LuaRef::fromStack(L, index);

    AudioWaveInfo info {
        .base_key = opt_or<uint8_t>(ref["base_key"], AUDIO_RES_DEFAULT_KEY),
        .loop = opt_or<bool>(ref["loop"], false),
        .loop_start_sample = opt_or<uint32_t>(ref["loop_start_sample"], 0),
        .loop_end_sample = opt_or<uint32_t>(ref["loop_end_sample"], std::numeric_limits<uint32_t>::max()),
    };

    std::unique_ptr<AudioRawWave> raw;
    if (!ref["raw_wave"].isNil()) {
        raw = std::make_unique<AudioRawWave>(ref["raw_wave"]);
        info.raw_wave = raw.get();
    }

    return OwnedWaveInfo { info, std::move(raw) };
}

namespace {

using namespace std::string_view_literals;

constexpr luau_runtime::lua_helpers::EnumName<AudioWaveFormat> kWaveFormatNames[] = {
    { AUDIO_WAVE_FORMAT_ADPCM4, "adpcm4"sv },
    { AUDIO_WAVE_FORMAT_ADPCM2, "adpcm2"sv },
    { AUDIO_WAVE_FORMAT_PCM8, "pcm8"sv },
    { AUDIO_WAVE_FORMAT_PCM16, "pcm16"sv },
};

constexpr luau_runtime::lua_helpers::EnumName<AudioWaveBank> kWaveBankNames[] = {
    { AUDIO_WAVE_BANK_SOUND_EFFECTS, "sound_effects"sv } ,
    { AUDIO_WAVE_BANK_MUSIC_SAMPLES, "music_samples"sv } ,
};

}

DEFINE_STRING_ENUM(AudioWaveFormat, kWaveFormatNames);
DEFINE_STRING_ENUM(AudioWaveBank, kWaveBankNames);


}  // namespace luabridge

#pragma once

#include <cmath>
#include <dolphin/types.h>

namespace dusk::audio {

    // Converts a 0-1 volume to a linear amplitude multiplier.
    // The curve is -4 dB per 10% step: 100% = 0 dB, 90% = -4 dB, ..., 0% = -inf dB
    inline f32 MasterVolumeToLinear(f32 v) {
        if (v <= 0.0f) {
            return 0.0f;
        }
        return std::pow(10.0f, (v - 1.0f) * 2.0f);
    }

    /**
     * Converts a 0-1 volume to a linear amplitude multiplier.
     * This curve is gentler than the master curve: 100% = 0 dB, 50% = -6 dB, 0% = -inf dB.
     */
    inline f32 VolumeToLinear(f32 v) {
        if (v <= 0.0f) {
            return 0.0f;
        }
        return std::pow(10.0f, (v - 1.0f) * 2.0f * 0.5f);
    }

    /**
     * Volume slider model (PC Audio settings):
     *   Master -> global DSP volume (multiplies everything).
     *   Music  -> MusicVolume, applied in JAISeqMgr/JAIStreamMgr::mixOut.
     *   SE     -> JAISeMgr per-category scale via SeCategoryScale():
     *             0 = menu/UI, 1+5+7 = voices, 2+3+6 = footsteps/motion,
     *             9 = ambience, and 4+8 (default) = sound effects.
     * Each scale is a 0-1 multiplier of the game's own volume; applied every frame.
     */
    extern f32 MusicVolume;

    /** User sound-effect volume scale (0-1). */
    extern f32 SfxVolume;
    /** User voice volume scale (0-1). */
    extern f32 VoiceVolume;
    /** User menu/UI sound volume scale (0-1). */
    extern f32 MenuVolume;
    /** User ambient/environment volume scale (0-1). */
    extern f32 AmbienceVolume;
    /** User footsteps/motion volume scale (0-1). */
    extern f32 FootstepVolume;

    /**
     * Returns the player-selected volume scale for a given JAISeMgr SE category index.
     * Used by JAISeMgr::mixOut on PC to apply the SE volume sliders.
     */
    f32 SeCategoryScale(int categoryIndex);

    /**
     * Initialize the audio system and start playing audio.
     */
    void Initialize();

    void Reinitialize();

    void Shutdown();

    void SetEnableReverb(bool value);

    void SetMasterVolume(f32 value);

    void SetMusicVolume(f32 value);

    void SetSfxVolume(f32 value);

    void SetVoiceVolume(f32 value);

    void SetMenuVolume(f32 value);

    void SetAmbienceVolume(f32 value);

    void SetFootstepVolume(f32 value);

    void SetPaused(bool paused);

    u32 GetResetCount(int channelIdx);

    f32 VolumeFromU16(u16 value);
}

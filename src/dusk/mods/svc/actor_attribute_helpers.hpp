#pragma once

#include "actor_attribute.hpp"

#include "dolphin/types.h"
#include "SSystem/SComponent/c_lib.h"
#include "angle_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

/*
 * Reusable math and conversion helpers for semantic actor-attribute integration.
 *
 * These helpers deliberately do not decide which actions, timers, movement
 * paths, attachment constraints, health writes, or child actors should be
 * scaled. Those semantic decisions remain in the owning actor source.
 */
namespace dusk::mods::svc::actor_attr {

inline f32 safe_action_speed(const f32 actionSpeed) {
    return std::isfinite(actionSpeed) && actionSpeed > 0.0f
               ? actionSpeed
               : 1.0f;
}

/* Scale a signed movement, acceleration, or interpolation step in action time. */
inline f32 action_step(const f32 vanillaStep, const f32 requestedActionSpeed) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return vanillaStep;
    }

    return vanillaStep * actionSpeed;
}

/* Resolve a vanilla multiplier of 1.0 and clamp the composed result. */
inline f32 resolve_multiplier(
    const fopAc_ac_c* actor,
    const ActorAttribute attribute,
    const f32 minimum = 0.1f,
    const f32 maximum = 100.0f
) {
    const f32 lower = std::min(minimum, maximum);
    const f32 upper = std::max(minimum, maximum);
    const f32 resolved = resolve_actor_attribute(actor, attribute, 1.0f);
    return std::clamp(std::isfinite(resolved) ? resolved : 1.0f, lower, upper);
}

/* Resolve a fresh value whose meaningful range begins at zero. */
inline f32 resolve_nonnegative(
    const fopAc_ac_c* actor,
    const ActorAttribute attribute,
    const f32 vanillaValue
) {
    const f32 resolved = resolve_actor_attribute(actor, attribute, vanillaValue);
    return std::max(std::isfinite(resolved) ? resolved : vanillaValue, 0.0f);
}

/*
 * Convert a vanilla duration to real frames. At exactly 1x this preserves the
 * original float-to-s16 truncation. Other speeds round up so a state never
 * ends before its scaled action time has elapsed.
 */
inline s16 sync_timer(const f32 vanillaFrames, const f32 requestedActionSpeed) {
    if (!(vanillaFrames > 0.0f)) {
        return 0;
    }

    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return static_cast<s16>(std::min(
            vanillaFrames, static_cast<f32>(std::numeric_limits<s16>::max())));
    }

    const double frames = std::ceil(static_cast<double>(vanillaFrames) / actionSpeed);
    return static_cast<s16>(std::clamp(
        frames, 1.0, static_cast<double>(std::numeric_limits<s16>::max())));
}

/* Scale a duration directly. Unlike sync_timer(), a larger multiplier keeps
 * the state active longer instead of making an action clock run faster. */
inline s16 duration_timer(
    const f32 vanillaFrames,
    const f32 requestedDurationMultiplier
) {
    if (!(vanillaFrames > 0.0f)) {
        return 0;
    }

    const f32 multiplier = safe_action_speed(requestedDurationMultiplier);
    if (multiplier == 1.0f) {
        return static_cast<s16>(std::min(
            vanillaFrames, static_cast<f32>(std::numeric_limits<s16>::max())));
    }

    const double frames = std::ceil(
        static_cast<double>(vanillaFrames) * multiplier);
    return static_cast<s16>(std::clamp(
        frames, 1.0, static_cast<double>(std::numeric_limits<s16>::max())));
}

/* Synchronize a timer stored in a byte without allowing overflow to wrap. */
inline u8 sync_u8_timer(
    const f32 vanillaFrames,
    const f32 requestedActionSpeed
) {
    return static_cast<u8>(std::clamp<s16>(
        sync_timer(vanillaFrames, requestedActionSpeed), 0, 255));
}

/* Synchronize a timer stored in a signed byte without allowing overflow. */
inline s8 sync_s8_timer(
    const f32 vanillaFrames,
    const f32 requestedActionSpeed
) {
    return static_cast<s8>(std::clamp<s16>(
        sync_timer(vanillaFrames, requestedActionSpeed), 0, 127));
}

inline u8 duration_u8_timer(
    const f32 vanillaFrames,
    const f32 requestedDurationMultiplier
) {
    return static_cast<u8>(std::clamp<s16>(
        duration_timer(vanillaFrames, requestedDurationMultiplier), 0, 255));
}

inline s8 duration_s8_timer(
    const f32 vanillaFrames,
    const f32 requestedDurationMultiplier
) {
    return static_cast<s8>(std::clamp<s16>(
        duration_timer(vanillaFrames, requestedDurationMultiplier), 0, 127));
}

/* Round a meaningful positive value into an s16 field. Do not use for zero
 * death writes or special sentinel values. */
inline s16 positive_s16(const f32 value) {
    const f32 clamped = std::clamp(
        std::isnan(value) ? 1.0f : value, 1.0f,
        static_cast<f32>(std::numeric_limits<s16>::max()));
    return static_cast<s16>(std::lround(clamped));
}

inline s16 resolve_positive_s16(
    const fopAc_ac_c* actor,
    const ActorAttribute attribute,
    const f32 vanillaValue
) {
    return positive_s16(resolve_actor_attribute(actor, attribute, vanillaValue));
}

/* Round and clamp a value consumed by a byte-sized engine field. */
inline int byte_value(const f32 value) {
    return static_cast<int>(std::lround(std::clamp(
        std::isnan(value) ? 0.0f : value, 0.0f, 255.0f)));
}

inline int resolve_byte(
    const fopAc_ac_c* actor,
    const ActorAttribute attribute,
    const f32 vanillaValue
) {
    return byte_value(resolve_actor_attribute(actor, attribute, vanillaValue));
}

/*
 * Advance a vanilla exponential interpolation by S frames at a time. The 1x
 * fast path returns the exact original argument.
 */
inline f32 action_fraction(
    const f32 vanillaFraction,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return vanillaFraction;
    }

    const f32 fraction = std::clamp(vanillaFraction, 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - fraction, actionSpeed);
}

struct ActionCalcParams {
    f32 fraction;
    f32 maximumStep;
};

/* Parameters for a cLib_addCalc2/cLib_addCalc0-style action interpolation. */
inline ActionCalcParams action_calc_params(
    const f32 vanillaFraction,
    const f32 vanillaMaximumStep,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return ActionCalcParams{vanillaFraction, vanillaMaximumStep};
    }

    return ActionCalcParams{
        action_fraction(vanillaFraction, actionSpeed),
        action_step(vanillaMaximumStep, actionSpeed),
    };
}

struct ActionAnimationParams {
    f32 morph;
    f32 playbackSpeed;
};

/* Keep animation playback and positive morph durations on the action clock. */
inline ActionAnimationParams action_animation_params(
    const f32 vanillaMorph,
    const f32 vanillaPlaybackSpeed,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return ActionAnimationParams{vanillaMorph, vanillaPlaybackSpeed};
    }

    return ActionAnimationParams{
        vanillaMorph > 0.0f ? vanillaMorph / actionSpeed : vanillaMorph,
        vanillaPlaybackSpeed * actionSpeed,
    };
}

/* Scale a multiplicative per-frame damping/retention factor in action time. */
inline f32 action_damping(
    const f32 vanillaFactor,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return vanillaFactor;
    }

    return std::pow(std::clamp(vanillaFactor, 0.0f, 1.0f), actionSpeed);
}

struct ActionAngleParams {
    s16 divisor;
    s16 maximumStep;
};

/*
 * cLib_addCalcAngleS2-style turns have two rate controls. Scaling both keeps
 * the turn consistent both far from and close to its target.
 */
inline ActionAngleParams action_angle_params(
    const s16 vanillaDivisor,
    const s16 vanillaMaximumStep,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return ActionAngleParams{vanillaDivisor, vanillaMaximumStep};
    }

    const f32 s16Maximum = static_cast<f32>(std::numeric_limits<s16>::max());
    const long divisor = std::lround(std::clamp(
        static_cast<f32>(vanillaDivisor) / actionSpeed, 1.0f, s16Maximum));
    const long maximumStep = vanillaMaximumStep == 0 ? 0 : std::lround(std::clamp(
        static_cast<f32>(vanillaMaximumStep) * actionSpeed, 1.0f, s16Maximum));

    return ActionAngleParams{
        static_cast<s16>(divisor),
        static_cast<s16>(maximumStep),
    };
}

/* Scale and safely convert a positive s16 action step. */
inline s16 action_s16_step(
    const s16 vanillaStep,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f || vanillaStep == 0) {
        return vanillaStep;
    }

    const long scaledStep = std::lround(
        static_cast<f32>(vanillaStep) * actionSpeed
    );

    return static_cast<s16>(std::clamp(
        scaledStep,
        1L,
        static_cast<long>(std::numeric_limits<s16>::max())
    ));
}

/* Scale a signed s16 step while preserving its direction. */
inline s16 signed_action_s16_step(
    const s16 vanillaStep,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f || vanillaStep == 0) {
        return vanillaStep;
    }
    if (vanillaStep < 0) {
        // Widen before negating: -INT16_MIN is not representable in s16.
        const long magnitude = std::lround(-static_cast<f32>(vanillaStep) * actionSpeed);
        return static_cast<s16>(-std::clamp(magnitude, 1L, 32768L));
    }

    return action_s16_step(vanillaStep, actionSpeed);
}

/* Scale a byte-sized chase step without allowing a positive step to vanish. */
inline u8 action_u8_step(
    const u8 vanillaStep,
    const f32 requestedActionSpeed
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f || vanillaStep == 0) {
        return vanillaStep;
    }

    const long scaledStep = std::lround(
        static_cast<f32>(vanillaStep) * actionSpeed);
    return static_cast<u8>(std::clamp(scaledStep, 1L, 255L));
}

/* Scale an always-running counter used as a procedural angle/phase source. */
inline s16 action_phase_angle(
    const s32 counter,
    const s32 vanillaRate,
    const f32 requestedActionSpeed,
    const s32 phaseOffset = 0
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return static_cast<s16>(static_cast<std::int64_t>(counter) * vanillaRate + phaseOffset);
    }

    const double angle = static_cast<double>(counter) * actionSpeed * vanillaRate +
                         phaseOffset;
    return static_cast<s16>(std::lround(std::fmod(angle, 65536.0)));
}

inline s16 action_phase_angle(
    const s32 counter,
    const f32 vanillaRate,
    const f32 requestedActionSpeed,
    const f32 phaseOffset = 0.0f
) {
    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        // Preserve vanilla float arithmetic and truncation without converting
        // an out-of-range float directly to the signed 16-bit angle type.
        const f32 angle = counter * vanillaRate + phaseOffset;
        return static_cast<s16>(static_cast<s32>(std::fmod(angle, 65536.0f)));
    }

    const double angle = static_cast<double>(counter) * actionSpeed * vanillaRate +
                         phaseOffset;
    return static_cast<s16>(std::lround(std::fmod(angle, 65536.0)));
}

/*
 * Direct-inheritance enemies use this default accessor. Enemies containing
 * their actor as a member specialize this trait in their actor source.
 */
template <typename Enemy>
struct EnemyActorAccessor {
    static fopAc_ac_c* get(Enemy* i_this) {
        return static_cast<fopAc_ac_c*>(i_this);
    }
};

/*
 * A boss part may inherit attributes from its owner, but it is still a separate
 * physical actor. Keep ownership out of EnemyActorAccessor so movement never
 * updates the parent in place of the child.
 */
template <typename Enemy>
struct EnemyAttributeOwner {
    static fopAc_ac_c* get(Enemy* i_this) {
        return EnemyActorAccessor<Enemy>::get(i_this);
    }
};

/*
 * Action time is enabled by default. Actors with exact-time attachment,
 * cutscene, or controller states specialize this trait in their actor source.
 */
template <typename Enemy>
struct EnemyActionTimePolicy {
    static bool uses(Enemy*) {
        return true;
    }
};

template <typename Enemy>
inline fopAc_ac_c* enemy_attribute_actor(Enemy* i_this) {
    return EnemyAttributeOwner<Enemy>::get(i_this);
}

template <typename Enemy>
inline f32 enemy_action_speed_multiplier(Enemy* i_this) {
    return resolve_multiplier(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_MOVEMENT_SPEED);
}

template <typename Enemy>
inline f32 enemy_size_multiplier(Enemy* i_this) {
    return resolve_multiplier(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_SIZE);
}

/* A fresh geometric length/radius, not an already-scaled world position. */
template <typename Enemy>
inline f32 enemy_size_value(Enemy* i_this, const f32 vanillaLength) {
    return vanillaLength * enemy_size_multiplier(i_this);
}

template <typename Enemy>
inline f32 enemy_notice_range_multiplier(Enemy* i_this) {
    return resolve_multiplier(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_NOTICE_RANGE);
}

/*
 * Scale only a distance that controls whether an enemy notices, acquires, or
 * forgets the player. Attack reach, grab bounds, scripted triggers, and
 * movement/formation distances remain size- or world-space calculations.
 */
template <typename Enemy>
inline f32 enemy_notice_distance(Enemy* i_this, const f32 vanillaDistance) {
    return vanillaDistance * enemy_notice_range_multiplier(i_this);
}

/*
 * Resolve the horizontal distance multiplier applied when this source actor
 * damages Link. Projectile/part ownership is resolved by the registered mod
 * resolver because Link's collision record exposes only a raw actor pointer.
 */
inline f32 enemy_player_knockback_multiplier(const fopAc_ac_c* sourceActor) {
    return resolve_multiplier(
        sourceActor,
        ACTOR_ATTRIBUTE_PLAYER_KNOCKBACK);
}

inline f32 enemy_player_knockback_value(
    const fopAc_ac_c* sourceActor,
    const f32 vanillaValue
) {
    const f32 multiplier = enemy_player_knockback_multiplier(sourceActor);
    return multiplier == 1.0f ? vanillaValue : vanillaValue * multiplier;
}

template <typename Enemy>
inline f32 enemy_stun_duration_multiplier(Enemy* i_this) {
    return resolve_multiplier(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_STUN_DURATION);
}

template <typename Enemy>
inline f32 enemy_stun_duration_value(
    Enemy* i_this,
    const f32 vanillaDuration
) {
    return resolve_nonnegative(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_STUN_DURATION,
        vanillaDuration);
}

/*
 * A stun whose exit is gated by animation progress needs the inverse of an
 * action-speed scale: a 2x duration plays at half speed, while positive morph
 * time is doubled. The 1x path remains byte-for-byte equivalent in value.
 */
template <typename Enemy>
inline ActionAnimationParams enemy_stun_animation_params(
    Enemy* i_this,
    const f32 vanillaMorph,
    const f32 vanillaPlaybackSpeed
) {
    const f32 durationMultiplier = enemy_stun_duration_multiplier(i_this);
    if (durationMultiplier == 1.0f) {
        return ActionAnimationParams{vanillaMorph, vanillaPlaybackSpeed};
    }

    return ActionAnimationParams{
        vanillaMorph > 0.0f
            ? vanillaMorph * durationMultiplier
            : vanillaMorph,
        vanillaPlaybackSpeed / durationMultiplier,
    };
}

template <typename Enemy>
inline cXyz enemy_size_multiplier(Enemy* i_this, const f32 vanillaScale) {
    const f32 scale = vanillaScale * enemy_size_multiplier(i_this);
    return cXyz(scale, scale, scale);
}

template <typename Enemy>
inline cXyz enemy_size_multiplier(Enemy* i_this, cXyz vanillaScale) {
    vanillaScale *= enemy_size_multiplier(i_this);
    return vanillaScale;
}

template <typename Enemy>
inline bool enemy_uses_action_time_scale(Enemy* i_this) {
    return EnemyActionTimePolicy<Enemy>::uses(i_this);
}

template <typename Enemy>
inline f32 enemy_action_time_speed(Enemy* i_this) {
    return enemy_uses_action_time_scale(i_this)
               ? enemy_action_speed_multiplier(i_this)
               : 1.0f;
}

inline int action_event_count(
    const s32 frameCounter,
    const s32 vanillaPeriod,
    const f32 requestedActionSpeed,
    const s32 vanillaPhaseOffset = 0
) {
    if (vanillaPeriod <= 0) {
        return 0;
    }

    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return (static_cast<std::int64_t>(frameCounter) + vanillaPhaseOffset) %
                           vanillaPeriod == 0 ? 1 : 0;
    }

    const double currentActionFrame =
        static_cast<double>(frameCounter) * actionSpeed + vanillaPhaseOffset;
    const double previousActionFrame = currentActionFrame - actionSpeed;
    const double currentBoundary = std::floor(currentActionFrame / vanillaPeriod);
    const double previousBoundary = std::floor(previousActionFrame / vanillaPeriod);

    // Windows long is only 32 bits. Convert the small per-frame difference,
    // not the absolute action-frame boundary of a long-running counter.
    return static_cast<int>(std::clamp(currentBoundary - previousBoundary,
        0.0, static_cast<double>(std::numeric_limits<int>::max())));
}

template <typename Enemy>
inline int enemy_action_event_count(Enemy* i_this, const s32 frameCounter,
                                    const s32 vanillaPeriod,
                                    const s32 vanillaPhaseOffset = 0) {
    return action_event_count(
        frameCounter,
        vanillaPeriod,
        enemy_action_time_speed(i_this),
        vanillaPhaseOffset);
}

/* Periodic checks for signed 16-bit counters that intentionally wrap. */
template <typename Enemy>
inline bool enemy_action_period_check(
    Enemy* i_this,
    const s16 frameCounter,
    const f32 vanillaPeriod
) {
    return action_event_count(
               static_cast<u16>(frameCounter),
               static_cast<s32>(vanillaPeriod),
               enemy_action_time_speed(i_this)) > 0;
}

template <typename Enemy>
inline bool enemy_action_period_is_odd(
    Enemy* i_this,
    const s16 frameCounter,
    const f32 vanillaPeriod
) {
    if (!(vanillaPeriod > 0.0f)) {
        return false;
    }

    const double actionFrame =
        static_cast<double>(static_cast<u16>(frameCounter)) *
        enemy_action_time_speed(i_this);
    return std::fmod(std::floor(actionFrame / vanillaPeriod), 2.0) != 0.0;
}

/* Count periodic boundaries crossed by a timer that decreases once per frame. */
inline int action_countdown_event_count(
    const s32 remainingRealFrames,
    const s32 vanillaPeriod,
    const f32 requestedActionSpeed
) {
    if (remainingRealFrames < 0 || vanillaPeriod <= 0) {
        return 0;
    }

    const f32 actionSpeed = safe_action_speed(requestedActionSpeed);
    if (actionSpeed == 1.0f) {
        return remainingRealFrames % vanillaPeriod == 0 ? 1 : 0;
    }

    const double currentRemaining =
        static_cast<double>(remainingRealFrames) * actionSpeed;
    const double previousRemaining =
        (static_cast<double>(remainingRealFrames) + 1.0) * actionSpeed;
    const double currentBoundary = std::ceil(currentRemaining / vanillaPeriod);
    const double previousBoundary = std::ceil(previousRemaining / vanillaPeriod);

    return static_cast<int>(std::clamp(previousBoundary - currentBoundary,
        0.0, static_cast<double>(std::numeric_limits<int>::max())));
}

template <typename Enemy>
inline int enemy_action_countdown_event_count(
    Enemy* i_this,
    const s32 remainingRealFrames,
    const s32 vanillaPeriod
) {
    return action_countdown_event_count(
        remainingRealFrames,
        vanillaPeriod,
        enemy_action_time_speed(i_this));
}

/* Identify the current alternating block of a decreasing countdown. */
template <typename Enemy>
inline bool enemy_action_countdown_period_is_odd(
    Enemy* i_this,
    const s32 remainingRealFrames,
    const s32 vanillaPeriod
) {
    if (remainingRealFrames < 0 || vanillaPeriod <= 0) {
        return false;
    }

    const double remainingActionFrames =
        static_cast<double>(remainingRealFrames) *
        enemy_action_time_speed(i_this);
    return std::fmod(std::floor(remainingActionFrames / vanillaPeriod), 2.0) != 0.0;
}

template <typename Enemy>
inline f32 enemy_action_step(
    Enemy* i_this,
    const f32 vanillaStep
) {
    return action_step(
        vanillaStep,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline void enemy_angle_add(
    Enemy* i_this,
    s16& value,
    const f32 vanillaStep
) {
    const f32 step = enemy_action_step(i_this, vanillaStep);
    const s32 wrappedStep = static_cast<s32>(std::fmod(step, 65536.0f));
    value = static_cast<s16>(static_cast<s32>(value) + wrappedStep);
}

template <typename Enemy>
inline s16 enemy_action_s16_step(
    Enemy* i_this,
    const s16 vanillaStep
) {
    return action_s16_step(
        vanillaStep,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline s16 enemy_signed_action_s16_step(
    Enemy* i_this,
    const s16 vanillaStep
) {
    return signed_action_s16_step(
        vanillaStep,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline f32 enemy_move_step(Enemy* i_this, const f32 vanillaStep) {
    return action_step(
        vanillaStep,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline cXyz enemy_move_step(Enemy* i_this, cXyz vanillaStep) {
    const f32 actionSpeed = enemy_action_time_speed(i_this);
    if (actionSpeed == 1.0f) {
        return vanillaStep;
    }

    vanillaStep *= actionSpeed;
    return vanillaStep;
}

template <typename Enemy>
inline int enemy_chase_action_pos(
    Enemy* i_this,
    cXyz* value,
    const cXyz& target,
    const f32 vanillaStep
) {
    return cLib_chasePos(
        value,
        target,
        enemy_action_step(i_this, vanillaStep));
}

template <typename Enemy>
inline int enemy_chase_action_pos_xz(
    Enemy* i_this,
    cXyz* value,
    const cXyz& target,
    const f32 vanillaStep
) {
    return cLib_chasePosXZ(
        value,
        target,
        enemy_action_step(i_this, vanillaStep));
}

/* Integrate already-resolved physical values without the engine's generic
 * forward-speed resolver. Collision separation remains in world units. */
inline void pos_move_resolved_f(
    fopAc_ac_c* actor,
    const f32 physicalSpeedF,
    const f32 physicalGravity,
    const f32 physicalMaxFallSpeed,
    const cXyz* collisionMove
) {
    const f32 physicalX = physicalSpeedF * cM_ssin(actor->current.angle.y);
    f32 physicalY = actor->speed.y + physicalGravity;
    const f32 physicalZ = physicalSpeedF * cM_scos(actor->current.angle.y);
    if (physicalY < physicalMaxFallSpeed) {
        physicalY = physicalMaxFallSpeed;
    }
    actor->speed.x = physicalX;
    actor->speed.y = physicalY;
    actor->speed.z = physicalZ;
    fopAcM_posMove(actor, collisionMove);
}

/*
 * Compatibility path for earlier actors: only forward speed is resolved here.
 * Their gravity and vertical velocity already use physical frame units, and
 * their callers may already resolve gravity at assignment. Do not resolve it
 * again or change their stored velocity convention. Forward speed still must
 * be resolved only once, not here and again in fopAcM_calcSpeed.
 */
template <typename Enemy>
inline void enemy_pos_move_f(Enemy* i_this, const cXyz* collisionMove) {
    fopAc_ac_c* actor = enemy_attribute_actor(i_this);
    pos_move_resolved_f(actor, enemy_move_step(i_this, actor->speedF),
                        actor->gravity, actor->maxFallSpeed, collisionMove);
}

/*
 * For explicitly migrated actors, stored velocities stay in vanilla units.
 * Convert them only while integrating
 * this frame, then restore the units for the actor's next action.
 * Gravity is resolved separately (the mod controls whether it follows speed),
 * and collision separation is deliberately not time-scaled.
 *
 * The host's fopAcM_calcSpeed already resolves forward speed for generic actors.
 * Reproduce its velocity calculation here with our already-resolved speed and
 * call only fopAcM_posMove, avoiding a second resolution (and honoring child
 * ownership and attachment policies even when the action speed is exactly 1).
 */
template <typename Enemy>
inline void enemy_action_pos_move_f(
    Enemy* i_this,
    const cXyz* collisionMove
) {
    fopAc_ac_c* actor = EnemyActorAccessor<Enemy>::get(i_this);
    const f32 actionSpeed = enemy_action_time_speed(i_this);
    const f32 vanillaGravity = actor->gravity;
    const f32 scaledGravity = enemy_uses_action_time_scale(i_this)
        ? std::copysign(resolve_nonnegative(
              enemy_attribute_actor(i_this), ACTOR_ATTRIBUTE_GRAVITY,
              std::fabs(vanillaGravity)), vanillaGravity)
        : vanillaGravity;

    actor->speed.y = action_step(actor->speed.y, actionSpeed);
    pos_move_resolved_f(actor, action_step(actor->speedF, actionSpeed), scaledGravity,
                        action_step(actor->maxFallSpeed, actionSpeed), collisionMove);
    if (actionSpeed != 1.0f) {
        actor->speed /= actionSpeed;
    }
}

template <typename Enemy>
inline f32 enemy_gravity_step(Enemy* i_this, const f32 vanillaStep) {
    if (!enemy_uses_action_time_scale(i_this)) {
        return vanillaStep;
    }

    return resolve_nonnegative(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_GRAVITY,
        vanillaStep);
}

template <typename Enemy>
inline void enemy_add_action_calc2(
    Enemy* i_this,
    f32* value,
    const f32 target,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep
) {
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        enemy_action_time_speed(i_this));

    cLib_addCalc2(
        value,
        target,
        params.fraction,
        params.maximumStep);
}

template <typename Enemy>
inline void enemy_add_action_calc0(
    Enemy* i_this,
    f32* value,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep
) {
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        enemy_action_time_speed(i_this));

    cLib_addCalc0(
        value,
        params.fraction,
        params.maximumStep);
}

template <typename Enemy>
inline f32 enemy_add_action_calc(
    Enemy* i_this,
    f32* value,
    const f32 target,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep,
    const f32 vanillaMinStep
) {
    const f32 actionSpeed = enemy_action_time_speed(i_this);
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        actionSpeed);

    return cLib_addCalc(
        value,
        target,
        params.fraction,
        params.maximumStep,
        action_step(vanillaMinStep, actionSpeed));
}

template <typename Enemy>
inline f32 enemy_add_action_calc_pos(
    Enemy* i_this,
    cXyz* value,
    const cXyz& target,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep,
    const f32 vanillaMinStep
) {
    const f32 actionSpeed = enemy_action_time_speed(i_this);
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        actionSpeed);

    return cLib_addCalcPos(
        value,
        target,
        params.fraction,
        params.maximumStep,
        action_step(vanillaMinStep, actionSpeed));
}

template <typename Enemy>
inline f32 enemy_add_action_calc_pos_xz(
    Enemy* i_this,
    cXyz* value,
    const cXyz& target,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep,
    const f32 vanillaMinStep
) {
    const f32 actionSpeed = enemy_action_time_speed(i_this);
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        actionSpeed);

    return cLib_addCalcPosXZ(
        value,
        target,
        params.fraction,
        params.maximumStep,
        action_step(vanillaMinStep, actionSpeed));
}

template <typename Enemy>
inline void enemy_add_action_calc_pos2(
    Enemy* i_this,
    cXyz* value,
    const cXyz& target,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep
) {
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        enemy_action_time_speed(i_this));

    cLib_addCalcPos2(
        value,
        target,
        params.fraction,
        params.maximumStep);
}

template <typename Enemy>
inline void enemy_add_action_calc_pos_xz2(
    Enemy* i_this,
    cXyz* value,
    const cXyz& target,
    const f32 vanillaFraction,
    const f32 vanillaMaxStep
) {
    const ActionCalcParams params = action_calc_params(
        vanillaFraction,
        vanillaMaxStep,
        enemy_action_time_speed(i_this));

    cLib_addCalcPosXZ2(
        value,
        target,
        params.fraction,
        params.maximumStep);
}

template <typename Enemy>
inline f32 enemy_action_damping(
    Enemy* i_this,
    const f32 vanillaFactor
) {
    return action_damping(
        vanillaFactor,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline f32 enemy_action_growth(Enemy* i_this, const f32 vanillaFactor) {
    const f32 speed = enemy_action_time_speed(i_this);
    return speed == 1.0f ? vanillaFactor : std::pow(std::max(vanillaFactor, 0.0f), speed);
}

/* For visual timers whose remaining count is also an amplitude/phase value. */
template <typename Enemy, typename Timer>
inline void enemy_tick_visual_timer(Enemy* i_this, Timer& value, const s32 frameCounter) {
    const int steps = enemy_action_event_count(i_this, frameCounter, 1);
    value = static_cast<Timer>(std::max(0, static_cast<int>(value) - steps));
}

template <typename Enemy>
inline f32 enemy_move_acceleration(
    Enemy* i_this,
    const f32 vanillaStep
) {
    const f32 actionSpeed = enemy_action_time_speed(i_this);
    return vanillaStep * actionSpeed * actionSpeed;
}

template <typename Enemy>
inline void enemy_add_action_angle(
    Enemy* i_this,
    s16* value,
    const s16 target,
    const s16 vanillaDivisor,
    const s16 vanillaMaxStep
) {
    const ActionAngleParams params = action_angle_params(
        vanillaDivisor,
        vanillaMaxStep,
        enemy_action_time_speed(i_this));

    cLib_addCalcAngleS2(
        value,
        target,
        params.divisor,
        params.maximumStep);
}

template <typename Enemy>
inline int enemy_chase_action_angle(
    Enemy* i_this,
    s16* value,
    const s16 target,
    const s16 vanillaStep
) {
    const s16 step = action_s16_step(
        vanillaStep,
        enemy_action_time_speed(i_this));

    return cLib_chaseAngleS(
        value,
        target,
        step);
}

template <typename Enemy>
inline s16 enemy_add_action_angle(
    Enemy* i_this,
    s16* value,
    const s16 target,
    const s16 vanillaDivisor,
    const s16 vanillaMaxStep,
    const s16 vanillaMinStep
) {
    const f32 actionSpeed = enemy_action_time_speed(i_this);

    const ActionAngleParams params = action_angle_params(
        vanillaDivisor,
        vanillaMaxStep,
        actionSpeed);

    const s16 minimumStep = action_s16_step(
        vanillaMinStep,
        actionSpeed);

    return cLib_addCalcAngleS(
        value,
        target,
        params.divisor,
        params.maximumStep,
        minimumStep);
}

template <typename Enemy>
inline s16 enemy_action_phase_angle(
    Enemy* i_this,
    const s32 vanillaCounter,
    const s32 vanillaRate,
    const s32 phaseOffset = 0
) {
    return action_phase_angle(
        vanillaCounter,
        vanillaRate,
        enemy_action_time_speed(i_this),
        phaseOffset);
}

template <typename Enemy>
inline s16 enemy_action_phase_angle(
    Enemy* i_this,
    const s32 vanillaCounter,
    const f32 vanillaRate,
    const f32 phaseOffset = 0.0f
) {
    return action_phase_angle(
        vanillaCounter,
        vanillaRate,
        enemy_action_time_speed(i_this),
        phaseOffset);
}

template <typename Enemy>
inline int enemy_chase_action_float(
    Enemy* i_this,
    f32* value,
    const f32 target,
    const f32 vanillaStep
) {
    const f32 step = action_step(
        vanillaStep,
        enemy_action_time_speed(i_this));

    return cLib_chaseF(
        value,
        target,
        step);
}

template <typename Enemy>
inline int enemy_chase_action_s16(
    Enemy* i_this,
    s16* value,
    const s16 target,
    const s16 vanillaStep
) {
    const s16 step = action_s16_step(
        vanillaStep,
        enemy_action_time_speed(i_this));

    return cLib_chaseS(value, target, step);
}

template <typename Enemy>
inline int enemy_chase_action_u8(
    Enemy* i_this,
    u8* value,
    const u8 target,
    const u8 vanillaStep
) {
    const u8 step = action_u8_step(
        vanillaStep,
        enemy_action_time_speed(i_this));

    return cLib_chaseUC(value, target, step);
}

template <typename Enemy>
inline s16 enemy_sync_timer(
    Enemy* i_this,
    const f32 vanillaFrames
) {
    return sync_timer(
        vanillaFrames,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline s16 enemy_stun_timer(
    Enemy* i_this,
    const f32 vanillaFrames
) {
    return duration_timer(
        vanillaFrames,
        enemy_stun_duration_multiplier(i_this));
}

template <typename Timer = s16, typename Enemy>
inline Timer enemy_stun_terminal_timer(
    Enemy* i_this,
    const f32 vanillaActiveFrames
) {
    const int maximum = std::numeric_limits<Timer>::max();
    return static_cast<Timer>(std::min<int>(
        enemy_stun_timer(i_this, vanillaActiveFrames), maximum - 1) + 1);
}

template <typename Enemy>
inline s16 enemy_stun_countdown_milestone(
    Enemy* i_this,
    const f32 vanillaStartFrames,
    const f32 vanillaRemainingFrames
) {
    const f32 durationMultiplier = enemy_stun_duration_multiplier(i_this);
    if (durationMultiplier == 1.0f) {
        return duration_timer(vanillaRemainingFrames, 1.0f);
    }

    const f32 elapsedFrames = std::max(
        vanillaStartFrames - vanillaRemainingFrames, 0.0f);
    const s16 remaining = static_cast<s16>(
        duration_timer(vanillaStartFrames, durationMultiplier) -
        duration_timer(elapsedFrames, durationMultiplier));
    return vanillaRemainingFrames > 0.0f ? std::max<s16>(remaining, 1) : 0;
}

template <typename Enemy>
inline u8 enemy_stun_u8_timer(
    Enemy* i_this,
    const f32 vanillaFrames
) {
    return duration_u8_timer(
        vanillaFrames,
        enemy_stun_duration_multiplier(i_this));
}

template <typename Enemy>
inline s8 enemy_stun_s8_timer(
    Enemy* i_this,
    const f32 vanillaFrames
) {
    return duration_s8_timer(
        vanillaFrames,
        enemy_stun_duration_multiplier(i_this));
}

/* Reserve a terminal/sentinel frame before converting to the storage type.
 * Adding one after a saturated byte/s16 conversion would wrap the timer. */
template <typename Timer = s16, typename Enemy>
inline Timer enemy_sync_terminal_timer(Enemy* i_this, const f32 vanillaActiveFrames) {
    const int maximum = std::numeric_limits<Timer>::max();
    return static_cast<Timer>(std::min<int>(
        enemy_sync_timer(i_this, vanillaActiveFrames), maximum - 1) + 1);
}

/*
 * Convert a remaining-value milestone on a countdown that began at
 * vanillaStartFrames.  Synchronizing vanillaRemainingFrames by itself is
 * subtly wrong because both rounded durations must share the same origin.
 */
template <typename Enemy>
inline s16 enemy_sync_countdown_milestone(
    Enemy* i_this,
    const f32 vanillaStartFrames,
    const f32 vanillaRemainingFrames
) {
    if (enemy_action_time_speed(i_this) == 1.0f) {
        return sync_timer(vanillaRemainingFrames, 1.0f);
    }
    const f32 elapsedFrames = std::max(
        vanillaStartFrames - vanillaRemainingFrames, 0.0f);
    const s16 remaining = static_cast<s16>(
        enemy_sync_timer(i_this, vanillaStartFrames) -
        enemy_sync_timer(i_this, elapsedFrames));
    // A positive "last frame" event must precede the zero/expired branch.
    return vanillaRemainingFrames > 0.0f ? std::max<s16>(remaining, 1) : 0;
}

/* Recover the countdown's approximate remaining position on its action clock. */
template <typename Enemy>
inline f32 enemy_countdown_action_frames_remaining(
    Enemy* i_this,
    const s32 remainingRealFrames,
    const f32 vanillaStartFrames
) {
    const s32 elapsedRealFrames = std::max<s32>(
        enemy_sync_timer(i_this, vanillaStartFrames) - remainingRealFrames, 0);
    return std::max(
        vanillaStartFrames -
            elapsedRealFrames * enemy_action_time_speed(i_this),
        0.0f);
}

template <typename Enemy>
inline u8 enemy_sync_u8_timer(
    Enemy* i_this,
    const f32 vanillaFrames
) {
    return sync_u8_timer(
        vanillaFrames,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline s8 enemy_sync_s8_timer(
    Enemy* i_this,
    const f32 vanillaFrames
) {
    return sync_s8_timer(
        vanillaFrames,
        enemy_action_time_speed(i_this));
}

template <typename Enemy>
inline ActionAnimationParams enemy_animation_params(
    Enemy* i_this,
    const f32 vanillaMorph,
    const f32 vanillaPlaybackSpeed
) {
    return action_animation_params(
        vanillaMorph,
        vanillaPlaybackSpeed,
        enemy_action_time_speed(i_this));
}

template <typename Enemy, typename Morph, typename Animation, typename... Extra>
inline void enemy_set_animation(
    Enemy* i_this,
    Morph* morph,
    Animation animation,
    const int mode,
    const f32 vanillaMorph,
    const f32 vanillaPlaybackSpeed,
    Extra... extra
) {
    const ActionAnimationParams params = enemy_animation_params(
        i_this,
        vanillaMorph,
        vanillaPlaybackSpeed);

    morph->setAnm(
        animation,
        mode,
        params.morph,
        params.playbackSpeed,
        extra...);
}

template <typename Enemy, typename Morph>
inline void enemy_set_animation_play_speed(
    Enemy* i_this,
    Morph* morph,
    const f32 vanillaPlaybackSpeed
) {
    morph->setPlaySpeed(
        action_step(vanillaPlaybackSpeed, enemy_action_time_speed(i_this)));
}

template <typename Enemy>
inline s16 enemy_health_value(
    Enemy* i_this,
    const f32 vanillaHealth
) {
    return resolve_positive_s16(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_HEALTH,
        vanillaHealth);
}

/* Bosses with their own damage budgets keep the budget in its native units. */
template <typename Enemy>
inline f32 enemy_health_amount(Enemy* i_this, const f32 vanillaHealth) {
    return resolve_nonnegative(enemy_attribute_actor(i_this), ACTOR_ATTRIBUTE_HEALTH,
                               vanillaHealth);
}

template <typename Enemy>
inline f32 enemy_attack_damage(
    Enemy* i_this,
    const f32 vanillaDamage
) {
    return resolve_nonnegative(
        enemy_attribute_actor(i_this),
        ACTOR_ATTRIBUTE_ATTACK_DAMAGE,
        vanillaDamage);
}

template <typename Enemy>
inline int enemy_attack_power_byte(
    Enemy* i_this,
    const f32 vanillaDamage
) {
    return byte_value(
        enemy_attack_damage(i_this, vanillaDamage));
}

template <typename Enemy>
inline f32 enemy_signed_gravity_step(
    Enemy* i_this,
    const f32 vanillaAcceleration
) {
    if (vanillaAcceleration < 0.0f) {
        return -enemy_gravity_step(i_this, -vanillaAcceleration);
    }

    return enemy_gravity_step(i_this, vanillaAcceleration);
}

/*
 * Manual integrators keep velocity in vanilla units, then use enemy_move_step
 * at the displacement consumer. Convert resolved physical acceleration back
 * to those units; applying resolved gravity directly would scale it twice.
 */
template <typename Enemy>
inline f32 enemy_velocity_gravity_step(
    Enemy* i_this,
    const f32 vanillaAcceleration
) {
    return enemy_signed_gravity_step(i_this, vanillaAcceleration) /
           enemy_action_time_speed(i_this);
}

}  // namespace dusk::mods::svc::actor_attr

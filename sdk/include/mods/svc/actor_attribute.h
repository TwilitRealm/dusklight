#pragma once

#include <mods/api.h>

#ifdef __cplusplus
#include <mods/service.hpp>
#endif

#define ACTOR_ATTRIBUTE_SERVICE_ID "dev.twilitrealm.dusklight.actor_attribute"
#define ACTOR_ATTRIBUTE_SERVICE_MAJOR 1u
#define ACTOR_ATTRIBUTE_SERVICE_MINOR 5u

/* 0 is never a valid handle. */
typedef uint64_t ActorAttributeResolverHandle;

/*
 * Semantic actor values exposed by the host. A game integration should pass a
 * freshly calculated vanilla value immediately before it is consumed. Mods
 * may then transform that value without replacing the owning actor function.
 */
typedef enum ActorAttribute {
    ACTOR_ATTRIBUTE_MOVEMENT_SPEED = 0,
    ACTOR_ATTRIBUTE_SIZE = 1,
    ACTOR_ATTRIBUTE_HEALTH = 2,
    ACTOR_ATTRIBUTE_ATTACK_DAMAGE = 3,
    /* Per-frame downward acceleration magnitude, not a stored actor field. */
    ACTOR_ATTRIBUTE_GRAVITY = 4,
    /* Enemy awareness/aggro distance, separate from physical reach. */
    ACTOR_ATTRIBUTE_NOTICE_RANGE = 5,
    /* Horizontal distance Link is pushed when this actor damages him. */
    ACTOR_ATTRIBUTE_PLAYER_KNOCKBACK = 6,
    /* Duration of a genuine living-enemy stun/vulnerability window. */
    ACTOR_ATTRIBUTE_STUN_DURATION = 7,
} ActorAttribute;

/* Host-owned callback data, valid only for the duration of the callback. */
typedef struct ActorAttributeInfo {
    const void* actor; /* const fopAc_ac_c* */
    ActorAttribute attribute;
    float vanilla_value;
    float current_value;
} ActorAttributeInfo;

/*
 * Return true and write out_value to replace current_value, or return false to
 * leave it unchanged. Resolvers are chained in mod load order.
 */
typedef bool (*ActorAttributeResolveFn)(ModContext* ctx, const ActorAttributeInfo* info,
    float* out_value, void* user_data);

typedef struct ActorAttributeService {
    ServiceHeader header;

    ModResult (*register_resolver)(ModContext* ctx, ActorAttributeResolveFn fn, void* user_data,
        ActorAttributeResolverHandle* out_handle);

    ModResult (*unregister_resolver)(ModContext* ctx, ActorAttributeResolverHandle handle);

    /* An actor-specific mod hook asks for the currently resolved semantic
     * value where it is actually used. No actor-source switch is required. */
    float (*resolve)(const void* actor, ActorAttribute attribute, float vanilla_value);
} ActorAttributeService;

MOD_DECLARE_SERVICE(ActorAttributeService, svc_actor_attribute, ACTOR_ATTRIBUTE_SERVICE_ID,
    ACTOR_ATTRIBUTE_SERVICE_MAJOR, ACTOR_ATTRIBUTE_SERVICE_MINOR);

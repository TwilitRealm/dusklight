#pragma once

#include "mods/svc/actor_attribute.h"

class fopAc_ac_c;

namespace dusk::mods::svc {

/* Host-side entry point used at semantic value-consumption sites. */
float resolve_actor_attribute(
    const fopAc_ac_c* actor, ActorAttribute attribute, float vanillaValue);


}  // namespace dusk::mods::svc

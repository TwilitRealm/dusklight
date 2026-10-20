#pragma once

#include "dusk/mods/svc/actor_attribute_helpers.hpp"

namespace dusk::mods::svc::actor_attr {

// Acch can miss the floor when one scaled movement step crosses all the way
// through it. Check the feet's path, then let the usual CrrPos call finish
// ground, wall, and speed correction.
template <typename Enemy>
inline void enemy_swept_ground_correct(Enemy* i_this, dBgS_Acch& acch, const cXyz& movementStart) {
    if (enemy_action_time_speed(i_this) <= 1.0f) {
        return;
    }

    fopAc_ac_c* actor = EnemyActorAccessor<Enemy>::get(i_this);
    if (movementStart.y <= actor->current.pos.y || actor->speed.y >= 0.0f) {
        return;
    }

    cXyz sweepStart = movementStart;
    sweepStart.y += 1.0f;
    cBgS_LinChk groundSweep;
    groundSweep.Set2(&sweepStart, &actor->current.pos, fopAcM_GetID(actor));
    groundSweep.SetExtChk(acch);
    if (!dComIfG_Bgsp().LineCross(&groundSweep) || !dBgS_CheckBGroundPoly(groundSweep)) {
        return;
    }

    const int groundCode = dComIfG_Bgsp().GetGroundCode(groundSweep);
    if (groundCode == 4 || groundCode == 10 || groundCode == 5) {
        return;
    }

    actor->current.pos = groundSweep.GetCross();
    actor->current.pos.y -= 1.0f;
}

}  // namespace dusk::mods::svc::actor_attr

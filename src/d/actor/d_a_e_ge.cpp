/**
 * d_a_e_ge.cpp
 * Enemy - Guay
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_boomerang.h"
#include "d/actor/d_a_e_ge.h"
#include "d/d_cc_d.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"
#endif
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_camera_mng.h"

#if TARGET_PC  // additional actor attribute integration
namespace actor_attr = dusk::mods::svc::actor_attr;
#endif

class daE_GE_HIO_c : public JORReflexible {
public:
    daE_GE_HIO_c();
    virtual ~daE_GE_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x04 */ s8 id;
    /* 0x08 */ f32 player_recognition_distance;
    /* 0x0C */ f32 arrow_recognition_distance;
    /* 0x10 */ f32 peer_recognition_distance;
    /* 0x14 */ f32 attack_speed;
    /* 0x18 */ f32 turning_speed;
    /* 0x1C */ f32 attack_turn_altitude;
    /* 0x20 */ f32 attack_turn_radius;
    /* 0x24 */ f32 attack_start_angle;
    /* 0x28 */ f32 add_attack_descent_speed;
    /* 0x2C */ f32 minimum_attack_radius;
    /* 0x30 */ f32 next_attack_timer;
};

enum daE_GE_Action {
    /* 0 */ ACTION_WAIT,
    /* 1 */ ACTION_FLY,
    /* 2 */ ACTION_ATTACK,
    /* 3 */ ACTION_BACK,
    /* 4 */ ACTION_DOWN,
    /* 5 */ ACTION_SURPRISE,
    /* 6 */ ACTION_CAW,
    /* 7 */ ACTION_WIND,
    /* 8 */ ACTION_SHIELD,
    /* 9 */ ACTION_MAX
};
#if TARGET_PC  // additional actor attribute integration

namespace dusk::mods::svc::actor_attr {

/* While caught, the Guay must remain synchronized to Link's boomerang. */
template <>
struct EnemyActionTimePolicy<daE_GE_c> {
    static bool uses(daE_GE_c* i_this) {

        return i_this->getActionMode() != ACTION_WIND;

}
};

}  // namespace dusk::mods::svc::actor_attr
#endif

static u8 hio_set;

static daE_GE_HIO_c l_HIO;
#if TARGET_PC  // enemy attribute integration
static f32 e_ge_hover_offset(daE_GE_c* i_this, f32 vanillaOffset) {

    constexpr f32 bodyOffset = 30.0f;
    return vanillaOffset + bodyOffset * (actor_attr::enemy_size_multiplier(i_this) - 1.0f);
}
#endif

daE_GE_HIO_c::daE_GE_HIO_c() {
    id = -1;
    player_recognition_distance = 1000.0f;
    arrow_recognition_distance = 300.0f;
    peer_recognition_distance = 600.0f;
    attack_speed = 15.0f;
    turning_speed = 15.0f;
    attack_turn_altitude = 600.0f;
    attack_turn_radius = 1000.0f;
    attack_start_angle = 28000.0f;
    add_attack_descent_speed = 1.3f;
    minimum_attack_radius = 500.0f;
    next_attack_timer = 110.0f;
}

void daE_GE_c::bckSet(int i_index, f32 i_morf, u8 i_attr, f32 i_rate) {
    J3DAnmTransform* transform = (J3DAnmTransform*)dComIfG_getObjectRes("E_GE", i_index);
    DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorfSO, transform, i_attr, i_morf, i_rate, 0.0f, -1.0f), mpMorfSO->setAnm(transform, i_attr, i_morf, i_rate, 0.0f, -1.0f));
}

bool daE_GE_c::bckCheck(int i_index) {
    J3DAnmTransform* transform = (J3DAnmTransform*)dComIfG_getObjectRes("E_GE", i_index);

    if (mpMorfSO->getAnm() == transform) {
        return true;
    }

    return false;
}

int daE_GE_c::draw() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    J3DModel* model = mpMorfSO->getModel();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(model, &tevStr);
    mpMorfSO->entryDL();
    cXyz modified_pos;
#if TARGET_PC  // enemy attribute integration
    modified_pos.set(current.pos.x, current.pos.y + 100.0f * sizeMultiplier, current.pos.z);
    mShadowKey = dComIfGd_setShadow(mShadowKey, 1, model, &modified_pos, 400.0f * sizeMultiplier, 0.0f,
#else
    modified_pos.set(current.pos.x, current.pos.y + 100.0f, current.pos.z);
    mShadowKey = dComIfGd_setShadow(mShadowKey, 1, model, &modified_pos, 400.0f, 0.0f,
#endif
                                    current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr,
                                    0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    return 1;
}

static int daE_GE_Draw(daE_GE_c* i_this) {
    return i_this->draw();
}

bool daE_GE_c::checkBeforeBg(int param_0, f32 param_1) {
    cXyz end;
    if (param_0 == 0) {
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
        cXyz vec(param_1 * sizeMultiplier, 0.0f, 300.0f * sizeMultiplier);
#else
        cXyz vec(param_1, 0.0f, 300.0f);
#endif
        cLib_offsetPos(&end, &current.pos, shape_angle.y, &vec);
    } else {
        mDoMtx_stack_c::copy(daPy_getLinkPlayerActorClass()->getModelJointMtx(0));
        mDoMtx_stack_c::multVecZero(&end);
    }

    dBgS_LinChk linChk;
    linChk.Set(&current.pos, &end, NULL);
    if (dComIfG_Bgsp().LineCross(&linChk)) {
        return true;
    }
    return false;
}

static void* s_arrow_sub(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_ARROW_e) {
            if (fopAcM_GetSpeedF((fopAc_ac_c*)i_actor) &&
                fopAcM_searchActorDistance((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_data) <
                    DUSK_IF_ELSE(l_HIO.arrow_recognition_distance * actor_attr::enemy_size_multiplier(static_cast<daE_GE_c*>(i_data)), l_HIO.arrow_recognition_distance))
            {
                static_cast<daE_GE_c*>(i_data)->setSurpriseTime(cM_rndF(10.0f));
            }
        }
    }
    return NULL;
}

static void* s_ge_surprise(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_E_GE_e) {
            if (fopAcM_searchActorDistance((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_data) <
                    DUSK_IF_ELSE(l_HIO.peer_recognition_distance * actor_attr::enemy_size_multiplier(static_cast<daE_GE_c*>(i_data)) &&, l_HIO.peer_recognition_distance &&)
                static_cast<daE_GE_c*>(i_actor)->getSurpriseTime() == 0)
            {
                static_cast<daE_GE_c*>(i_actor)->setSurpriseTime(cM_rndF(5.0f) + 5.0f);
            }
        }
    }
    return NULL;
}

static void* s_ge_caw(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_E_GE_e) {
            if (i_actor != i_data &&
                fopAcM_searchActorDistance((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_data) <
                    DUSK_IF_ELSE(l_HIO.peer_recognition_distance * actor_attr::enemy_size_multiplier(static_cast<daE_GE_c*>(i_data)), l_HIO.peer_recognition_distance))
            {
                static_cast<daE_GE_c*>(i_actor)->setCaw();
            }
        }
    }
    return NULL;
}

static s32 target_count;

static void* s_ge_attack(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_E_GE_e) {
            if (static_cast<daE_GE_c*>(i_actor)->getActionMode() == 2) {
                target_count++;
            }
        }
    }
    return NULL;
}

static void* s_ge_attack2(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_E_GE_e) {
            if (static_cast<daE_GE_c*>(i_actor)->checkOtherAttacker()) {
                target_count++;
                return NULL;
            }
        }
    }
    return NULL;
}

void daE_GE_c::setActionMode(int i_actionMode) {
    mPrevActionMode = mActionMode;
    mActionMode = i_actionMode;
    mAnmChangeTimer = 0;
    mSubMode = 0;
    mMode = 0;
    field_0xb8e[1] = 0;
    field_0xb8e[0] = 0;
    shape_angle.z = 0;
    shape_angle.x = 0;
    attention_info.distances[fopAc_attn_BATTLE_e] = 3;
}

void daE_GE_c::damage_check() {
    if (mDamageCooldownTimer == 0) {
        mStts.Move();
        if (mActionMode != ACTION_DOWN) {
            mAtInfo.mpCollider = NULL;
            if (mSphere.ChkTgHit()) {
                mAtInfo.mpCollider = mSphere.GetTgHitObj();
            } else if (mSphere2.ChkTgHit()) {
                mAtInfo.mpCollider = mSphere2.GetTgHitObj();
            }

            if (mAtInfo.mpCollider != NULL) {
                cc_at_check(this, &mAtInfo);

                if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_UNK)) {
                    mDamageCooldownTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20.0f), 20);
                } else {
                    mDamageCooldownTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10.0f), 10);
                }

                if (mAtInfo.mAttackPower <= 1) {
                    mDamageCooldownTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10.0f), 10);
                }

                if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
                    setActionMode(ACTION_SHIELD);
                } else if (mMoveType != 2 && mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG)) {
                    if (mActionMode != ACTION_WIND) {
                        setActionMode(ACTION_WIND);
                    }
                } else {
                    cXyz position;
#if TARGET_PC  // enemy attribute integration
                    const cXyz effectScale = actor_attr::enemy_size_multiplier(this, 1.0f);
#endif

                    mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(3));
                    mDoMtx_stack_c::multVecZero(&position);
                    dComIfGp_setHitMark(3, this, &position, NULL, DUSK_IF_ELSE(&effectScale, NULL), 0);

#if TARGET_PC  // enemy attribute integration
                    const bool lethalHit = field_0x560 == actor_attr::enemy_health_value(this, 10) || health <= 0;
                    setActionMode(lethalHit ? ACTION_DOWN : ACTION_SURPRISE);
#else
                    setActionMode(ACTION_DOWN);
#endif

                    if (!mAtInfo.mpCollider->ChkAtType(AT_TYPE_ARROW)) {
                        speedF = cM_rndF(5.0f) + 15.0f;
                        speed.y = cM_rndF(5.0f) + 55.0f;
                    } else {
                        fpcM_Search(s_ge_surprise, this);
                        speedF = cM_rndF(5.0f) + 5.0f;
                        speed.y = cM_rndF(5.0f) + 25.0f;
                    }
                    shape_angle.y = mAtInfo.mHitDirection.y + 0x8000;
                    field_0xb68 = 30.0f;
                    mHitDirectionY = mAtInfo.mHitDirection.y;
                }
            }
        }
    }
}

bool daE_GE_c::checkOtherAttacker() {
    if (mActionMode != ACTION_ATTACK) {
        return false;
    }
    if (mMode != 2) {
        return false;
    }

    if (!(field_0xb8e[0] < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 200.0f - l_HIO.next_attack_timer), 200.0f - l_HIO.next_attack_timer))) {
        return true;
    }

    return false;
}

bool daE_GE_c::searchNextAttacker() {
    if (mMoveType == 2) {
        return false;
    }

    target_count = 0;
    fpcM_Search(s_ge_attack, this);
    if (target_count >= 2) {
        return false;
    }

    setActionMode(ACTION_ATTACK);
    field_0xb8e[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(300.0f) + 700.0f), cM_rndF(300.0f) + 700.0f);
    return true;
}

bool daE_GE_c::checkCircleSpeedAdd(cXyz* i_src, cXyz* param_1) {
    cXyz position;
    cLib_offsetPos(&position, i_src, field_0xb8c, param_1);
    s16 targetAngleY = cLib_targetAngleY(i_src, &position);

    return abs((s16)(field_0xb8c - targetAngleY)) < 0x2000;
}

void daE_GE_c::setAddCalcSpeed(cXyz& param_0, cXyz const& param_1, f32 param_2, f32 param_3,
                               f32 param_4, f32 param_5) {
    // Can't prove that this exists in debug but it seems necessary for the logic
    cXyz diff_vec = param_1 - param_0;
    f32 diff = diff_vec.abs();
    cXyz speed_vec(0.0f, 0.0f, 0.0f);

    if (param_0 != param_1 && diff >= param_4) {
        diff *= param_2;
        diff_vec *= param_2;
        if (!cLib_IsZero(diff)) {
            if (diff > param_3) {
                speed_vec = (diff_vec * param_3) / diff;
            } else if (diff < param_4) {
                speed_vec = (diff_vec * param_4) / diff;
            }
        }
    }

    speedF = speed_vec.absXZ();
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, speed_vec.y, param_5), cLib_chaseF(&speed.y, speed_vec.y, param_5));
}

cXyz daE_GE_c::calcCircleFly(cXyz* i_src, cXyz* i_vec, s16 i_angle, f32 param_3, s16 i_scale,
                             f32 param_5) {
    cXyz position;

    if (i_vec != 0x0) {
        cLib_offsetPos(&position, i_src, i_angle, i_vec);
    } else {
        position = *i_src;
    }
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(this, &shape_angle.y, (s16)cLib_targetAngleY(&current.pos, &position), i_scale,
#else
    cLib_addCalcAngleS(&shape_angle.y, (s16)cLib_targetAngleY(&current.pos, &position), i_scale,
#endif
                       0x800, 0x100);
    current.angle.y = shape_angle.y;
    setAddCalcSpeed(current.pos, position, 8.0f, param_3, 1.0f, param_5);

    return current.pos - position;
}

void daE_GE_c::executeWait() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    fpcM_Search(s_arrow_sub, this);

    switch (mMode) {
    case 0:
        bckSet(0xC, 3.0f, 2, 1.0f);
        mMode = 1;
        field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(300.0f) + 300.0f), (s16)(cM_rndF(300.f) + 300.0f));
        speedF = 0.0f;
        speed.y = 0.0f;
        /* fallthrough */

    case 1:
        if (field_0xb8e[0] == 0) {
            mMode = 3;
        } else if (field_0xb8e[1] == 0 &&
                   fopAcM_searchPlayerDistanceXZ(this) < DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, l_HIO.player_recognition_distance), l_HIO.player_recognition_distance))
        {
            bckSet(5, 3.0f, 0, 1.0f);
            mMode = 2;
            fpcM_Search(s_ge_caw, this);
        }
        break;

    case 2:
        if (mpMorfSO->checkFrame(6.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_GE_V_NAKU, -1);
        }
        if (mpMorfSO->isStop()) {
            setActionMode(ACTION_CAW);
        }
        break;

    case 3:
        bckSet(5, 3.0f, 0, 1.0f);
        mMode = 4;
        break;

    case 4:
        if (mpMorfSO->checkFrame(6.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_GE_V_NAKU, -1);
        }
        if (mpMorfSO->isStop()) {
            mMode = 0;
        }
        break;
    }
}

void daE_GE_c::executeFly() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz position;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    f32 turning_speed = l_HIO.turning_speed;
    if (mMoveType == 2) {
        turning_speed = 8.0f;
    }

    switch (mMode) {
    case 10:
    case 0:
        field_0xb9e = 0;
        if (mMode == 10) {
            bckSet(7, 10.0f, 2, 1.0f);
        } else {
            bckSet(7, 3.0f, 2, 1.0f);
        }
        mMode = 1;
        if (cM_rnd() < 0.5f) {
            field_0xb9d = true;
        } else {
            field_0xb9d = false;
        }

        if (field_0xb9d) {
            field_0xb8a = (s16)(cM_rndFX(5.0f) * 20.0f + 400.0f);
        } else {
            field_0xb8a = (s16)(cM_rndFX(5.0f) * 20.0f + -400.0f);
        }
        mAnmChangeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(30.0f) + 30.0f), (s16)(cM_rndF(30.0f) + 30.0f));
        mSubMode = 0;
        field_0xb5c = 0.0f;
        break;

    case 1:
        if (mSubMode == 0) {
            field_0xb5c += DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 4.0f), 4.0f);
            if (mpMorfSO->checkFrame(0.0f) && field_0xb5c >= 0.0f) {
                bckSet(10, 10.0f, 2, 1.0f);
                mAnmChangeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(50.0f) + 50.0f), (s16)(cM_rndF(50.0f) + 50.0f));
                mSubMode = 1;
            }
        } else {
            field_0xb5c -= DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 3.0f), 3.0f);
            if (mAnmChangeTimer == 0) {
                bckSet(7, 10.0f, 2, 1.0f);
                mSubMode = 0;
            }
        }

        position.set(0.0f, field_0xb5c, DUSK_IF_ELSE(1000.0f * sizeMultiplier, 1000.0f));

        if (checkCircleSpeedAdd(&home.pos, &position)) {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0xb8c , field_0xb8a);
#else
            field_0xb8c += field_0xb8a;
#endif
        }

        calcCircleFly(&home.pos, &position, field_0xb8c, turning_speed, 6, 1.0f);

        if (field_0xb8e[1] == 0) {
            if (DUSK_IF_ELSE(player->current.pos.absXZ(home.pos) < 2000.0f * sizeMultiplier && fopAcM_searchPlayerDistanceXZ(this) < actor_attr::enemy_notice_distance(this, l_HIO.player_recognition_distance), player->current.pos.absXZ(home.pos) < 2000.0f &&
                fopAcM_searchPlayerDistanceXZ(this) < l_HIO.player_recognition_distance))
            {
                bckSet(4, 3.0f, 0, 1.0f);
                mMode = 2;
            }
        }
        break;

    case 2:
        position.set(0.0f, 0.0f, DUSK_IF_ELSE(1000.0f * sizeMultiplier, 1000.0f));

        if (checkCircleSpeedAdd(&home.pos, &position)) {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0xb8c , field_0xb8a);
#else
            field_0xb8c += field_0xb8a;
#endif
        }

        calcCircleFly(&home.pos, &position, field_0xb8c, turning_speed, 6, 1.0f);

        if (mpMorfSO->checkFrame(3.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_GE_V_NAKU, -1);
        }

        if (mpMorfSO->isStop() && !searchNextAttacker()) {
            mMode = 0;
        }
        break;
    }
}

bool daE_GE_c::checkAttackPossible(s16 i_angle, bool param_1) {
    if (field_0xb8e[0] == 0 && (i_angle > (s16)l_HIO.attack_start_angle || param_1)) {
        if (fopAcM_searchPlayerDistanceXZ(this) < DUSK_IF_ELSE(l_HIO.minimum_attack_radius * actor_attr::enemy_size_multiplier(this), l_HIO.minimum_attack_radius)) {
            return false;
        }
        target_count = 0;
        fpcM_Search(s_ge_attack2, this);
        if (target_count == 0) {
            return true;
        }
    }
    return false;
}

void daE_GE_c::executeAttack() {
    daPy_py_c* player = daPy_getPlayerActorClass();
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    cXyz position;
    mDoMtx_stack_c::copy(daPy_getLinkPlayerActorClass()->getModelJointMtx(0));
    mDoMtx_stack_c::multVecZero(&position);

    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    s16 distAngleS =
        cLib_distanceAngleS(fopCamM_GetAngleY(camera), fopAcM_searchPlayerAngleY(this));

    cXyz flyVec;

    // I don't think these cXyz's are correct like this in debug but I have no better solution
    cXyz temp;
    cXyz temp2;

    temp = position - home.pos;
    if (temp.absXZ() > DUSK_IF_ELSE(2500.0f * sizeMultiplier, 2500.0f)) {
        setActionMode(ACTION_BACK);
        return;
    }

    bool bVar = false;
    if (dComIfGp_getAttention()->LockonTruth() && dComIfGp_getAttention()->LockonTarget(0) == this)
    {
        bVar = true;
    }

    switch (mMode) {
    case 0:
        field_0xb9e = 0;
#if TARGET_PC  // enemy attribute integration
        field_0xb8e[0] = actor_attr::enemy_sync_timer(this, cM_rndF(100.0f) + 100.0f);
        field_0xb8a = (s16)(260892.0f / (l_HIO.attack_turn_radius * sizeMultiplier) + cM_rndFX(128.0f));
#else
        field_0xb8e[0] = (s16)(cM_rndF(100.0f) + 100.0f);
        field_0xb8a = (s16)(260892.0f / l_HIO.attack_turn_radius + cM_rndFX(128.0f));
#endif
        if (cM_rnd() < 0.5f) {
            field_0xb8a = -field_0xb8a;
        }
        mMode = 1;
        mAnmChangeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(30.0f) + 30.0f), (s16)(cM_rndF(30.0f) + 30.0f));
        bckSet(7, 3.0f, 2, 1.0f);
        attention_info.distances[fopAc_attn_BATTLE_e] = 4;
        field_0xb5c = 0.0f;
        /* fallthrough */

    case 1:
        if (field_0xb8e[1] == 0 && getMoveType() == 0) {
            temp2 = current.pos - home.pos;
            if (temp2.absXZ() > DUSK_IF_ELSE(1000.0f * sizeMultiplier, 1000.0f)) {
                setActionMode(ACTION_BACK);
                return;
            }
        }
        switch (mSubMode) {
        case 0:
            field_0xb5c += DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 4.0f), 4.0f);
            if (field_0xb5c >= 0.0f) {
                bckSet(10, 10.0f, 2, 1.0f);
                mAnmChangeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(50.0f) + 50.0f), (s16)(cM_rndF(50.0f) + 50.0f));
                mSubMode = 1;
            }

            break;
        case 1:
            field_0xb5c -= DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 3.0f), 3.0f);
            if (mAnmChangeTimer == 0) {
                bckSet(7, 10.0f, 2, 1.0f);
                mSubMode = 0;
            }
            break;

        case 2:
            bckSet(8, 3.0f, 0, 1.0f);
            mSubMode = 3;
            /* fallthrough */

        case 3:
            mSound.startCreatureSoundLevel(Z2SE_EN_GE_GLIDE, 0, -1);
            if (mpMorfSO->checkFrame(12.0f) || mpMorfSO->checkFrame(19.0f) ||
                mpMorfSO->checkFrame(26.0f))
            {
                mSound.startCreatureSound(Z2SE_EN_GE_WING, 0, -1);
            }
            if (mpMorfSO->isStop()) {
                bckSet(7, 3.0f, 2, 1.0f);
                mSubMode = 0;
            }
            break;
        }

        if (checkAttackPossible(distAngleS, bVar)) {
            bckSet(4, 10.0f, 0, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_GE_V_ATTACK, -1);
            field_0xb4c = position;
            field_0xb8c += 0x8000;
            mMode = 2;
            mSubMode = 0;
            field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 200.0f), 200);
            mCircleFlyScale = 14;
            return;
        }
#if TARGET_PC  // enemy attribute integration
        flyVec.set(0.0f, e_ge_hover_offset(this, l_HIO.attack_turn_altitude) + field_0xb5c, l_HIO.attack_turn_radius * sizeMultiplier);
#else
        flyVec.set(0.0f, l_HIO.attack_turn_altitude + field_0xb5c, l_HIO.attack_turn_radius);
#endif
        if (checkCircleSpeedAdd(&position, &flyVec)) {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0xb8c , field_0xb8a);
#else
            field_0xb8c += field_0xb8a;
#endif
        }
        calcCircleFly(&position, &flyVec, field_0xb8c, l_HIO.turning_speed, 6, 1.0f);
        break;

    case 2:
        if (mSubMode == 0) {
            if (mpMorfSO->checkFrame(3.0f)) {
                mSound.startCreatureVoice(Z2SE_EN_GE_V_NAKU, -1);
            }
            if (mpMorfSO->isStop()) {
                bckSet(10, 3.0f, 2, 1.0f);
                mSubMode = 1;
            }
        }
        mSphere.OnAtSetBit();
        flyVec.set(0.0f, 0.0f, 0.0f);

        temp = calcCircleFly(&field_0xb4c, &flyVec, 0x0, l_HIO.attack_speed, mCircleFlyScale, 1.0f);
        if (temp.abs() > DUSK_IF_ELSE(300.0f * sizeMultiplier, 300.0f)) {
            field_0xb4c = position;
            field_0xb8c = shape_angle.y;
#if TARGET_PC  // enemy attribute integration
            if (temp.y >= 200.0f * sizeMultiplier) {
                speed.y -= actor_attr::enemy_move_acceleration(this, l_HIO.add_attack_descent_speed);
#else
            if (temp.y >= 200.0f) {
                speed.y -= l_HIO.add_attack_descent_speed;
#endif
            }
            if (distAngleS < (s16)(l_HIO.attack_start_angle - 4096.0f) && !bVar) {
                field_0xb8e[0] = 0;
            }
        } else {
            mCircleFlyScale = 8;
        }

        if (temp.abs() < DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f) || field_0xb8e[0] == 0 || checkBeforeBg(1, 0.0f)) {
            bckSet(7, 1.0f, 2, 1.0f);
            mMode = 3;
            field_0xb8e[0] = 0;
        }
        break;

    case 3:
        mSphere.OnAtSetBit();
#if TARGET_PC  // enemy attribute integration
        flyVec.set(0.0f, e_ge_hover_offset(this, l_HIO.attack_turn_altitude), l_HIO.attack_turn_radius * sizeMultiplier);
#else
        flyVec.set(0.0f, l_HIO.attack_turn_altitude, l_HIO.attack_turn_radius);
#endif
        // I don't think this cXyz is correct in debug but abs() and y are needed from this
        // variable.
        temp = calcCircleFly(&position, &flyVec, field_0xb8c, l_HIO.attack_speed, 6, 0.5f);
        if (temp.abs() < DUSK_IF_ELSE(400.0f * sizeMultiplier, 400.0f) || mObjAcch.ChkWallHit()) {
            mMode = 0;
        }
        break;
    }
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, cM_atan2s(speedF, speed.y) - 0x4000, 0x180), cLib_chaseAngleS(&shape_angle.x, cM_atan2s(speedF, speed.y) - 0x4000, 0x180));
}

void daE_GE_c::setBackAnime(int param_0) {
    if (speed.y > 0.0f || mBackAnimeTimer != 0) {
        J3DAnmTransform* anm = (J3DAnmTransform*)dComIfG_getObjectRes("E_GE", 7);
        if (mpMorfSO->getAnm() != anm) {
            bckSet(7, 3.0f, 2, 1.0f);
        }
    } else if (speed.y < 0.0f) {
        if (param_0) {
            bckSet(6, 20.0f, 2, 1.0f);
        } else {
            bckSet(10, 20.0f, 2, 1.0f);
        }
    }

    if (bckCheck(6)) {
        mSound.startCreatureSoundLevel(Z2SE_EN_GE_GLIDE, 0, -1);
    }
}

void daE_GE_c::executeBack() {
    cXyz position;
    cXyz position2;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    s16 chaseAngle = 0x0;
    s16 distAngleS;

    switch (mMode) {
    case 0:
    case 10:
        field_0xb9e = 0;
        if (getMoveType() != 0) {
            setActionMode(ACTION_FLY);
        } else {
            if (mMode == 10) {
                bckSet(7, 10.0f, 2, 1.0f);
            } else {
                bckSet(7, 3.0f, 2, 1.0f);
            }
            if (current.pos.absXZ(home.pos) < DUSK_IF_ELSE(1500.0f * sizeMultiplier, 1500.0f)) {
                mMode = 1;
                field_0xb8a = 32;
            } else {
                mMode = 2;
            }
            field_0xb58 = l_HIO.turning_speed;
            field_0xb8c = cLib_targetAngleY(&home.pos, &current.pos);
        }
        break;

    case 1:
        setBackAnime(0);

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_s16(this, &field_0xb8a, 8, 1);
#else
        if (field_0xb8a > 8) {
            field_0xb8a--;
        }
#endif

#if TARGET_PC  // enemy attribute integration
        position2.set(0.0f, e_ge_hover_offset(this, 200.0f), 1000.0f * sizeMultiplier);
#else
        position2.set(0.0f, 200.0f, 1000.0f);
#endif
        position =
            calcCircleFly(&home.pos, &position2, field_0xb8c, field_0xb58, field_0xb8a, 1.0f);

        distAngleS =
            cLib_distanceAngleS(cLib_targetAngleY(&current.pos, &home.pos), shape_angle.y);
#if TARGET_PC  // enemy attribute integration
        if (position.y > 100.0f * sizeMultiplier || mObjAcch.ChkWallHit() || distAngleS >= 0x5000) {
            actor_attr::enemy_angle_add(this, field_0xb8c , 0x190);
#else
        if (position.y > 100.0f || mObjAcch.ChkWallHit() || distAngleS >= 0x5000) {
            field_0xb8c += 0x190;
#endif
        }

        if (position.abs() < DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f)) {
            mMode = 2;
        }

        chaseAngle = cM_atan2s(speedF, speed.y) - 0x4000;
        break;

    case 2:
        if (mSubMode == 0) {
            s16 targetAngleY = cLib_targetAngleY(&current.pos, &home.pos);
            if ((s16)cLib_distanceAngleS(targetAngleY, shape_angle.y) <= 0x800) {
                field_0xb8c = targetAngleY + 0x8000;
                mSubMode = 1;
            }
        }
#if TARGET_PC  // enemy attribute integration
        position2.set(0.0f, e_ge_hover_offset(this, 100.0f), 400.0f * sizeMultiplier);
#else
        position2.set(0.0f, 100.0f, 400.0f);
#endif
        position = calcCircleFly(&home.pos, &position2, field_0xb8c, field_0xb58, 0x10, 1.0f);

        if (DUSK_IF_ELSE(position.y < 150.0f * sizeMultiplier && position.abs() < 700.0f * sizeMultiplier, position.y < 150.0f && position.abs() < 700.0f)) {
            setBackAnime(1);
        } else {
            setBackAnime(0);
        }

        if (position.abs() < DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f)) {
            mMode = 3;
            bckSet(0xB, 3.0f, 0, 1.0f);
            field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10.0f), 10);
            speed.y = 0.0f;
            field_0xb9e = 1;
        }

        chaseAngle = cM_atan2s(speedF, speed.y) - 0x4000;
        break;

    case 3:
        if (mpMorfSO->checkFrame(5.0f)) {
            mpMorfSO->setPlaySpeed(0.0f);
        }

        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0xb58, 15.0f, 1.0f), cLib_chaseF(&field_0xb58, 15.0f, 1.0f));

#if TARGET_PC  // enemy attribute integration
        position2.set(0.0f, e_ge_hover_offset(this, 140.0f), 200.0f * sizeMultiplier);
#else
        position2.set(0.0f, 140.0f, 200.0f);
#endif
        position = calcCircleFly(&home.pos, &position2, field_0xb8c, field_0xb58, 4, 1.0f);

        if (position.abs() < DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f)) {
            mMode = 4;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMorfSO, 1.0f), mpMorfSO->setPlaySpeed(1.0f));
        }
        break;

    case 4:
        if (mpMorfSO->checkFrame(17.0f) || mpMorfSO->checkFrame(24.0f)) {
            mSound.startCreatureSound(Z2SE_EN_GE_WING, 0, -1);
        }

        s16 oldShapeAngleY = shape_angle.y;

        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0xb58, 0.0f, 1.0f), cLib_chaseF(&field_0xb58, 0.0f, 1.0f));

        position2.set(0.0f, 0.0f, 0.0f);
        calcCircleFly(&home.pos, &position2, field_0xb8c, field_0xb58, 4, 1.0f);
        shape_angle.y = current.angle.y = oldShapeAngleY;

        if (DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &current.pos, home.pos, 0.1f, 15.0f - field_0xb58, 1.0f) < 4.0f * sizeMultiplier, cLib_addCalcPos(&current.pos, home.pos, 0.1f, 15.0f - field_0xb58, 1.0f) < 4.0f)) {
            if (mpMorfSO->isStop()) {
                old.pos = home.pos;
                current.pos = old.pos;
                if (mPrevActionMode == ACTION_CAW) {
                    setActionMode(ACTION_CAW);
                    field_0xb8e[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100.0f), 100);
                    return;
                }

                setActionMode(ACTION_WAIT);
                return;
            }
        }

        break;
    }
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, chaseAngle, 0x180), cLib_chaseAngleS(&shape_angle.x, chaseAngle, 0x180));
}

void daE_GE_c::executeDown() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
    mDamageCooldownTimer = actor_attr::enemy_sync_timer(this, 10.0f);
#else
    mDamageCooldownTimer = 10;
#endif

    if (speed.y >= 0.0f) {
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -50.0f, 5.0f), cLib_chaseF(&speed.y, -50.0f, 5.0f));
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -50.0f, 3.0f), cLib_chaseF(&speed.y, -50.0f, 3.0f));
    }

    switch (mMode) {
    case 0:
#if !DEBUG
        mSphere.OffTgSetBit();
        mSphere2.OffTgSetBit();
#endif
        fopAcM_OffStatus(this, fopAcStts_UNK_0x20_e);
        field_0xb9e = 0;
        attention_info.flags = 0;
        bckSet(4, 3.0f, 2, 1.0f);
        mMode = 1;
        mSound.startCreatureVoice(Z2SE_EN_GE_V_DEATH, -1);
        field_0xb8a = 0;
        current.angle.y = (s16)((mAtInfo.mHitDirection.y + 0x8000) + cM_rndFX(4096.0f));
        if (mMoveType != 2) {
            field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x96), 0x96);
        } else {
            field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x5A), 0x5A);
        }
        /* fallthrough */

    case 1:
        if (field_0xb8e[0] == 0) {
            fopAcM_createDisappear(this, &current.pos, DUSK_IF_ELSE(actor_attr::byte_value(10.0f * sizeMultiplier), 10), 0, 0x21);
            fopAcM_delete(this);
            dComIfGs_onSwitch(mSwitch, fopAcM_GetRoomNo(this));
        } else {
            if (mObjAcch.ChkGroundHit()) {
                mSound.startCreatureSound(Z2SE_CM_BODYFALL_S, 0, -1);
                fopAcM_effSmokeSet1(&mSmokeKey, &mSmokeKey2, &current.pos, NULL, DUSK_IF_ELSE(sizeMultiplier, 1.0f), &tevStr, 1);
                field_0xb8e[0] = 0;
                mMode = 2;
                speed.y = 20.0f;
                speedF = 5.0f;
                shape_angle.x = -0x8000;
                shape_angle.z = 0x0;
                mObjAcch.SetGroundUpY(DUSK_IF_ELSE(20.0f * sizeMultiplier, 20.0f));
            }
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, shape_angle.z , 0x800);
            actor_attr::enemy_add_action_angle(this, &shape_angle.x, 0x4000, 8, 0x1000, 0x10);
#else
            shape_angle.z += 0x800;
            cLib_addCalcAngleS(&shape_angle.x, 0x4000, 8, 0x1000, 0x10);
#endif
        }
        break;

    case 2:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.x, -0x8000, 8, 0x1000, 0x10), cLib_addCalcAngleS(&shape_angle.x, -0x8000, 8, 0x1000, 0x10));
        if (mObjAcch.ChkGroundHit()) {
            mSound.startCreatureSound(Z2SE_CM_BODYFALL_S, 0, -1);
            fopAcM_effSmokeSet1(&mSmokeKey, &mSmokeKey2, &current.pos, NULL, DUSK_IF_ELSE(sizeMultiplier, 1.0f), &tevStr, 1);
            speed.y = 0.0f;
            speedF = 0.0f;
            mMode = 3;
            mpMorfSO->setFrame(16.0f);
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMorfSO, 0.5f), mpMorfSO->setPlaySpeed(0.5f));
        }
        break;

    case 3:
        if (mpMorfSO->checkFrame(23.0f)) {
            field_0xb8e[0] = 0;
            mMode = 4;
        }
        break;

    case 4:
        if (field_0xb8e[0] == 0) {
            fopAcM_createDisappear(this, &current.pos, DUSK_IF_ELSE(actor_attr::byte_value(10.0f * sizeMultiplier), 10), 0, 0x21);
            fopAcM_delete(this);
            dComIfGs_onSwitch(mSwitch, fopAcM_GetRoomNo(this));
        }
        break;
    }
}

void daE_GE_c::setSurpriseTime(s16 i_surpriseTime) {
    if (getMoveType() == 0 && (mActionMode == ACTION_WAIT || mActionMode == ACTION_CAW)) {
        mSurpriseTime = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, i_surpriseTime), i_surpriseTime);
    }
}

void daE_GE_c::setSurprise() {
    if (getMoveType() == 0 && (mActionMode == ACTION_WAIT || mActionMode == ACTION_CAW)) {
        setActionMode(ACTION_SURPRISE);
    }
    fpcM_Search(s_ge_surprise, this);
}

void daE_GE_c::executeSurprise() {
    cXyz vec;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    switch (mMode) {
    case 0:
        field_0xb9e = 0;
        bckSet(8, 3.0f, 0, 1.0f);
        field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(100.0f) + 150.0f), (s16)(cM_rndF(100.0f) + 150.0f));
        mMode = 1;
        field_0xb8c = (s16)cM_rndFX(32768.0f);
#if TARGET_PC  // enemy attribute integration
        vec.set(0.0f, e_ge_hover_offset(this, cM_rndFX(300.0f) + 800.0f), cM_rndFX(1500.0f) * sizeMultiplier);
#else
        vec.set(0.0f, cM_rndFX(300.0f) + 800.0f, cM_rndFX(1500.0f));
#endif
        cLib_offsetPos(&field_0xb4c, &home.pos, (s16)cM_rndFX(32768.0f), &vec);
        /* fallthrough */

    case 1:
        if (bckCheck(8)) {
            mSound.startCreatureSoundLevel(Z2SE_EN_GE_GLIDE, 0, -1);
            if (mpMorfSO->checkFrame(12.0f) || mpMorfSO->checkFrame(19.0f) ||
                mpMorfSO->checkFrame(26.0f))
            {
                mSound.startCreatureSound(Z2SE_EN_GE_WING, 0, -1);
            }
            if (mpMorfSO->isStop()) {
                bckSet(7, 3.0f, 2, 1.0f);
            }
        }

        vec.set(0.0f, 0.0f, DUSK_IF_ELSE(700.0f * sizeMultiplier, 700.0f));
        calcCircleFly(&field_0xb4c, &vec, field_0xb8c, l_HIO.turning_speed, 6, 1.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_angle_add(this, field_0xb8c , 0x258);
#else
        field_0xb8c += 0x258;
#endif

        if (field_0xb8e[0] == 0) {
            if (searchNextAttacker()) {
                break;
            }
            if (daPy_getPlayerActorClass()->current.pos.absXZ(home.pos) > DUSK_IF_ELSE(1000.0f * sizeMultiplier, 1000.0f)) {
                if (mPrevActionMode == ACTION_CAW) {
                    mActionMode = ACTION_CAW;
                }
                setActionMode(ACTION_BACK);
                break;
            }
            field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(50.0f) + 70.0f), (s16)(cM_rndF(50.0f) + 70.0f));
            // fallthrough
        }
    default:
        s16 targetAngle = cM_atan2s(speedF, speed.y) - 0x4000;

        if (targetAngle < -6000) {
            targetAngle = -6000;
        }
        if (targetAngle > 6000) {
            targetAngle = 6000;
        }
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, targetAngle, 0x180), cLib_chaseAngleS(&shape_angle.x, targetAngle, 0x180));
        break;
    }
}

void daE_GE_c::setCaw() {
    if (getMoveType() == 0 && mActionMode == ACTION_WAIT) {
        setActionMode(ACTION_CAW);
        field_0xb8e[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(30.0f) + 40.0f), cM_rndFX(30.0f) + 40.0f);
    }
}

void daE_GE_c::executeCaw() {
    fpcM_Search(s_arrow_sub, this);

    daPy_py_c* player = daPy_getPlayerActorClass();
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    // Also not sure if this is in debug since debug doesn't show a ctor
    cXyz diff = player->current.pos - home.pos;

    if (diff.absXZ() > DUSK_IF_ELSE(2000.0f * sizeMultiplier, 2000.0f)) {
        setActionMode(ACTION_WAIT);
    } else if (getMoveType() == 0 && diff.absXZ() < DUSK_IF_ELSE(500.0f * sizeMultiplier, 500.0f)) {
        setActionMode(ACTION_SURPRISE);
    } else {
        switch (mMode) {
        case 0:
            if (field_0xb8e[1] == 0) {
                bckSet(5, 3.0f, 0, 1.0f);
                mMode = 1;
                if (mSubMode == 0) {
                    fpcM_Search(s_ge_caw, this);
                    mSubMode = 1;
                }
            }
            break;

        case 1:
            if (mpMorfSO->checkFrame(6.0f)) {
                mSound.startCreatureVoice(Z2SE_EN_GE_V_NAKU, -1);
            }
            if (mpMorfSO->isStop()) {
                setActionMode(ACTION_ATTACK);
                field_0xb8e[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(300.0f) + 700.0f), (s16)(cM_rndF(300.0f) + 700.0f));
                mSubMode = 2;
            } else if (field_0xb8e[0] == 0) {
                mMode = 0;
                field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(50.0f) + 50.0f), (s16)(cM_rndFX(50.0f) + 50.0f));
            }
            break;
        }
    }
}

void daE_GE_c::executeWind() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    if (daPy_py_c::getThrowBoomerangActor() == NULL && mMode != 2) {
        mMode = 2;
        field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(30.0f) + 60.0f), (s16)(cM_rndFX(30.0f) + 60.0f));
    }

    switch (mMode) {
    case 0: {
        field_0xb9e = 0;
        cXyz boomerangPos(daPy_py_c::getThrowBoomerangActor()->current.pos);
        field_0xb58 = current.pos.absXZ(boomerangPos);
        field_0xb5c = current.pos.y - boomerangPos.y;
        speedF = speed.y = 0.0f;
        mMode = 1;
        bckSet(9, 3.0f, 2, 1.0f);
#if TARGET_PC  // enemy attribute integration
        field_0xb64 = cM_rndFX(50.0f) * sizeMultiplier;
        field_0xb60 = cM_rndFX(100.0f) * sizeMultiplier;
#else
        field_0xb64 = cM_rndFX(50.0f);
        field_0xb60 = cM_rndFX(100.0f);
#endif
    }
        /* fallthrough */

    case 1: {
        if (mpMorfSO->checkFrame(0.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_GE_V_FURA, -1);
        }

        cXyz boomerangPos2(daPy_py_c::getThrowBoomerangActor()->current.pos);
        DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0xb8c, 0x800), ANGLE_ADD(field_0xb8c, 0x800));
        current.pos.x = boomerangPos2.x + field_0xb58 * cM_ssin(field_0xb8c);
        current.pos.z = boomerangPos2.z + field_0xb58 * cM_scos(field_0xb8c);
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0xb58, field_0xb60, 2.0f), cLib_chaseF(&field_0xb58, field_0xb60, 2.0f));
        current.pos.y = boomerangPos2.y + field_0xb5c;

#if TARGET_PC  // enemy attribute integration
        if (field_0xb5c < 50.0f * sizeMultiplier) {
            actor_attr::enemy_chase_action_float(this, &field_0xb5c, field_0xb64, 10.0f);
#else
        if (field_0xb5c < 50.0f) {
            cLib_chaseF(&field_0xb5c, field_0xb64, 10.0f);
#endif
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0xb5c, field_0xb64, 5.0f), cLib_chaseF(&field_0xb5c, field_0xb64, 5.0f));
        }

        field_0xb8a = 0x3000;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_angle_add(this, shape_angle.y , field_0xb8a);
#else
        shape_angle.y += field_0xb8a;
#endif
        break;
    }

    case 2:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(this, &field_0xb8a, 0, 4, 0x180);
        actor_attr::enemy_angle_add(this, shape_angle.y , field_0xb8a);
#else
        cLib_addCalcAngleS2(&field_0xb8a, 0, 4, 0x180);
        shape_angle.y += field_0xb8a;
#endif
        if (field_0xb8e[0] == 0) {
            if (getMoveType() != 0) {
                setActionMode(ACTION_FLY);
                mMode = 10;
                break;
            }
            IF_NOT_DUSK(mBackAnimeTimer = 90;)
            setActionMode(ACTION_BACK);
            mMode = 10;
            IF_DUSK(mBackAnimeTimer = actor_attr::enemy_sync_timer(this, 90.0f);)
        }
    }
}

void daE_GE_c::executeShield() {
    switch (mMode) {
    case 0:
        field_0xb9e = 0;
        speedF = 20.0f;
        speed.y = 0.0f;
        mMode = 1;
        bckSet(9, 3.0f, 2, 1.0f);
        field_0xb8e[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60.0f), 60);
        current.angle.y = fopAcM_searchPlayerAngleY(this) + 0x8000;
        /* fallthrough */
    case 1:
        if (mpMorfSO->checkFrame(0.0f)) {
            mSound.startCreatureVoice(Z2SE_EN_GE_V_FURA, -1);
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 1.5f);
        actor_attr::enemy_chase_action_float(this, &speed.y, 0.0f, 1.0f);
        actor_attr::enemy_add_action_angle(this, &shape_angle.y, current.angle.y + 0x8000, 8, 0x400, 0x80);
#else
        cLib_chaseF(&speedF, 0.0f, 1.5f);
        cLib_chaseF(&speed.y, 0.0f, 1.0f);
        cLib_addCalcAngleS(&shape_angle.y, current.angle.y + 0x8000, 8, 0x400, 0x80);
#endif

        if (field_0xb8e[0] == 0) {
            if (getMoveType() != 0) {
                setActionMode(ACTION_FLY);
                mMode = 10;
            } else {
                IF_NOT_DUSK(mBackAnimeTimer = 90;)
                setActionMode(ACTION_BACK);
                mMode = 10;
                IF_DUSK(mBackAnimeTimer = actor_attr::enemy_sync_timer(this, 90.0f);)
            }
        }

        break;
    }
}

DUSK_GAME_DATA daE_GE_c::actionFunc daE_GE_c::l_actionmenu[9] = {
    (&daE_GE_c::executeWait), (&daE_GE_c::executeFly),  (&daE_GE_c::executeAttack),
    (&daE_GE_c::executeBack), (&daE_GE_c::executeDown), (&daE_GE_c::executeSurprise),
    (&daE_GE_c::executeCaw),  (&daE_GE_c::executeWind), (&daE_GE_c::executeShield),
};

void daE_GE_c::action() {
    mSphere.OffAtSetBit();
    damage_check();
    s16 angleY = shape_angle.y;

    if (mActionMode < ACTION_MAX) {
        (this->*l_actionmenu[mActionMode])();
    }

    if (bckCheck(7) && mpMorfSO->checkFrame(0.0f)) {
        mSound.startCreatureSound(Z2SE_EN_GE_WING, 0, -1);
    }

    if (mActionMode == ACTION_ATTACK) {
        mSound.setLinkSearch(true);
    } else {
        mSound.setLinkSearch(false);
    }

    if (mActionMode != ACTION_DOWN) {
        f32 actor_speed = speedF;
        if (actor_speed >= 20.0f) {
            actor_speed = 20.0f;
        }

        s16 targetAngle = (angleY - shape_angle.y) * 7;
        targetAngle = targetAngle * actor_speed / 20.0f;

        if (targetAngle < -0x2000) {
            targetAngle = -0x2000;
        }
        if (targetAngle > 0x2000) {
            targetAngle = 0x2000;
        }
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.z, targetAngle, 0x400), cLib_chaseAngleS(&shape_angle.z, targetAngle, 0x400));
    }

    cXyz speed_vec;
    cXyz mult_vec;

    mDoMtx_stack_c::YrotS(current.angle.y);
    speed_vec.set(0.0f, 0.0f, speedF);
    mDoMtx_stack_c::multVec(&speed_vec, &mult_vec);

    speed.x = mult_vec.x;
    speed.z = mult_vec.z;

    current.pos += DUSK_IF_ELSE(actor_attr::enemy_move_step(this, speed), speed);

    cXyz* ccMoveP = mStts.GetCCMoveP();
    if (ccMoveP != NULL) {
        current.pos.x += ccMoveP->x;
        current.pos.y += ccMoveP->y;
        current.pos.z += ccMoveP->z;
    }

    if (field_0xb9e == 0) {
        mObjAcch.CrrPos(dComIfG_Bgsp());
    }

    mpMorfSO->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

void daE_GE_c::mtx_set() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    IF_DUSK(mDoMtx_stack_c::scaleM(sizeMultiplier, sizeMultiplier, sizeMultiplier);)
    mpMorfSO->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorfSO->modelCalc();
}

void daE_GE_c::cc_set() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    MTXCopy(mpMorfSO->getModel()->getAnmMtx(2), mDoMtx_stack_c::get());

    mDoMtx_stack_c::multVecZero(&eyePos);

    if (mMoveType == 2) {
        mSphere.SetC(eyePos);
        mSphere.SetR(DUSK_IF_ELSE(30.0f * sizeMultiplier, 30.0f));
        dComIfG_Ccsp()->Set(&mSphere);
        mSphere2.SetC(eyePos);

        if (mActionMode == ACTION_WAIT || mActionMode == ACTION_CAW) {
            mSphere2.SetR(DUSK_IF_ELSE(30.0f * sizeMultiplier, 30.0f));
        } else {
            mSphere2.SetR(DUSK_IF_ELSE(70.0f * sizeMultiplier, 70.0f));
        }
        dComIfG_Ccsp()->Set(&mSphere2);
    } else {
        mSphere.SetC(eyePos);
        mSphere.SetR(DUSK_IF_ELSE(35.0f * sizeMultiplier, 35.0f));
        dComIfG_Ccsp()->Set(&mSphere);
        mSphere2.SetC(eyePos);

        if (mActionMode == ACTION_WAIT || mActionMode == ACTION_CAW) {
            mSphere2.SetR(DUSK_IF_ELSE(35.0f * sizeMultiplier, 35.0f));
        } else {
            mSphere2.SetR(DUSK_IF_ELSE(70.0f * sizeMultiplier, 70.0f));
        }
        dComIfG_Ccsp()->Set(&mSphere2);
    }
}

int daE_GE_c::execute() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    for (int i = 0; i < 2; i++) {
        if (field_0xb8e[i] != 0) {
            field_0xb8e[i]--;
        }
    }

    if (mDamageCooldownTimer != 0) {
        mDamageCooldownTimer--;
    }

    if (mAnmChangeTimer != 0) {
        mAnmChangeTimer--;
    }

    if (mSurpriseTime != 0) {
        mSurpriseTime--;
        if (mSurpriseTime == 0) {
            setSurprise();
        }
    }

    if (mBackAnimeTimer != 0) {
        mBackAnimeTimer--;
    }

    action();
    mtx_set();
    cc_set();
    attention_info.position = eyePos;
    attention_info.position.y += DUSK_IF_ELSE(30.0f * sizeMultiplier, 30.0f);

    cXyz effect_pos(current.pos);
#if TARGET_PC  // enemy attribute integration
    effect_pos.y += 50.0f * sizeMultiplier;
    cXyz size(sizeMultiplier, sizeMultiplier, sizeMultiplier);
#else
    effect_pos.y += 50.0f;
    cXyz size(1.0f, 1.0f, 1.0f);
#endif
    setMidnaBindEffect(this, &mSound, &effect_pos, &size);

    return 1;
}

static int daE_GE_Execute(daE_GE_c* i_this) {
    return i_this->execute();
}

static int daE_GE_IsDelete(daE_GE_c* i_this) {
    return 1;
}

int daE_GE_c::_delete() {
    dComIfG_resDelete(&mPhaseReq, "E_GE");

    if (mHIOInit) {
        hio_set = FALSE;
        mDoHIO_DELETE_CHILD(l_HIO.id);
    }

    if (heap != NULL) {
        mSound.deleteObject();
    }

    return 1;
}

static int daE_GE_Delete(daE_GE_c* i_this) {
    return i_this->_delete();
}

int daE_GE_c::CreateHeap() {
    mpMorfSO = JKR_NEW mDoExt_McaMorfSO((J3DModelData*)dComIfG_getObjectRes("E_GE", 0xF), NULL, NULL,
                                    (J3DAnmTransform*)dComIfG_getObjectRes("E_GE", 0xC), 0, 1.0f, 0,
                                    -1, &mSound, 0x80000, 0x11000084);

    if (mpMorfSO == NULL || mpMorfSO->getModel() == NULL) {
        return 0;
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    return static_cast<daE_GE_c*>(i_this)->CreateHeap();
}

int daE_GE_c::create() {
    static dCcD_SrcSph cc_sph_src = {
        {
            {0x0, {{AT_TYPE_CSTATUE_SWING, 0x1, 0xd}, {0xd8fbfdff, 0x3}, 0x75}},  // mObj
            {dCcD_SE_METAL, 0x0, 0x0, 0x0, 0x0},                                  // mGObjAt
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x22},                                  // mGObjTg
            {0x0},                                                                // mGObjCo
        },                                                                        // mObjInf
        {
            {{0.0f, 0.0f, 0.0f}, 40.0f}  // mSph
        }  // mSphAttr
    };

    fopAcM_ct(this, daE_GE_c);

    mSwitch = fopAcM_GetParam(this) >> 8;
    if (mSwitch != 0xFF && dComIfGs_isSwitch(mSwitch, fopAcM_GetRoomNo(this))) {
        // "E_GE I've been defeated so I won't reset it\n"
        OS_REPORT("E_GE やられ後なので再セットしません\n");
        return cPhs_ERROR_e;
    }

    mMoveType = fopAcM_GetParam(this);
    if (mMoveType == 0xFF) {
        mMoveType = 0;
    }

    mGroupNo = fopAcM_GetParam(this) >> 0x10;

    int phase = dComIfG_resLoad(&mPhaseReq, "E_GE");
    if (phase == cPhs_COMPLEATE_e) {
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
        OS_REPORT("E_GE PARAM %x\n", fopAcM_GetParam(this));

        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x1340)) {
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            mHIOInit = true;
            hio_set = TRUE;
            l_HIO.id = mDoHIO_CREATE_CHILD("グェー", &l_HIO);
        }

        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        fopAcM_SetMtx(this, mpMorfSO->getModel()->getBaseTRMtx());
#if TARGET_PC  // enemy attribute integration
        fopAcM_SetMin(this, -200.0f * sizeMultiplier, -200.0f * sizeMultiplier, -200.0f * sizeMultiplier);
        fopAcM_SetMax(this, 200.0f * sizeMultiplier, 200.0f * sizeMultiplier, 200.0f * sizeMultiplier);
#else
        fopAcM_SetMin(this, -200.0f, -200.0f, -200.0f);
        fopAcM_SetMax(this, 200.0f, 200.0f, 200.0f);
#endif
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir,
                     fopAcM_GetSpeed_p(this), NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        mAcchCir.SetWall(0.0f, 50.0f * sizeMultiplier);
        mObjAcch.SetGroundUpY(30.0f * sizeMultiplier);
        health = actor_attr::enemy_health_value(this, 10.0f);
        field_0x560 = health;
#else
        mAcchCir.SetWall(0.0f, 50.0f);
        mObjAcch.SetGroundUpY(30.0f);
        health = 10;
        field_0x560 = 10;
#endif
        mStts.Init(100, 0, this);
        mSphere.Set(cc_sph_src);
        mSphere.SetStts(&mStts);
        IF_DUSK(mSphere.SetAtAtp(actor_attr::enemy_attack_power_byte(this, 1.0f));)
        if (mMoveType != 2) {
            mSphere.SetTgType(0xd8fbdddf);
        } else {
            mSphere.SetTgType(0xd8fadddf);
        }
#if !DEBUG
        mSphere.OnTgNoHitMark();
#endif
        mSphere2.Set(cc_sph_src);
        mSphere2.SetStts(&mStts);
        mSphere2.SetTgType(0x2020);
        mSphere2.OnTgNoHitMark();
        mSphere2.OffCoSetBit();
        mSphere2.OffAtSetBit();
        mSound.init(&current.pos, &eyePos, 3, 1);
        mSound.setEnemyName("E_ge");
        mAtInfo.mpSound = &mSound;
        mAtInfo.mPowerType = 1;

        if (getMoveType() != 0) {
            home.pos.y = old.pos.y = current.pos.y;
            setActionMode(ACTION_FLY);
        } else {
            dBgS_GndChk gndChk;
            cXyz modified_home_pos(home.pos);
            modified_home_pos.y += DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f);
            gndChk.SetPos(&modified_home_pos);
            f32 groundCross = dComIfG_Bgsp().GroundCross(&gndChk);
#if TARGET_PC  // enemy attribute integration
            if (groundCross != -G_CM3D_F_INF && fabsf(current.pos.y - groundCross) < 200.0f * sizeMultiplier) {
                current.pos.y = old.pos.y = home.pos.y = groundCross + 30.0f * sizeMultiplier;
#else
            if (groundCross != -G_CM3D_F_INF && fabsf(current.pos.y - groundCross) < 200.0f) {
                current.pos.y = old.pos.y = home.pos.y = groundCross + 30.0f;
#endif
            }
            setActionMode(ACTION_WAIT);
        }
        IF_DUSK(actor_attr::enemy_set_animation_play_speed(this, mpMorfSO, 1.0f);)
        mtx_set();
    }
    return phase;
}

static int daE_GE_Create(daE_GE_c* i_this) {
    return i_this->create();
}

static DUSK_CONST actor_method_class l_daE_GE_Method = {
    (process_method_func)daE_GE_Create,  (process_method_func)daE_GE_Delete,
    (process_method_func)daE_GE_Execute, (process_method_func)daE_GE_IsDelete,
    (process_method_func)daE_GE_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_GE = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_GE_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daE_GE_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_GE_e,
    /* Actor SubMtd */ &l_daE_GE_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x10000_e | fopAcStts_CULL_e | fopAcStts_UNK_0x20_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

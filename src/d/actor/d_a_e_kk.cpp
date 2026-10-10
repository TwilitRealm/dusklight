/**
* @file d_a_e_kk.cpp
 *
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_e_kk.h"
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_item.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"
#endif
#include "f_op/f_op_actor_enemy.h"
#if TARGET_PC  // additional actor attribute integration

namespace actor_attr = dusk::mods::svc::actor_attr;
#endif

class daE_KK_HIO_c : public JORReflexible {
public:
    daE_KK_HIO_c();
    virtual ~daE_KK_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x04 */ s8 field_0x4;
    /* 0x08 */ f32 model_size;
    /* 0x0C */ f32 spear_throw_range;
    /* 0x10 */ f32 direct_attack_range;
    /* 0x14 */ f32 default_moving_range;
    /* 0x18 */ s16 escape_time;
    /* 0x1A */ u8 range_display;
};

enum daE_KK_Action {
    ACTION_WAIT,
    ACTION_ICICLEWAIT,
    ACTION_WALK,
    ACTION_SPEARTHROW,
    ACTION_BACKWALK,
    ACTION_YOROKE,
    ACTION_GUARD,
    ACTION_DAMAGE,
    ACTION_ATTACK,
    ACTION_WEAPONMOVE,
    ACTION_DEAD
};
#if TARGET_PC  // additional actor attribute integration

namespace dusk::mods::svc::actor_attr {

/* A thrown Chilfos spear keeps the attributes of the Chilfos that launched it. */
template <>
struct EnemyActorAccessor<daE_KK_c> {
    static fopAc_ac_c* get(daE_KK_c* i_this) {

        return i_this;

}
};

template <>
struct EnemyAttributeOwner<daE_KK_c> {
    static fopAc_ac_c* get(daE_KK_c* i_this) {

        if ((fopAcM_GetParam(i_this) & 0xFF) == 1) {
            fopAc_ac_c* parent = fopAcM_SearchByID(i_this->parentActorID);
            if (parent != NULL && fopAcM_GetName(parent) == fpcNm_E_KK_e) {
                return parent;
            }
        }

        return EnemyActorAccessor<daE_KK_c>::get(i_this);

}
};

}  // namespace dusk::mods::svc::actor_attr
#endif

namespace {
static dCcD_SrcCyl cc_kk_src = {
    {
        {0x0, {{0x0, 0x0, 0x0}, {0xD8FBF9FF, 0x43}, 0x75}},  // mObj
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                  // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x1404},               // mGObjTg
        {0x0},                                               // mGObjCo
    },                                                       // mObjInf
    {
        {
            {0.0f, 0.0f, 0.0f},  // mCenter
            40.0f,               // mRadius
            0.0f                 // mHeight
        }  // mCyl
    }
};

static dCcD_SrcSph cc_kk_at_src = {
    {
        {0x0, {{0x400, 0x1, 0xD}, {0xD8FBFDFF, 0x43}, 0x15}},  // mObj
        {dCcD_SE_NONE, 0x0, 0x1, 0x0, 0x0},                    // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x1404},                 // mGObjTg
        {0x0},                                                 // mGObjCo
    },                                                         // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f}  // mSph
    },                               // mSphAttr
};
}  // namespace

daE_KK_HIO_c::daE_KK_HIO_c() {
    field_0x4 = -1;
    model_size = 1.0f;
    spear_throw_range = 1500.0f;
    direct_attack_range = 900.0f;
    default_moving_range = 1000.0f;
    escape_time = 100;
    range_display = 0;
}

int daE_KK_c::ctrlJoint(J3DJoint* i_joint, J3DModel* i_model) {
    s32 jntNo = i_joint->getJntNo();
    mDoMtx_stack_c::copy(i_model->getAnmMtx(jntNo));

    switch (jntNo) {
    case 4:
        mDoMtx_stack_c::YrotM(field_0x758);

    default:
        i_model->setAnmMtx(jntNo, mDoMtx_stack_c::get());
        MTXCopy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
        break;
    }

    return 1;
}

int daE_KK_c::JointCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        J3DModel* model = j3dSys.getModel();
        daE_KK_c* kk_p = (daE_KK_c*)model->getUserArea();

        if (kk_p != NULL) {
            kk_p->ctrlJoint(i_joint, model);
        }
    }
    return 1;
}

int daE_KK_c::draw() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    if (field_0x679 == 2 && !checkItemGet(dItemNo_IRONBALL_e, 1)) {
        return 1;
    } else {
        cXyz position;
        position.set(current.pos.x, current.pos.y + DUSK_IF_ELSE(10.0f * sizeMultiplier, 10.0f), current.pos.z);

        if (field_0x679 != 1) {
            J3DModel* model = mpMorfSO->getModel();
            g_env_light.settingTevStruct(0, &current.pos, &tevStr);
            g_env_light.setLightTevColorType_MAJI(model, &tevStr);

            J3DShape* shape =
                mpMorfSO->getModel()->getModelData()->getMaterialNodePointer(2)->getShape();
            if (shape != NULL) {
                if (mActionMode == 10 && mMoveMode != 2) {
                    shape->hide();
                } else {
                    shape->show();
                }
            }

            fopAcM_setEffectMtx(this, model->getModelData());
            mpMorfSO->entryDL();

            u32 shadow =
                dComIfGd_setShadow(mModelShadow, 1, model, &position, DUSK_IF_ELSE(1000.0f * sizeMultiplier, 1000.0f), 0.0f, current.pos.y,
                                   mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0, 1.0f,
                                   dDlst_shadowControl_c::getSimpleTex());
            mModelShadow = shadow;
        }

        if (field_0x67d == 0) {
            J3DModel* weaponModel = mpWeaponMorfSO->getModel();
            fopAcM_setEffectMtx(this, weaponModel->getModelData());
            mpWeaponMorfSO->entryDL();

            g_env_light.settingTevStruct(0, &current.pos, &tevStr);
            g_env_light.setLightTevColorType_MAJI(weaponModel, &tevStr);

            u32 shadow =
                dComIfGd_setShadow(mWeaponShadow, 1, weaponModel, &position, DUSK_IF_ELSE(2000.0f * sizeMultiplier, 2000.0f), 0.0f,
                                   current.pos.y, mObjAcch.GetGroundH(), mObjAcch.m_gnd, &tevStr, 0,
                                   1.0f, dDlst_shadowControl_c::getSimpleTex());
            mWeaponShadow = shadow;
        }
    }
    return 1;
}

static int daE_KK_Draw(daE_KK_c* i_this) {
    return i_this->draw();
}

void daE_KK_c::setBck(int i_index, u8 i_attr, f32 i_morf, f32 i_rate) {
    J3DAnmTransform* animation = (J3DAnmTransform*)dComIfG_getObjectRes("E_KK", i_index);
#if TARGET_PC  // enemy attribute integration
    const actor_attr::ActionAnimationParams params = mActionMode == ACTION_YOROKE ? actor_attr::enemy_stun_animation_params(this, i_morf, i_rate) : actor_attr::enemy_animation_params(this, i_morf, i_rate);
    mpMorfSO->setAnm(animation, i_attr, params.morph, params.playbackSpeed, 0.0f, -1.0f);
#else
    mpMorfSO->setAnm(animation, i_attr, i_morf, i_rate, 0.0f, -1.0f);
#endif
    field_0x764 = i_index;
}

void daE_KK_c::setWeaponBck(int i_index, u8 i_attr, f32 i_morf, f32 i_rate) {
    J3DAnmTransform* animation = (J3DAnmTransform*)dComIfG_getObjectRes("E_KK", i_index);
    DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpWeaponMorfSO, animation, i_attr, i_morf, i_rate, 0.0f, -1.0f), mpWeaponMorfSO->setAnm(animation, i_attr, i_morf, i_rate, 0.0f, -1.0f));
    field_0x768 = i_index;
}

bool daE_KK_c::mCutTypeCheck(int param_0) {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);

    if (param_0 == 0 && player->getCutCount() >= 4) {
        return 1;
    }

    if (param_0 != 2) {
        u8 cutType = player->getCutType();
        if (cutType == daPy_py_c::CUT_TYPE_TURN_RIGHT || cutType == daPy_py_c::CUT_TYPE_JUMP ||
            cutType == daPy_py_c::CUT_TYPE_TWIRL || cutType == daPy_py_c::CUT_TYPE_TURN_LEFT)
        {
            return 1;
        }
    }
    if (player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_JUMP ||
        player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH ||
        player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT ||
        player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT ||
        player->getCutType() == daPy_py_c::CUT_TYPE_MORTAL_DRAW_A ||
        player->getCutType() == daPy_py_c::CUT_TYPE_MORTAL_DRAW_B)
    {
        return 1;
    }

    if (player->getCutType() == daPy_py_c::CUT_TYPE_HEAD_JUMP ||
        player->getCutType() == daPy_py_c::CUT_TYPE_TWIRL)
    {
        return 1;
    }

    return 0;
}

void daE_KK_c::setActionMode(int i_action, int i_mode) {
    mActionMode = i_action;
    mMoveMode = i_mode;
}

static u8 hio_set;

static daE_KK_HIO_c l_HIO;

void daE_KK_c::damage_check() {
    if (health <= DUSK_IF_ELSE(0, 1) || field_0x679 == 1 || mActionMode == 10) {
        return;
    }
    cXyz effPos;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
    cXyz effectScale = actor_attr::enemy_size_multiplier(this, l_HIO.model_size);
#endif
    mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(2));
    mDoMtx_stack_c::multVecZero(&effPos);
    setMidnaBindEffect(this, &mCreatureSound, &effPos, DUSK_IF_ELSE(&effectScale, &scale));

    if (mDamageTimer == 0) {
        if (mSph.ChkAtHit() && mSph.ChkAtShieldHit()) {
            mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 0x28);
            mSph.ClrAtHit();
            mDamageTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(this, 8.0f), 8);
            speedF = -70.0f;
        }

        for (int i = 0; i < 6; i++) {
            if (mSpheres[i].ChkAtHit() && mSpheres[i].ChkAtShieldHit()) {
                mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 0x28);
                mSpheres[i].ClrAtHit();
                mDamageTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(this, 8.0f), 8);
                speedF = -70.0f;
            }
        }

        if (field_0x67d == 0) {
            if (fopAcM_searchPlayerDistance(this) <= DUSK_IF_ELSE(l_HIO.direct_attack_range * sizeMultiplier, l_HIO.direct_attack_range)) {
                if (mCutTypeCheck(0)) {
                    mCyl.OnTgShield();
                } else {
                    mCyl.OffTgShield();
                }
            }
        }

        if (mCyl.ChkTgHit()) {
            cXyz position;
            position.set(current.pos);
#if TARGET_PC  // enemy attribute integration
            position.y += 100.0f * sizeMultiplier;
            cXyz hioScale = actor_attr::enemy_size_multiplier(this, l_HIO.model_size);
            const cXyz hitMarkScale = actor_attr::enemy_size_multiplier(this, 1.0f);
#else
            position.y += 100.0f;
            cXyz hioScale(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
#endif
            current.angle.y = shape_angle.y;
            mDamageTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(this, 8.0f), 8);
            mAtInfo.mpCollider = mCyl.GetTgHitObj();

            if (mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_HOOKSHOT) ||
                mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_ARROW) ||
                mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_BOOMERANG))
            {
                mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 0x28);
                dComIfGp_setHitMark(2, this, &position, &shape_angle, DUSK_IF_ELSE(&hitMarkScale, NULL), 0);
                setActionMode(5, 0);
            } else if (mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
                onHeadLockFlg();
                if (field_0x67d == 0) {
                    mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 0x28);
                    dComIfGp_setHitMark(2, this, &position, &shape_angle, DUSK_IF_ELSE(&hitMarkScale, NULL), 0);
                    setActionMode(5, 1);
                } else {
                    setActionMode(5, 0);
                }
            } else if ((mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_IRON_BALL) &&
                        !dComIfGp_checkPlayerStatus0(0, 0x400)) ||
                       mCutTypeCheck(2))
            {
                mCyl.OnTgNoHitMark();
                health = 0;
                dScnPly_c::setPauseTimer(5);
                mCreatureSound.startCollisionSE(Z2SE_HIT_HAMMER, 0x20);
                setActionMode(10, 0);
            } else {
                dComIfGp_particle_set(0x85BA, &position, &shape_angle, &hioScale);
                if (field_0x67d == 0) {
                    at_power_check(&mAtInfo);
                    if (mAtInfo.mHitStatus != 0 || dComIfGp_checkPlayerStatus0(0, 0x400) ||
                        mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_MIDNA_LOCK))
                    {
                        def_se_set(&mCreatureSound, mAtInfo.mpCollider, 0x1F, NULL);
                        setActionMode(7, 0);
                    } else {
                        mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 7);
                        dComIfGp_setHitMark(2, this, &position, &shape_angle, DUSK_IF_ELSE(&hitMarkScale, NULL), 0);
                        setActionMode(6, 0);
                    }
                } else {
                    cc_at_check(this, &mAtInfo);
                    if (health <= 1 || mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_BOMB)) {
                        if (field_0x67d == 0) {
                            Z2GetAudioMgr()->seStart(Z2SE_EN_KK_CRASH, &field_0x698, 0, 0, 1.0f,
                                                     1.0f, -1.0f, -1.0f, 0);
                        }
                        setActionMode(10, 0);
                        mCyl.ClrTgHit();
                        mStts.Move();
                        return;
                    }

                    mCyl.OffTgNoHitMark();
                    setActionMode(7, 1);
                    if (mCyl.GetTgHitObj()->ChkAtType(AT_TYPE_NORMAL_SWORD) && mCutTypeCheck(1)) {
                        mDamageTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(this, 8.0f), 8);
                        mCyl.ClrTgHit();
                        mCyl.OnTgStopNoConHit();
                        return;
                    }
                }
            }
        }
        mCyl.OffTgStopNoConHit();
        mStts.Move();
    }
}

void daE_KK_c::nextActionCheck() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
    const f32 playerDistance = fopAcM_searchPlayerDistance(this);
    const f32 noticeDistance = actor_attr::enemy_notice_distance(this, l_HIO.spear_throw_range);
    const f32 spearThrowDistance = l_HIO.spear_throw_range * sizeMultiplier;
    const f32 directAttackDistance = l_HIO.direct_attack_range * sizeMultiplier;
    const bool playerDetected = !dComIfGp_event_runCheck() && playerDistance <= noticeDistance && !fopAcM_otherBgCheck(this, player);

    if (playerDetected && playerDistance > directAttackDistance) {
        if (playerDistance <= spearThrowDistance && eyePos.y + 200.0f * sizeMultiplier >= player->current.pos.y)
#else
    if (!dComIfGp_event_runCheck() && fopAcM_searchPlayerDistance(this) > l_HIO.direct_attack_range &&
        fopAcM_searchPlayerDistance(this) <= l_HIO.spear_throw_range)
    {
        if (!fopAcM_otherBgCheck(this, dComIfGp_getPlayer(0)) &&
            eyePos.y + 200.0f >= player->current.pos.y)
#endif
        {
#if TARGET_PC  // enemy attribute integration
            setActionMode(ACTION_SPEARTHROW, 0);
            return;
        }

        if (playerDistance > spearThrowDistance) {
            field_0x67c = 0;
            setActionMode(ACTION_ATTACK, 4);
#else
            setActionMode(3, 0);
#endif
            return;
        }
    }
    if (DUSK_IF_ELSE(playerDetected && playerDistance <= directAttackDistance, !dComIfGp_event_runCheck() && fopAcM_searchPlayerDistance(this) <= l_HIO.direct_attack_range &&
        !fopAcM_otherBgCheck(this, dComIfGp_getPlayer(0))))
    {
        if (daPy_getPlayerActorClass()->getDamageWaitTimer() != 0 && mActionMode != 0) {
            setActionMode(DUSK_IF_ELSE(ACTION_WAIT, 0), 0);
            return;
        }
#if TARGET_PC  // enemy attribute integration

        if (!dComIfGp_checkPlayerStatus0(0, 0x100) && eyePos.y + 50.0f * sizeMultiplier > player->current.pos.y) {
            setActionMode(ACTION_ATTACK, 0);
            return;
#else
        if (!dComIfGp_event_runCheck()) {
            if (!fopAcM_otherBgCheck(this, dComIfGp_getPlayer(0)) &&
                !dComIfGp_checkPlayerStatus0(0, 0x100) && eyePos.y + 50.0f > player->current.pos.y)
            {
                setActionMode(8, 0);
                return;
            }
            setActionMode(3, 0);
#endif
        }
#if TARGET_PC  // enemy attribute integration

        setActionMode(ACTION_SPEARTHROW, 0);
        return;
    }

    if (field_0x672 == 0 && mActionMode != ACTION_WAIT) {
        setActionMode(ACTION_WAIT, 0);
    } else if (field_0x672 == 0 && mActionMode != ACTION_WALK) {
        setActionMode(ACTION_WALK, 0);
#else
    } else {
        if (field_0x672 == 0 && mActionMode != 0) {
            setActionMode(0, 0);
        } else if (field_0x672 == 0 && mActionMode != 2) {
            setActionMode(2, 0);
        }
#endif
    }
}

bool daE_KK_c::way_gake_check() {
    dBgS_GndChk gndChk;
    cXyz position;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    if (mObjAcch.ChkWallHit()) {
        return false;
    }

    s16 angle_edit = -0x2000;
    for (int i = 0; i < 3; i++, angle_edit += 0x2000) {
        mDoMtx_YrotS(*calc_mtx, current.angle.y + angle_edit);
        position.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
        position.y = 300.0f * sizeMultiplier;
        position.z = 200.0f * sizeMultiplier;
#else
        position.y = 300.0f;
        position.z = 200.0f;
#endif
        MtxPosition(&position, &field_0x6b0[i]);
        field_0x6b0[i] += current.pos;
        field_0x6d4[i].set(field_0x6b0[i]);
        gndChk.SetPos(&field_0x6d4[i]);
        field_0x6d4[i].y = dComIfG_Bgsp().GroundCross(&gndChk);

        if (field_0x6d4[i].y == -G_CM3D_F_INF) {
            field_0x6d4[i].y = current.pos.y;
            return true;
        }

        if (current.pos.y - field_0x6d4[i].y > DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f)) {
            return true;
        }
    }
    return false;
}

void daE_KK_c::mDeadEffSet(cXyz& param_0) {
#if TARGET_PC  // enemy attribute integration
    cXyz hioScale = actor_attr::enemy_size_multiplier(this, l_HIO.model_size);
#else
    cXyz hioScale(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
#endif

    dComIfGp_particle_set(0x85B8, &param_0, &shape_angle, &hioScale);
    dComIfGp_particle_set(0x85B9, &param_0, &shape_angle, &hioScale);

    mCreatureSound.startCreatureSound(Z2SE_EN_KK_DEAD, 0, -1);
}

void daE_KK_c::executeWait() {
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    switch (mMoveMode) {
    case 0:
        if (fopAcM_searchPlayerDistance(this) <= DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, l_HIO.direct_attack_range) && current.pos.y + 100.0f * sizeMultiplier, l_HIO.direct_attack_range &&
            current.pos.y + 100.0f) >= player->current.pos.y &&
            !fopAcM_otherBgCheck(this, dComIfGp_getPlayer(0)))
        {
            setBck(0x1A, 2, 3.0f, 1.0f);
            mTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30.0f), 30);
        } else {
            setBck(0x19, 2, 10.0f, 1.0f);
            field_0x672 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(60.0f) + 30.0f), (s16)cM_rndF(60.0f) + 0x1E);
        }
        mMoveMode = 1;
        break;

    case 1:
        if (field_0x672 == 0 && daPy_getPlayerActorClass()->getDamageWaitTimer() == 0) {
            nextActionCheck();
        }
        break;
    }
}

void daE_KK_c::executeIcicleWait() {
    switch (mMoveMode) {
    case 0:
        setBck(4, 0, 3.0f, 1.0f);
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_APPEAR, 0, -1);
        mMoveMode = 1;
        break;

    case 1:
        if (mpMorfSO->isStop()) {
            setBck(0x13, 0, 0.0f, 1.0f);
            setWeaponBck(0x1E, 0, 0.0f, 1.0f);
            mMoveMode++;
        }
        break;

    case 2:
        if ((s32)mpWeaponMorfSO->getFrame() >= 0x22) {
            if (field_0x67d != 0) {
                mCreatureSound.startCreatureSound(Z2SE_EN_KK_RESTORE, 0, -1);
            }
            field_0x67d = 0;
            mCyl.OnTgNoHitMark();
            field_0x76c = 0;
        }
        if (mpMorfSO->isStop() && mpWeaponMorfSO->isStop()) {
            mCyl.OnTgNoHitMark();
            field_0x67d = 0;
            field_0x76c = 0;
            nextActionCheck();
            fopAcM_OffStatus(this, fopAcStts_UNK_0x4000_e);
        }
    }
}

void daE_KK_c::executeWalk() {
    dBgS_LinChk linChk;
    cXyz position1;
    cXyz position2;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    mDoMtx_YrotS(*calc_mtx, current.angle.y);
    position1.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
    position1.y = 100.0f * sizeMultiplier;
    position1.z = 300.0f * sizeMultiplier;
#else
    position1.y = 100.0f;
    position1.z = 300.0f;
#endif
    MtxPosition(&position1, &position2);
    field_0x6a4 = position2 + current.pos;
    position1.set(current.pos);
    position1.y += DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f);
    linChk.Set(&position1, &field_0x6a4, this);

    switch (mMoveMode) {
    case 0:
        setBck(0x1B, 2, 3.0f, 1.0f);
        position1.x = cM_rndFX(mMovingRange) + field_0x740.x;
        position1.y = field_0x740.y;
        position1.z = cM_rndFX(mMovingRange) + field_0x740.z;
        position1 -= current.pos;
        field_0x674 = position1.atan2sX_Z() - current.angle.y;

        if (field_0x674 > 0x3000) {
            field_0x674 = 0x3000;
        } else if (field_0x674 < -0x3000) {
            field_0x674 = -0x3000;
        }
        field_0x674 += current.angle.y;
        if (way_gake_check()) {
            position1 = field_0x740 - current.pos;
            field_0x674 = position1.atan2sX_Z();
        }
        field_0x672 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(100.0f) + 100.0f), (s16)cM_rndF(100.0f) + 100);
        mMoveMode++;
        break;

    case 1:
        if (dComIfG_Bgsp().LineCross(&linChk) || way_gake_check()) {
            position1 = field_0x740 - current.pos;

            if (abs((s16)(current.angle.y - position1.atan2sX_Z())) > 0x100) {  // Problem here
                field_0x674 = position1.atan2sX_Z();
                speedF = 0.0f;
                mMoveMode++;
                break;
            }
        }
        nextActionCheck();
        break;

    case 2:
        if (abs((s16)(shape_angle.y - field_0x674)) <= 0x100) {
            mMoveMode = 1;
        }
        break;
    }

    if (field_0x764 == 0x1B) {
        if (DUSK_IF_ELSE(mpMorfSO->checkFrame(25.0f) || mpMorfSO->checkFrame(49.0f), (s32)mpMorfSO->getFrame() == 0x19 || (s32)mpMorfSO->getFrame() == 0x31)) {
            mCreatureSound.startCreatureSound(Z2SE_EN_KK_FOOTNOTE, 0, -1);
        }
    }
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(this, &speedF, 5.0f, 0.8f, 2.0f);
    actor_attr::enemy_add_action_angle(this, &current.angle.y, field_0x674, 8, 0x500);
#else
    cLib_addCalc2(&speedF, 5.0f, 0.8f, 2.0f);
    cLib_addCalcAngleS2(&current.angle.y, field_0x674, 8, 0x500);
#endif
    shape_angle.y = current.angle.y;
}

void daE_KK_c::executeSpearThrow() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    switch (mMoveMode) {
    case 0:
        if (field_0x764 != 0x14) {
            setBck(0x14, 2, 3.0f, 1.0f);
        }
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, fopAcM_searchPlayerAngleY(this), 4, 0x500), cLib_addCalcAngleS2(&current.angle.y, fopAcM_searchPlayerAngleY(this), 4, 0x500));
        shape_angle.y = current.angle.y;
        if (abs((s16)(shape_angle.y - fopAcM_searchPlayerAngleY(this))) <= 0x100) {
        case 1:
            shape_angle.y = current.angle.y = fopAcM_searchPlayerAngleY(this);
            setBck(0x18, 0, 3.0f, 1.0f);
            mMoveMode++;
        }
        break;

    case 2:
        if (!dComIfGp_event_runCheck() && fopAcM_searchPlayerDistance(this) <= DUSK_IF_ELSE(l_HIO.direct_attack_range * sizeMultiplier, l_HIO.direct_attack_range) &&
            !fopAcM_otherBgCheck(this, dComIfGp_getPlayer(0)) && (s32)mpMorfSO->getFrame() < 0x17 &&
            !dComIfGp_event_runCheck() && !dComIfGp_checkPlayerStatus0(0, 0x100))
        {
            fopAc_ac_c* player = dComIfGp_getPlayer(0);
            if (!fopAcM_otherBgCheck(this, player) && eyePos.y + DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f) > player->current.pos.y) {
                setActionMode(8, 0);
                break;
            }
        }
        if (DUSK_IF_ELSE(mpMorfSO->checkFrame(23.0f), (s32)mpMorfSO->getFrame() == 0x17)) {
            fopAcM_createChild(fpcNm_E_KK_e, fopAcM_GetID(this), 0xFF0001, &field_0x698,
                               fopAcM_GetRoomNo(this), &shape_angle, NULL, -1, NULL);
            field_0x67d = 1;
            mCyl.OffTgNoHitMark();
            mCyl.OffTgShield();
            mCreatureSound.startCreatureSound(Z2SE_EN_KK_THROW, 0, -1);
        }
        if (DUSK_IF_ELSE(mpMorfSO->checkFrame(9.0f) || mpMorfSO->checkFrame(23.0f) || mpMorfSO->checkFrame(34.0f) || mpMorfSO->checkFrame(61.0f), (s32)mpMorfSO->getFrame() == 9 || (s32)mpMorfSO->getFrame() == 0x17 ||
            (s32)mpMorfSO->getFrame() == 0x22 || (s32)mpMorfSO->getFrame() == 0x3D))
        {
            mCreatureSound.startCreatureSound(Z2SE_EN_KK_FOOTNOTE, 0, -1);
        }
        if (mpMorfSO->isStop()) {
            mMoveMode++;
        case 3:
            setBck(0x12, 0, 0.0f, 1.0f);
            setWeaponBck(0x1E, 0, 0.0f, 1.0f);
            mMoveMode++;
        }

        break;

    case 4:
        if ((s32)mpWeaponMorfSO->getFrame() >= 0x22) {
            if (field_0x67d != 0) {
                mCreatureSound.startCreatureSound(Z2SE_EN_KK_RESTORE, 0, -1);
            }
            field_0x67d = 0;
            mCyl.OnTgNoHitMark();
            field_0x76c = 0;
        }
        if (mpMorfSO->isStop()) {
            if (mpWeaponMorfSO->isStop()) {
                mCyl.OnTgNoHitMark();
                field_0x67d = 0;
                field_0x76c = 0;
                nextActionCheck();
            }
        }
        break;
    }

    if (field_0x764 == 0x14 &&
        (DUSK_IF_ELSE(mpMorfSO->checkFrame(9.0f) || mpMorfSO->checkFrame(18.0f), (s32)mpMorfSO->getFrame() == 9 || (s32)mpMorfSO->getFrame() == 0x12)))
    {
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_FOOTNOTE, 0, -1);
    }
}

void daE_KK_c::executeBackWalk() {
    dBgS_LinChk linChk;
    cXyz position1;
    cXyz position2;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    mDoMtx_YrotS(*calc_mtx, current.angle.y);
    position1.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
    position1.y = 100.0f * sizeMultiplier;
    position1.z = 300.0f * sizeMultiplier;
#else
    position1.y = 100.0f;
    position1.z = 300.0f;
#endif
    MtxPosition(&position1, &position2);
    field_0x6a4 = position2 + current.pos;
    position1.set(current.pos);
    position1.y += DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f);
    linChk.Set(&position1, &field_0x6a4, this);

    switch (mMoveMode) {
    case 0:
        setBck(7, 2, 3.0f, 1.0f);
        if (field_0x76c == 0) {
            mTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.escape_time), l_HIO.escape_time);
            field_0x76c = 1;
        }
        mCyl.OffTgNoHitMark();
        if (!dComIfG_Bgsp().LineCross(&linChk)) {
            mMoveMode = 1;
        } else {
            if (cM_rndF(1.0f) < 0.5f) {
                current.angle.y += 0x4000;
            } else {
                current.angle.y -= 0x4000;
            }
            mMoveMode = 2;
        }
        break;

    case 1:
        current.angle.y = fopAcM_searchPlayerAngleY(this) + 0x8000;

    case 2:
        speedF = 15.0f;
        if (mTimer == 0 || dComIfG_Bgsp().LineCross(&linChk) || way_gake_check()) {
            setBck(0x13, 0, 0.0f, 1.0f);
            setWeaponBck(0x1E, 0, 0.0f, 1.0f);
            current.angle.y = shape_angle.y;
            speedF = 0.0f;
            mMoveMode = 3;
        }
        break;

    case 3:
        if ((s32)mpWeaponMorfSO->getFrame() >= 0x22) {
            if (field_0x67d != 0) {
                mCreatureSound.startCreatureSound(Z2SE_EN_KK_RESTORE, 0, -1);
            }
            field_0x67d = 0;
            mCyl.OnTgNoHitMark();
            field_0x76c = 0;
        }
        if (mpMorfSO->isStop()) {
            if (mpWeaponMorfSO->isStop()) {
                field_0x67d = 0;
                field_0x76c = 0;
                mCyl.OnTgNoHitMark();
                mMoveMode++;
                nextActionCheck();
            }
        }
        break;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(this, &shape_angle.y, fopAcM_searchPlayerAngleY(this), 2, 0x200);
    if (field_0x764 == 7 && (mpMorfSO->checkFrame(9.0f) || mpMorfSO->checkFrame(19.0f))) {
#else
    cLib_addCalcAngleS2(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 2, 0x200);
    if (field_0x764 == 7 && ((s32)mpMorfSO->getFrame() == 9 || (s32)mpMorfSO->getFrame() == 0x13)) {
#endif
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_FOOTNOTE, 0, -1);
    }
}

void daE_KK_c::executeYoroke() {
    switch (mMoveMode) {
    case 0:
        if (field_0x67d == 0) {
            setBck(9, 0, 3.0f, 1.0f);
        } else {
            setBck(0xB, 0, 3.0f, 1.0f);
        }
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_NO_DAMAGE, 0, -1);
        mMoveMode = 2;
        return;

    case 1:
        setBck(0xF, 0, 3.0f, 1.0f);
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_NO_DAMAGE, 0, -1);
        mMoveMode = 2;
        return;
    case 2:
        if (mpMorfSO->isStop()) {
            offHeadLockFlg();
            if (field_0x67d == 0) {
                nextActionCheck();
                return;
            }
            current.angle.y = fopAcM_searchPlayerAngleY(this) + 0x8000;
            setActionMode(4, 0);
        } else {
            return;
        }
        break;
    }
}

void daE_KK_c::executeGuard() {
    switch (mMoveMode) {
    case 0:
        setWeaponBck(0x1D, 0, 0.0f, 1.0f);
        field_0x67c &= 1;
        if (field_0x67c == 0) {
            setBck(0x10, 0, 3.0f, 1.0f);
        } else {
            setBck(0x11, 0, 3.0f, 1.0f);
        }
        field_0x67c++;
        mMoveMode = 1;
        break;

    case 1:
        if (mpMorfSO->isStop()) {
            nextActionCheck();
        }
        break;
    }
    DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x500), cLib_addCalcAngleS2(&current.angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x500));
    shape_angle.y = current.angle.y;
}

void daE_KK_c::executeDamage() {
    switch (mMoveMode) {
    case 0:
        setBck(8, 0, 3.0f, 1.0f);
        mDeadEffSet(field_0x698);
        speedF = -80.0f;
        Z2GetAudioMgr()->seStart(Z2SE_EN_KK_CRASH, &field_0x698, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        field_0x67d = 1;
        mCyl.OffTgShield();
        field_0x67c = (s32)cM_rndF(2.0f) & 1;
        mMoveMode = 2;
        return;

    case 1:
        if (field_0x67c == 0) {
            setBck(0xC, 0, 3.0f, 1.0f);
        } else {
            setBck(0xD, 0, 3.0f, 1.0f);
        }
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_DAMAGE, 0, -1);
        field_0x67c++;
        field_0x67c &= 1;
        mMoveMode = 3;
        return;

    case 2:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc0(this, &speedF, 0.7f, 2.0f), cLib_addCalc0(&speedF, 0.7f, 2.0f));

    case 3:
        mCyl.OffTgNoHitMark();
        if (mpMorfSO->isStop()) {
            current.angle.y = fopAcM_searchPlayerAngleY(this) + 0x8000;
            setActionMode(4, 0);
        }
        return;
    }
}

void daE_KK_c::executeAttack() {
    dBgS_LinChk linChk;
    cXyz position1;
    cXyz position2;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    mDoMtx_YrotS(*calc_mtx, current.angle.y);
    position1.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
    position1.y = 100.0f * sizeMultiplier;
    position1.z = 300.0f * sizeMultiplier;
#else
    position1.y = 100.0f;
    position1.z = 300.0f;
#endif
    MtxPosition(&position1, &position2);
    field_0x74c = position2 + current.pos;
    position1.set(current.pos);
    position1.y += DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f);
    linChk.Set(&position1, &field_0x74c, this);

    switch (mMoveMode) {
    case 0:
        field_0x67c = 0;
        if (fopAcM_searchPlayerDistance(this) > DUSK_IF_ELSE(600.0f * sizeMultiplier, 600.0f)) {
            setBck(0x1C, 2, 3.0f, 1.0f);
            mMoveMode = 1;
        } else {
            mMoveMode = 2;
        }
        break;

    case 1:
        if (fopAcM_searchPlayerDistance(this) > DUSK_IF_ELSE(600.0f * sizeMultiplier, 600.0f)) {
            speedF = 20.0f;
            if (fopAcM_searchPlayerDistance(this) > DUSK_IF_ELSE(l_HIO.direct_attack_range * sizeMultiplier, l_HIO.direct_attack_range)) {
                speedF = 0.0f;
                nextActionCheck();
            }
        } else {
            mMoveMode = 2;
        case 2:
            if (cM_rndF(1.0f) < 0.5f) {
                setBck(5, 0, 3.0f, 1.0f);
            } else {
                setBck(6, 0, 3.0f, 1.0f);
            }
            mMoveMode++;
        }
        break;

    case 3:
        if (DUSK_IF_ELSE(mpMorfSO->checkFrame(24.0f), (s32)mpMorfSO->getFrame() == 0x18)) {
            speedF = 80.0f;
            field_0x67e = 0;
            if (field_0x764 == 6) {
                mCreatureSound.startCreatureSound(Z2SE_EN_KK_ATTACK03, 0, -1);
            }
        }
        if (field_0x764 == 5 && DUSK_IF_ELSE(mpMorfSO->checkFrame(27.0f), (s32)mpMorfSO->getFrame() == 0x1B)) {
            mCreatureSound.startCreatureSound(Z2SE_EN_KK_ATTACK01, 0, -1);
        }
        if (mpMorfSO->isStop()) {
            nextActionCheck();
        }
        break;
#if TARGET_PC  // enemy attribute integration

    case 4: {
        const f32 playerDistance = fopAcM_searchPlayerDistance(this);
        const f32 noticeDistance = actor_attr::enemy_notice_distance(this, l_HIO.spear_throw_range);
        const f32 spearThrowDistance = l_HIO.spear_throw_range * sizeMultiplier;
        fopAc_ac_c* player = dComIfGp_getPlayer(0);

        if (dComIfGp_event_runCheck() || playerDistance > noticeDistance || fopAcM_otherBgCheck(this, player)) {
            speedF = 0.0f;
            setActionMode(ACTION_WAIT, 0);
            return;
        } else if (playerDistance <= spearThrowDistance) {
            speedF = 0.0f;
            nextActionCheck();
            return;
        } else if (dComIfG_Bgsp().LineCross(&linChk) || way_gake_check()) {
            speedF = 0.0f;
            setActionMode(ACTION_WALK, 0);
            return;
        } else {
            if (field_0x764 != 0x1C) {
                setBck(0x1C, 2, 3.0f, 1.0f);
            }
            speedF = 20.0f;
        }
        break;
    }
#endif
    }

    if (dComIfG_Bgsp().LineCross(&linChk) && field_0x67c == 0) {
        mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 0x28);
        speedF = -70.0f;
        field_0x67c = 1;
    }

    if ((s32)mpMorfSO->getFrame() < 0x19) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, fopAcM_searchPlayerAngleY(this), 1, 0x500), cLib_addCalcAngleS2(&current.angle.y, fopAcM_searchPlayerAngleY(this), 1, 0x500));
        shape_angle.y = current.angle.y;
    } else if ((s32)mpMorfSO->getFrame() >= 0x21) {
        field_0x67e = 1;
    }

    if (field_0x764 == 0x1C && (DUSK_IF_ELSE(mpMorfSO->checkFrame(0.0f) || mpMorfSO->checkFrame(10.0f), (s32)mpMorfSO->getFrame() == 0 || (s32)mpMorfSO->getFrame() == 10)))
    {
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_FOOTNOTE, 0, -1);
    }
}

void daE_KK_c::executeDead() {
    mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(0x11));
    cXyz effect_position;
    mDoMtx_stack_c::multVecZero(&effect_position);

    switch (mMoveMode) {
    case 0:
        fopAcM_OffStatus(this, 0);
        attention_info.flags &= ~fopAc_AttnFlag_BATTLE_e;
        attention_info.distances[fopAc_attn_BATTLE_e] = 0;
        offHeadLockFlg();
        health = 0;
        speedF = 0.0f;
        mCyl.OffTgSetBit();
        mCyl.OffCoSetBit();
        mSph.OffAtSetBit();
        mDeadEffSet(effect_position);
        mCreatureSound.startCreatureSound(Z2SE_EN_KK_DEAD, 0, -1);
        if (field_0x679 != 1) {
            if (field_0x67b != 0xFF) {
                fopAcM_onSwitch(this, field_0x67b);
            }
        }
        field_0x67d = 1;
        mCyl.OffTgShield();
        setBck(0xE, 0, 3.0f, 1.0f);
        mMoveMode++;
        return;

    case 1:
        if (mpMorfSO->isStop()) {
            field_0x564 = 0x1A;
            fopAcM_createDisappear(this, &current.pos, DUSK_IF_ELSE(actor_attr::byte_value(8.0f * actor_attr::enemy_size_multiplier(this)), 8), 0, field_0x564);
            fopAcM_delete(this);
            return;
        }
        break;

    case 2:
        if (mTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 1.0f), 1) || (mTimer == 0 && mObjAcch.ChkGroundHit())) {
            mDeadEffSet(effect_position);
            fopAcM_delete(this);
        }
        break;
    }
}

void daE_KK_c::executeWeaponMove() {
    fopAc_ac_c* actor = NULL;
    dBgS_LinChk linChk;
    cXyz position1;
    cXyz position2;
    cXyz new_speed;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    J3DModel* weaponModel = mpWeaponMorfSO->getModel();

    mDoMtx_stack_c::copy(weaponModel->getAnmMtx(1));
    mDoMtx_stack_c::multVecZero(&position1);

    if (mMoveMode == 0) {
        if (!fopAcM_SearchByID(parentActorID, &actor) || actor == NULL) {
            fopAcM_delete(this);
            return;
        }
        current.angle.y = actor->shape_angle.y + 0x190;
    }

    f32 temp;
    f32 player_distance_y;
#if TARGET_PC  // enemy attribute integration
    f32 normalizedPlayerDistance;
    f32 normalizedPlayerDistanceY;
#endif

    switch (mMoveMode) {
    case 0:
        shape_angle.x = home.angle.x + 0x878C;
        shape_angle.y = home.angle.y + 0x4399;

        current.angle.x = fopAcM_searchPlayerAngleX(this);
        field_0x674 = fopAcM_searchPlayerAngleX(this) - 0x2EE0;
        shape_angle.z = -0x389A;

        mTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 200.0f), 200);
        speedF = 120.0f;

#if TARGET_PC  // enemy attribute integration
        normalizedPlayerDistance = fopAcM_searchPlayerDistance(this) / sizeMultiplier;
        mMovingRange = 20.0f - (normalizedPlayerDistance - l_HIO.direct_attack_range) / 7.0f;
#else
        mMovingRange = 20.0f - (fopAcM_searchPlayerDistance(this) - l_HIO.direct_attack_range) / 7.0f;
#endif

        mDistance = (DUSK_IF_ELSE(normalizedPlayerDistance, fopAcM_searchPlayerDistance(this)) - l_HIO.direct_attack_range) * 1.5f;

        temp = 2000.0f;

#if TARGET_PC  // enemy attribute integration
        normalizedPlayerDistanceY = fopAcM_searchPlayerDistanceY(this) / sizeMultiplier;
        if (normalizedPlayerDistanceY < -300.0f) {
            player_distance_y = (normalizedPlayerDistanceY + 300.0f) * -2.0f;
#else
        if (fopAcM_searchPlayerDistanceY(this) < -300.0f) {
            player_distance_y = (fopAcM_searchPlayerDistanceY(this) + 300.0f) * -2.0f;
#endif
            temp = player_distance_y + 2000.0f;
            mDistance += player_distance_y;
        }
        if (mDistance > (s16)temp) {
            mDistance = temp;
        }
        mMoveMode = 1;
        goto end;

    case 1:
        if (mSph.ChkAtHit()) {
            if (mSph.ChkAtShieldHit()) {
                mCreatureSound.startCollisionSE(Z2SE_HIT_SWORD, 0x28);
            }
            Z2GetAudioMgr()->seStart(Z2SE_EN_KK_CRASH, &field_0x68c, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f,
                                     0);
            mDeadEffSet(field_0x68c);
            fopAcM_delete(this);
            return;
        }

        mDoMtx_YrotS(*calc_mtx, current.angle.y);
        mDoMtx_XrotM(*calc_mtx, current.angle.x);

        position2.x = 0.0f;
        position2.y = mMovingRange;
        position2.z = speedF;

        MtxPosition(&position2, &new_speed);
        speed.y = -new_speed.y;

        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.x, field_0x674, 1, mDistance), cLib_addCalcAngleS2(&current.angle.x, field_0x674, 1, mDistance));
        if (speed.y < 0.0f) {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.z, -20000, 4, 0x500), cLib_addCalcAngleS2(&shape_angle.z, -20000, 4, 0x500));
        }

        linChk.Set(&current.pos, &field_0x68c, this);

        if (mTimer == 0 || mObjAcch.ChkGroundHit() || dComIfG_Bgsp().LineCross(&linChk)) {
            speedF = 0.0f;
            gravity = 0.0f;
            speed.zero();
            field_0x67e = 1;
            mSph.OffAtSetBit();
            if (mTimer != 0) {
                Z2GetAudioMgr()->seStart(Z2SE_EN_KK_SPEAR_STICK, &field_0x68c, 0, 0, 1.0f, 1.0f,
                                         -1.0f, -1.0f, 0);
            }
            mStts.Init(0xFF, 0, this);
            mTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(20.0f) + 30.0f), (s16)cM_rndF(20.0f) + 30);
            mMoveMode = 2;
        }
        goto end;

    case 2:
        if (mTimer == 0) {
            mDeadEffSet(position1);
            Z2GetAudioMgr()->seStart(Z2SE_EN_KK_CRASH, &position1, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f,
                                     0);
            fopAcM_delete(this);
        }

    default:
    end:
        cXyz mult_vec;
        if (mSph.ChkTgHit()) {
            mDoMtx_stack_c::copy(weaponModel->getAnmMtx(2));
            mult_vec.set(100.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVec(&mult_vec, &field_0x680);
            mSph.ClrTgHit();
            mDeadEffSet(field_0x680);
            Z2GetAudioMgr()->seStart(Z2SE_EN_KK_CRASH, &field_0x680, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f,
                                     0);
            fopAcM_delete(this);
            return;
        }

        for (int i = 0; i < 6; i++) {
            if (mSpheres[i].ChkTgHit()) {
                mDoMtx_stack_c::copy(weaponModel->getAnmMtx(2));
                mult_vec.set(100.0f, 0.0f, 0.0f);
                mDoMtx_stack_c::multVec(&mult_vec, &field_0x680);
                mSpheres[i].ClrTgHit();
                mDeadEffSet(field_0x680);
                Z2GetAudioMgr()->seStart(Z2SE_EN_KK_CRASH, &field_0x680, 0, 0, 1.0f, 1.0f, -1.0f,
                                         -1.0f, 0);
                fopAcM_delete(this);
                return;
            }
        }
    }
}

void daE_KK_c::action() {
    BOOL link_search = false;
    damage_check();

    switch (mActionMode) {
    case ACTION_WAIT:
        executeWait();
        break;

    case ACTION_ICICLEWAIT:
        executeIcicleWait();
        break;

    case ACTION_WALK:
        executeWalk();
        break;

    case ACTION_SPEARTHROW:
        executeSpearThrow();
        link_search = true;
        break;

    case ACTION_BACKWALK:
        executeBackWalk();
        break;

    case ACTION_YOROKE:
        executeYoroke();
        break;

    case ACTION_GUARD:
        executeGuard();
        link_search = true;
        break;

    case ACTION_DAMAGE:
        executeDamage();
        break;

    case ACTION_ATTACK:
        executeAttack();
        link_search = true;
        break;

    case ACTION_DEAD:
        executeDead();
        break;

    case ACTION_WEAPONMOVE:
        executeWeaponMove();
        break;
    }

    mCreatureSound.setLinkSearch(link_search);
    if (field_0x679 != 1) {
        if (mActionMode != 8) {
            field_0x67e = 1;
        }
#if TARGET_PC  // enemy attribute integration
        const bool isNoticeApproach = mActionMode == ACTION_ATTACK && mMoveMode == 4;
        if (mActionMode != ACTION_BACKWALK && mActionMode != ACTION_WALK && !isNoticeApproach) {
            actor_attr::enemy_add_action_calc0(this, &speedF, 0.7f, 10.0f);
#else
        if (mActionMode != 4 && mActionMode != 2) {
            cLib_addCalc0(&speedF, 0.7f, 10.0f);
#endif
        }
    }
    if (mActionMode != 10 || mMoveMode >= 2) {
#if TARGET_PC  // enemy attribute integration
        if (field_0x679 == 1) {
            /* The spear inherits its speed multiplier from its Chilfos owner,
             * but this child actor must remain the physical movement target. */
            actor_attr::enemy_action_pos_move_f(this, mStts.GetCCMoveP());
        } else {
            actor_attr::enemy_pos_move_f(this, mStts.GetCCMoveP());
        }
#else
        fopAcM_posMoveF(this, mStts.GetCCMoveP());
#endif
    }
    if (field_0x679 != 1) {
        s16 angle = -(shape_angle.y - fopAcM_searchPlayerAngleY(this));

        if (angle < -10000) {
            angle = -10000;
        } else if (angle > 10000) {
            angle = 10000;
        }

        if ((mActionMode == 5 || mActionMode == 7) ||
            fopAcM_searchPlayerDistance(this) > DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, l_HIO.spear_throw_range), l_HIO.spear_throw_range))
        {
            angle = 0;
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(this, &field_0x758, angle, 8, 0x400);
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
        mAcchCir.SetWall(70.0f * sizeMultiplier, 150.0f * sizeMultiplier);
#else
        cLib_addCalcAngleS2(&field_0x758, angle, 8, 0x400);
        mAcchCir.SetWall(70.0f, 150.0f);
#endif
        mObjAcch.CrrPos(dComIfG_Bgsp());
        if (mActionMode != 10) {
            dBgS_GndChk gndChk;
            cXyz position;
            position.set(current.pos);
            position.y += DUSK_IF_ELSE(300.0f * sizeMultiplier, 300.0f);
            gndChk.SetPos(&position);
            position.y = dComIfG_Bgsp().GroundCross(&gndChk);
            if (position.y == -G_CM3D_F_INF) {
                mTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100.0f), 100);
                setActionMode(10, 2);
            } else {
                if (field_0x760 - current.pos.y > DUSK_IF_ELSE(200.0f * sizeMultiplier, 200.0f)) {
                    setActionMode(10, 2);
                } else {
                    if (mObjAcch.ChkGroundHit()) {
                        field_0x760 = current.pos.y;
                    }
                }
            }
        }
        mpMorfSO->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    } else {
        if (speed.y < 0.0f) {
#if TARGET_PC  // enemy attribute integration
            const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
            mAcchCir.SetWall(1.0f * sizeMultiplier, 1.0f * sizeMultiplier);
#else
            mAcchCir.SetWall(1.0f, 1.0f);
#endif
            mObjAcch.CrrPos(dComIfG_Bgsp());
        }
    }
    if (field_0x679 != 1 && (field_0x67d == 0 || field_0x768 == 0x1E)) {
        mpWeaponMorfSO->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
        if (field_0x768 == 0x1E) {
            if (mpWeaponMorfSO->isStop()) {
                setWeaponBck(0x1D, 0, 0.0f, 1.0f);
            }
        }
    }
}

void daE_KK_c::mtx_set() {
    cXyz mult_vec;
#if TARGET_PC  // enemy attribute integration
    const f32 modelScale = l_HIO.model_size * actor_attr::enemy_size_multiplier(this);
#endif

    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
#if TARGET_PC  // enemy attribute integration
    mDoMtx_stack_c::scaleM(modelScale, modelScale, modelScale);
#else
    mDoMtx_stack_c::scaleM(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
#endif

    J3DModel* morfModel = mpMorfSO->getModel();
    morfModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorfSO->modelCalc();
    mDoMtx_stack_c::copy(morfModel->getAnmMtx(0xE));

    mDoMtx_stack_c::multVecZero(&field_0x698);

    J3DModel* weaponModel = mpWeaponMorfSO->getModel();
    weaponModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpWeaponMorfSO->modelCalc();
    mDoMtx_stack_c::copy(weaponModel->getAnmMtx(2));

    mDoMtx_stack_c::multVecZero(&field_0x680);

    mult_vec.set(230.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVec(&mult_vec, &field_0x68c);

    for (int i = 0; i < 6; i++) {
        mDoMtx_stack_c::copy(weaponModel->getAnmMtx(1));
        mult_vec.set(i * -60.0f + 80.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::multVec(&mult_vec, &field_0x6f8[i]);
    }
}

void daE_KK_c::weapon_mtx_set() {
    cXyz mult_vec;
#if TARGET_PC  // enemy attribute integration
    const f32 modelScale = l_HIO.model_size * actor_attr::enemy_size_multiplier(this);
#endif

    mDoMtx_stack_c::transS(current.pos.x, current.pos.y, current.pos.z);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
#if TARGET_PC  // enemy attribute integration
    mDoMtx_stack_c::scaleM(modelScale, modelScale, modelScale);
#else
    mDoMtx_stack_c::scaleM(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
#endif

    J3DModel* weaponModel = mpWeaponMorfSO->getModel();
    weaponModel->setBaseTRMtx(mDoMtx_stack_c::get());
    mpWeaponMorfSO->modelCalc();
    mDoMtx_stack_c::copy(weaponModel->getAnmMtx(1));

    mDoMtx_stack_c::multVecZero(&field_0x680);

    mult_vec.set(100.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVec(&mult_vec, &field_0x68c);

    for (int i = 0; i < 6; i++) {
        mDoMtx_stack_c::copy(weaponModel->getAnmMtx(1));
        mult_vec.set(i * -60.0f + 80.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::multVec(&mult_vec, &field_0x6f8[i]);
    }
}

void daE_KK_c::cc_set() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    if (field_0x679 != 1) {
        mDoMtx_stack_c::copy(mpMorfSO->getModel()->getAnmMtx(4));
        mDoMtx_stack_c::multVecZero(&attention_info.position);
        eyePos = attention_info.position;
        eyePos.y -= DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f);
        setHeadLockPos(&eyePos);
        mCyl.SetC(current.pos);
#if TARGET_PC  // enemy attribute integration
        mCyl.SetH(250.0f * sizeMultiplier);
        mCyl.SetR(70.0f * sizeMultiplier);
#else
        mCyl.SetH(250.0f);
        mCyl.SetR(70.0f);
#endif
        dComIfG_Ccsp()->Set(&mCyl);
    }

    if (field_0x67d == 0 && field_0x67e == 0) {
        mSph.SetC(field_0x68c);
        mSph.SetR(DUSK_IF_ELSE(40.0f * sizeMultiplier, 40.0f));
        dComIfG_Ccsp()->Set(&mSph);
    }

    if (field_0x679 == 1 || field_0x67d == 0) {
        for (int i = 0; i < 6; i++) {
            if (field_0x679 != 1) {
                if (field_0x67e != 0) {
                    mSpheres[i].OffAtSetBit();
                } else {
                    mSpheres[i].OnAtSetBit();
                }
            }
            mSpheres[i].SetC(field_0x6f8[i]);
            mSpheres[i].SetR(DUSK_IF_ELSE(10.0f * sizeMultiplier, 10.0f));
            dComIfG_Ccsp()->Set(&mSpheres[i]);
        }
    }
}

int daE_KK_c::execute() {
    if (field_0x679 == 2) {
        if (!checkItemGet(dItemNo_IRONBALL_e, 1)) {
            return 1;
        }

        if (attention_info.distances[fopAc_attn_BATTLE_e] == 0) {
            attention_info.distances[fopAc_attn_BATTLE_e] = 0x45;
            fopAcM_SetGroup(this, 2);
            fopAcM_OnStatus(this, 0);
            attention_info.flags |= fopAc_AttnFlag_BATTLE_e;
        }
    }

    if (mTimer != 0) {
        mTimer--;
    }
    if (field_0x672 != 0) {
        field_0x672--;
    }
    if (mDamageTimer != 0) {
        mDamageTimer--;
    }

    action();

    if (field_0x679 != 1) {
        mtx_set();
    } else {
        weapon_mtx_set();
    }

    cc_set();
    mCreatureSound.framework(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    return 1;
}

static int daE_KK_Execute(daE_KK_c* i_this) {
    return i_this->execute();
}

static int daE_KK_IsDelete(daE_KK_c* i_this) {
    return 1;
}

int daE_KK_c::_delete() {
    dComIfG_resDelete(&mPhaseReq, "E_KK");

    if (mHIOInit != 0) {
        hio_set = false;
        mDoHIO_DELETE_CHILD(l_HIO.field_0x4);
    }

    if (heap != NULL) {
        mCreatureSound.deleteObject();
        offHeadLockFlg();
    }
    return 1;
}

static int daE_KK_Delete(daE_KK_c* i_this) {
    return i_this->_delete();
}

int daE_KK_c::CreateHeap() {
    if (field_0x679 != 1) {
        J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_KK", 0x22);
        JUT_ASSERT(2212, modelData != NULL);

        mpMorfSO = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL,
                                        (J3DAnmTransform*)dComIfG_getObjectRes("E_KK", 0x19), 0,
                                        1.0f, 0, -1, &mCreatureSound, 0, 0x31000084);
        if (mpMorfSO == NULL || mpMorfSO->getModel() == NULL) {
            return 0;
        }
        model = mpMorfSO->getModel();
        model->setUserArea((uintptr_t)this);
        for (u16 i = 0; i < model->getModelData()->getJointNum(); i++) {
            if (i != 0) {
                model->getModelData()->getJointNodePointer(i)->setCallBack(JointCallBack);
            }
        }
    }

    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_KK", 0x23);
    JUT_ASSERT(2247, modelData != NULL);

    mpWeaponMorfSO = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL,
                                          (J3DAnmTransform*)dComIfG_getObjectRes("E_KK", 0x1D), 0,
                                          1.0f, 0, -1, &mCreatureSound, 0, 0x31000084);
    if (mpWeaponMorfSO == NULL || mpWeaponMorfSO->getModel() == NULL) {
        return 0;
    }
    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    return static_cast<daE_KK_c*>(i_this)->CreateHeap();
}

int daE_KK_c::create() {
    fopAcM_ct(this, daE_KK_c);

    int phase_state = dComIfG_resLoad(&mPhaseReq, "E_KK");
    if (phase_state == cPhs_COMPLEATE_e) {
        OS_REPORT("E_KK PARAM %x\n", fopAcM_GetParam(this));

        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x3e90)) {
            return cPhs_ERROR_e;
        }

        field_0x679 = fopAcM_GetParam(this);
        field_0x67a = fopAcM_GetParam(this) >> 8;
        field_0x67b = fopAcM_GetParam(this) >> 0x10;

        if (field_0x679 == 0xFF) {
            field_0x679 = 0;
        }

#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);

#endif
        if (field_0x679 != 1 && field_0x67b != 0xFF && fopAcM_isSwitch(this, field_0x67b)) {
            return cPhs_ERROR_e;
        }
        if (!hio_set) {
            hio_set = 1;
            mHIOInit = 1;
            l_HIO.field_0x4 = mDoHIO_CREATE_CHILD("氷の剣士", &l_HIO);
        }
        mObjAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1,
                     &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        mAcchCir.SetWall(10.0f * sizeMultiplier, 60.0f * sizeMultiplier);
#else
        mAcchCir.SetWall(10.0f, 60.0f);
#endif
        mStts.Init(100, 0, this);

#if TARGET_PC  // enemy attribute integration
        health = actor_attr::enemy_health_value(this, 0x8C);
        field_0x560 = health;
#else
        health = 0x8C;
        field_0x560 = 0x8C;
#endif

        mCyl.Set(cc_kk_src);
        mCyl.SetStts(&mStts);
        mCyl.OnTgNoHitMark();

        mSph.Set(cc_kk_at_src);
        mSph.SetStts(&mStts);
        IF_DUSK(mSph.SetAtAtp(actor_attr::enemy_attack_power_byte(this, 1.0f));)
        mSph.OffTgSetBit();

        for (int i = 0; i < 6; i++) {
            mSpheres[i].Set(cc_kk_at_src);
            mSpheres[i].SetStts(&mStts);
            IF_DUSK(mSpheres[i].SetAtAtp(actor_attr::enemy_attack_power_byte(this, 1.0f));)
            if (field_0x679 == 1) {
                mSpheres[i].OffAtSetBit();
                mSph.OffCoSetBit();
                mSph.OnTgSetBit();
            } else {
                mSpheres[i].OffTgSetBit();
            }
        }

        mCreatureSound.init(&current.pos, &eyePos, 3, 1);
        mCreatureSound.setEnemyName("E_kk");

        mAtInfo.mpSound = &mCreatureSound;
        mAtInfo.mPowerType = 1;

        field_0x740.set(home.pos);

        if (field_0x679 == 1) {
            fopAcM_SetMtx(this, mpWeaponMorfSO->getModel()->getBaseTRMtx());
            setActionMode(9, 0);
        } else {
            gravity = -3.0f;
            field_0x67e = 1;

            fopAcM_SetMtx(this, mpMorfSO->getModel()->getBaseTRMtx());

            field_0x6a4.set(current.pos);
            field_0x74c.set(current.pos);

            for (int i = 0; i < 3; i++) {
                field_0x6b0[i].set(current.pos);
                field_0x6d4[i].set(current.pos);
            }

            field_0x760 = current.pos.y;

            attention_info.flags = fopAc_AttnFlag_BATTLE_e;
            attention_info.distances[fopAc_attn_BATTLE_e] = 0x4C;

            if (field_0x679 != 3) {
                setActionMode(0, 0);
            } else {
                fopAcM_OnStatus(this, fopAcStts_UNK_0x4000_e);
                field_0x67d = 1;
                s16 player_angle = fopAcM_searchPlayerAngleY(this);
                shape_angle.y = player_angle;
                current.angle.y = player_angle;
                setActionMode(1, 0);
            }
        }

#if TARGET_PC  // enemy attribute integration
        fopAcM_SetMin(this, -600.0f * sizeMultiplier, -600.0f * sizeMultiplier, -600.0f * sizeMultiplier);
        fopAcM_SetMax(this, 600.0f * sizeMultiplier, 600.0f * sizeMultiplier, 600.0f * sizeMultiplier);
#else
        fopAcM_SetMin(this, -600.0f, -600.0f, -600.0f);
        fopAcM_SetMax(this, 600.0f, 600.0f, 600.0f);
#endif

        if (field_0x679 == 2 && !checkItemGet(dItemNo_IRONBALL_e, 1)) {
            attention_info.distances[fopAc_attn_BATTLE_e] = 0;
            fopAcM_SetGroup(this, 0);
            fopAcM_OffStatus(this, 0);
            attention_info.flags &= ~fopAc_AttnFlag_BATTLE_e;
        }

        if (field_0x67a == 0xFF || field_0x67a == 0) {
            mMovingRange = DUSK_IF_ELSE(l_HIO.default_moving_range * sizeMultiplier, l_HIO.default_moving_range);
        } else {
            mMovingRange = DUSK_IF_ELSE(field_0x67a * 100.0f * sizeMultiplier, field_0x67a * 100.0f);
        }
#if TARGET_PC  // enemy attribute integration

        if (field_0x679 != 1) {
            actor_attr::enemy_set_animation_play_speed(this, mpMorfSO, 1.0f);
        }
        actor_attr::enemy_set_animation_play_speed(this, mpWeaponMorfSO, 1.0f);
#endif

        if (field_0x679 != 1) {
            mtx_set();
        } else {
            weapon_mtx_set();
        }
    }
    return phase_state;
}

static int daE_KK_Create(daE_KK_c* i_this) {
    return i_this->create();
}

static DUSK_CONST actor_method_class l_daE_KK_Method = {
    (process_method_func)daE_KK_Create,  (process_method_func)daE_KK_Delete,
    (process_method_func)daE_KK_Execute, (process_method_func)daE_KK_IsDelete,
    (process_method_func)daE_KK_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_KK = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_KK_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daE_KK_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_KK_e,
    /* Actor SubMtd */ &l_daE_KK_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_CULL_e | fopAcStts_UNK_0x20_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

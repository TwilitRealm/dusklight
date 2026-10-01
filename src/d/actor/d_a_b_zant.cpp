/**
 * @file d_a_b_zant.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#if TARGET_PC  // additional actor attribute integration
#include <algorithm>
#include <cmath>
#include "JSystem/JAudio2/JAUSectionHeap.h"
#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2Instances.h"
#include "d/actor/d_a_alink.h"
#endif
#include "d/actor/d_a_b_zant.h"
#include "d/actor/d_a_b_zant_mobile.h"
#if TARGET_PC  // additional actor attribute integration
#include "d/actor/d_a_mirror.h"
#endif
#include "d/actor/d_a_nbomb.h"
#include "d/actor/d_a_obj_pillar.h"
#include "d/d_com_inf_game.h"
#if !TARGET_PC
#include "d/actor/d_a_alink.h"
#include "d/actor/d_a_mirror.h"
#endif
#include "d/d_s_play.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"
#endif
#include "f_op/f_op_msg_mng.h"
#if TARGET_PC  // additional actor attribute integration

namespace actor_attr = dusk::mods::svc::actor_attr;
#else
#include "SSystem/SComponent/c_math.h"
#include "Z2AudioLib/Z2Instances.h"
#include "JSystem/JAudio2/JAUSectionHeap.h"
#include <cmath>
#endif

enum Joint {
    /* 0x00 */ JNT_CENTER,
    /* 0x01 */ JNT_BACKBONE,
    /* 0x02 */ JNT_NECK,
    /* 0x03 */ JNT_HEAD,
    /* 0x04 */ JNT_CHIN,
    /* 0x05 */ JNT_HELMET,
    /* 0x06 */ JNT_TONGUE1,
    /* 0x07 */ JNT_TONGUE2,
    /* 0x08 */ JNT_TONGE3,
    /* 0x09 */ JNT_TONGUE4,
    /* 0x0A */ JNT_TONGUE5,
    /* 0x0B */ JNT_MOUTH,
    /* 0x0C */ JNT_SHOULDERL,
    /* 0x0D */ JNT_ARML1,
    /* 0x0E */ JNT_ARML2,
    /* 0x0F */ JNT_ARML3,
    /* 0x10 */ JNT_ARML4,
    /* 0x11 */ JNT_HIRALB1,
    /* 0x12 */ JNT_HIRALB2,
    /* 0x13 */ JNT_HIRALF1,
    /* 0x14 */ JNT_HIRALF2,
    /* 0x15 */ JNT_SPADL,
    /* 0x16 */ JNT_SHOULDERR,
    /* 0x17 */ JNT_ARMR1,
    /* 0x18 */ JNT_ARMR2,
    /* 0x19 */ JNT_ARMR3,
    /* 0x1A */ JNT_ARMR4,
    /* 0x1B */ JNT_HIRARB1,
    /* 0x1C */ JNT_HIRARB2,
    /* 0x1D */ JNT_HIRARF1,
    /* 0x1E */ JNT_HIRARF2,
    /* 0x1F */ JNT_SPADR,
    /* 0x20 */ JNT_WAIST,
    /* 0x21 */ JNT_LEGL1,
    /* 0x22 */ JNT_LEGL2,
    /* 0x23 */ JNT_FOOTL,
    /* 0x24 */ JNT_LEGR1,
    /* 0x25 */ JNT_LEGR2,
    /* 0x26 */ JNT_FOOTR,
    /* 0x27 */ JNT_TAREB1,
    /* 0x28 */ JNT_TAREB2,
    /* 0x29 */ JNT_TAREF1,
    /* 0x2A */ JNT_TAREF2,
};

enum OPENING_MODE {
    MODE_START_DEMO,
    MODE_START_DEMO_WAIT,
    MODE_PAN_GROUND,

    MODE_MSG_1 = 4,
    MODE_MSG_1_WAIT,
    MODE_WARP_OUT_SE,
    MODE_START_WARP,

    MODE_WARP_WAIT = 100,
    MODE_PAN_THRONE,
    MODE_ZOOM_THRONE,
    MODE_MSG_2,
    MODE_FLY_UP,
    MODE_CLOSE_UP,
    MODE_SET_BOSS_TITLE,

    MODE_WARP_IN_SE = 20,
    MODE_WARP_IN_SCALE,
    MODE_WARP_IN_WAIT,
    MODE_ZOOM_OUT_ROOM_CHANGE,
    MODE_START_ROOM_CHANGE,
    MODE_ROOM_CHANGE,
    MODE_END_ROOM_CHANGE,
    MODE_END_DEMO,
};

static u8 const lit_3757[12] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

namespace {
dCcD_SrcSph cc_zant_src = {
    {
        {0x0, {{0x400, 0x1, 0x4}, {0xD8FBFDFF, 0x43}, 0x75}}, // mObj
        {dCcD_SE_METAL, 0x0, 0x1, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

dCcD_SrcSph cc_zant_sword_src = {
    {
        {0x0, {{0x400, 0x1, 0x4}, {0xD8FBFDFF, 0x0}, 0x0}}, // mObj
        {dCcD_SE_METAL, 0x0, 0x0, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

dCcD_SrcCyl cc_zant_roll_src = {
    {
        {0x0, {{0x400, 0x2, 0x4}, {0xD8FBFDFF, 0x42}, 0x74}},  // mObj
        {dCcD_SE_METAL, 0x0, 0x6, 0x0, 0x0},                 // mGObjAt
        {dCcD_SE_NONE, 0x2, 0x0, 0x0, 0x303},                 // mGObjTg
        {0x0},                                              // mGObjCo
    },     
    {                                                 // mObjInf
        {
            {0.0f, 0.0f, 0.0f},  // mCenter
            40.0f,              // mRadius
            40.0f                // mHeight
        }                        // mCyl
    }
};

dCcD_SrcSph cc_zant_foot_src = {
    {
        {0x0, {{0x0, 0x0, 0x0}, {0x400000, 0x3}, 0x0}}, // mObj
        {dCcD_SE_METAL, 0x0, 0xA, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x106}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

dCcD_SrcSph cc_zant_foot_src2 = {
    {
        {0x0, {{0x400, 0x2, 0x1E}, {0xD8BBFDFF, 0x43}, 0x75}}, // mObj
        {dCcD_SE_METAL, 0x0, 0xD, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x2, 0x0, 0x0, 0x3}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

dCcD_SrcSph cc_zant_camera_src = {
    {
        {0x0, {{0x400, 0x2, 0x1E}, {0xD8BBFDFF, 0x0}, 0x5}}, // mObj
        {dCcD_SE_METAL, 0x0, 0xD, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x2, 0x0, 0x0, 0x3}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

static s8 warp_next_room[] = {
    50,
    53,
    54,
    55,
    56,
    57,
    60,
};
}

daB_ZANT_HIO_c::daB_ZANT_HIO_c() {
    field_0x4 = -1;
    mModelSize = 1.0f;
    mMahojinWaitTime = 10.0f;
    mBulletNum = 15.0f;
    mAttackAnmSpeed = 1.7f;
    mBulletSpeed = 100.0f;
    mBulletSpeedUnderwater = 40.0f;
    mDemoWarpTime = 22.0f;
    mPlayWarpTime = 0.0f;
    mSwordAttackSize = 1.2f;
    mMahojinSize = 0.5f;
    mMahojinOffsetX = 0.0f;
    mMahojinOffsetY = 200.0f;
    mMahojinOffsetZ = -100.0f;
    mAppearAnmSpeed = 2.0f;
    mDisappearAnmSpeed = 0.5f;
}

int daB_ZANT_c::ctrlJoint(J3DJoint* i_joint, J3DModel* i_model) {
    u16 jnt_no = i_joint->getJntNo();

    mDoMtx_stack_c::copy(i_model->getAnmMtx(jnt_no));

    switch (jnt_no) {
    case JNT_BACKBONE:
        mDoMtx_stack_c::ZrotM(mBackboneRotZ);
        break;
    case JNT_NECK:
        mDoMtx_stack_c::ZXYrotM(mNeckRotX, 0, mNeckRotZ);
        break;
    }

    i_model->setAnmMtx(jnt_no, mDoMtx_stack_c::get());
    cMtx_copy(mDoMtx_stack_c::get(), J3DSys::mCurrentMtx);
    return 1;
}

int daB_ZANT_c::JointCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        J3DModel* model = j3dSys.getModel();
        daB_ZANT_c* actor = (daB_ZANT_c*)model->getUserArea();

        if (actor != NULL) {
            actor->ctrlJoint(i_joint, model);
        }
    }

    return 1;
}

namespace {
static int const iron_tg_cc[] = {
    0,
    1,
    2,
    4,
    5,
    6,
};
}

int daB_ZANT_c::draw() {
    J3DModel* model = mpModelMorf->getModel();

    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(model, &tevStr);

    mpModelMorf->entryDL();
    daMirror_c::entry(model);

    cXyz sp38;
    sp38.set(current.pos.x, current.pos.y + 100.0f, current.pos.z);

    f32 var_f31;
    if (mFightPhase == PHASE_YO) {
        var_f31 = 2000.0f - current.pos.y;
        if (var_f31 < 0.0f) {
            var_f31 = 0.0f;
        }

        tevStr.field_0x344 = (var_f31 * 3.0f) / 2000.0f;
        if (tevStr.field_0x344 >= 1.0f) {
            tevStr.field_0x344 = 1.0f;
        }

        mShadowKey = dComIfGd_setShadow(mShadowKey, 0, model, &sp38, 1500.0f, 0.0f, current.pos.y, mAcch.GetGroundH(), mAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    } else {
        mShadowKey = dComIfGd_setShadow(mShadowKey, 1, model, &sp38, 800.0f, 0.0f, current.pos.y, mAcch.GetGroundH(), mAcch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }

    if (mDrawSwords) {
        g_env_light.setLightTevColorType_MAJI(mpSwordLModel, &tevStr);
        mDoExt_modelUpdateDL(mpSwordLModel);
        dComIfGd_addRealShadow(mShadowKey, mpSwordLModel);

        g_env_light.setLightTevColorType_MAJI(mpSwordRModel, &tevStr);
        mDoExt_modelUpdateDL(mpSwordRModel);
        dComIfGd_addRealShadow(mShadowKey, mpSwordRModel);
    }

    if (mMahojinAnmMode != 0) {
        g_env_light.setLightTevColorType_MAJI(mpMahojinModel, &tevStr);
        mpMahojinEndBrk->entry(mpMahojinModel->getModelData());
        mpMahojinBtk->entry(mpMahojinModel->getModelData());
        mpMahojinStartBtk->entry(mpMahojinModel->getModelData());
        
        mDoExt_modelUpdateDL(mpMahojinModel);

        if (mMahojin2AnmMode) {
            g_env_light.setLightTevColorType_MAJI(mpMahojinModel2, &tevStr);
            mpMahojinBrk2->entry(mpMahojinModel2->getModelData());
            mpMahojinStartBtk2->entry(mpMahojinModel2->getModelData());
            
            mDoExt_modelUpdateDL(mpMahojinModel2);
        }
    }

    return 1;
}

static int daB_ZANT_Draw(daB_ZANT_c* i_this) {
    return i_this->draw();
}

void daB_ZANT_c::setBck(int i_resID, u8 i_attr, f32 i_morf, f32 i_speed) {
#if TARGET_PC  // enemy attribute integration

    const actor_attr::ActionAnimationParams actorAttributeAnimation =
        actor_attr::enemy_animation_params(this, i_morf, i_speed);
    i_morf = actorAttributeAnimation.morph;
    i_speed = actorAttributeAnimation.playbackSpeed;
    mpModelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", i_resID), i_attr, i_morf,
        i_speed, 0.0f, -1.0f);
#else
    mpModelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", i_resID), i_attr, i_morf, i_speed, 0.0f, -1.0f);
#endif
}

bool daB_ZANT_c::checkBck(int i_resID) {
    return mpModelMorf->getAnm() == (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", i_resID) ? TRUE : FALSE;
}

void daB_ZANT_c::setActionMode(int i_action, int i_mode) {
    mSwordSize = 1.0f;
    field_0x701 = 0;
    field_0x702 = 1;
    field_0x717 = 0;

    mLastAction = mAction;
    mAction = i_action;
    mMode = i_mode;
}

bool daB_ZANT_c::checkBigDamage() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    BOOL taken_big_dmg = false;

    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_NORMAL_SWORD) && daPy_py_c::checkMasterSwordEquip()) {
        if (mAtInfo.mpCollider->GetAtAtp() >= 4) {
            taken_big_dmg = true;
        } else if (player->getSwordAtUpTime() != 0) {
            taken_big_dmg = true;
        } else if (player->getCutCount() >= 4) {
            taken_big_dmg = true;
        }
    }

    return taken_big_dmg;
}

int daB_ZANT_c::checkDamageType() {
    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) && mFightPhase == PHASE_OI) {
        return DMGTYPE_HOOK_OI;
    }

    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) ||
        mAtInfo.mpCollider->ChkAtType(AT_TYPE_SPINNER) ||
        mAtInfo.mpCollider->ChkAtType(AT_TYPE_ARROW) ||
        mAtInfo.mpCollider->ChkAtType(AT_TYPE_SHIELD_ATTACK))
    {
        return DMGTYPE_OBJ;
    }

    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG) || mAtInfo.mpCollider->ChkAtType(AT_TYPE_40)) {
        return DMGTYPE_BOOMERANG;
    }

    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_MASTER_SWORD)) {
        return DMGTYPE_SWORD;
    }

    return DMGTYPE_MISC;
}

void daB_ZANT_c::setDamageSe(dCcD_Sph* i_hitSph, int i_dmgAmount) {
    health -= i_dmgAmount;
    if (health < 0) {
        health = 0;
    }

    BOOL var_r29;
    u8 at_se = ((dCcD_GObjInf*)mAtInfo.mpCollider)->GetAtSe();

    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT) && !fopAcM_CheckStatus(this, fopAcStts_UNK_0x200000_e | fopAcStts_UNK_0x80000_e)) {
        var_r29 = 1;
    } else {
        var_r29 = 0;
    }

    u32 var_r30 = 30;
    if (mAtInfo.mHitStatus == 1) {
        var_r30 = 31;
    } else if (mAtInfo.mHitStatus == 2) {
        var_r30 = 32;
    }

    mSound.startCollisionSE(i_hitSph->getHitSeID(at_se, var_r29), var_r30);

    if (mAtInfo.mHitStatus == 0) {
        dComIfGp_setHitMark(1, this, i_hitSph->GetTgHitPosP(), NULL, NULL, 0);
    } else {
        dComIfGp_setHitMark(3, this, i_hitSph->GetTgHitPosP(), NULL, NULL, 0);
    }
}

static u8 hio_set;

static daB_ZANT_HIO_c l_HIO;

namespace {
static cXyz fly_warp_pos[] = {
    cXyz(0.0f, 400.0f, 1000.0f),
    cXyz(1000.0f, 400.0f, 1500.0f),
    cXyz(-1000.0f, 400.0f, 1500.0f),
};
}

void daB_ZANT_c::damage_check() {
    field_0x9a4.Move();

    if (mAction != ACT_ROOM_CHANGE) {
        mAtInfo.mpSound = NULL;
        
        if (field_0x702 != 0) {
            mBodySphCc[0].OnTgNoHitMark();
            mBodySphCc[1].OnTgNoHitMark();

            if (mBodySphCc[0].ChkTgShield()) {
                mAtInfo.mpSound = &mSound;
            }
        }

        mAtInfo.mpCollider = NULL;

        dCcD_Sph tg_hit_sph;
        if (mBodySphCc[0].ChkTgHit()) {
            mAtInfo.mpCollider = mBodySphCc[0].GetTgHitObj();
            tg_hit_sph = mBodySphCc[0];
        } else if (mBodySphCc[1].ChkTgHit()) {
            mAtInfo.mpCollider = mBodySphCc[1].GetTgHitObj();
            tg_hit_sph = mBodySphCc[1];
        }

        if (mAtInfo.mpCollider != NULL) {
            daPy_py_c* player = daPy_getPlayerActorClass();

            if (tg_hit_sph.ChkTgShield()) {
                mAtInfo.field_0x18 = 42;
                mAtInfo.mpCollider->SetAtAtp(0);
            } else {
                mAtInfo.field_0x18 = 0;
            }

            s16 prev_hp = health;
            int dmg_amount = 0;

            if (field_0x702 != 0) {
                health = DUSK_IF_ELSE(actor_attr::enemy_health_value(this, 280), 280);
                cc_at_check(this, &mAtInfo);
            } else {
                cc_at_check(this, &mAtInfo);
                dmg_amount = prev_hp - health;
            }

            health = prev_hp;

            if (!mAtInfo.mpCollider->ChkAtType(AT_TYPE_MASTER_SWORD)) {
                dScnPly_c::setPauseTimer(0);
            }

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK | AT_TYPE_WOLF_CUT_TURN | AT_TYPE_10000000 | AT_TYPE_MIDNA_LOCK)) {
				field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            } else {
                field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
            }

            if (mAtInfo.mAttackPower <= 1) {
                field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
            }

            mTakenBigDmg = checkBigDamage();

            switch (mFightPhase) {
            case PHASE_BB:
                if (field_0x70c == 0) {
                    setActionMode(ACT_FLY, 10);
                } else {
                    switch (checkDamageType()) {
                    case DMGTYPE_SWORD:
                        setDamageSe(&tg_hit_sph, dmg_amount);

                        if (mAction != ACT_DAMAGE) {
                            field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
                        }

                        setActionMode(ACT_DAMAGE, pl_cut_LRC(player->getCutType()));
                        break;
                    case DMGTYPE_MISC:
                        field_0x70c = 0;
                        gravity = 0.0f;

                        mFlyWarpPosID = (f32)mFlyWarpPosID + cM_rndF(1.9f) + 1.0f;
                        mFlyWarpPos = fly_warp_pos[mFlyWarpPosID % 3];
                        field_0x711 = 2;

                        setActionMode(ACT_WARP, 1);
                        break;
                    case DMGTYPE_BOOMERANG:
                        setActionMode(ACT_CONFUSE, 5);
                        break;
                    case DMGTYPE_OBJ:
                        setActionMode(ACT_CONFUSE, 0);
                        break;
                    }
                }
                break;
            case PHASE_MG:
                switch (checkDamageType()) {
                case DMGTYPE_SWORD:
                    setDamageSe(&tg_hit_sph, dmg_amount);

                    if (mAction != ACT_DAMAGE) {
                        field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
                    }

                    setActionMode(ACT_DAMAGE, pl_cut_LRC(player->getCutType()));
                    break;
                case DMGTYPE_MISC:
                    setActionMode(ACT_SIMA_JUMP, 11);
                    break;
                case DMGTYPE_BOOMERANG:
                    setActionMode(ACT_CONFUSE, 5);
                    break;
                case DMGTYPE_OBJ:
                    setActionMode(ACT_CONFUSE, 0);
                    break;
                }
                break;
            case PHASE_OI:
                switch (checkDamageType()) {
                case DMGTYPE_SWORD:
                    setDamageSe(&tg_hit_sph, dmg_amount);

                    if (mAction != ACT_DAMAGE) {
                        field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
                    }

                    setActionMode(ACT_DAMAGE, pl_cut_LRC(player->getCutType()));
                    break;
                case DMGTYPE_HOOK_OI:
                    setActionMode(ACT_HOOK, 0);
                    break;
                }
                break;
            case PHASE_MK:
                if (field_0x707 != 0) {
                    if (mAction != ACT_MONKEY_FALL) {
                        if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG) || mAtInfo.mpCollider->ChkAtType(AT_TYPE_40)) {
                            setActionMode(ACT_MONKEY_FALL, 20);
                        } else {
                            setActionMode(ACT_CONFUSE, 0);
                        }
                    } else {
                        setActionMode(ACT_MONKEY_FALL, 10);
                    }
                } else {
                    switch (checkDamageType()) {
                    case DMGTYPE_SWORD:
                        setDamageSe(&tg_hit_sph, dmg_amount);
                        setActionMode(ACT_MONKEY_DAMAGE, 0);
                        break;
                    case DMGTYPE_MISC:
                        setNearPillarPos();
                        field_0x711 = 0;
                        setActionMode(ACT_WARP, 1);
                        break;
                    case DMGTYPE_BOOMERANG:
                    case DMGTYPE_OBJ:
                        field_0x6f0 = 0;
                        setActionMode(ACT_MONKEY_DAMAGE, 0);
                        break;
                    }
                }
                break;
            case PHASE_LAST:
                if (!tg_hit_sph.ChkTgShield()) {
                    switch (checkDamageType()) {
                    case DMGTYPE_SWORD:
                        setDamageSe(&tg_hit_sph, dmg_amount);

                        if (mAction == ACT_LAST_ATTACK && mMode == 13) {
                            mAction = ACT_LAST_TIRED;
                        }

                        setActionMode(ACT_LAST_DAMAGE, pl_cut_LRC(player->getCutType()));
                        break;
                    case DMGTYPE_MISC:
                        mSwordCc[0].OffAtSetBit();
                        mSwordCc[1].OffAtSetBit();
                        setTgHitBit(0);
                        setLastWarp(1, 0);
                        break;
                    case DMGTYPE_BOOMERANG:
                        setActionMode(ACT_LAST_DAMAGE, 20);
                        break;
                    case DMGTYPE_OBJ:
                        setActionMode(ACT_LAST_DAMAGE, 10);
                        break;
                    case DMGTYPE_HOOK_OI:
                        break;
                    }
                }
                break;
            }

            mBodySphCc[0].ClrTgHit();
            mBodySphCc[1].ClrTgHit();
            return;
        }
    }
}

void daB_ZANT_c::ice_damage_check() {
    field_0xc74.Move();

    if (field_0x6e4 == 0) {
        s16 prev_hp = health;
        mAtInfo.mpCollider = NULL;

        for (int i = 0; i < 6; i++) {
            int tg_idx = iron_tg_cc[i];

            if (mFootCc[tg_idx].ChkTgHit()) {
                field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                mAtInfo.mpCollider = mFootCc[tg_idx].GetTgHitObj();

                if (!mFootCc[tg_idx].ChkTgShield()) {
                    if (mAction == ACT_ICE_STEP || mAction == ACT_ICE_JUMP) {
                        setDamageSe(&mFootCc[tg_idx], 0);

                        if (i < 2) {
                            setActionMode(ACT_ICE_DAMAGE, 0);
                        } else {
                            setActionMode(ACT_ICE_DAMAGE, 1);
                        }
                    } else if (mAction == ACT_ICE_DAMAGE) {
                        setActionMode(ACT_ICE_DAMAGE, 30);
                        mModeTimer = 0;
                    }
                }

                for (int j = 0; j < 6; j++) {
                    mFootCc[iron_tg_cc[j]].ClrTgHit();
                }

                return;
            }
        }

        for (int i = 0; i < 11; i++) {
            if (mFoot2Cc[i].ChkTgHit()) {
                if (field_0x6e4 == 0) {
                    mAtInfo.mpCollider = mFoot2Cc[i].GetTgHitObj();

                    if (mFoot2Cc[i].ChkTgShield()) {
                        mAtInfo.field_0x18 = 42;
                        mAtInfo.mpCollider->SetAtAtp(0);
                    } else {
                        mAtInfo.field_0x18 = 0;
                    }

                    cc_at_check(this, &mAtInfo);

                    int dmg_amount = prev_hp - health;
                    health = prev_hp;

                    if (!mAtInfo.mpCollider->ChkAtType(AT_TYPE_MASTER_SWORD)) {
                        dScnPly_c::setPauseTimer(0);
                    }

                    if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_WOLF_ATTACK | AT_TYPE_WOLF_CUT_TURN | AT_TYPE_10000000 | AT_TYPE_MIDNA_LOCK)) {
                        field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
                    } else {
                        field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                    }

                    if (mAtInfo.mAttackPower <= 1) {
                        field_0x6e4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                    }

                    mTakenBigDmg = checkBigDamage();

                    if (mAction == ACT_ICE_DAMAGE && mAtInfo.field_0x18 != 42) {
                        switch (checkDamageType()) {
                        case DMGTYPE_BOOMERANG:
                            setActionMode(ACT_ICE_DAMAGE, 40);
                            mModeTimer = 0;
                            break;
                        case DMGTYPE_OBJ:
                            setActionMode(ACT_ICE_DAMAGE, 20);
                            mModeTimer = 0;
                            break;
                        case DMGTYPE_MISC:
                            setActionMode(ACT_ICE_DAMAGE, 30);
                            mModeTimer = 0;
                            break;
                        case DMGTYPE_SWORD:
                            setDamageSe(&mFoot2Cc[i], dmg_amount);
                            setActionMode(ACT_ICE_DAMAGE, 10);
                            break;
                        }
                    }
                }

                mFoot2Cc[i].ClrTgHit();
            }
        }
    }
}

bool daB_ZANT_c::setNextDamageMode(BOOL i_checkHealth) {
#if TARGET_PC  // enemy attribute integration

    // Phase progression stays cycle-based. Derive the randomized health
    // multiplier from Zant's stored maximum HP (600 is his vanilla maximum),
    // rather than re-querying a phase-local 280 HP value. For rolls above
    // vanilla, add 50% of the health multiplier to the vanilla two cycles and
    // round it. This makes a 400% HP Zant require exactly four completed
    // damage/stun cycles before changing phase.
    const f32 health_mul = (f32)field_0x560 / 600.0f;
    int required_cycles = 2;
    if (health_mul > 1.0f) {
        required_cycles += (int)std::floor((health_mul * 0.5f) + 0.5f);
    }

    if (health <= 0) {
#else
    if (i_checkHealth) {
        if (mFightCycle == 0 && health < 140) {
            mFightCycle++;
            setBaseActionMode(2);
            return true;
        }
    } else if (mFightCycle == 0) {
        if (mTakenBigDmg) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
            mFightCycle++;
            setBaseActionMode(2);
            return true;
        }
    } else if (health <= 0 || mTakenBigDmg) {
#endif
        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
        mFightCycle = 0;
        setActionMode(ACT_ROOM_CHANGE, 0);
        return true;
    }

#if TARGET_PC  // enemy attribute integration
    if (!i_checkHealth && mTakenBigDmg) {
        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);

        if ((int)mFightCycle + 1 >= required_cycles) {
            mFightCycle = 0;
            setActionMode(ACT_ROOM_CHANGE, 0);
        } else {
            mFightCycle++;
            setBaseActionMode(2);
        }
        return true;
    }

    // Do not grant the vanilla half-health "free" cycle here. With randomized
    // health, phase progression is strictly based on completed damage/stun
    // cycles so a 400% health roll that requires four cycles really takes four.

#endif
    return false;
}

static void* s_obj_sub(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && !fopAcM_checkCarryNow((fopAc_ac_c*)i_actor)) {
            f32 obj_dist = fopAcM_searchActorDistanceXZ((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_data);
            if (obj_dist < 300.0f && fopAcM_GetSpeed((fopAc_ac_c*)i_actor).y) {
                if (fopAcM_GetName(i_actor) == fpcNm_Obj_Carry_e) {
                    return i_actor;
                }

                if (fopAcM_GetName(i_actor) == fpcNm_NBOMB_e) {
                    return i_actor;
                }
            }

            if (obj_dist < 300.0f && fopAcM_GetName(i_actor) == fpcNm_NBOMB_e && ((daNbomb_c*)i_actor)->getExTime() < 10) {
                return i_actor;
            }

            if (obj_dist < 700.0f && fopAcM_GetName(i_actor) == fpcNm_ARROW_e && fopAcM_GetSpeedF((fopAc_ac_c*)i_actor)) {
                s16 actor_angle = ((fopAc_ac_c*)i_actor)->current.angle.y;
                s16 angle_to_boss = fopAcM_searchActorAngleY((fopAc_ac_c*)i_actor, (fopAc_ac_c*)i_data);
                if (abs((s16)(actor_angle - angle_to_boss)) < 0x2000) {
                    return i_actor;
                }
            }
        }
    }

    return NULL;
}

bool daB_ZANT_c::checkAvoidWeapon(BOOL i_allowBoomerang) {
    if (i_allowBoomerang == 2) {
        return FALSE;
    }

    daPy_py_c* player = daPy_getPlayerActorClass();

    // avoid bomb, arrow, carry obj
    fopAc_ac_c* obj = (fopAc_ac_c*)fpcM_Search(s_obj_sub, this);
    if (obj != NULL) {
        return TRUE;
    }

    // avoid clawshot
    if (dComIfGp_checkPlayerStatus0(0, 0x4000)) {
        cXyz* ppos = player->getHookshotTopPos();
        if (ppos != NULL && ppos->absXZ(current.pos) < 300.0f) {
            return TRUE;
        }
    }

    // avoid ball and chain
    cXyz* ppos = player->getIronBallCenterPos();
    if (ppos != NULL) {
        daAlink_c* player_alink = daAlink_getAlinkActorClass();
        if ((player_alink->checkIronBallThrowMode() || player_alink->checkIronBallThrowReturnMode()) && !player->checkIronBallGroundStop()) {
            if (ppos->absXZ(current.pos) < 300.0f) {
                return TRUE;
            }
        }
    }

    // avoid boomerang
    if (!i_allowBoomerang && player->getThrowBoomerangActor() != NULL) {
        fopAc_ac_c* pboomerang = (fopAc_ac_c*)player->getThrowBoomerangActor();
        if (pboomerang->current.pos.absXZ(current.pos) < 300.0f) {
            return TRUE;
        }
    }

    return FALSE;
}

void daB_ZANT_c::setTgHitBit(BOOL i_onBit) {
    if (i_onBit) {
        mBodySphCc[0].OnTgSetBit();
        mBodySphCc[1].OnTgSetBit();
    } else {
        mBodySphCc[0].OffTgSetBit();
        mBodySphCc[1].OffTgSetBit();
    }
}

void daB_ZANT_c::setCoHitBit(BOOL i_onBit) {
    if (i_onBit) {
        mBodySphCc[0].OnCoSetBit();
        mBodySphCc[1].OnCoSetBit();
    } else {
        mBodySphCc[0].OffCoSetBit();
        mBodySphCc[1].OffCoSetBit();
    }
}

void daB_ZANT_c::setTgShield(BOOL i_onShield) {
    if (i_onShield) {
        mBodySphCc[0].OnTgShield();
        mBodySphCc[0].OnTgSpinnerReflect();
        mBodySphCc[0].OnTgIronBallRebound();

        mBodySphCc[1].OnTgShield();
        mBodySphCc[1].OnTgSpinnerReflect();
        mBodySphCc[1].OnTgIronBallRebound();

        mBodySphCc[0].OffTgNoHitMark();
        mBodySphCc[1].OffTgNoHitMark();

        mBodySphCc[0].SetTgHitMark(CcG_Tg_UNK_MARK_2);
        mBodySphCc[1].SetTgHitMark(CcG_Tg_UNK_MARK_2);
    } else {
        mBodySphCc[0].OffTgShield();
        mBodySphCc[0].OffTgSpinnerReflect();
        mBodySphCc[0].OffTgIronBallRebound();

        mBodySphCc[1].OffTgShield();
        mBodySphCc[1].OffTgSpinnerReflect();
        mBodySphCc[1].OffTgIronBallRebound();

        mBodySphCc[0].SetTgHitMark(CcG_Tg_UNK_MARK_0);
        mBodySphCc[1].SetTgHitMark(CcG_Tg_UNK_MARK_0);
    }
}

void daB_ZANT_c::setTgType(u32 i_type) {
    mBodySphCc[0].SetTgType(i_type);
    mBodySphCc[1].SetTgType(i_type);
}

void daB_ZANT_c::setZantMessage(int i_msgNo) {
    mMsgNo = i_msgNo;
    mMsgID = fopMsgM_messageSet(i_msgNo, 1000);
}

int daB_ZANT_c::doZantMessage() {
    if (mpMsg != NULL) {
        if (mpMsg->mode == 14) {
            mpMsg->mode = 16;
        } else if (mpMsg->mode == 18) {
            mpMsg->mode = 19;
            mMsgID = fpcM_ERROR_PROCESS_ID_e;
            return 1;
        }
    } else {
        mpMsg = fopMsgM_SearchByID(mMsgID);
    }

    return 0;
}

void daB_ZANT_c::setIceLandingEffect(BOOL i_landFootR) {
    static u16 l_landing_effect_id[] = {
        0x86DC, 0x86DD, 0x86DE, 0x86DF, 0x86E0, 0x86E1,
    };

    cXyz particle_pos;
    if (!i_landFootR) {
        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTL));
    } else {
        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTR));
    }

    mDoMtx_stack_c::transM(10.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&particle_pos);
    particle_pos.y = 2.0f;

    cXyz size(1.0f, 1.0f, 1.0f);
    for (int i = 0; i < 6; i++) {
        dComIfGp_particle_set(l_landing_effect_id[i], &particle_pos, &tevStr, &shape_angle, &size);
    }

    particle_pos.set(0.0f, 0.0f, 0.0f);
    dComIfGp_particle_set(0x86E2, &particle_pos, &tevStr, &shape_angle, NULL);
}

void daB_ZANT_c::setWaterBubble() {
    cXyz particle_pos;
    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_MOUTH));
    mDoMtx_stack_c::multVecZero(&particle_pos);

    field_0x389c[0] = dComIfGp_particle_set(field_0x389c[0], 0x1E8, &particle_pos, &tevStr);
}

void daB_ZANT_c::setMonkeyFallEffect() {
    static u16 l_landing_effect_id[] = {0x8901, 0x8902, 0x8903};

    for (int i = 0; i < 3; i++) {
        dComIfGp_particle_set(l_landing_effect_id[i], &current.pos, &tevStr, &shape_angle, NULL);
    }
}

void daB_ZANT_c::setLastRollEffect() {
    static u16 l_roll_effect_id[] = {0x8904, 0x8905};

    mSound.startCreatureSoundLevel(Z2SE_EN_ZAN_CTL_SPIN_ATK, 0, -1);

    for (int i = 0; i < 2; i++) {
        field_0x38ac[i] = dComIfGp_particle_set(field_0x38ac[i], l_roll_effect_id[i], &current.pos, &tevStr, &shape_angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
    }

    fopAcM_effSmokeSet2(&field_0x3894, &field_0x3898, &current.pos, NULL, 2.0f, &tevStr);
}

static int target_info_count;

static void* s_pillar_sub(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_Obj_Pillar_e) {
            if (((daPillar_c*)i_actor)->getMdlType() != 0) {
                ((daB_ZANT_c*)i_data)->mPillarIDs[8] = fopAcM_GetID(i_actor);
            } else {
                ((daB_ZANT_c*)i_data)->mPillarIDs[target_info_count] = fopAcM_GetID(i_actor);
                target_info_count++;
            }
        }
    }

    return NULL;
}

f32 daB_ZANT_c::getMagicSpeed() {
    return l_HIO.mBulletSpeed;
}

f32 daB_ZANT_c::getMagicWaterSpeed() {
    return l_HIO.mBulletSpeedUnderwater;
}

void daB_ZANT_c::executeSmallAttack() {
    cXyz sp44;
    s16 aim_target_angle = fopAcM_searchPlayerAngleY(this);

    if (mFightPhase == PHASE_OI) {
        fopAc_ac_c* pmobile;
        fopAcM_SearchByID(mMobileIDs[mCorrectMobileNo], &pmobile);

        if (pmobile != NULL) {
            s16 var_r26 = pmobile->shape_angle.y - aim_target_angle;
            if (abs(var_r26) > 0x1400) {
                if (var_r26 < 0) {
                    aim_target_angle = pmobile->shape_angle.y + 0x1000;
                } else {
                    aim_target_angle = pmobile->shape_angle.y - 0x1000;
                }
            }
        }
    }

    switch (mMode) {
    case 0:
        setTgHitBit(TRUE);
        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        mMode = 2;
        field_0x6fd = 0;

        if (field_0x711 != 0) {
            setBck(BCK_ZAN_MAGICSHOOTA_B, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        } else {
            setBck(BCK_ZAN_MAGICSHOOTA_B_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        }

        field_0x717 = 1;
        break;
    case 2:
        if (field_0x711 != 0) {
            if (mpModelMorf->checkFrame(15)) {
                mSound.startCreatureVoice(Z2SE_EN_ZAN_V_ATK_BALL, -1);
            }
        } else {
            if (mpModelMorf->checkFrame(16)) {
                mSound.startCreatureVoice(Z2SE_EN_ZAN_V_ATK_BALL, -1);
            }
        }

        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, aim_target_angle, 8, 0x400, 0x80), cLib_addCalcAngleS(&shape_angle.y, aim_target_angle, 8, 0x400, 0x80));
        if (mpModelMorf->isStop()) {
            f32 anm_speed = l_HIO.mAttackAnmSpeed;
            if (mFightPhase == PHASE_OI) {
                anm_speed = 1.0f;
            }

            if (field_0x711 != 0) {
                setBck(BCK_ZAN_MAGICSHOOTA_C, J3DFrameCtrl::EMode_LOOP, 3.0f, anm_speed);
            } else {
                setBck(BCK_ZAN_MAGICSHOOTA_B_B, J3DFrameCtrl::EMode_LOOP, 3.0f, anm_speed);
            }
    
            mMode = 3;
            field_0x6fd = 0;
        }
        break;
    case 3:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, aim_target_angle, 8, 0x400, 0x80), cLib_addCalcAngleS(&shape_angle.y, aim_target_angle, 8, 0x400, 0x80));

        if (mpModelMorf->checkFrame(3) || mpModelMorf->checkFrame(13)) {
            if (mpModelMorf->checkFrame(13)) {
                mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_ARML4));
            } else {
                mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_ARMR4));
            }

            mDoMtx_stack_c::multVecZero(&sp44);

            // Keep the projectile spawn position exactly on the scaled hand.
            // Oversized Zant can put that hand far above Link, so pre-seed only
            // the projectile pitch toward the same target that B_ZANTM uses on
            // its first frame. This avoids the projectile's 0x2000 first-frame
            // pitch correction limit without moving the shot down Zant's body.
            u32 parameter = field_0x6fd + 1;
            if (field_0x703 < 10) {
                parameter += 2;
            }

            if (parameter > 6) {
                parameter = 6;
            }

#if TARGET_PC  // enemy attribute integration
            csXyz projectile_angle = shape_angle;
            if (actor_attr::enemy_size_multiplier(this) > 1.0f) {
                cXyz target_pos = daPy_getPlayerActorClass()->current.pos;
                target_pos.y += parameter * 60.0f - 260.0f;
                projectile_angle.x = cLib_targetAngleX(&sp44, &target_pos);
            }

            fopAcM_createChild(fpcNm_B_ZANTM_e, fopAcM_GetID(this), parameter, &sp44, fopAcM_GetRoomNo(this), &projectile_angle, NULL, -1, NULL);
#else
            fopAcM_createChild(fpcNm_B_ZANTM_e, fopAcM_GetID(this), parameter, &sp44, fopAcM_GetRoomNo(this), &shape_angle, NULL, -1, NULL);
#endif
            dComIfGp_particle_set(0x886B, &sp44, &shape_angle, NULL);

            field_0x6fd++;
            field_0x704++;
        } else if ((mpModelMorf->checkFrame(9) || mpModelMorf->checkFrame(19)) && field_0x6fd >= field_0x703) {
            mMode = 4;

            if (field_0x711 != 0) {
                setBck(BCK_ZAN_MAGICSHOOTA_D, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            } else {
                setBck(BCK_ZAN_MAGICSHOOTA_B_C, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            }

            field_0x717 = 0;
        }
        break;
    case 4:
        if (mpModelMorf->isStop()) {
            attention_info.flags = fopAc_AttnFlag_BATTLE_e;
            
            if (mFightPhase == PHASE_BB) {
                field_0x711 = 1;
                setActionMode(ACT_WARP, 1);
            } else if (mFightPhase == PHASE_MG) {
                setActionMode(ACT_SIMA_JUMP, 0);
            } else if (mFightPhase == PHASE_OI) {
                setActionMode(ACT_WATER, 0);
            } else if (mFightPhase == PHASE_MK) {
                setActionMode(ACT_MONKEY, 0);
            }
        }
        break;
    }

    if (mFightPhase == PHASE_BB && checkAvoidWeapon(TRUE)) {
        field_0x711 = 1;
        setActionMode(ACT_WARP, 1);
    }
}

bool daB_ZANT_c::calcScale(BOOL param_0) {
    if (!param_0) {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc(this, &mModelScaleXZ, 0.0f, 0.5f, 0.25f, 0.1f);
        actor_attr::enemy_add_action_calc(this, &mModelScaleY, 1.2f, 0.5f, 0.1f, 0.1f);
#else
        cLib_addCalc(&mModelScaleXZ, 0.0f, 0.5f, 0.25f, 0.1f);
        cLib_addCalc(&mModelScaleY, 1.2f, 0.5f, 0.1f, 0.1f);
#endif

        if (!mModelScaleXZ) {
            mModelScaleY = 0.0f;
            mModelScaleXZ = 0.0f;
            return true;
        }
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc(this, &mModelScaleY, 1.0f, 0.5f, 0.2f, 0.1f), cLib_addCalc(&mModelScaleY, 1.0f, 0.5f, 0.2f, 0.1f));

        if (mModelScaleY > 0.5f) {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc(this, &mModelScaleXZ, 1.0f, 0.5f, 0.2f, 0.1f), cLib_addCalc(&mModelScaleXZ, 1.0f, 0.5f, 0.2f, 0.1f));
        }

        if (mModelScaleY == 1.0f && mModelScaleXZ == 1.0f) {
            return true;
        }
    }

    return false;
}

void daB_ZANT_c::executeWarp() {
    switch (mMode) {
    case 0:
    case 1:
        attention_info.flags = 0;
        setTgHitBit(FALSE);
        setCoHitBit(FALSE);

        if (mFightPhase != PHASE_MK && mFightPhase != PHASE_YO && mFightPhase != PHASE_LAST) {
            if (field_0x711 & 1) {
                setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            } else {
                setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            }
        }

        mMode += 2;

        speed.y = 0.0f;
        speedF = 0.0f;

        dComIfGp_particle_set(0x88FF, &current.pos, &shape_angle, NULL);
        dComIfGp_particle_set(0x8900, &current.pos, &shape_angle, NULL);
        mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_OUT, 0, -1);
    case 2:
    case 3:
        if (calcScale(0)) {
            mMode += 2;

            switch (mFightPhase) {
            case PHASE_BB:
                if (field_0x70b == 0) {
                    field_0x70b = 1;
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 120), 120);
                } else if (mFightCycle == 0) {
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 40), 40);
                } else {
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
                }
                break;
            case PHASE_MG:
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
                break;
            case PHASE_OI:
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
                break;
            case PHASE_MK:
                if (mLastAction == ACT_MONKEY) {
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
                } else {
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
                }
                break;
            case PHASE_YO:
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
                break;
            case PHASE_LAST:
                if (mFlyWarpPosID != 0) {
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
                } else {
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
                }
                break;
            }

            mModeTimer += DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, (int)l_HIO.mPlayWarpTime), (int)l_HIO.mPlayWarpTime);
        }
        break;
    case 4:
    case 5:
        if (mModeTimer == 0) {
            if (mMode == 4) {
                dBgS_ObjGndChk gndchk;
                gndchk.SetPos(&mFlyWarpPos);

                f32 gnd_y = dComIfG_Bgsp().GroundCross(&gndchk);
                if (gnd_y != -G_CM3D_F_INF) {
                    mFlyWarpPos.y = gnd_y;
                }

                shape_angle.y = field_0x6b8;
                shape_angle.x = 0;
                current.pos = mFlyWarpPos;
            } else {
                current.pos = mFlyWarpPos;
                shape_angle.x = 0;
                shape_angle.y = fopAcM_searchPlayerAngleY(this);
            }

            old.pos = current.pos;
            mMode = 6;

            if (field_0x711 & 2) {
                field_0x711 = (field_0x711 & 1) ^ 1;
            } else {
                field_0x711 &= 1;
            }

            if (field_0x711 != 0) {
                setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            } else {
                setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            }

            dComIfGp_particle_set(0x88FE, &current.pos, &shape_angle, NULL);
            mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_IN, 0, -1);
        }
        break;
    case 6:
        if (calcScale(1)) {
            attention_info.flags = fopAc_AttnFlag_BATTLE_e;
            setCoHitBit(TRUE);
            setBaseActionMode(0);
        }
        break;
    }
}

void daB_ZANT_c::executeDamage() {
    switch (mMode) {
    case 0:
    case 1:
    case 2:
        attention_info.flags = fopAc_AttnFlag_BATTLE_e;

        if (setNextDamageMode(FALSE)) {
            setTgHitBit(FALSE);
            return;
        } else if (mFightPhase == PHASE_OI && setNextDamageMode(TRUE)) {
            setTgHitBit(FALSE);
            return;
        }

        field_0x702 = 0;
        speedF = 0.0f;
        setTgHitBit(TRUE);

        if (mMode == 0) {
            if (cM_rnd() < 0.5f) {
                mMode = 1;
            } else {
                mMode = 2;
            }
        }

        if (mFightPhase == PHASE_OI) {
            if (mMode == 1) {
                setBck(BCK_ZAN_FLOAT_DAMAGEL, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            } else {
                setBck(BCK_ZAN_FLOAT_DAMAGER, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            }

            speed.y = 0.0f;
            speedF = 0.0f;
        } else if (mMode == 1) {
            setBck(BCK_ZAN_DAMAGEL_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        } else {
            setBck(BCK_ZAN_DAMAGER_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        }

        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
        mMode = 5;
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 40), 40);
    case 5:
        if (field_0x6f4 == 0) {
            setTgHitBit(FALSE);
        }

        if (mpModelMorf->isStop() && !setNextDamageMode(TRUE)) {
            setBaseActionMode(0);
        }
        break;
    }
}

void daB_ZANT_c::executeConfuse() {
    switch (mMode) {
    case 0:
        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        speedF = 0.0f;
        speed.y = 0.0f;
        mMode = 1;

        setBck(BCK_ZAN_GROUND_REACTION, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_NO_DMG, -1);
        field_0x702 = 0;
    case 1:
        if (mpModelMorf->isStop()) {
            setBaseActionMode(0);
        }
        break;
    case 5:
        setBck(BCK_ZAN_FAINT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        speedF = 0.0f;
        speed.y = 0.0f;
        field_0x702 = 0;
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 20), 20);
        mMode = 6;
    case 6:
        if (mModeTimer == 0) {
            setBaseActionMode(0);
        }
        break;
    }
}

void daB_ZANT_c::executeOpening() {
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    daPy_py_c* player = daPy_getPlayerActorClass();

    cXyz sp34(0.0f, 0.0f, 0.0f);
    cXyz sp40;
    cXyz sp4C;

    switch (mMode) {
    case MODE_START_DEMO:
        current.pos.set(0.0f, 0.0f, 160.0f);
        shape_angle.y = -0x8000;

        if (!eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(this, 2, 0xFFFF, 4);
            eventInfo.onCondition(2);
            return;
        }

        Z2GetAudioMgr()->setDemoName("force_start");
        fopAcM_OffStatus(this, fopAcStts_UNK_0x4000_e);
        
        sp34.set(0.0f, 0.0f, -700.0f);
        player->setPlayerPosAndAngle(&sp34, 0, 0);

        setBck(BCK_ZAN_OP_1, J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f);
        mMode = MODE_START_DEMO_WAIT;
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);

        camera->mCamera.Stop();
        camera->mCamera.SetTrimSize(3);
        field_0x77c = 220.0f;
    case MODE_START_DEMO_WAIT:
        sp4C.set(0.0f, 175.0f, 3.0f);
        sp40.set(0.0f, 270.0f, -194.0f);
        mDemoCamCenter = sp4C;
        mDemoCamEye = sp40;
        mDemoCamBank = 30.0f;

        if (mModeTimer == 0) {
            mMode = MODE_PAN_GROUND;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 80), 80);
        }
        break;
    case MODE_PAN_GROUND:
        sp4C.set(0.0f, 81.0f, -187.0f);
        sp40.set(0.0f, 67.0f, -408.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamCenter, sp4C, 0.1f, 4.3f);
        actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamEye, sp40, 0.1f, 6.0f);
#else
        cLib_addCalcPos2(&mDemoCamCenter, sp4C, 0.1f, 4.3f);
        cLib_addCalcPos2(&mDemoCamEye, sp40, 0.1f, 6.0f);
#endif

        if (mModeTimer == 0) {
            mMode = MODE_MSG_1;
            setBck(BCK_ZAN_OP_2, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_ZAN_OP_V_WAKEUP, -1);
        }
        break;
    case MODE_MSG_1:
        if (mpModelMorf->getFrame() > 5.0f) {
            sp4C.set(0.0f, 86.0f, -9.0f);
            sp40.set(0.0f, 10.0f, -209.0f);
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamCenter, sp4C, 0.5f, 26.0f);
            actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamEye, sp40, 0.5f, 30.0f);
#else
            cLib_addCalcPos2(&mDemoCamCenter, sp4C, 0.5f, 26.0f);
            cLib_addCalcPos2(&mDemoCamEye, sp40, 0.5f, 30.0f);
#endif
        }

        if (mpModelMorf->isStop()) {
            mMode = MODE_MSG_1_WAIT;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            setBck(BCK_ZAN_OP_3, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            setZantMessage(0xE3B);  // My god had only one wish...
        }
        break;
    case MODE_MSG_1_WAIT:
        if (doZantMessage() == true) {
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            mMode = MODE_WARP_OUT_SE;
        }
        break;
    case MODE_WARP_OUT_SE:
        if (mModeTimer == 0) {
            mMode = MODE_START_WARP;
            mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_OUT, 0, -1);
        }
        break;
    case MODE_START_WARP:
        if (calcScale(0)) {
            current.pos.set(0.0f, 225.0f, -1700.0f);
            sp34.set(0.0f, 0.0f, -500.0f);
            player->setPlayerPosAndAngle(&sp34, 0, 0);
            player->changeOriginalDemo();
            player->changeDemoMode(1, 0, 0, 0);

            mMode = MODE_WARP_WAIT;
            mDemoCamCenter.set(0.0f, 200.0f, -1700.0f);
            mDemoCamEye.set(0.0f, 70.0f, -300.0f);
            mDemoCamBank = 68.0f;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
        }
        break;
    case MODE_WARP_WAIT:
        if (mModeTimer == 0) {
            setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            player->changeDemoMode(0x48, 0, 0, 0);
            mMode = MODE_PAN_THRONE;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 105), 105);
        }
        break;
    case MODE_PAN_THRONE:
        sp4C.set(32.0f, 290.0f, -1678.0f);
        sp40.set(100.0f, 70.0f, -300.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamCenter, sp4C, 0.1f, 1.0f);
        actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamEye, sp40, 0.1f, 1.0f);
#else
        cLib_addCalcPos2(&mDemoCamCenter, sp4C, 0.1f, 1.0f);
        cLib_addCalcPos2(&mDemoCamEye, sp40, 0.1f, 1.0f);
#endif

#if TARGET_PC  // enemy attribute integration
        if (mModeTimer <= actor_attr::enemy_sync_countdown_milestone(this, 105, 30)) {
            if (mModeTimer == actor_attr::enemy_sync_countdown_milestone(this, 105, 30)) {
#else
        if (mModeTimer <= 30) {
            if (mModeTimer == 30) {
#endif
                current.angle.y = 0;
                shape_angle.y = 0;
                dComIfGp_particle_set(0x88FE, &current.pos, &shape_angle, NULL);
                mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_IN, 0, -1);
            }
            calcScale(1);
        }

        if (mModeTimer == 0) {
            mMode = MODE_ZOOM_THRONE;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
        }
        break;
    case MODE_ZOOM_THRONE:
        calcScale(1);
        sp4C.set(32.0f, 290.0f, -1678.0f);
        sp40.set(66.0f, 180.0f, -989.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamCenter, sp4C, 0.5f, 60.0f);
        actor_attr::enemy_add_action_calc_pos2(this, &mDemoCamEye, sp40, 0.5f, 60.0f);
#else
        cLib_addCalcPos2(&mDemoCamCenter, sp4C, 0.5f, 60.0f);
        cLib_addCalcPos2(&mDemoCamEye, sp40, 0.5f, 60.0f);
#endif

        if (mModeTimer == 0) {
            mMode = MODE_MSG_2;
            setZantMessage(0xE3C);
        }
        break;
    case MODE_MSG_2:
        if (doZantMessage() == true) {
            setBck(BCK_ZAN_OP_RISE, J3DFrameCtrl::EMode_LOOP, 10.0f, 1.0f);
            mMode = MODE_FLY_UP;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 90), 90);
            mSound.startCreatureSound(Z2SE_EN_ZAN_OP_FLY, 0, -1);
        }
        break;
    case MODE_FLY_UP:
#if TARGET_PC  // enemy attribute integration
        current.pos.y += actor_attr::enemy_action_step(this, 3.0f);
        mDemoCamCenter.y += actor_attr::enemy_action_step(this, 2.0f);
#else
        current.pos.y += 3.0f;
        mDemoCamCenter.y += 2.0f;
#endif

        if (mModeTimer == 0) {
            mMode = MODE_CLOSE_UP;
        }
        break;
    case MODE_CLOSE_UP:
        if (mModeTimer == 0) {
            mDoMtx_stack_c::YrotS(-0x8000);
            mDoMtx_stack_c::transM(0.0f, 300.0f, 700.0f);
            mDoMtx_stack_c::multVecZero(&current.pos);
            current.pos.y += 100.0f;
            current.pos.z -= 800.0f;
            old.pos = current.pos;
            speedF = 0.0f;
            speed.y = 0.0f;
            gravity = 0.0f;
            shape_angle.y = fopAcM_searchPlayerAngleY(this);
            shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

            setBck(BCK_ZAN_OP_RISE, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            
            sp34.x = -140.0f;
            sp34.y += 250.0f;
            sp34.z = -800.0f;
            player->setPlayerPosAndAngle(&sp34, 0x8000, 0);
            player->changeOriginalDemo();
            player->changeDemoPos0(&sp34);
            player->changeDemoMode(0x17, 1, 4, 0);
            mDemoCamBank = 58.0f;

            mDoMtx_stack_c::YrotS(-0x8000);
            mDoMtx_stack_c::transM(-85.0f, 344.0f, 382.0f);
            mDoMtx_stack_c::multVecZero(&mDemoCamEye);
            mDemoCamEye.y += 250.0f;
            mDemoCamEye.z -= 800.0f;

            mDoMtx_stack_c::YrotS(-0x8000);
            mDoMtx_stack_c::transM(258.0f, 672.0f, 1374.0f);
            mDoMtx_stack_c::multVecZero(&mDemoCamCenter);
            mDemoCamCenter.y += 200.0f;
            mDemoCamCenter.z -= 800.0f;

            Z2GetAudioMgr()->bgmStart(Z2BGM_BOSS_ZANT, 0, 0);
            field_0x77c = 0.0f;
            field_0x6fc = 1;
            mMode = MODE_SET_BOSS_TITLE;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 160), 160);
        }
        break;
    case MODE_SET_BOSS_TITLE:
        if (mModeTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_countdown_milestone(this, 160, 100), 100)) {
            fopMsgM_messageSetDemo(0x486);
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc(this, &current.pos.y, 250.0f + 300.0f, 0.1f, 3.0f, 0.9f);
        actor_attr::enemy_add_action_calc(this, &mDemoCamCenter.y, 250.0f + 672.0f, 0.1f, 1.0f, 0.3f);
#else
        cLib_addCalc(&current.pos.y, 250.0f + 300.0f, 0.1f, 3.0f, 0.9f);
        cLib_addCalc(&mDemoCamCenter.y, 250.0f + 672.0f, 0.1f, 1.0f, 0.3f);
#endif

        if (mModeTimer == 0) {
            mMode = MODE_WARP_IN_WAIT;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
        }
        break;
    case MODE_WARP_IN_SE:
        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        if (mModeTimer == 0) {
            dComIfGp_particle_set(0x88FE, &current.pos, &shape_angle, NULL);
            mMode = MODE_WARP_IN_SCALE;
            mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_IN, 0, -1);
        }
        break;
    case MODE_WARP_IN_SCALE:
        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        if (calcScale(1)) {
            mMode = MODE_WARP_IN_WAIT;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
        }
        break;
    case MODE_WARP_IN_WAIT:
        if (mModeTimer == 0) {
            mMode = MODE_ZOOM_OUT_ROOM_CHANGE;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 50), 50);
        }
        break;
    case MODE_ZOOM_OUT_ROOM_CHANGE:
        calcRoomChangeCamera(0);
        if (mModeTimer == 0) {
            mMode = MODE_START_ROOM_CHANGE;
            setBck(BCK_ZAN_FLOAT_APPEAR, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f);
            mSound.startCreatureSound(Z2SE_EN_ZAN_MAHOJIN_BB, 0, -1);
            field_0x714 = 1;
            mKankyoBlend = 0.0f;
        }
        break;
    case MODE_START_ROOM_CHANGE:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mKankyoBlend, 1.0f, 0.006f), cLib_chaseF(&mKankyoBlend, 1.0f, 0.006f));
        if (mpModelMorf->checkFrame(110)) {
            mMahojinAnmMode = 1;
            field_0x715 = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(this, 30), 30);
        }

        if (mpModelMorf->getFrame() > 110.0f) {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mKankyoBlend, 1.0f, 0.01f), cLib_chaseF(&mKankyoBlend, 1.0f, 0.01f));
        }

        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        if (mpModelMorf->isStop()) {
            dComIfGp_getVibration().StopQuake(31);
            setBck(BCK_ZAN_FLOAT_APPEARWAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            mMode = MODE_ROOM_CHANGE;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mMahojinWaitTime), l_HIO.mMahojinWaitTime);

            cXyz pos(0.0f, 0.0f, 0.0f);
            dComIfGs_setRestartRoom(pos, 0, warp_next_room[1]);
            return;
        }
        break;
    case MODE_ROOM_CHANGE:
        mFightPhase++;
        if (mFightPhase >= PHASE_MAX) {
            mFightPhase = PHASE_OP;
        }

        current.pos.set(-140.0f, 300.0f, 700.0f);
        
        sp34.set(0.0f, 0.0f, 0.0f);
        player->setPlayerPosAndAngle(&sp34, 0, 0);
        player->changeDemoMode(0x17, 1, 4, 0);
        
        mDemoCamEye.x += 140.0f;
        mDemoCamEye.y -= 250.0f;
        mDemoCamEye.z += 800.0f;

        mDoMtx_stack_c::YrotS(-0x8000);
        mDoMtx_stack_c::transM(mDemoCamEye);
        mDoMtx_stack_c::multVecZero(&mDemoCamEye);

        mDemoCamCenter.x += 140.0f;
        mDemoCamCenter.y -= 250.0f;
        mDemoCamCenter.z += 800.0f;

        mDoMtx_stack_c::YrotS(-0x8000);
        mDoMtx_stack_c::transM(mDemoCamCenter);
        mDoMtx_stack_c::multVecZero(&mDemoCamCenter);

        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        initNextRoom();
        mMode = MODE_END_ROOM_CHANGE;
    case MODE_END_ROOM_CHANGE:
        if (mModeTimer == 0 && dComIfGp_roomControl_checkStatusFlag(warp_next_room[mFightPhase], 0x10)) {
            mMode = MODE_END_DEMO;
            field_0x714 = 0;
            mKankyoBlend = 0.0f;
            setBck(BCK_ZAN_FLOAT_WAITRETURN, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f);
            mMahojinAnmMode = 4;
        }
        break;
    case MODE_END_DEMO:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mKankyoBlend, 1.0f, 0.02f), cLib_chaseF(&mKankyoBlend, 1.0f, 0.02f));
        player->setPlayerPosAndAngle(&sp34, 0, 0);

        if (mpModelMorf->isStop()) {
            camera->mCamera.Reset(mDemoCamCenter, mDemoCamEye);
            camera->mCamera.Start();
            camera->mCamera.SetTrimSize(0);
            dComIfGp_event_reset();

            field_0x6fc = 0;
            setTgHitBit(TRUE);
            setBaseActionMode(1);
            Z2GetAudioMgr()->setDemoName("force_end");
            return;
        }
        break;
    }

    if (field_0x715 != 0) {
        calcRoomChangeCamera(1);
        field_0x715--;
    }

    camera->mCamera.Set(mDemoCamCenter, mDemoCamEye, mDemoCamBank, 0);
}

void daB_ZANT_c::executeFly() {
    dBgS_ObjGndChk_All gndchk;
    cXyz sp9C;

    switch (mMode) {
    case 0:
        setTgHitBit(TRUE);
        setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        mModeTimer = 0;
        mMode = 1;
        gravity = 0.0f;
        field_0x70c = 0;
        setTgType(0x10040);
    case 1:
        if (mModeTimer == 0) {
            if (mFightCycle == 0) {
                field_0x703 = l_HIO.mBulletNum;
            } else {
                field_0x703 = 8;
            }

            mFlyWarpPosID = (f32)mFlyWarpPosID + cM_rndF(1.9f) + 1.0f;
            mFlyWarpPos = fly_warp_pos[mFlyWarpPosID % 3];
            field_0x711 = 1;
            setActionMode(ACT_SMALL_ATTACK, 0);
            return;
        }
        break;
    case 10:
        setBck(BCK_ZAN_SWAMP_FALL_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_ZAN_BB_V_FALL, -1);
        mMode = 11;

        speedF = 0.0f;
        speed.y = 45.0f;
        current.angle.y = shape_angle.y;
        gravity = -5.0f;
        field_0x6f8 = 0x1000;
        setTgHitBit(FALSE);
    case 11:
    case 12:
        if (mMode == 11) {
            if (mpModelMorf->isStop()) {
                setBck(BCK_ZAN_SWAMP_FALL_LOOP, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
                mMode = 12;
            }
        } else if (speed.y < -10.0f) {
            setBck(BCK_ZAN_SWAMP_FALL_B, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            mMode = 13;
        }
    case 13:
        if (speed.y < 5.0f) {
            gravity = -2.0f;
        } else {
            gravity = -5.0f;
        }

        if (field_0x6f8 != 0) {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, shape_angle.y, field_0x6f8);
            actor_attr::enemy_chase_action_angle(this, &field_0x6f8, 0, 0x40);
#else
            shape_angle.y += field_0x6f8;
            cLib_chaseAngleS(&field_0x6f8, 0, 0x40);
#endif
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, current.angle.y, 4, 0x1000), cLib_addCalcAngleS2(&shape_angle.y, current.angle.y, 4, 0x1000));
        }

        sp9C = current.pos;
        sp9C.y += 100.0f;
        gndchk.SetPos(&sp9C);
        {
            f32 gnd_pos = dComIfG_Bgsp().GroundCross(&gndchk);
            if (gnd_pos != -G_CM3D_F_INF && current.pos.y <= gnd_pos) {
                if (dComIfG_Bgsp().GetPolyAtt0(gndchk) == 11) {
                    speed.y = 50.0f;
                    speedF = 15.0f;
                    current.angle.y = (cM_rndFX(2.9f) * (f32)0x1000) - (f32)0x8000;

                    mMode = 11;
                    setBck(BCK_ZAN_SWAMP_FALL_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_BB_V_JUMP, -1);

                    cXyz pos(current.pos.x, gnd_pos, current.pos.z);
                    cXyz size(1.5f, 1.5f, 1.5f);
                    for (int i = 0; i < 4; i++) {
                        static u16 w_eff_id[] = {0x01B8, 0x01B9, 0x01BA, 0x01BB};
                        field_0x389c[i] = dComIfGp_particle_setPolyColor(field_0x389c[i], w_eff_id[i], gndchk, &pos, &tevStr, &shape_angle, &size, 0, NULL, -1, NULL);
                    }

                    mSound.startCreatureSound(Z2SE_EN_ZAN_BB_WTR, 0, -1);
                } else {
                    fopAcM_effSmokeSet1(&field_0x3894, &field_0x3898, &current.pos, NULL, 2.0f, &tevStr, 1);
                    speedF = 0.0f;
                    setBck(BCK_ZAN_SWAMP_LANDING, J3DFrameCtrl::EMode_NONE, 0.0f, 1.0f);
                    mMode = 14;
                    setTgHitBit(TRUE);
                    setTgType(0xD8FBFDFF);
                    field_0x702 = 0;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 150), 150);
                    field_0x70c = 1;
                }
            }
        }
        break;
    case 14:
        if (mpModelMorf->checkFrame(3)) {
            mSound.startCreatureSound(Z2SE_EN_ZAN_BB_LAND, 0, -1);
        } else if (mpModelMorf->checkFrame(18) || mpModelMorf->checkFrame(23)) {
            mSound.startCreatureSound(Z2SE_EN_ZAN_BB_FOOT, 0, -1);
        }

        if (mpModelMorf->checkFrame(5)) {
            speedF = 3.0f;
        }

        if (mpModelMorf->isStop()) {
            setActionMode(ACT_FLY_GROUND, 0);
            field_0x702 = 0;
        }
        break;
    }
}

void daB_ZANT_c::executeFlyGround() {
    switch (mMode) {
    case 0:
        if (mLastAction == ACT_DAMAGE || mLastAction == ACT_CONFUSE) {
            field_0x70c = 0;
            gravity = 0.0f;
            mFlyWarpPosID = (f32)mFlyWarpPosID + cM_rndF(1.9f) + 1.0f;
            mFlyWarpPos = fly_warp_pos[mFlyWarpPosID % 3];
            field_0x711 = 2;
            setActionMode(ACT_WARP, 1);
            break;
        }
        speed.y = 0.0f;
        speedF = 0.0f;
        setBck(BCK_ZAN_LV1_FATIGUE, J3DFrameCtrl::EMode_LOOP, 5.0f, 1.0f);
        mMode = 1;
        field_0x702 = 0;
        mSound.startCreatureVoice(Z2SE_EN_ZAN_BB_V_ZEIZEI, -1);
    case 1:
        if (field_0x6f0 == 0) {
            field_0x70c = 0;
            gravity = 0.0f;
            mFlyWarpPosID = (f32)mFlyWarpPosID + cM_rndF(1.9f) + 1.0f;
            mFlyWarpPos = fly_warp_pos[mFlyWarpPosID % 3];
            field_0x711 = 2;
            setActionMode(ACT_WARP, 1);
        }
        break;
    }
}

bool daB_ZANT_c::checkSwimLinkNearMouth() {
    fopAc_ac_c* pmobile;
    fopAcM_SearchByID(mMobileIDs[mCorrectMobileNo], &pmobile);
    if (pmobile == NULL) {
        return false;
    }

#if TARGET_PC  // enemy attribute integration
    // This is a close-to-mouth check, not the long-range snort hitbox. The
    // snort cylinders start 1000 units in front of the head; using their
    // centers here catches Link farther away and misses him beside the mouth.
    const f32 size_mul = actor_attr::enemy_size_multiplier(this);
    daAlink_c* player = static_cast<daAlink_c*>(daPy_getPlayerActorClass());
    cXyz mouth_offset(0.0f, -300.0f * size_mul, 300.0f * size_mul);
    cXyz mouth_base;
    cLib_offsetPos(&mouth_base, &pmobile->current.pos, pmobile->shape_angle.y, &mouth_offset);

    // Link stays normal-sized, even when the head is 25% size. Include the
    // extent of his collision body instead of checking his origin alone.
    for (int i = 0; i < 3; ++i) {
        const dCcD_Cyl& link_cyl = player->mTgCyls[i];
        const cXyz link_pos = link_cyl.GetC();
        if (mouth_base.absXZ(link_pos) < 200.0f * size_mul + link_cyl.GetR() &&
            mouth_base.y < link_pos.y + link_cyl.GetH() &&
            link_pos.y < mouth_base.y + 1100.0f * size_mul)
        {
            return true;
        }
    }
#else
    s16 mobile_angle = pmobile->shape_angle.y;
    cXyz check_area(cM_ssin(mobile_angle) * 900.0f, 0.0f, cM_scos(mobile_angle) * 900.0f);
    check_area += pmobile->current.pos;

    cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
    if (check_area.absXZ(player_pos) < 400.0f && check_area.y - 300.0f < player_pos.y && check_area.y + 800.0f > player_pos.y) {
        return true;
    }
#endif

    return false;
}

bool daB_ZANT_c::checkSwimLinkNear() {
    fopAc_ac_c* pmobile;
    fopAcM_SearchByID(mMobileIDs[mCorrectMobileNo], &pmobile);
    if (pmobile == NULL) {
        return false;
    }

    if (checkSwimLinkNearMouth()) {
        // The nearby-player retreat applies only while Zant is actually
        // attacking from the open mouth. During his swim and the subsequent
        // mouth-open transition, this check can immediately close the head
        // again and keep the fight cycling without another attack.
        if (DUSK_IF_ELSE(field_0x706 != 0 && ((daB_ZANTZ_c*)pmobile)->getMouthMode() == 1 && mAction == ACT_SMALL_ATTACK && mMode == 3, field_0x706 != 0 && ((daB_ZANTZ_c*)pmobile)->getMouthMode() == 1)) {
            s16 mobile_angle = pmobile->shape_angle.y;
#if TARGET_PC  // enemy attribute integration
            // Zant's position follows the scaled mouth at both small and
            // large head sizes.
            const f32 zant_mouth_offset = actor_attr::enemy_size_value(this, 300.0f);
            cXyz check_area(cM_ssin(mobile_angle) * zant_mouth_offset, 0.0f,
                cM_scos(mobile_angle) * zant_mouth_offset);
#else
            cXyz check_area(cM_ssin(mobile_angle) * 300.0f, 0.0f, cM_scos(mobile_angle) * 300.0f);
#endif
            check_area += pmobile->current.pos;

            // Do not let the proximity helper bypass the underwater mouth-open
            // hold.  executeWater() mode 25 owns the close timing; at non-1x
            // stun duration field_0x6f0 can intentionally remain active much
            // longer.  Previously this path closed the statue in the same frame
            // the timer was started, which also cut the visible snort sequence
            // short.
            if (DUSK_IF_ELSE(check_area.abs(current.pos) < 300.0f && field_0x6f0 == 0, check_area.abs(current.pos) < 300.0f)) {
                setTgHitBit(FALSE);
                ((daB_ZANTZ_c*)pmobile)->setMouthMode(2);
                setActionMode(ACT_WATER, 27);
            }
        }

        ((daB_ZANTZ_c*)pmobile)->setSnortEffect(30);
#if TARGET_PC  // enemy attribute integration
        // The existing snort cylinders are farther out in front of the head.
        // Push Link from this mouth-range trigger as well, or a small head can
        // close on him without its air collision ever touching him.
        daPy_py_c* player = daPy_getPlayerActorClass();
        s16 throw_angle = cLib_targetAngleY(&pmobile->current.pos, &player->current.pos);
        player->setThrowDamage(throw_angle, 30.0f, 10.0f, 0, 0, 2);
#endif
        return true;
    }

    return false;
}

void daB_ZANT_c::executeHook() {
    switch (mMode) {
    case 0:
        setTgHitBit(FALSE);
        setCoHitBit(FALSE);
        gravity = 0.0f;
        speed.y = 0.0f;
        speedF = 0.0f;
        field_0x705 = 0;
        field_0x706 = 0;

        setBck(BCK_ZAN_HOOK_HIT, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mMode = 2;
        field_0x6ff++;
        break;
    case 2:
        if (mpModelMorf->checkFrame(2)) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_OI_V_CAUGHT, -1);
        }

        if (mpModelMorf->isStop()) {
            mMode = 3;
            setBck(BCK_ZAN_HOOK_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        }
    case 3:
        if (!fopAcM_CheckStatus(this, fopAcStts_HOOK_CARRY_NOW_e)) {
            setTgHitBit(TRUE);
            setCoHitBit(TRUE);
            setActionMode(ACT_SWIM, 10);
        }
        break;
    }
}

void daB_ZANT_c::executeWater() {
    fopAc_ac_c* pmobile;
    fopAcM_SearchByID(mMobileIDs[mCorrectMobileNo], &pmobile);
    if (pmobile != NULL) {
        s16 mobile_angle = pmobile->shape_angle.y;
#if TARGET_PC  // enemy attribute integration
        // Keep Zant at the same relative depth inside the head at any size,
        // including before Link grabs him.
        const f32 zant_mouth_offset = actor_attr::enemy_size_value(this, 300.0f);
        cXyz sp58(cM_ssin(mobile_angle) * zant_mouth_offset, 0.0f, cM_scos(mobile_angle) * zant_mouth_offset);
#else
        cXyz sp58(cM_ssin(mobile_angle) * 300.0f, 0.0f, cM_scos(mobile_angle) * 300.0f);
#endif
        sp58 += pmobile->current.pos;

        switch (mMode) {
        case 0:
            if (mLastAction == ACT_SMALL_ATTACK) {
                mMode = 0x19;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 0x78), 0x78);
                field_0x705 = 1;
                attention_info.flags = fopAc_AttnFlag_BATTLE_e;
                setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            } else if (field_0x705 != 0) {
                if (((daB_ZANTZ_c*)pmobile)->getAppearMode() == 3) {
                    mMode = 15;
                } else {
                    ((daB_ZANTZ_c*)pmobile)->setMouthMode(2);
                    mMode = 27;
                }

                attention_info.flags = 0;
            } else {
                setActionMode(ACT_SWIM, 0);
            }
            break;
        case 15:
            {
                daPy_py_c* player = daPy_getPlayerActorClass();
                if (player->current.pos.y < 1000.0f) {
                    mMode = 20;

                    if (mFightCycle == 0) {
                        ((daB_ZANTZ_c*)pmobile)->setAppearMode(0);
                    } else {
                        for (int i = 0; i < 4; i++) {
                            fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                            if (pmobile != NULL) {
                                ((daB_ZANTZ_c*)pmobile)->setAppearMode(0);
                            }
                        }
                    }

                    field_0x704 = 0;
                }
            }
            break;
        case 20:
            old.pos = sp58;
            current.pos = old.pos;
            shape_angle.y = mobile_angle;

            if (((daB_ZANTZ_c*)pmobile)->getAppearMode() == 1) {
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
                mMode = 21;

                if (mFightCycle != 0) {
                    // set zant to farthest mobile from player
                    cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
                    f32 farthest_dist = 0.0f;
                    int correct_no = 0;

                    for (int i = 0; i < 4; i++) {
                        fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                        if (pmobile != NULL && farthest_dist < pmobile->current.pos.abs(player_pos)) {
                            farthest_dist = pmobile->current.pos.abs(player_pos);
                            correct_no = i;
                        }
                    }

                    mCorrectMobileNo = correct_no;
                }
            }
            break;
        case 21:
            if (mModeTimer == 0) {
                field_0x6fd = 1;

                if (mFightCycle == 0) {
                    ((daB_ZANTZ_c*)pmobile)->setAppearMode(4);
                } else {
                    int sp78 = 0;

                    for (int i = 0; i < 4; i++) {
                        fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                        if (pmobile != NULL && abs((s16)(fopAcM_searchPlayerAngleY(pmobile) - pmobile->shape_angle.y)) > 0x1000) {
                            sp78++;
                        }
                    }

                    if (sp78 != 0) {
                        for (int i = 0; i < 4; i++) {
                            fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                            if (pmobile != NULL) {
                                ((daB_ZANTZ_c*)pmobile)->setAppearMode(5);
                            }
                        }

                        field_0x6fd = 0;
                    }
                }

                mMode = 22;
            }
            break;
        case 22:
            if (mFightCycle == 0) {
                if (((daB_ZANTZ_c*)pmobile)->getAppearMode() == 1) {
                    ((daB_ZANTZ_c*)pmobile)->setSnortEffect(30);
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
                    mMode = 23;
                }
            } else {
                int sp84 = 0;

                if (field_0x6fd == 0) {
                    for (int i = 0; i < 4; i++) {
                        fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                        if (pmobile != NULL && ((daB_ZANTZ_c*)pmobile)->isSearchContinue()) {
                            if (abs((s16)(fopAcM_searchPlayerAngleY(pmobile) - pmobile->shape_angle.y)) < 0xC00) {
                                sp84++;
                            }
                        }
                    }

                    if (sp84 == 4) {
                        for (int i = 0; i < 4; i++) {
                            fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                            if (pmobile != NULL) {
                                ((daB_ZANTZ_c*)pmobile)->offSearchContinue();
                            }
                        }
                        field_0x6fd++;
                    }
                } else {
                    for (int i = 0; i < 4; i++) {
                        fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                        if (pmobile != NULL && ((daB_ZANTZ_c*)pmobile)->getAppearMode() == 1) {
                            sp84++;
                        }
                    }

                    if (sp84 == 4) {
                        for (int i = 0; i < 4; i++) {
                            fopAcM_SearchByID(mMobileIDs[i], &pmobile);
                            if (pmobile != NULL) {
                                ((daB_ZANTZ_c*)pmobile)->setSnortEffect(30);
                            }
                        }
                        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
                        mMode = 23;
                    }
                }
            }
            break;
        case 23:
            if (mModeTimer == 0) {
                old.pos = sp58;
                current.pos = old.pos;
                ((daB_ZANTZ_c*)pmobile)->setMouthMode(0);
                mMode = 24;
                setTgHitBit(TRUE);
                field_0x706 = 1;
            }
            break;
        case 24:
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x400, 0x80), cLib_addCalcAngleS(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x400, 0x80));
            if (((daB_ZANTZ_c*)pmobile)->getMouthMode() == 1) {
                attention_info.flags = fopAc_AttnFlag_BATTLE_e;
                field_0x703 = 8;
                field_0x6ff = 0;
                field_0x711 = 1;
                setActionMode(ACT_SMALL_ATTACK, 0);
            }
            break;
        case 25:
            if (field_0x6f0 == 0) {
                field_0x706 = 0;
                setTgHitBit(FALSE);
                ((daB_ZANTZ_c*)pmobile)->setMouthMode(2);
                mMode = 27;
            }
            break;
        case 27:
            if (((daB_ZANTZ_c*)pmobile)->getMouthMode() == 3) {
                current.pos = sp58;
                field_0x705 = 1;
                attention_info.flags = 0;

                if (mFightCycle == 0) {
                    mMode = 30;
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(30.0f) + 100.0f), cM_rndF(30.0f) + 100.0f);
                } else if (field_0x712 == 0) {
                    field_0x712++;
                    mMode = 28;
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
                } else {
                    mMode = 30;
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(30.0f) + 100.0f), cM_rndF(30.0f) + 100.0f);
                    mCorrectMobileNo = (int)((f32)mCorrectMobileNo + cM_rndF(2.9f) + 1.0f) & 3;
                }
            }
            break;
        case 28:
            if (mModeTimer == 0) {
                ((daB_ZANTZ_c*)pmobile)->setAppearMode(2);
                mMode = 29;
            }
            break;
        case 29:
            current.pos = sp58;
            shape_angle.y = mobile_angle;

            if (((daB_ZANTZ_c*)pmobile)->getAppearMode() == 3) {
                mMode = 15;
            }
            break;
        case 30:
            if (mModeTimer == 0) {
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
                mMode = 21;
            }
            break;
        }
    }
}

void daB_ZANT_c::executeSwim() {
    fopAc_ac_c* pmobile;
    fopAcM_SearchByID(mMobileIDs[mCorrectMobileNo], &pmobile);

    if (pmobile != NULL) {
        s16 mobile_angle = pmobile->shape_angle.y;
#if TARGET_PC  // enemy attribute integration
        // Match the scaled depth and height of the opening while swimming
        // back; executeWater() then places him fully inside the head.
        const f32 mouth_depth = actor_attr::enemy_size_value(this, 300.0f);
        cXyz sp50(cM_ssin(mobile_angle) * mouth_depth, mouth_depth, cM_scos(mobile_angle) * mouth_depth);
#else
        cXyz sp50(cM_ssin(mobile_angle) * 300.0f, 300.0f, cM_scos(mobile_angle) * 300.0f);
#endif
        sp50 += pmobile->current.pos;

        s16 sp10 = cLib_targetAngleY(&pmobile->current.pos, &current.pos);
        s16 spE = mobile_angle - sp10;

        switch (mMode) {
        case 10:
            if (field_0x6ff > 5) {
                field_0x6b8 = mobile_angle;
                mFlyWarpPos = sp50;
                field_0x705 = 1;
                field_0x711 = 1;
                setActionMode(ACT_WARP, 0);
                return;
            }
            
            setBck(BCK_ZAN_HOOK_RELEASE, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            mMode = 11;
        case 11:
            if (current.pos.y < 50.0f) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &current.pos.y, 50.0f, 0.1f, 10.0f), cLib_addCalc2(&current.pos.y, 50.0f, 0.1f, 10.0f));
            } else if (current.pos.y > 100.0f) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &current.pos.y, 100.0f, 0.1f, 10.0f), cLib_addCalc2(&current.pos.y, 100.0f, 0.1f, 10.0f));
            }

            if (mpModelMorf->isStop()) {
                mMode = 0;
            }
            break;
        case 0:
            setBck(BCK_ZAN_SWIM, J3DFrameCtrl::EMode_LOOP, 5.0f, 1.0f);
            mpModelMorf->setFrame(10.0f);
            field_0x6cc = 0.0f;
            attention_info.flags = fopAc_AttnFlag_BATTLE_e;
            mMode = 1;
            field_0x702 = 0;
        case 1:
        case 2:
            if (abs(spE) < 0x1000) {
                if (mMode == 1) {
                    field_0x6ac.set(cM_ssin(mobile_angle) * 600.0f, 300.0f, cM_scos(mobile_angle) * 600.0f);
                    field_0x6ac += pmobile->current.pos;
                } else {
                    field_0x6ac = sp50;
                }

                mMode = 4;
#if TARGET_PC  // enemy attribute integration
                if (actor_attr::enemy_size_multiplier(this) > 1.0f) {
                    // The enlarged head's solid background can otherwise
                    // stop Zant before he reaches the inside-mouth target.
                    field_0x705 = 1;
                }
#endif
            } else {
                s16 spC;
                if (spE < 0) {
                    spC = sp10 - 0x1000;
                } else {
                    spC = sp10 + 0x1000;
                }

                field_0x6ac.set(cM_ssin(spC) * 1200.0f, 250.0f, cM_scos(spC) * 1200.0f);
                field_0x6ac += pmobile->current.pos;
                mMode = 3;
            }

            // This is the temporary post-hookshot swim slowdown/recovery window.
            // It intentionally follows Zant's movement/action speed, not stun duration.
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
        case 3:
#if TARGET_PC  // enemy attribute integration
        case 4:
			{
				if (current.pos.y < 50.0f) {
					actor_attr::enemy_add_action_calc2(this, &current.pos.y, 50.0f, 0.1f, 10.0f);
				}
#else
        case 4:
            {
                if (current.pos.y < 50.0f) {
                    cLib_addCalc2(&current.pos.y, 50.0f, 0.1f, 10.0f);
                }
#endif

                f32 player_dist = fopAcM_searchPlayerDistance(this);
                s16 spA = -cLib_targetAngleX(&current.pos, &field_0x6ac) + 0x4000;
                s16 sp8 = cLib_targetAngleY(&current.pos, &field_0x6ac);
                f32 anm_frame = mpModelMorf->getFrame();

                int sp64 = 0;
                if (anm_frame < 13.0f || anm_frame >= 45.0f) {
                    sp64 = 1;
                }

                if ((13.0f <= anm_frame && anm_frame <= 19.0f) || (29.0f <= anm_frame && anm_frame <= 45.0f)) {
                    sp64 = 2;
                }

#if TARGET_PC  // enemy attribute integration
				if (abs((s16)(sp8 - shape_angle.y)) < 0x2000) {
					switch (sp64) {
					case 0:
						actor_attr::enemy_chase_action_float(this, &field_0x6cc, 0.0f, 2.0f);
						break;
					case 1:
						actor_attr::enemy_chase_action_float(this, &field_0x6cc, 0.0f, 0.5f);
						break;
					case 2:
						actor_attr::enemy_chase_action_float(this, &field_0x6cc, 15.0f, 3.0f);
						break;
					}
				} else {
					switch (sp64) {
					case 0:
						actor_attr::enemy_chase_action_float(this, &field_0x6cc, 0.0f, 1.0f);
						break;
					case 1:
						actor_attr::enemy_chase_action_float(this, &field_0x6cc, 0.0f, 0.3f);
						break;
					case 2:
						actor_attr::enemy_chase_action_float(this, &field_0x6cc, 5.0f, 1.0f);
						break;
					}
				}
				if (sp64 == 2) {
					actor_attr::enemy_add_action_angle(this, &shape_angle.y, sp8, 0x10, 0x180);
					actor_attr::enemy_add_action_angle(this, &shape_angle.x, spA, 0x10, 0x400);
				} else {
					actor_attr::enemy_add_action_angle(this, &shape_angle.y, sp8, 0x10, 0x80);
					actor_attr::enemy_add_action_angle(this, &shape_angle.x, spA, 0x10, 0x200);
				}
#else
                if (abs((s16)(sp8 - shape_angle.y)) < 0x2000) {
                    switch (sp64) {
                    case 0:
                        cLib_chaseF(&field_0x6cc, 0.0f, 2.0f);
                        break;
                    case 1:
                        cLib_chaseF(&field_0x6cc, 0.0f, 0.5f);
                        break;
                    case 2:
                        cLib_chaseF(&field_0x6cc, 15.0f, 3.0f);
                        break;
                    }
                } else {
                    switch (sp64) {
                    case 0:
                        cLib_chaseF(&field_0x6cc, 0.0f, 1.0f);
                        break;
                    case 1:
                        cLib_chaseF(&field_0x6cc, 0.0f, 0.3f);
                        break;
                    case 2:
                        cLib_chaseF(&field_0x6cc, 5.0f, 1.0f);
                        break;
                    }
                }

                if (sp64 == 2) {
                    cLib_addCalcAngleS2(&shape_angle.y, sp8, 0x10, 0x180);
                    cLib_addCalcAngleS2(&shape_angle.x, spA, 0x10, 0x400);
                } else {
                    cLib_addCalcAngleS2(&shape_angle.y, sp8, 0x10, 0x80);
                    cLib_addCalcAngleS2(&shape_angle.x, spA, 0x10, 0x200);
                }
#endif

                current.angle.y = shape_angle.y;
                current.angle.x = cLib_targetAngleX(&current.pos, &field_0x6ac);

                if (mModeTimer != 0) {
                    speed.y = (field_0x6cc / 2) * cM_ssin(current.angle.x);
                    speedF = std::abs((field_0x6cc / 2) * cM_scos(current.angle.x));
                } else {
                    speed.y = field_0x6cc * cM_ssin(current.angle.x);
                    speedF = std::abs(field_0x6cc * cM_scos(current.angle.x));
                }

#if TARGET_PC   // enemy attribute integration
				// Allow for the movement that will be applied after action(). A
				// fast swim can step past a small arrival volume in one frame.
				// Snap to the inside-mouth target before starting the close so
				// the closing MoveBG cannot push him back outside.
				const bool vanilla_swim = actor_attr::enemy_size_multiplier(this) == 1.0f && actor_attr::enemy_action_speed_multiplier(this) == 1.0f;
				const f32 mouth_arrival = vanilla_swim ? 200.0f : 50.0f + actor_attr::enemy_action_step(this, speedF);
				if (current.pos.absXZ(sp50) < mouth_arrival) {
					if (!vanilla_swim) {
						current.pos.x = sp50.x;
						current.pos.z = sp50.z;
						old.pos.x = current.pos.x;
						old.pos.z = current.pos.z;
						speedF = 0.0f;
						field_0x705 = 1;
					}
#else
				if (current.pos.absXZ(sp50) < 200.0f) {
#endif
                    mMode = 6;
                    attention_info.flags = 0;
                    setTgHitBit(FALSE);
                    ((daB_ZANTZ_c*)pmobile)->setMouthMode(2);
                    mModeTimer = 60;
                } else if (current.pos.abs(field_0x6ac) < 200.0f) {
                    if (mMode == 3) {
                        mMode = 1;
                    } else {
                        mMode = 2;
                    }
                }
            }
            break;
        case 6:
#if TARGET_PC  // enemy attribute integration
            if (field_0x705 != 0) {
                // Keep him inside the mouth if the head turns while closing.
                current.pos.x = sp50.x;
                current.pos.z = sp50.z;
                old.pos.x = current.pos.x;
                old.pos.z = current.pos.z;
            }
            actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 0.5f);
            actor_attr::enemy_chase_action_float(this, &speed.y, 5.0f, 0.5f);
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, mobile_angle, 0x10, 0x200);
            actor_attr::enemy_add_action_angle(this, &shape_angle.x, 0, 0x10, 0x200);
#else
            cLib_chaseF(&speedF, 0.0f, 0.5f);
            cLib_chaseF(&speed.y, 5.0f, 0.5f);
            cLib_addCalcAngleS2(&shape_angle.y, mobile_angle, 0x10, 0x200);
            cLib_addCalcAngleS2(&shape_angle.x, 0, 0x10, 0x200);
#endif

            if (!speedF && speed.y == 5.0f) {
                mMode = 7;
                gravity = -1.0f;
                maxFallSpeed = -5.0f;
            }
            break;
        case 7:
            {
                field_0x706 = 1;
                maxFallSpeed = -100.0f;

                cXyz target(sp50.x, current.pos.y, sp50.z);
#if TARGET_PC  // enemy attribute integration
				if (field_0x705 != 0) {
					current.pos.x = target.x;
					current.pos.z = target.z;
					old.pos.x = target.x;
					old.pos.z = target.z;
				} else {
					actor_attr::enemy_chase_action_pos_xz(this, &current.pos, target, 3.0f);
				}
#else
                cLib_chasePosXZ(&current.pos, target, 3.0f);
#endif
                
                if (((daB_ZANTZ_c*)pmobile)->getMouthMode() == 3) {
                    field_0x705 = 1;
                    mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
                    setActionMode(ACT_WATER, 27);

                    shape_angle.x = 0;
                    shape_angle.y = 0;
                    gravity = 0.0f;
                    speed.y = 0.0f;
                    speedF = 0.0f;
                }
            }
            break;
        }

        if (checkBck(BCK_ZAN_SWIM)) {
            if (mpModelMorf->checkFrame(12)) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_OI_SWIM1, 0, -1);
            } else if (mpModelMorf->checkFrame(28)) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_OI_SWIM2, 0, -1);
            }
        }
    }
}

void daB_ZANT_c::executeSimaJump() {
    switch (mMode) {
    case 0:
        setTgHitBit(TRUE);
        mBodySphCc[0].OnTgNoHitMark();
        mBodySphCc[1].OnTgNoHitMark();

        setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);

        if (mLastAction == ACT_SMALL_ATTACK) {
            field_0x702 = 0;
            mMode = 10;

            if (mFightCycle == 0) {
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 120), 120);
            } else {
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 105), 105);
            }

            attention_info.flags = fopAc_AttnFlag_BATTLE_e;
            setBck(BCK_ZAN_LV1_FATIGUE, J3DFrameCtrl::EMode_LOOP, 10.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_ZAN_MG_V_ZEIZEI, -1);
            return;
        }

        if (mLastAction == ACT_CONFUSE || mLastAction == ACT_DAMAGE) {
            mMode = 11;
            return;
        }

        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        field_0x6ec = 0;
        mMode = 1;
        // Vertical movement is already advanced through enemy_action_pos_move_f(),
        // so keep the stored ballistic values in vanilla frame units here.
        gravity = -5.0f;
        field_0x6fd = 0;

        if (field_0x70f < 2) {
            if (field_0x70f == 0) {
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 150), 150);
            }

            field_0x70f++;
        } else {
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(50.0f) + 100.0f), cM_rndF(50.0f) + 100.0f);

            if (field_0x70b >= 4) {
                mModeTimer = 0;
#if TARGET_PC  // enemy attribute integration
            } else if (field_0x70b < 2 && mModeTimer < actor_attr::enemy_sync_timer(this, 120)) {
                mModeTimer = actor_attr::enemy_sync_timer(this, 120);
#else
            } else if (field_0x70b < 2 && mModeTimer < 120) {
                mModeTimer = 120;
#endif
            }

            if (mModeTimer < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 120), 120)) {
                field_0x70b = 0;
                mModeTimer = 0;

                if (mFightCycle == 0) {
                    field_0x703 = l_HIO.mBulletNum;
                } else {
                    field_0x703 = 8;
                }

                field_0x711 = 0;
                setActionMode(ACT_SMALL_ATTACK, 0);
                field_0x702 = 0;
                return;
            }
        }

        field_0x70b++;
        if (mFightCycle != 0) {
#if TARGET_PC  // enemy attribute integration
            mModeTimer = std::max<s16>(0, mModeTimer - actor_attr::enemy_sync_timer(this, 50));
#else
            mModeTimer -= 50;
#endif
        }

        setTgHitBit(FALSE);
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x800, 0x80), cLib_addCalcAngleS(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x800, 0x80));

        // This is an avoidance/state-transition radius, not a body-size offset.
        // Scaling it to 2200 at 400% size makes Zant immediately choose mode 11
        // after warping in, so he warps away before beginning the magma jump.
        if (fopAcM_searchPlayerDistance(this) < 550.0f) {
            if (mAcch.ChkGroundHit()) {
                mMode = 11;
                return;
            }
        } else {
            if (checkAvoidWeapon(FALSE)) {
                mMode = 11;
                return;
            }
        }

        if (mMode == 1) {
            if (mAcch.ChkGroundHit() && field_0x6ec == 0) {
                setBck(BCK_ZAN_JUMP_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                mMode = 2;
            }
        } else if (mMode == 2) {
            if (mpModelMorf->checkFrame(6)) {
                mSound.startCreatureVoice(Z2SE_EN_ZAN_MG_V_JUMP, -1);
            }

            if (mpModelMorf->checkFrame(10)) {
                speed.y = 55.0f;
                mMode = 3;
            }
        } else if (mMode == 3) {
            if (speed.y <= 0.0f) {
                setBck(BCK_ZAN_JUMP_B, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                mMode = 4;
            }
        } else if (mMode == 4) {
            if (mpModelMorf->checkFrame(7)) {
                DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 0.0f), mpModelMorf->setPlaySpeed(0.0f));
            }

            if (mAcch.ChkGroundHit()) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_MG_LAND, 0, -1);
                DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 1.0f), mpModelMorf->setPlaySpeed(1.0f));
                mpModelMorf->setFrame(8.0f);
                mMode = 5;
            }
        } else if (mpModelMorf->isStop()) {
            fopAcM_effSmokeSet1(&field_0x3894, &field_0x3898, &current.pos, NULL, 2.0f, &tevStr, 1);
            mMode = 1;
            field_0x6ec = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5), 5);
        }

        if (mAcch.ChkGroundHit() && mModeTimer == 0) {
            mMode = 11;
        }
        break;
    case 10:
        if (field_0x6f0 != 0) {
            return;
        }
    case 11:
        s16 var_r27 = cM_atan2s(daPy_getPlayerActorClass()->current.pos.x, daPy_getPlayerActorClass()->current.pos.z);
        s16 sp1C = (f32)(var_r27 + 0x8000) + cM_rndFX(4.0f) * (f32)0x800;

        mFlyWarpPos.set(cM_ssin(sp1C) * 1100.0f, 1000.0f, cM_scos(sp1C) * 1100.0f);
        field_0x6b8 = sp1C + 0x8000;
        field_0x711 = 0;
        setActionMode(ACT_WARP, 0);
        break;
    }
}

void daB_ZANT_c::executeIceDemo() {
    switch (mMode) {
    case 0:
        current.pos.set(-140.0f, 300.0f, 700.0f);
        old.pos = current.pos;
    case 100:
        for (int i = 0; i < 11; i++) {
            mFoot2Cc[i].OnTgSetBit();
        }

        for (int i = 0; i < 6; i++) {
            mFootCc[iron_tg_cc[i]].OnTgSetBit();
            mFootCc[iron_tg_cc[i]].OnTgShield();
            mFootCc[iron_tg_cc[i]].OnTgIronBallRebound();
        }

        setBck(BCK_ZAN_HUGE, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        speedF = 0.0f;
        gravity = 0.0f;
        shape_angle.z = 0;
        shape_angle.x = 0;
        attention_info.flags = 0;
        maxFallSpeed = -100.0f;

        mMode = 1;
        field_0x70e = 0;
    case 1:
        if (mpModelMorf->checkFrame(8)) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_HUGE, -1);
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &mModelScaleXZ, 10.0f, 0.1f);
        actor_attr::enemy_chase_action_float(this, &mModelScaleY, 10.0f, 0.1f);
#else
        cLib_chaseF(&mModelScaleXZ, 10.0f, 0.1f);
        cLib_chaseF(&mModelScaleY, 10.0f, 0.1f);
#endif

        if (mpModelMorf->checkFrame(115)) {
            mMode = 2;
        }
        break;
    case 2:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, 50.0f, 5.0f), cLib_chaseF(&speed.y, 50.0f, 5.0f));
        if (mpModelMorf->isStop()) {
            setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            setActionMode(ACT_ICE_JUMP, 3);
        }
        break;
    }
}

void daB_ZANT_c::executeIceJump() {
    cXyz sp44(daPy_getPlayerActorClass()->current.pos);
    f32 var_f31 = sp44.absXZ(current.pos);
    cXyz sp50;
    cXyz sp5C;
    cXyz sp68;

    switch (mMode) {
    case 0:
        setBck(BCK_ZAN_HUGE_LANDING, J3DFrameCtrl::EMode_NONE, 3.0f, -1.0f);
        mMode = 1;
        attention_info.flags = 0;
        field_0x70e = 0;
        mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_JUMP, -1);
    case 1:
        if (mpModelMorf->checkFrame(10)) {
            mMode = 2;
            speed.y = 130.0f;
            current.angle.y = shape_angle.y;
        }
        break;
    case 2:
        if (mpModelMorf->isStop()) {
            setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            mMode = 3;
        }
    case 3:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, 15.0f, 5.0f), cLib_chaseF(&speed.y, 15.0f, 5.0f));
        if (current.pos.y > 1500.0f) {
            field_0x70e = 3;
        }

        if (current.pos.y > 2000.0f) {
            mMode = 4;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 90), 90);
            current.angle.y = shape_angle.y;
        }
        break;
    case 4:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, 0.0f, 1.0f), cLib_chaseF(&speed.y, 0.0f, 1.0f));
        if (var_f31 < 300.0f) {
            var_f31 -= 50.0f;
            if (var_f31 < 0.0f) {
                var_f31 = 0.0f;
            }

            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, var_f31 / 10.0f, 1.0f), cLib_chaseF(&speedF, var_f31 / 10.0f, 1.0f));
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_float(this, &speedF, 30.0f, 1.0f);
            actor_attr::enemy_add_action_angle(this, &current.angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x400, 0x80);
#else
            cLib_chaseF(&speedF, 30.0f, 1.0f);
            cLib_addCalcAngleS(&current.angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x400, 0x80);
#endif
        }
        shape_angle.y = current.angle.y;

        if (mModeTimer == 0 && sp44.absXZ(current.pos) < 550.0f) {
            mMode = 5;
            gravity = -5.0f;
            speed.y = 0.0f;
            speedF = 0.0f;

            setBck(BCK_ZAN_HUGE_LANDING, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_LAND, -1);

            mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTL));
            mDoMtx_stack_c::transM(10.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&sp50);

            mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTR));
            mDoMtx_stack_c::transM(10.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&sp5C);

            if (sp44.absXZ(sp50) < sp44.absXZ(sp5C)) {
                field_0x70f = 0;
            } else {
                field_0x70f = 1;
            }
        }
        break;
    case 5:
    case 6:
        if (field_0x70f == 0) {
            mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTL));
        } else {
            mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTR));
        }

        mDoMtx_stack_c::transM(10.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::multVecZero(&sp5C);
        sp68 = sp5C - sp44;
#if TARGET_PC  // enemy attribute integration
        if (sp68.abs() > 0.0f) {
            current.pos -= actor_attr::enemy_move_step(this, sp68 * (20.0f / sp68.abs()));
        }
#else
        current.pos -= sp68 * (20.0f / sp68.abs());
#endif

        if (mMode == 5) {
            if (mpModelMorf->checkFrame(10)) {
                DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 0.0f), mpModelMorf->setPlaySpeed(0.0f));
                mMode = 6;
                mFoot2Cc[0].OnAtSetBit();
                mFoot2Cc[4].OnAtSetBit();
#if TARGET_PC  // enemy attribute integration
                mFoot2Cc[0].SetAtAtp(actor_attr::enemy_attack_power_byte(this, 4));
                mFoot2Cc[4].SetAtAtp(actor_attr::enemy_attack_power_byte(this, 4));
#else
                mFoot2Cc[0].SetAtAtp(4);
                mFoot2Cc[4].SetAtAtp(4);
#endif
            }
        } else {
            if (mAcch.ChkGroundHit()) {
                field_0x70e = 1;
                mSound.startCreatureSound(Z2SE_EN_ZAN_YO_LAND_HUGE, 0, -1);
                dComIfGp_getVibration().StartShock(8, 31, cXyz(0.0f, 1.0f, 0.0f));

                setIceLandingEffect(0);
                setIceLandingEffect(1);

                mFoot2Cc[0].OffAtSetBit();
                mFoot2Cc[4].OffAtSetBit();
#if TARGET_PC  // enemy attribute integration
                mFoot2Cc[0].SetAtAtp(actor_attr::enemy_attack_power_byte(this, 2));
                mFoot2Cc[4].SetAtAtp(actor_attr::enemy_attack_power_byte(this, 2));
#else
                mFoot2Cc[0].SetAtAtp(2);
                mFoot2Cc[4].SetAtAtp(2);
#endif

                gravity = 0.0f;

                DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 1.0f), mpModelMorf->setPlaySpeed(1.0f));
                mMode = 7;
                attention_info.flags = fopAc_AttnFlag_BATTLE_e;
            }
        }
        break;
    case 7:
        if (mpModelMorf->checkFrame(15)) {
            for (int i = 0; i < 6; i++) {
                mFootCc[iron_tg_cc[i]].OffTgShield();
            }
        }

        if (mpModelMorf->isStop()) {
            setActionMode(ACT_ICE_STEP, 0);
        }
        break;
    }
}

void daB_ZANT_c::executeIceStep() {
    s16 var_r29 = shape_angle.y;
    int var_r28;
    cXyz sp60;

    switch (mMode) {
    case 0:
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(50.0f) + 600.0f), cM_rndFX(50.0f) + 600.0f);
        for (int i = 0; i < 6; i++) {
            mFootCc[iron_tg_cc[i]].OffTgShield();
        }
    case 1:
        {
            f32 var_f30 = cM_rnd();
            s16 sp8 = shape_angle.y - fopAcM_searchPlayerAngleY(this);
            f32 var_f29 = DUSK_IF_ELSE(fopAcM_searchPlayerDistance(this) / actor_attr::enemy_size_multiplier(this), fopAcM_searchPlayerDistance(this));

            if (var_f29 < 300.0f) {
                if (sp8 < 0) {
                    if (var_f30 < 0.5f) {
                        var_r28 = 4;
                    } else {
                        var_r28 = 5;
                    }

                    field_0x6ba = fopAcM_searchPlayerAngleY(this) + 0x1000;
                } else {
                    if (var_f30 < 0.5f) {
                        var_r28 = 2;
                    } else {
                        var_r28 = 3;
                    }

                    field_0x6ba = fopAcM_searchPlayerAngleY(this) - 0x1000;
                }
            } else if (var_f29 < 400.0f) {
                if (sp8 > 0) {
                    if (var_f30 < 0.5f) {
                        var_r28 = 2;
                    } else {
                        var_r28 = 3;
                    }

                    field_0x6ba = fopAcM_searchPlayerAngleY(this) + 0x4000;
                } else {
                    if (var_f30 < 0.5f) {
                        var_r28 = 4;
                    } else {
                        var_r28 = 5;
                    }

                    field_0x6ba = fopAcM_searchPlayerAngleY(this) - 0x4000;
                }
            } else if (abs(sp8) > 0x4000) {
                if (sp8 < 0) {
                    var_r28 = 4;
                    field_0x6ba = fopAcM_searchPlayerAngleY(this) - 0x1000;
                } else {
                    var_r28 = 2;
                    field_0x6ba = fopAcM_searchPlayerAngleY(this) + 0x1000;
                }
            } else {
                if (sp8 < 0) {
                    var_r28 = 2;
                    field_0x6ba = fopAcM_searchPlayerAngleY(this) + 0x4000;
                } else {
                    var_r28 = 4;
                    field_0x6ba = fopAcM_searchPlayerAngleY(this) - 0x4000;
                }
            }

            switch (var_r28) {
            case 2:
                setBck(BCK_ZAN_TRAMPLEA, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                mMode = 2;
                break;
            case 3:
                setBck(BCK_ZAN_TRAMPLEC, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                mMode = 3;
                break;
            case 4:
                setBck(BCK_ZAN_TRAMPLEB, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                mMode = 4;
                break;
            case 5:
                setBck(BCK_ZAN_TRAMPLED, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
                mMode = 5;
                break;
            }
        }
    case 2:
    case 3:
    case 4:
    case 5:
        f32 anm_frame = mpModelMorf->getFrame();
        int sp98 = 0;

        if (mMode == 2) {
            if (8.0f <= anm_frame && anm_frame <= 24.0f) {
                sp98 = 1;
            }

            if (mpModelMorf->checkFrame(23)) {
                mFoot2Cc[4].OnAtSetBit();
                mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_FUMI_1, -1);
            }

            if (mpModelMorf->checkFrame(28)) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_YO_FUMI, 0, -1);
                dComIfGp_getVibration().StartShock(6, 31, cXyz(0.0f, 1.0f, 0.0f));
                setIceLandingEffect(1);
            }

            if (mpModelMorf->checkFrame(30)) {
                mFoot2Cc[4].OffAtSetBit();
            }
        } else if (mMode == 3) {
            if (7.0f <= anm_frame && anm_frame <= 14.0f) {
                sp98 = 1;
            }

            if (22.0f <= anm_frame && anm_frame <= 28.0f) {
                sp98 = 1;
            }

            if (mpModelMorf->checkFrame(13) || mpModelMorf->checkFrame(27)) {
                mFoot2Cc[4].OnAtSetBit();
                
                if (mpModelMorf->checkFrame(13)) {
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_FUMI_2, -1);
                } else {
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_FUMI_3, -1);
                }
            }

            if (mpModelMorf->checkFrame(17) || mpModelMorf->checkFrame(30)) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_YO_FUMI, 0, -1);
                dComIfGp_getVibration().StartShock(6, 31, cXyz(0.0f, 1.0f, 0.0f));
                setIceLandingEffect(1);
            }

            if (mpModelMorf->checkFrame(18) || mpModelMorf->checkFrame(31)) {
                mFoot2Cc[4].OffAtSetBit();
            }
        } else if (mMode == 4) {
            if (8.0f <= anm_frame && anm_frame <= 23.0f) {
                sp98 = 1;
            }

            if (mpModelMorf->checkFrame(22)) {
                mFoot2Cc[0].OnAtSetBit();
                mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_FUMI_1, -1);
            }

            if (mpModelMorf->checkFrame(28)) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_YO_FUMI, 0, -1);
                dComIfGp_getVibration().StartShock(6, 31, cXyz(0.0f, 1.0f, 0.0f));
                setIceLandingEffect(0);
            }

            if (mpModelMorf->checkFrame(30)) {
                mFoot2Cc[0].OffAtSetBit();
            }
        } else {
            if (7.0f <= anm_frame && anm_frame <= 13.0f) {
                sp98 = 1;
            }

            if (21.0f <= anm_frame && anm_frame <= 27.0f) {
                sp98 = 1;
            }

            if (mpModelMorf->checkFrame(11) || mpModelMorf->checkFrame(26)) {
                mFoot2Cc[0].OnAtSetBit();
                
                if (mpModelMorf->checkFrame(11)) {
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_FUMI_2, -1);
                } else {
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_FUMI_3, -1);
                }
            }

            if (mpModelMorf->checkFrame(16) || mpModelMorf->checkFrame(31)) {
                mSound.startCreatureSound(Z2SE_EN_ZAN_YO_FUMI, 0, -1);
                dComIfGp_getVibration().StartShock(6, 31, cXyz(0.0f, 1.0f, 0.0f));
                setIceLandingEffect(0);
            }

            if (mpModelMorf->checkFrame(18) || mpModelMorf->checkFrame(32)) {
                mFoot2Cc[0].OffAtSetBit();
            }
        }

        if (sp98 != 0) {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, field_0x6ba, 0x10, 0x400), cLib_addCalcAngleS2(&shape_angle.y, field_0x6ba, 0x10, 0x400));
        }

        if (mMode <= 3) {
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::YrotM(var_r29);
            mDoMtx_stack_c::transM(mModelScaleXZ * 35.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&sp60);
            mDoMtx_stack_c::transS(sp60);
            mDoMtx_stack_c::YrotM(shape_angle.y);
            mDoMtx_stack_c::transM(mModelScaleXZ * -35.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&current.pos);
        } else {
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::YrotM(var_r29);
            mDoMtx_stack_c::transM(mModelScaleXZ * -30.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&sp60);
            mDoMtx_stack_c::transS(sp60);
            mDoMtx_stack_c::YrotM(shape_angle.y);
            mDoMtx_stack_c::transM(mModelScaleXZ * 30.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&current.pos);
        }

        if (mpModelMorf->isStop()) {
            if (mModeTimer != 0) {
                mMode = 1;
            } else {
                for (int i = 0; i < 6; i++) {
                    mFootCc[iron_tg_cc[i]].OnTgShield();
                }

                setActionMode(ACT_ICE_JUMP, 0);
            }
        }
        break;
    }
}

void daB_ZANT_c::executeIceDamage() {
    static f32 const damage_scale[] = {
        10.0f, 8.0f, 7.5f, 6.0f, 4.5f, 3.0f, 2.0f, 1.4f, 1.0f, 0.69999999f, 0.5f,
    };

    static f32 const damage_jump_speed[] = {
        100.0f, 95.0f, 90.0f, 85.0f, 80.0f, 75.0f, 70.0f, 65.0f, 60.0f, 55.0f, 50.0f
    };

    daPy_py_c* player = daPy_getPlayerActorClass();
#if TARGET_PC  // enemy attribute integration

    // Vanilla has many host frames between the two hop markers, so the
    // three-frame shrink calculation finishes while Zant is grounded.  At a
    // fast action clock the next marker can arrive first; continue the same
    // interpolation during the hop so the shrink cannot freeze until the next
    // hit.  The literal 100% path remains unchanged.
    if (actor_attr::enemy_action_time_speed(this) != 1.0f && (mMode == 3 || mMode == 4) && field_0x70b <= 10)
    {
        actor_attr::enemy_add_action_calc2(this, &mModelScaleXZ, damage_scale[field_0x70b], 0.5f, field_0x6cc);
        mModelScaleY = mModelScaleXZ;
    }
#endif

    if (mMode >= 10 && mModeTimer == 0) {
        for (int i = 0; i < 11; i++) {
            mFoot2Cc[i].OnTgShield();
            mFoot2Cc[i].OffTgSetBit();
            mFoot2Cc[i].OffTgNoHitMark();
        }

        for (int i = 0; i < 6; i++) {
            mFootCc[iron_tg_cc[i]].OffTgSetBit();
            mFootCc[iron_tg_cc[i]].OnTgShield();
            mFootCc[iron_tg_cc[i]].OnTgIronBallRebound();
        }
    }

    switch (mMode) {
    case 0:
    case 1:
        field_0x70e = 2;

        if (mMode == 0) {
            setBck(BCK_ZAN_SHIND_L, J3DFrameCtrl::EMode_LOOP, 10.0f, 1.0f);
        } else {
            setBck(BCK_ZAN_SHIND_R, J3DFrameCtrl::EMode_LOOP, 10.0f, 1.0f);
        }

        mFoot2Cc[0].OffAtSetBit();
        mFoot2Cc[4].OffAtSetBit();

        for (int i = 0; i < 6; i++) {
            mFootCc[iron_tg_cc[i]].OnTgShield();
        }

        current.angle.y = shape_angle.y;
        mMode = 2;
        //mModeTimer = actor_attr::enemy_stun_timer(this, 300); // need to include time it takes for zant to become vulnerable.
        // this should do for now until above is fixed.
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 300.0f), 300);
        field_0x70b = 0;
        field_0x70b++;
        field_0x6cc = (mModelScaleXZ - damage_scale[field_0x70b]) / 3.0f;
    case 2:
        if (mpModelMorf->checkFrame(6) || mpModelMorf->checkFrame(19)) {
            if (mpModelMorf->getFrame() < 14) {
                mpModelMorf->setFrame(6);
            } else {
                mpModelMorf->setFrame(19);
            }

            if (field_0x70b <= 8) {
                speedF = 25.0f;
            } else if (field_0x70b == 9) {
                if (mFightCycle == 0) {
                    speedF = 20.0f;
                } else {
                    speedF = 23.0f;
                }
            } else if (mFightCycle == 0) {
                speedF = 15.0f;
            } else {
                speedF = 22.0f;
            }

            speed.y = damage_jump_speed[field_0x70b];
            gravity = -15.0f;
            mMode = 3;

            mSound.startCreatureVoice(Z2SE_EN_ZAN_YO_V_KENKEN, -1);
        } else {
            if (field_0x70b <= 10) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &mModelScaleXZ, damage_scale[field_0x70b], 0.5f, field_0x6cc), cLib_addCalc2(&mModelScaleXZ, damage_scale[field_0x70b], 0.5f, field_0x6cc));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mModelScaleXZ, 0.5f, 0.1f), cLib_chaseF(&mModelScaleXZ, 0.5f, 0.1f));
            }

            mModelScaleY = mModelScaleXZ;
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, field_0x6ba, 8, 0x1000), cLib_addCalcAngleS2(&shape_angle.y, field_0x6ba, 8, 0x1000));
            current.angle.y = shape_angle.y;
            speedF = 0.0f;

            if (mModeTimer == 0) {
                mMode = 30;
            }
        }
        break;
    case 3:
        if (mpModelMorf->checkFrame(12) || mpModelMorf->checkFrame(25)) {
            if (mpModelMorf->getFrame() < 14) {
                mpModelMorf->setFrame(12);
            } else {
                mpModelMorf->setFrame(25);
            }

            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 0.0f), mpModelMorf->setPlaySpeed(0.0f));
            mMode = 4;
        }
        break;
    case 4:
        if (mAcch.ChkGroundHit()) {
            s16 sp8 = fopAcM_searchPlayerAngleY(this) + (4096.0f * cM_rndFX(4.9f)) + 32768.0f;

            if (current.pos.absXZ() > 1200.0f) {
                sp8 = cM_atan2s(-current.pos.x, -current.pos.z);
            }

            field_0x6ba = sp8;

            if (field_0x70b < 10) {
                field_0x70b++;
            }

            field_0x6cc = (mModelScaleXZ - damage_scale[field_0x70b]) / 3.0f;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, (10.0f - mModelScaleXZ) / 10.0f + 1.0f), mpModelMorf->setPlaySpeed((10.0f - mModelScaleXZ) / 10.0f + 1.0f));

            if (field_0x70b == 8) {
                for (int i = 0; i < 11; i++) {
                    mFoot2Cc[i].OffTgShield();
                    mFoot2Cc[i].OnTgNoHitMark();
                }

                for (int i = 0; i < 6; i++) {
                    mFootCc[iron_tg_cc[i]].OffTgShield();
                    mFootCc[iron_tg_cc[i]].OffTgIronBallRebound();
                }
            }

            mSound.startCreatureSound(Z2SE_EN_ZAN_YO_LAND, 0, -1);
            mMode = 2;
        }
        break;
    case 10:
        if (setNextDamageMode(0)) {
            return;
        }

        if (cM_rnd() < 0.5f) {
            setBck(BCK_ZAN_DAMAGEL_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        } else {
            setBck(BCK_ZAN_DAMAGER_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        }

        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
        speed.y = 0.0f;
        speedF = 0.0f;
        mMode = 11;
    case 11:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mModelScaleXZ, 0.5f, 0.1f), cLib_chaseF(&mModelScaleXZ, 0.5f, 0.1f));
        mModelScaleY = mModelScaleXZ;

        if (mpModelMorf->isStop()) {
            mMode = 12;
#if TARGET_PC  // enemy attribute integration
            if (mModeTimer >= actor_attr::enemy_sync_timer(this, 30)) {
                mModeTimer = actor_attr::enemy_sync_timer(this, 30);
#else
            if (mModeTimer >= 30) {
                mModeTimer = 30;
#endif
            }

            setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        }
        break;
    case 12:
        if (mModeTimer == 0) {
            mMode = 30;
        }
        break;
    case 20:
        setBck(BCK_ZAN_GROUND_REACTION, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_NO_DMG, -1);
        mMode = 21;
        speed.y = 0.0f;
        speedF = 0.0f;
    case 21:
        if (mpModelMorf->isStop()) {
            mMode = 30;
        }
        break;
    case 40:
        setBck(BCK_ZAN_FAINT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        mMode = 41;
        speed.y = 0.0f;
        speedF = 0.0f;
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 30), 30);
    case 41:
        if (mModeTimer == 0) {
            mMode = 30;
        }
        break;
    case 30:
        for (int i = 0; i < 11; i++) {
            mFoot2Cc[i].OnTgShield();
            mFoot2Cc[i].OffTgSetBit();
            mFoot2Cc[i].OffTgNoHitMark();
        }

        for (int i = 0; i < 6; i++) {
            mFootCc[iron_tg_cc[i]].OffTgSetBit();
            mFootCc[iron_tg_cc[i]].OnTgShield();
            mFootCc[iron_tg_cc[i]].OnTgIronBallRebound();
        }

        if (!setNextDamageMode(1)) {
            mFlyWarpPos.set(0.0f, 300.0f, 0.0f);
            field_0x711 = 1;
            setActionMode(ACT_WARP, 1);
            gravity = 0.0f;
        }
        break;
    }
}

void daB_ZANT_c::setFarPillarPos() {
    fopAc_ac_c* ppillar;
    cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
    int pillar_no = -1;
    f32 farthest_dist = 0.0f;

    for (int i = 0; i < 8; i++) {
        if (i != field_0x70a) {
            fopAcM_SearchByID(mPillarIDs[i], &ppillar);
            if (ppillar != NULL) {
                f32 dist = player_pos.abs(ppillar->current.pos);
                if (dist > farthest_dist) {
                    pillar_no = i;
                    farthest_dist = dist;
                }
            }
        }
    }

    if (pillar_no != -1) {
        fopAcM_SearchByID(mPillarIDs[pillar_no], &ppillar);
        if (ppillar != NULL) {
            mFlyWarpPos = ppillar->current.pos;
            mFlyWarpPos.y += 500.0f;
            field_0x709 = pillar_no;
            field_0x708 = pillar_no;
            field_0x70a = pillar_no;
            field_0x71b = 1;
        }
    }
}

void daB_ZANT_c::setNearPillarPos() {
    fopAc_ac_c* ppillar;
    fopAcM_SearchByID(mPillarIDs[8], &ppillar);

    if (ppillar != NULL) {
        mFlyWarpPos = ppillar->current.pos;
        mFlyWarpPos.y += 500.0f;
        field_0x709 = 8;
        field_0x708 = 8;
        field_0x70a = 8;
        field_0x71b = 0;
    }
}

void daB_ZANT_c::setNextPillarInfo(int i_pillarNo) {
    fopAc_ac_c* ppillar;
    fopAcM_SearchByID(mPillarIDs[i_pillarNo], &ppillar);

    if (ppillar != NULL) {
        field_0x6ac = ppillar->current.pos;
        field_0x6ac.y += 500.0f;
        field_0x708 = field_0x709;
        field_0x709 = i_pillarNo;
    }
}

void daB_ZANT_c::setNextPillarPos() {
    fopAc_ac_c* ppillar;
    cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
    int pillar_no = -1;
    f32 nearest_dist = 10000.0f;

    if (field_0x708 != 8 && field_0x709 != 8 && cM_rnd() < 0.5) {
        fopAcM_SearchByID(mPillarIDs[8], &ppillar);

        if (ppillar != NULL && player_pos.absXZ(ppillar->current.pos) > 500.0f) {
            setNextPillarInfo(8);
            return;
        }
    }

    if (field_0x709 == 8) {
        fopAcM_SearchByID(mPillarIDs[8], &ppillar);
        if (ppillar != NULL) {
            cXyz pillar_pos(ppillar->current.pos);
            s16 sp8 = cLib_targetAngleY(&pillar_pos, &player_pos);

            for (int i = 0; i < 8; i++) {
                if (i != field_0x708 && i != field_0x709) {
                    fopAcM_SearchByID(mPillarIDs[i], &ppillar);
                    if (ppillar != NULL && abs((s16)(cLib_targetAngleY(&pillar_pos, &ppillar->current.pos) - sp8)) > 0x6000) {
                        setNextPillarInfo(i);
                        return;
                    }
                }
            }
        }
    }

    for (int i = 0; i < 8; i++) {
        if (i != field_0x708 && i != field_0x709) {
            fopAcM_SearchByID(mPillarIDs[i], &ppillar);
            if (ppillar != NULL && player_pos.absXZ(ppillar->current.pos) > 500.0f) {
                f32 dist = current.pos.abs(ppillar->current.pos);
                if (dist < nearest_dist) {
                    pillar_no = i;
                    nearest_dist = dist;
                }
            }
        }
    }

    if (pillar_no != -1) {
        setNextPillarInfo(pillar_no);
    }
}

void daB_ZANT_c::checkPillarSwing() {
    if (field_0x707 != 0) {
        fopAc_ac_c* ppillar;
        fopAcM_SearchByID(mPillarIDs[field_0x70a], &ppillar);

        if (ppillar != NULL && ((daPillar_c*)ppillar)->checkRollAttack()) {
            if (mAction != ACT_MONKEY_FALL) {
                setActionMode(ACT_MONKEY_FALL, 0);
            } else {
                setActionMode(ACT_MONKEY_FALL, 10);
            }
        }
    }
}

void daB_ZANT_c::executeMonkey() {
    fopAc_ac_c* ppillar;

    switch (mMode) {
    case 0:
        gravity = -5.0f;
        field_0x707 = 1;
        setTgHitBit(FALSE);
        mBodySphCc[0].OnTgNoHitMark();
        mBodySphCc[1].OnTgNoHitMark();
        field_0x708 = field_0x709 = field_0x70a;

        if (mLastAction == ACT_WARP) {
            if (field_0x71b != 0) {
                field_0x71b = 0;
                field_0x703 = l_HIO.mBulletNum;
                field_0x711 = 0;
                setActionMode(ACT_SMALL_ATTACK, 0);
                setNextPillarPos();
                return;
            }

            setNextPillarPos();
            setBck(BCK_ZAN_LV1_JUMP_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            mMode = 8;
        } else if (mLastAction == ACT_SMALL_ATTACK) {
            mMode = 5;
            setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            mModeTimer = 0;
        } else {
            mMode = 5;
            setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        }

        field_0x70b = cM_rndF(2.9f) + 3.0f;
        field_0x6fd = 0;
        field_0x71b = 0;
        break;
    case 1:
        if (mpModelMorf->checkFrame(10) || mpModelMorf->checkFrame(14)) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_MK_LAND, -1);
        }

        if (mpModelMorf->isStop()) {
            field_0x6fd++;

            if (field_0x6fd >= field_0x70b) {
                if (mFightCycle != 0) {
                    setFarPillarPos();
                    field_0x711 = 0;
                    setActionMode(ACT_WARP, 1);
                } else {
                    setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
                    mMode = 2;
                }
            } else {
                mMode = 5;
                setBck(BCK_ZAN_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            }
        }
        break;
    case 2:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x800, 0x80), cLib_addCalcAngleS(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 8, 0x800, 0x80));
        if (abs((s16)(shape_angle.y - fopAcM_searchPlayerAngleY(this))) < 0x2000) {
            field_0x703 = l_HIO.mBulletNum;
            field_0x711 = 0;
            setActionMode(ACT_SMALL_ATTACK, 0);
        }
        break;
    case 5:
        setTgHitBit(FALSE);
        setNextPillarPos();
        mMode = 6;
    case 6:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x6ac), 8, 0x800), cLib_addCalcAngleS2(&shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x6ac), 8, 0x800));

        if (mModeTimer == 0 && mAcch.ChkGroundHit()) {
            setBck(BCK_ZAN_LV1_JUMP_A, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
            mMode = 8;
        }
        break;
    case 8:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x6ac), 8, 0x800), cLib_addCalcAngleS2(&shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x6ac), 8, 0x800));

        if (mpModelMorf->checkFrame(13)) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_MK_V_JUMP, -1);
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 18), 18);
            speed.y = 45.0f;
            mMode = 9;
            field_0x707 = 0;

            fopAcM_SearchByID(mPillarIDs[field_0x70a], &ppillar);
            if (ppillar != NULL) {
                ((daPillar_c*)ppillar)->setShake(daPillar_c::SHAKE_WEAK);
            }
        }
        break;
#if TARGET_PC  // enemy attribute integration
    case 9: {
#else
    case 9:
#endif
        if (mModeTimer != 0) {
#if TARGET_PC  // enemy attribute integration
            // This jump must arrive at the selected pillar exactly even when
            // action speed compresses the 18-frame travel window down to only
            // a handful of real frames.  cLib_chasePosXZ() is intended for
            // gradual convergence and can finish short with very large steps,
            // so consume an exact fraction of the remaining X/Z delta instead.
            const f32 framesRemaining = static_cast<f32>(mModeTimer);
            current.pos.x += (field_0x6ac.x - current.pos.x) / framesRemaining;
            current.pos.z += (field_0x6ac.z - current.pos.z) / framesRemaining;
#else
            cXyz sp2C(field_0x6ac.x, current.pos.y, field_0x6ac.z);
            cLib_chasePosXZ(&current.pos, sp2C, field_0x6ac.absXZ(current.pos) / mModeTimer);
#endif
        }

        if (mModeTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_countdown_milestone(this, 18, 8), 8)) {
            setBck(BCK_ZAN_LV1_JUMP_B, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        }

#if TARGET_PC  // enemy attribute integration
        // A quarter-size collision body can pass completely through the
        // pillar top in one 4x integration step.  Once the descending jump is
        // horizontally over its logical waypoint, treat crossing the target
        // height as the same landing that vanilla collision would report.
        const bool crossedPillarTop = actor_attr::enemy_action_time_speed(this) != 1.0f &&
                                      speed.y <= 0.0f && current.pos.y <= field_0x6ac.y &&
                                      current.pos.absXZ(field_0x6ac) < 150.0f;
        const bool reachedTargetXZ = current.pos.absXZ(field_0x6ac) < 150.0f;
        if ((mAcch.ChkGroundHit() && reachedTargetXZ) || crossedPillarTop) {
#else
        if (mAcch.ChkGroundHit()) {
#endif
            current.pos = field_0x6ac;
#if TARGET_PC  // enemy attribute integration
            if (speed.y < 0.0f) { // double check this
                speed.y = 0.0f;
            }
#endif
            field_0x707 = 1;
            field_0x70a = field_0x709;
            
            fopAcM_SearchByID(mPillarIDs[field_0x70a], &ppillar);
            if (ppillar != NULL) {
                ((daPillar_c*)ppillar)->setShake(daPillar_c::SHAKE_WEAK);
            }

            mMode = 1;
            field_0x6fd++;
        }
        break;
#if TARGET_PC  // enemy attribute integration
    }
#endif
    case 100:
        target_info_count = 0;
        fpcM_Search(s_pillar_sub, this);

        if (target_info_count < 8 || mPillarIDs[8] == fpcM_ERROR_PROCESS_ID_e) {
            for (int i = 0; i < 9; i++) {
                mPillarIDs[i] = fpcM_ERROR_PROCESS_ID_e;
            }
        } else {
            setNearPillarPos();
            field_0x711 = 0;
            setActionMode(ACT_WARP, 1);
        }
        break;
    }
}

void daB_ZANT_c::executeMonkeyFall() {
    switch (mMode) {
    case 0:
    case 20:
        setTgHitBit(TRUE);
        // While Zant is stunned on top of a pillar, allow the randomized
        // stun duration to shorten this state, but never let it exceed 50%
        // of the vanilla duration.
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 90), 90);

        if (mMode == 20) {
            // Direct-hit stun received while Zant is still standing on top of
            // the pillar. Keep randomized shorter stuns, but never allow this
            // vulnerable window to exceed 50% of its vanilla 30-frame length.
            mModeTimer = DUSK_IF_ELSE(std::min<s16>(actor_attr::enemy_stun_timer(this, 30), 15), 30);
        }

        setBck(BCK_ZAN_FAINT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        mMode = 1;

        mBodySphCc[0].OffTgNoHitMark();
        mBodySphCc[1].OffTgNoHitMark();

        mSound.startCreatureVoice(Z2SE_EN_ZAN_MK_V_OTT, -1);
    case 1:
        if (mModeTimer == 0) {
            setActionMode(ACT_MONKEY, 0);
        }
        break;
    case 10:
        setBck(BCK_ZAN_FALL, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mMode = 11;
        field_0x707 = 0;
        setTgHitBit(FALSE);
        speedF = 0.0f;
    case 11:
        if (mpModelMorf->checkFrame(5)) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_MK_V_FALL, -1);
        }

        if (mpModelMorf->isStop()) {
            cXyz offset(-30.0f, -170.0f, -175.0f);
            cLib_offsetPos(&current.pos, &current.pos, shape_angle.y, &offset);
            old.pos = current.pos;
            speed.y = -15.0f;
            speedF = 8.0f;
            current.angle.y = shape_angle.y + 0x8000;

            setBck(BCK_ZAN_LANDING, J3DFrameCtrl::EMode_NONE, 0.0f, 0.0f);
            mMode = 12;

            mBodySphCc[0].OnAtSetBit();
            mBodySphCc[1].OnAtSetBit();
        }
        break;
    case 12:
        if (mAcch.ChkGroundHit()) {
            mSound.startCreatureSound(Z2SE_EN_ZAN_MK_UMARU, 0, -1);
            setMonkeyFallEffect();
            dComIfGp_getVibration().StartShock(3, 31, cXyz(0.0f, 1.0f, 0.0f));

            mBodySphCc[0].OffAtSetBit();
            mBodySphCc[1].OffAtSetBit();

            speedF = 0.0f;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 1.0f), mpModelMorf->setPlaySpeed(1.0f));
            mMode = 13;
            field_0x702 = 0;
            setTgHitBit(TRUE);
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 120), 120);
        }
        break;
    case 13:
        if (mpModelMorf->isStop()) {
            setActionMode(ACT_MONKEY_DAMAGE, 5);
        }
        break;
    }
}

void daB_ZANT_c::executeMonkeyDamage() {
    switch (mMode) {
    case 0:
        if (setNextDamageMode(0)) {
            setTgHitBit(FALSE);
            return;
        }

        setBck(BCK_ZAN_LANDING_DAMAGE, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
        mMode = 1;
        field_0x702 = 0;
    case 1:
        if (field_0x6f0 == 0) {
            setTgHitBit(FALSE);
        }

        if (mpModelMorf->isStop()) {
            setBck(BCK_ZAN_LANDING_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            mMode = 10;
        }
        break;
    case 5:
    case 6:
        if (mMode == 5) {
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 100), 100);
            field_0x6ff = 0;
#if TARGET_PC  // enemy attribute integration
        } else if (field_0x6f0 < actor_attr::enemy_stun_timer(this, 30)) {
            field_0x6f0 = actor_attr::enemy_stun_timer(this, 30);
#else
        } else if (field_0x6f0 < 30) {
            field_0x6f0 = 30;
#endif
        }

        setBck(BCK_ZAN_LANDING_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        mMode = 10;
        field_0x702 = 0;
    case 10:
        if (mpModelMorf->checkFrame(1)) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_MK_V_MGGG, -1);
        }

        if (field_0x6f0 == 0) {
            setTgHitBit(FALSE);

            if (!setNextDamageMode(1)) {
                setNearPillarPos();
                field_0x711 = 0;
                setActionMode(ACT_WARP, 1);
            }
        }
        break;
    }
}

void daB_ZANT_c::setLastWarp(int param_0, int i_warpID) {
    mFlyWarpPosID = i_warpID;

    if (mFightCycle == 0) {
        if (health <= DUSK_IF_ELSE(actor_attr::enemy_health_value(this, 400), 400)) {
            mFightCycle = 1;
        }
    } else if (mFightCycle == 1 && health <= DUSK_IF_ELSE(actor_attr::enemy_health_value(this, 200), 200)) {
        mFightCycle = 2;
    }

    if (field_0x713 != mFightCycle) {
        field_0x71d = 0;
        field_0x703 = 0;
        field_0x71c = 0;
        field_0x712 = 0;

        if (mFightCycle == 2) {
            field_0x6ec = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 450), 450);
            field_0x71d = 1;
            field_0x6ff = 0;
        }
    }

    field_0x713 = mFightCycle;

    cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
    s16 var_r28;

    if (field_0x713 == 2 && param_0) {
        if (field_0x703 <= 1 || field_0x6ff >= 3) {
            if (field_0x71d == 1) {
                param_0 = 3;
            } else {
                param_0 = 1;
            }
        } else {
            if (cM_rnd() < 0.5f) {
                param_0 = 3;
            } else {
                param_0 = 1;
            }

            if (field_0x71d == param_0) {
                field_0x6ff++;
            } else {
                field_0x6ff = 0;
            }
        }

        field_0x71d = param_0;
    }

    switch (param_0) {
    case 0:
        mFlyWarpPos.set(0.0f, 0.0f, 1000.0f);
        field_0x70f = 0;
        mDrawSwords = true;
        mSwordSize = 1.0f;
        break;
    case 1:
        if (field_0x713 == 0) {
            field_0x70f = 0;
            
            field_0x703++;
            if (field_0x703 >= 3 && cM_rnd() < (field_0x703 / 10.0f) + 0.3f) {
                field_0x70f = 10;
                field_0x71d = 0;
                field_0x703 = 0;
                field_0x71c = 0;
                field_0x712 = 0;
            }

            field_0x704 = 0;

            var_r28 = cM_atan2s(-player_pos.x, -200.0f - player_pos.z);
            mFlyWarpPos.set(cM_ssin(var_r28) * 1000.0f, 0.0f, cM_scos(var_r28) * 1000.0f - 200.0f);
        } else {
            field_0x70f = 0;
            field_0x703++;

            if (field_0x713 != 2 && field_0x703 >= 3 && cM_rnd() < (field_0x703 / 10.0f) + 0.3f) {
                field_0x70f = 10;
                field_0x71d = 0;
                field_0x703 = 0;
                field_0x71c = 0;
                field_0x712 = 0;
            }

            field_0x704 = 0;
            f32 var_f31 = cM_rndF(200.0f) + 300.0f;
            var_r28 = daPy_getPlayerActorClass()->shape_angle.y + 0x8000;

            if (field_0x70f == 10) {
                var_f31 += 400.0f;
                var_r28 = daPy_getPlayerActorClass()->shape_angle.y + cM_rndFX(2.9f) * 8192.0f;
            } else if (dComIfGp_checkPlayerStatus0(0, 0x400)) {
                var_f31 += 400.0f;
            }

            mFlyWarpPos.set(cM_ssin(var_r28) * var_f31, 0.0f, cM_scos(var_r28) * var_f31);
            mFlyWarpPos += player_pos;

            if (mFlyWarpPos.z > 1000.0f || mFlyWarpPos.z < -1400.0f || std::abs(mFlyWarpPos.x) > 1200.0f) {
                var_r28 = cM_atan2s(-player_pos.x, -200.0f - player_pos.z);
                mFlyWarpPos.set(cM_ssin(var_r28) * var_f31, 0.0f, cM_scos(var_r28) * var_f31);
                mFlyWarpPos += player_pos;
            }
        }
        break;
    case 2: {
        f32 var_f29 = player_pos.absXZ(current.pos);
        s16 spE = cLib_targetAngleY(&player_pos, &current.pos);
        s16 spC = cM_atan2s(-player_pos.x, -player_pos.z);

        if ((s16)(spE - spC) < 0) {
            var_r28 = spE + 0x2000;
        } else {
            var_r28 = spE - 0x2000;
        }

        if (var_f29 < 400.0f) {
            var_f29 = 400.0f;
        }

        mFlyWarpPos.set(var_f29 * cM_ssin(var_r28), 0.0f, var_f29 * cM_scos(var_r28));
        mFlyWarpPos += player_pos;
        field_0x70f = 20;
        break;
    }
    case 3:
        f32 var_f30 = cM_rndF(200.0f) + 700.0f;
        if (field_0x713 == 0) {
            var_f30 += 400.0f;
            var_r28 = cM_atan2s(-player_pos.x, -200.0f - player_pos.z);
        } else {
            var_f30 += 400.0f;
            var_r28 = daPy_getPlayerActorClass()->shape_angle.y + cM_rndFX(3.9f) * 8192.0f;
        }

        mFlyWarpPos.set(cM_ssin(var_r28) * var_f30, 0.0f, cM_scos(var_r28) * var_f30);
        mFlyWarpPos += player_pos;

        if (mFlyWarpPos.z > 1000.0f || mFlyWarpPos.z < -1400.0f || std::abs(mFlyWarpPos.x) > 1200.0f) {
            var_r28 = cM_atan2s(-player_pos.x, -200.0f - player_pos.z);
            mFlyWarpPos.set(cM_ssin(var_r28) * var_f30, 0.0f, cM_scos(var_r28) * var_f30);
            mFlyWarpPos += player_pos;
        }

        field_0x70f = 10;
        field_0x704 = 1;
        break;
    }

    field_0x711 = 0;
    setActionMode(ACT_WARP, 1);
}

void daB_ZANT_c::executeLastStartDemo() {
    dCamera_c* camera = dCam_getBody();
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz sp1C;
    cXyz sp28(0.0f, 0.0f, 0.0f);

    switch (mMode) {
    case 0:
        if (!eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(this, 2, 0xFFFF, 0);
            eventInfo.onCondition(2);
        } else {
            camera->Stop();
            camera->SetTrimSize(3);
            setBck(BCK_ZAN_LAST_DEMO, J3DFrameCtrl::EMode_NONE, 10.0f, 1.0f);
            mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_OP, 0, -1);
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 0.0f), mpModelMorf->setPlaySpeed(0.0f));

            gravity = -5.0f;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
            mMode = 1;
            field_0x71d = 0;
            field_0x703 = 0;
            field_0x71c = 0;
            field_0x712 = 0;
        }
        break;
    case 1:
        if (mModeTimer == 0) {
            current.pos.set(0.0f, 480.0f, -1000.0f);
            shape_angle.y = 0;
            shape_angle.z = 0;
            shape_angle.x = 0;
            current.angle.y = -0x8000;
            speedF = 0.0f;

            mDemoCamEye.set(0.0f, 50.0f, -600.0f);
            mDemoCamCenter.set(0.0f, 200.0f, -1000.0f);
            mDemoCamBank = 60.0f;
            mMode = 2;
            return;
        }
        break;
    case 2:
        player->setPlayerPosAndAngle(&sp28, 0x8000, 0);

        sp1C.set(0.0f, 90.0f, -900.0f);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, sp1C, 0.2f, 5.0f, 1.0f), cLib_addCalcPos(&mDemoCamCenter, sp1C, 0.2f, 5.0f, 1.0f));

        if (mAcch.ChkGroundHit()) {
            speed.y = 0.0f;
            speedF = 0.0f;
            dComIfGp_getVibration().StartShock(4, 31, cXyz(0.0f, 1.0f, 0.0f));
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 1.0f), mpModelMorf->setPlaySpeed(1.0f));
            mMode = 3;
        }
        break;
    case 3:
        sp1C = eyePos;
        sp1C.y -= 30.0f;

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, sp1C, 0.5f, 50.0f, 1.0f), cLib_addCalcPos(&mDemoCamCenter, sp1C, 0.5f, 50.0f, 1.0f));
        if (mpModelMorf->checkFrame(97)) {
            mMode = 4;
        }
        break;
    case 4:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &mDemoCamBank, 72.0f, 0.3f, 1.0f), cLib_addCalc2(&mDemoCamBank, 72.0f, 0.3f, 1.0f));
        sp1C = eyePos;
        sp1C.y -= 30.0f;

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &mDemoCamCenter.y, sp1C.y, 0.5f, 50.0f), cLib_addCalc2(&mDemoCamCenter.y, sp1C.y, 0.5f, 50.0f));
        sp1C.y = mDemoCamCenter.y;

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos_xz(this, &mDemoCamCenter, sp1C, 0.5f, 3.0f, 1.0f), cLib_addCalcPosXZ(&mDemoCamCenter, sp1C, 0.5f, 3.0f, 1.0f));
        if (mpModelMorf->checkFrame(103)) {
            dComIfGp_getVibration().StartShock(2, 31, cXyz(0.0f, 1.0f, 0.0f));
            mMode = 5;
        }
        break;
    case 5:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &mDemoCamBank, 72.0f, 0.3f, 1.0f), cLib_addCalc2(&mDemoCamBank, 72.0f, 0.3f, 1.0f));
        sp1C = eyePos;
        sp1C.y = mDemoCamCenter.y;

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos_xz(this, &mDemoCamCenter, sp1C, 0.5f, 3.0f, 1.0f), cLib_addCalcPosXZ(&mDemoCamCenter, sp1C, 0.5f, 3.0f, 1.0f));
        if (mpModelMorf->checkFrame(134)) {
            mDrawSwords = true;
            mMode = 6;
        }
        break;
    case 6:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(this, &mDemoCamCenter.y, 128.0f, 0.3f, 3.0f);
        actor_attr::enemy_add_action_calc2(this, &mDemoCamBank, 55.0f, 0.3f, 3.0f);
#else
        cLib_addCalc2(&mDemoCamCenter.y, 128.0f, 0.3f, 3.0f);
        cLib_addCalc2(&mDemoCamBank, 55.0f, 0.3f, 3.0f);
#endif

        if (mpModelMorf->isStop()) {
            mFightCycle = 0;
            field_0x713 = 0;
            setActionMode(ACT_LAST_ATTACK, 0);
            field_0x70b = 0;
            attention_info.flags = fopAc_AttnFlag_BATTLE_e;

            mDemoCamEye.set(0.0f, 300.0f, 200.0f);
            camera->Reset(mDemoCamCenter, mDemoCamEye);
            camera->Start();
            camera->SetTrimSize(0);
            dComIfGp_event_reset();
            return;
        }
        break;
    }

    camera->Set(mDemoCamCenter, mDemoCamEye, mDemoCamBank, 0);
}

void daB_ZANT_c::executeLastAttack() {
    s16 angle_to_player = fopAcM_searchPlayerAngleY(this);

    switch (mMode) {
    case 0:
        gravity = -5.0f;

        if (field_0x713 == 0) {
            mMode = 1;
            setBck(BCK_ZAN_SW_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
        } else {
            mMode = 5;
        }
        break;
    case 1:
        if (mpModelMorf->checkFrame(2) || mpModelMorf->checkFrame(17)) {
            mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_SWD_RUB, 0, -1);
        }

        if (checkAvoidWeapon(0)) {
            mSwordCc[0].OffAtSetBit();
            mSwordCc[1].OffAtSetBit();
            setTgHitBit(FALSE);

            field_0x71c++;
            if (field_0x71c < 5) {
                setLastWarp(2, 0);
            } else {
                field_0x703 = 7;
                setLastWarp(1, 0);
            }
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, angle_to_player, 0x10, 0x400), cLib_addCalcAngleS2(&shape_angle.y, angle_to_player, 0x10, 0x400));
            current.angle.y = shape_angle.y;

            if (fopAcM_searchPlayerDistance(this) < DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 800.0f), 800.0f) || mModeTimer == 0) {
                mMode = 2;
                setBck(BCK_ZAN_SW_WALK, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            }
        }
        break;
    case 2:
        if (mpModelMorf->checkFrame(10) || mpModelMorf->checkFrame(26)) {
            mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_FOOT, 0, -1);
        }

        if (checkAvoidWeapon(0)) {
            mSwordCc[0].OffAtSetBit();
            mSwordCc[1].OffAtSetBit();
            setTgHitBit(FALSE);

            field_0x71c++;
            if (field_0x71c < 5) {
                setLastWarp(2, 0);
            } else {
                field_0x703 = 7;
                setLastWarp(1, 0);
            }
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, angle_to_player, 0x10, 0x400), cLib_addCalcAngleS2(&shape_angle.y, angle_to_player, 0x10, 0x400));
            current.angle.y = shape_angle.y;
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 5.0f, 1.0f), cLib_chaseF(&speedF, 5.0f, 1.0f));

            if (fopAcM_searchPlayerDistance(this) < DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 500.0f), 500.0f)) {
                mMode = 5;
            }
        }
        break;
    case 5:
        if (field_0x713 == 0) {
            setBck(BCK_ZAN_SW_ATTACK, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 200), 200);
            speedF = 5.0f;
        } else {
            setBck(BCK_ZAN_SW_ATTACKB, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);

            if (field_0x713 != 2) {
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(60.0f) + 60.0f), cM_rndF(60.0f) + 60.0f);
            } else {
                mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(15.0f) + 30.0f), cM_rndF(15.0f) + 30.0f);
            }

            speedF = 10.0f;
        }

        mMode = 6;
        field_0x6f8 = 0;
        current.angle.y = shape_angle.y;

        setTgType(2);
        setTgHitBit(TRUE);
        setTgShield(FALSE);
        field_0x702 = 0;
    case 6:
        if (checkAvoidWeapon(FALSE)) {
            mSwordCc[0].OffAtSetBit();
            mSwordCc[1].OffAtSetBit();
            setTgHitBit(FALSE);

            field_0x71c++;
            if (field_0x71c < 10) {
                setLastWarp(2, 0);
            } else {
                field_0x703 = 7;
                setLastWarp(1, 0);
            }
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_float(this, &mSwordSize, l_HIO.mSwordAttackSize, 0.1f);
            actor_attr::enemy_add_action_angle(this, &current.angle.y, angle_to_player, 8, 0x400);
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, angle_to_player, 8, 0x400);
#else
            cLib_chaseF(&mSwordSize, l_HIO.mSwordAttackSize, 0.1f);
            cLib_addCalcAngleS2(&current.angle.y, angle_to_player, 8, 0x400);
            cLib_addCalcAngleS2(&shape_angle.y, angle_to_player, 8, 0x400);
#endif

            if (field_0x713 == 0) {
                if (mpModelMorf->checkFrame(4) || mpModelMorf->checkFrame(24)) {
                    mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_SW_SWING_L, 0, -1);
                } else if (mpModelMorf->checkFrame(7) || mpModelMorf->checkFrame(28)) {
                    mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_FOOT_L, 0, -1);
                } else if (mpModelMorf->checkFrame(5)) {
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_CTL_V_SW_ATK_A1, -1);
                } else if (mpModelMorf->checkFrame(22)) {
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_CTL_V_SW_ATK_A2, -1);
                }

                if (mpModelMorf->checkFrame(1)) {
                    mSwordCc[1].OnAtSetBit();
                }

                if (mpModelMorf->checkFrame(12)) {
                    mSwordCc[1].OffAtSetBit();
                }

                if (mpModelMorf->checkFrame(22)) {
                    mSwordCc[0].OnAtSetBit();
                }

                if (mpModelMorf->checkFrame(33)) {
                    mSwordCc[0].OffAtSetBit();
                }
            } else {
                if (mpModelMorf->checkFrame(2) || mpModelMorf->checkFrame(20)) {
                    mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_SW_SWING_S1, 0, -1);
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_CTL_V_SW_ATK_B1, -1);
                } else if (mpModelMorf->checkFrame(11) || mpModelMorf->checkFrame(28)) {
                    mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_SW_SWING_S2, 0, -1);
                    mSound.startCreatureVoice(Z2SE_EN_ZAN_CTL_V_SW_ATK_B2, -1);
                } else if (mpModelMorf->checkFrame(0) || mpModelMorf->checkFrame(9) || mpModelMorf->checkFrame(17) || mpModelMorf->checkFrame(26)) {
                    mSound.startCreatureSound(Z2SE_EN_ZAN_CTL_FOOT, 0, -1);
                }

                if (mpModelMorf->checkFrame(1) || mpModelMorf->checkFrame(19)) {
                    mSwordCc[1].OnAtSetBit();
                    mSwordCc[0].OffAtSetBit();
                }

                if (mpModelMorf->checkFrame(9) || mpModelMorf->checkFrame(28)) {
                    mSwordCc[0].OnAtSetBit();
                    mSwordCc[1].OffAtSetBit();
                }
            }

            if (mModeTimer == 0 && mpModelMorf->checkFrame(1)) {
                mSwordCc[0].OffAtSetBit();
                mSwordCc[1].OffAtSetBit();
                setTgHitBit(FALSE);
                setLastWarp(1, 0);
            }
        }
        break;
    case 20:
        if (fopAcM_searchPlayerDistance(this) < DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 500.0f), 500.0f)) {
            mMode = 5;
        } else {
            mMode = 2;
            setBck(BCK_ZAN_SW_WALK, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        }
        break;
    case 10:
    case 30:
        dComIfGs_onOneZoneSwitch(1, fopAcM_GetRoomNo(this));
        setBck(BCK_ZAN_SPIN, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);

        if (field_0x704 == 0) {
            if (field_0x713 != 2) {
                field_0x6ec = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 300), 300);
                field_0x71d = 1;
                field_0x6ff = 0;
            }
#if TARGET_PC  // enemy attribute integration
        } else if (field_0x6ec < actor_attr::enemy_sync_timer(this, 30)) {
            field_0x6ec = actor_attr::enemy_sync_timer(this, 30);
#else
        } else if (field_0x6ec < 30) {
            field_0x6ec = 30;
#endif
        }

        if (field_0x713 == 0) {
            mMode = 11;
        } else {
            mMode = 31;
        }

        current.angle.y = angle_to_player;

        mRollCc.OnAtSetBit();
        mRollCc.OnTgSetBit();
        mRollCc.OnCoSetBit();
        setTgType(0xD8FBFDFF);
        setTgShield(TRUE);
        setTgHitBit(TRUE);
        field_0x6f8 = 0x3000;
        speedF = 30.0f;
        field_0x6fd = 0;
        field_0x70b = 0;
    case 11:
    case 31: {
        mSound.startCreatureVoiceLevel(Z2SE_EN_ZAN_CTL_V_SPIN_ATK, -1);
        setLastRollEffect();

        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mSwordSize, l_HIO.mSwordAttackSize, 0.1f), cLib_chaseF(&mSwordSize, l_HIO.mSwordAttackSize, 0.1f));
        
        f32 var_f31 = 1.2f;
        if (field_0x713 != 0) {
            var_f31 = 1.5f;
        }

        if (field_0x70b != 0) {
            if (mAcch.ChkGroundHit()) {
                field_0x70b--;

                if (field_0x70b != 0) {
                    speedF = 0.0f;
                    speed.y = 30.0f;
                    field_0x6ec = 0;
                }
            }
        } else if (mMode == 11) {
            int sp48 = abs((s16)(angle_to_player - current.angle.y));
            if (sp48 > 0x4000) {
                if (sp48 > 0x6800) {
                    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, -30.0f, var_f31 * 0.2f), cLib_chaseF(&speedF, -30.0f, var_f31 * 0.2f));
                } else {
                    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 5.0f, var_f31 * 0.2f), cLib_chaseF(&speedF, 5.0f, var_f31 * 0.2f));
                }

                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle( this, &current.angle.y, angle_to_player + 0x8000, 0x10, var_f31 * 512.0f), cLib_addCalcAngleS2(&current.angle.y, angle_to_player + 0x8000, 0x10, var_f31 * 512.0f));
                if (speedF < 0.0f && sp48 > 0x6000) {
                    current.angle.y += 0x8000;
                    speedF = -speedF;
                }
            } else {
                if (sp48 > 0x1800) {
                    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 10.0f, var_f31), cLib_chaseF(&speedF, 10.0f, var_f31));
                } else {
                    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, var_f31 * 30.0f, var_f31), cLib_chaseF(&speedF, var_f31 * 30.0f, var_f31));
                }

                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle( this, &current.angle.y, angle_to_player, 0x10, var_f31 * 768.0f), cLib_addCalcAngleS2(&current.angle.y, angle_to_player, 0x10, var_f31 * 768.0f));
            }
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, var_f31 * 30.0f, var_f31 * 2.0f), cLib_chaseF(&speedF, var_f31 * 30.0f, var_f31 * 2.0f));
        }

        int sp3C = 0;
        int sp40 = 0;
        int sp44 = 0;
        cCcD_Obj* sp48 = NULL;

        if (mRollCc.ChkTgHit()) {
            sp48 = mRollCc.GetTgHitObj();
            if (sp48->ChkAtType(AT_TYPE_IRON_BALL)) {
                sp40 = 1;
            } else if (sp48->ChkAtType(AT_TYPE_NORMAL_SWORD)) {
                daPy_py_c* player = daPy_getPlayerActorClass();
                if (player->getCutType() == daPy_py_c::CUT_TYPE_TURN_RIGHT || player->getCutType() == daPy_py_c::CUT_TYPE_TURN_LEFT ||
                    player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT || player->getCutType() == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT)
                {
                    sp40 = 1;
                }
            }

            def_se_set(&mSound, mRollCc.GetTgHitObj(), 0x2A, this);
            mRollCc.ClrTgHit();
        } else if (mRollCc.ChkAtHit()) {
            sp48 = mRollCc.GetAtHitObj();
            if (!mRollCc.ChkAtShieldHit()) {
                sp44 = 1;
            }

            mRollCc.ClrAtHit();
        }

        if (sp48 != NULL && fopAcM_GetName(dCc_GetAc(sp48->GetAc())) == fpcNm_ALINK_e) {
            sp3C = 1;
            field_0x6fd++;

            if (sp40 != 0) {
                speedF = -speedF;
                field_0x70b = 2;
                speed.y = 40.0f;
            } else if (sp44 != 0) {
                field_0x6fd += 5;
                speedF = -45.0f;
            } else {
                speedF = -2.0f;
            }
        } else {
            if (mAcch.ChkWallHit()) {
                s16 sp8 = mAcchCir.GetWallAngleY();

                if (speedF > 0.0f) {
                    if ((s16)cLib_distanceAngleS(sp8, current.angle.y) > 0x5800) {
                        current.angle.y = sp8 - 0x8000;
                        speedF *= 0.8f;
                        sp3C = 1;
                        field_0x6fd += 5;
                    }
                } else {
                    if ((s16)cLib_distanceAngleS(sp8, current.angle.y) < 0x2800) {
                        current.angle.y = sp8 - (s16)(current.angle.y - sp8);
                        speedF *= 0.8f;
                        sp3C = 1;
                        field_0x6fd += 5;
                    }
                }
            }
        }

        if (sp3C != 0) {
            if (mMode == 31 && field_0x6fd >= 5) {
                mRollCc.OffAtSetBit();
                mRollCc.OffTgSetBit();
                mRollCc.OffCoSetBit();
                setTgHitBit(FALSE);
                setLastWarp(3, sp44);
                return;
            }

#if TARGET_PC  // enemy attribute integration
            if (field_0x6ec < actor_attr::enemy_sync_timer(this, 10)) {
                field_0x6ec = actor_attr::enemy_sync_timer(this, 10);
#else
            if (field_0x6ec < 10) {
                field_0x6ec = 10;
#endif
                mMode = 12;
            }
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_angle(this, &field_0x6f8, 0x3000, 0x80);
        actor_attr::enemy_angle_add(this, shape_angle.y, field_0x6f8);
#else
        cLib_chaseAngleS(&field_0x6f8, 0x3000, 0x80);
        shape_angle.y += field_0x6f8;
#endif

        if (field_0x6ec == 0) {
            mMode = 12;
        }
        break;
    }
    case 12:
        mSound.startCreatureVoiceLevel(Z2SE_EN_ZAN_CTL_V_SPIN_ATK, -1);
        setLastRollEffect();

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 1.0f);
        actor_attr::enemy_chase_action_angle(this, &field_0x6f8, 0, 0x80);
        actor_attr::enemy_angle_add(this, shape_angle.y, field_0x6f8);
#else
        cLib_chaseF(&speedF, 0.0f, 1.0f);
        cLib_chaseAngleS(&field_0x6f8, 0, 0x80);
        shape_angle.y += field_0x6f8;
#endif

        if (field_0x6f8 < 0x1000) {
            mRollCc.OffAtSetBit();
            mRollCc.OffTgSetBit();
            mRollCc.OffCoSetBit();

            setBck(BCK_ZAN_SW_FATIGUE, J3DFrameCtrl::EMode_LOOP, 30.0f, 1.0f);
            mSound.startCreatureVoice(Z2SE_EN_ZAN_CTL_V_ZEIZEI, -1);
            mMode = 13;
            dComIfGs_offOneZoneSwitch(1, fopAcM_GetRoomNo(this));

            if (field_0x713 == 2) {
                field_0x6ec = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 450), 450);
            }
        }
        break;
    case 13:
        if (field_0x6f8 < 0x800) {
            field_0x702 = 0;
            setTgShield(FALSE);
            setTgHitBit(TRUE);

            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 60), 60);
            field_0x71d = 0;
            field_0x703 = 0;
            field_0x71c = 0;
            field_0x712 = 0;
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &mSwordSize, 1.0f, 0.1f);
        actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 1.0f);
        actor_attr::enemy_chase_action_angle(this, &field_0x6f8, 0, 0x80);
        actor_attr::enemy_angle_add(this, shape_angle.y, field_0x6f8);
#else
        cLib_chaseF(&mSwordSize, 1.0f, 0.1f);
        cLib_chaseF(&speedF, 0.0f, 1.0f);
        cLib_chaseAngleS(&field_0x6f8, 0, 0x80);
        shape_angle.y += field_0x6f8;
#endif

        if (field_0x6f8 == 0) {
            setActionMode(ACT_LAST_TIRED, 0);
        }
        break;
    }
}

void daB_ZANT_c::executeLastTired() {
    switch (mMode) {
    case 0:
        if (field_0x713 == 0) {
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 120), 120);
        } else if (field_0x713 == 1) {
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 90), 90);
        } else {
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 60), 60);
        }

        field_0x6f0 = mModeTimer;
        mMode = 5;
        field_0x702 = 0;

        setTgShield(FALSE);
        setTgHitBit(TRUE);
        field_0x71d = 0;
        field_0x703 = 0;
        field_0x71c = 0;
        field_0x712 = 0;
    case 5:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 1.0f), cLib_chaseF(&speedF, 0.0f, 1.0f));
        if (mModeTimer == 0) {
            setTgHitBit(FALSE);
            setLastWarp(1, 0);
        }
    }
}

void daB_ZANT_c::executeLastDamage() {
    switch (mMode) {
    case 0:
    case 1:
    case 2:
        if (mLastAction == ACT_LAST_ATTACK) {
            field_0x6f0 = 0;
        }

        if (field_0x6f0 == 0) {
            setTgHitBit(FALSE);
        } else {
            setTgHitBit(TRUE);
        }

        mSwordCc[0].OffAtSetBit();
        mSwordCc[1].OffAtSetBit();

        speedF = 0.0f;
        attention_info.flags = fopAc_AttnFlag_BATTLE_e;

        if (mTakenBigDmg) {
            if (mFightCycle == 0) {
#if TARGET_PC  // enemy attribute integration
                if (health < actor_attr::enemy_health_value(this, 400)) {
                    health = actor_attr::enemy_health_value(this, 400);
#else
                if (health < 400) {
                    health = 400;
#endif
                }
#if TARGET_PC  // enemy attribute integration
            } else if (mFightCycle == 1 && health < actor_attr::enemy_health_value(this, 200)) {
                health = actor_attr::enemy_health_value(this, 200);
#else
            } else if (mFightCycle == 1 && health < 200) {
                health = 200;
#endif
            }

            mFightCycle++;
            setTgHitBit(FALSE);
        }

        if (health <= 0 || mFightCycle >= 3) {
            mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
            setActionMode(ACT_LAST_END_DEMO, 0);
            return;
        }

        field_0x702 = 0;

        if (mMode == 1) {
            setBck(BCK_ZAN_SW_DAMAGER, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        } else if (mMode == 2) {
            setBck(BCK_ZAN_SW_DAMAGEL, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        } else if (cM_rnd() < 0.5f) {
            setBck(BCK_ZAN_SW_DAMAGEL, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        } else {
            setBck(BCK_ZAN_SW_DAMAGER, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        }

        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG, -1);
        mMode = 5;
    case 5: {
        BOOL var_r28 = false;
        if (checkBck(BCK_ZAN_SW_DAMAGEL)) {
            if (mpModelMorf->checkFrame(17)) {
                mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG_JITANDA, -1);
            }

            if (mLastAction == ACT_LAST_ATTACK) {
                if (mpModelMorf->checkFrame(18)) {
                    var_r28 = true;
                }
            } else {
                if (mpModelMorf->checkFrame(38)) {
                    var_r28 = true;
                }
            }

            if (var_r28) {
                setTgHitBit(FALSE);
                field_0x71c++;
                field_0x712++;

                if (field_0x71c >= 5 || field_0x712 >= 3) {
                    field_0x703 = 7;
                    setLastWarp(1, 0);
                } else if (mLastAction == ACT_LAST_ATTACK) {
                    setLastWarp(2, 0);
                } else {
                    setLastWarp(1, 0);
                }
            }
        } else {
            if (mpModelMorf->checkFrame(19)) {
                mSound.startCreatureVoice(Z2SE_EN_ZAN_V_DMG_SHINARU, -1);
            }

            if (mLastAction == ACT_LAST_ATTACK) {
                if (mpModelMorf->checkFrame(20)) {
                    var_r28 = true;
                }
            } else {
                if (mpModelMorf->checkFrame(30)) {
                    var_r28 = true;
                }
            }

            if (var_r28) {
                setTgHitBit(FALSE);
                field_0x71c++;
                field_0x712++;

                if (field_0x71c >= 5 || field_0x712 >= 3) {
                    field_0x703 = 7;
                    setLastWarp(1, 0);
                } else if (mLastAction == ACT_LAST_ATTACK) {
                    setLastWarp(2, 0);
                } else {
                    setLastWarp(1, 0);
                }
            }
        }
        break;
    }
    case 10:
        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        setBck(BCK_ZAN_GROUND_REACTION, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mSound.startCreatureVoice(Z2SE_EN_ZAN_V_NO_DMG, -1);
        mMode = 11;
        speedF = 0.0f;
        break;
    case 11:
        if (mpModelMorf->isStop()) {
            setTgHitBit(FALSE);
            setLastWarp(1, 0);
        }
        break;
    case 20:
        setBck(BCK_ZAN_FAINT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        speedF = 0.0f;
        speed.y = 0.0f;
        field_0x702 = 0;
        mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(this, 20), 20);
        mMode = 21;
    case 21:
        if (mModeTimer == 0) {
            setTgHitBit(FALSE);
            setLastWarp(1, 0);
        }
        break;
    }
}

static void* s_del_tp(void* i_actor, void* i_data) {
    if (fopAcM_IsActor(i_actor)) {
        if (!fpcM_IsCreating(fopAcM_GetID(i_actor)) && fopAcM_GetName(i_actor) == fpcNm_OBJ_TP_e) {
            fopAcM_delete((fopAc_ac_c*)i_actor);
        }
    }

    return NULL;
}

void daB_ZANT_c::executeLastEndDemo() {
    dCamera_c* camera = dCam_getBody();
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz sp34;
    cXyz sp40;

    switch (mMode) {
    case 0:
        if (!eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(this, 2, 0xFFFF, 0);
            eventInfo.onCondition(2);
            return;
        }

        mDrawSwords = false;
        camera->Stop();
        camera->SetTrimSize(3);
        
        setBck(BCK_ZAN_DIE_DEMO, J3DFrameCtrl::EMode_NONE, 3.0f, 1.0f);
        mSound.startCreatureSound(Z2SE_EN_ZAN_END, 0, -1);
        Z2GetAudioMgr()->bgmStop(30, 0);
        fpcM_Search(s_del_tp, this);
        speedF = 0.0f;

        mDemoCamCenter.set(0.0f, 120.0f, -1000.0f);
        mDemoCamEye.set(0.0f, 50.0f, -750.0f);
        mDemoCamBank = 80.0f;
        
        mMode = 1;
        field_0x6ba = 0x2F00;
        field_0x6bc = 0.0f;
        dComIfGs_onStageBossEnemy();
        return;
    case 1:
    case 2:
        if (mMode == 1) {
            cXyz sp4C(0.0f, 0.0f, 0.0f);
            player->setPlayerPosAndAngle(&sp4C, 0x8000, 0);

            current.pos.set(0.0f, 0.0f, -1000.0f);
            shape_angle.z = 0;
            shape_angle.x = 0;
            shape_angle.y = 0;
            sp34 = mDemoCamCenter;
            mMode = 2;
        } else {
            sp34 = eyePos;
            sp34.y -= 44.0f;
        }

        {
            f32 var_f31 = mpModelMorf->getFrame();
            if ((80 <= var_f31 && var_f31 <= 120) || (175 <= var_f31 && var_f31 <= 205)) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, sp34, 0.1f, 3.0f, 1.0f), cLib_addCalcPos(&mDemoCamCenter, sp34, 0.1f, 3.0f, 1.0f));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, sp34, 0.5f, 30.0f, 1.0f), cLib_addCalcPos(&mDemoCamCenter, sp34, 0.5f, 30.0f, 1.0f));
            }
        }

        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &field_0x6ba, 0, 0x10, 0x40, 0x10), cLib_addCalcAngleS(&field_0x6ba, 0, 0x10, 0x40, 0x10));

        sp34.set(0.0f, 50.0f, -1000.0f);
        sp40.set(0.0f, field_0x6bc, 250.0f);
        cLib_offsetPos(&mDemoCamEye, &sp34, field_0x6ba, &sp40);

        if (mpModelMorf->checkFrame(230)) {
            dComIfGp_getVibration().StartShock(4, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (mpModelMorf->checkFrame(250)) {
            mMode = 3;
        }
        break;
    case 3:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(this, &field_0x6ba, 0, 0x10, 0x40, 0x10);
        actor_attr::enemy_add_action_calc2(this, &field_0x6bc, 250.0f, 0.1f, 3.0f);
#else
        cLib_addCalcAngleS(&field_0x6ba, 0, 0x10, 0x40, 0x10);
        cLib_addCalc2(&field_0x6bc, 250.0f, 0.1f, 3.0f);
#endif
        sp34.set(0.0f, 300.0f, -1000.0f);
        sp40.set(0.0f, 0.0f, 150.0f);
        
        {
            cXyz sp58;
            cLib_offsetPos(&sp58, &sp34, field_0x6ba, &sp40);
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &mDemoCamEye, sp58, 0.1f, 4.0f, 1.0f), cLib_addCalcPos(&mDemoCamEye, sp58, 0.1f, 4.0f, 1.0f));
        }
        
        sp34 = eyePos;
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, sp34, 0.1f, 0.8f, 1.0f), cLib_addCalcPos(&mDemoCamCenter, sp34, 0.1f, 0.8f, 1.0f));

        if (mpModelMorf->checkFrame(340)) {
            mpModelMorf->setFrame(300);
            mMode = 14;
            
            mDemoCamEye.set(0.0f, 5.0f, -800.0f);
            field_0x6bc = 30.0f;
            mDemoCamCenter = eyePos;
            mDemoCamCenter.y -= DUSK_IF_ELSE(actor_attr::enemy_action_step(this, field_0x6bc), field_0x6bc);
            mDemoCamBank = 80.0f;
        }
        break;
    case 14:
        mDemoCamCenter = eyePos;
        mDemoCamCenter.y -= DUSK_IF_ELSE(actor_attr::enemy_action_step(this, field_0x6bc), field_0x6bc);

        if (mpModelMorf->checkFrame(360)) {
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpModelMorf, 0.5f), mpModelMorf->setPlaySpeed(0.5f));
            mMode = 15;
        }
        break;
    case 15:
        if (mpModelMorf->checkFrame(375)) {
            dComIfGp_setNextStage("D_MN08A", 25, 10, 9);
        }

        mDemoCamCenter = eyePos;
        mDemoCamCenter.y -= DUSK_IF_ELSE(actor_attr::enemy_action_step(this, field_0x6bc), field_0x6bc);

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &field_0x6bc, 0.0f, 1.0f);
        actor_attr::enemy_chase_action_float(this, &mDemoCamBank, 80.0f, 0.5f);
#else
        cLib_chaseF(&field_0x6bc, 0.0f, 1.0f);
        cLib_chaseF(&mDemoCamBank, 80.0f, 0.5f);
#endif
        break;
    }

    camera->Set(mDemoCamCenter, mDemoCamEye, mDemoCamBank, 0);
}

void daB_ZANT_c::calcMahojinAnime() {
    switch (mMahojinAnmMode) {
    case 1:
        DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMahojinStartBtk, 1.0f), mpMahojinStartBtk->setPlaySpeed(1.0f));
        mpMahojinStartBtk->setFrame(0);
        DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMahojinEndBrk, 0.0f), mpMahojinEndBrk->setPlaySpeed(0.0f));
        mpMahojinEndBrk->setFrame(0);
        mMahojinAnmMode = 2;
    case 2:
        if (mpMahojinStartBtk->checkFrame(9)) {
            mMahojin2AnmMode = 1;
        }

        if (mpMahojinStartBtk->isStop()) {
            mMahojinAnmMode = 3;
        }
        break;
    case 3:
        break;
    case 4:
        DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMahojinEndBrk, 1.0f), mpMahojinEndBrk->setPlaySpeed(1.0f));
        mMahojinAnmMode = 5;
    case 5:
        if (mpMahojinEndBrk->isStop()) {
            mMahojinAnmMode = 0;
        }
        break;
    }

    switch (mMahojin2AnmMode) {
    case 0:
        DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMahojinBrk2, 0.0f), mpMahojinBrk2->setPlaySpeed(0.0f));
        mpMahojinBrk2->setFrame(30);
        mpMahojinStartBtk2->setFrame(19);
        mMahojin2Size = 1.0f;
        break;
    case 1:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_set_animation_play_speed(this, mpMahojinBrk2, 0.5f);
        actor_attr::enemy_chase_action_float(this, &mMahojin2Size, 10.0f, 0.1f);
#else
        mpMahojinBrk2->setPlaySpeed(0.5f);
        cLib_chaseF(&mMahojin2Size, 10.0f, 0.1f);
#endif

        if (mpMahojinBrk2->isStop()) {
            mMahojin2AnmMode = 0;
            mpMahojinBrk2->init(mpMahojinModel2->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("B_zan", 0x4F), TRUE, J3DFrameCtrl::EMode_NONE, 0.0f, 0, -1);
        }
    }
}

void daB_ZANT_c::calcRoomChangeCamera(int param_0) {
    cXyz eye_target(-100.0f, 87.0f, -160.0f);
    cXyz center_target(106.0f, 483.0f, 843.0f);

    if (param_0 == 1) {
        cXyz sp38(-20.0f, -40.0f, -100.0f);
        eye_target += sp38;
        center_target += sp38;
    }

    if (mFightPhase == PHASE_OP) {
        eye_target.x += 140.0f;
        center_target.x += 140.0f;

        mDoMtx_stack_c::YrotS(-0x8000);
        mDoMtx_stack_c::transM(eye_target);
        mDoMtx_stack_c::multVecZero(&eye_target);

        eye_target.y += 250.0f;
        eye_target.z -= 800.0f;

        mDoMtx_stack_c::YrotS(-0x8000);
        mDoMtx_stack_c::transM(center_target);
        mDoMtx_stack_c::multVecZero(&center_target);

        center_target.y += 250.0f;
        center_target.z -= 800.0f;
    }

    switch (param_0) {
    case 0:
        mDemoCamBank = 58.0f;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &field_0x77c, 30.0f, 1.0f);
        actor_attr::enemy_add_action_calc_pos(this, &mDemoCamEye, eye_target, 0.3f, field_0x77c * 1.1f, 3.0f);
        actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, center_target, 0.3f, field_0x77c, 3.0f);
#else
        cLib_chaseF(&field_0x77c, 30.0f, 1.0f);
        cLib_addCalcPos(&mDemoCamEye, eye_target, 0.3f, field_0x77c * 1.1f, 3.0f);
        cLib_addCalcPos(&mDemoCamCenter, center_target, 0.3f, field_0x77c, 3.0f);
#endif
        break;
    case 1:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc_pos(this, &mDemoCamEye, eye_target, 0.2f, 15.0f, 3.0f);
        actor_attr::enemy_add_action_calc_pos(this, &mDemoCamCenter, center_target, 0.2f, 15.0f, 3.0f);
#else
        cLib_addCalcPos(&mDemoCamEye, eye_target, 0.2f, 15.0f, 3.0f);
        cLib_addCalcPos(&mDemoCamCenter, center_target, 0.2f, 15.0f, 3.0f);
#endif
        break;
    }
}

void daB_ZANT_c::initNextRoom() {
    field_0x70f = 0;
    field_0x6f0 = 0;
    field_0x70e = 0;
    mDrawSwords = false;

    setTgType(0xD8FBFDFF);
    mBodySphCc[0].OffTgNoHitMark();
    mBodySphCc[1].OffTgNoHitMark();

    dComIfGs_offSaveDunSwitch(20);
    dComIfGs_offSaveDunSwitch(21);
    dComIfGs_offSaveDunSwitch(22);

    if (mFightPhase != PHASE_LAST) {
        health = DUSK_IF_ELSE(actor_attr::enemy_health_value(this, 280), 280);
    } else {
        /* field_0x560 already contains the randomized maximum health. */
        health = field_0x560;
    }

#if TARGET_PC  // enemy attribute integration
    mAcchCir.SetWall(actor_attr::enemy_size_value(this, 100.0f), actor_attr::enemy_size_value(this, 100.f));
#else
    mAcchCir.SetWall(100.0f, 100.f);
#endif

    if (mFightPhase == PHASE_MG) {
        attention_info.distances[fopAc_attn_BATTLE_e] = 4;
    } else {
        attention_info.distances[fopAc_attn_BATTLE_e] = 24;
    }

    fopAc_ac_c* pmobile;
    fopAcM_SearchByID(mMobileIDs[0], &pmobile);

    if (mFightPhase == PHASE_OI) {
        fopAcM_OnStatus(this, fopAcStts_UNK_0x80000_e);

        if (pmobile == NULL) {
            cXyz pos(0.0f, -3300.0f, 0.0f);
            for (int i = 0; i < 4; i++) {
                mMobileIDs[i] = fopAcM_create(fpcNm_B_ZANTZ_e, i | 0xFFFFFF00, &pos, warp_next_room[mFightPhase], &shape_angle, NULL, -1);
            }

            mCorrectMobileNo = 0;
        }
    } else {
        fopAcM_OffStatus(this, fopAcStts_UNK_0x80000_e);

        if (pmobile != NULL) {
            fopAcM_delete(pmobile);
            mMobileIDs[0] = fpcM_ERROR_PROCESS_ID_e;
        }
    }

    tevStr.room_no = warp_next_room[mFightPhase];
}

void daB_ZANT_c::executeRoomChange() {
    dCamera_c* camera = dCam_getBody();
    cXyz sp34(0.0f, 0.0f, 0.0f);
    daPy_py_c* player = daPy_getPlayerActorClass();

    switch (mMode) {
    case 0:
        setTgHitBit(FALSE);
        if (!eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(this, 2, 0xFFFF, 0);
            eventInfo.onCondition(2);
            return;
        }

        camera->Stop();
        camera->SetTrimSize(3);
        
        mDemoCamCenter = dCam_getBody()->Center();
        mDemoCamEye = dCam_getBody()->Eye();
        mDemoCamBank = dCam_getBody()->Fovy();

        field_0x705 = 0;
        mMode = 1;

        if (mFightPhase != PHASE_MK) {
            setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);
        }

        dComIfGp_particle_set(0x88FE, &current.pos, &shape_angle, NULL);
        mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_OUT, 0, -1);
        break;
    case 1:
        if (calcScale(0)) {
            mMode = 4;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mDemoWarpTime), l_HIO.mDemoWarpTime);
        }
        break;
    case 4:
        if (mModeTimer == 0) {
            mMode = 10;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);

            current.pos.set(-140.0f, 300.0f, 700.0f);
            old.pos = current.pos;
            speedF = 0.0f;
            speed.y = 0.0f;
            gravity = 0.0f;

            shape_angle.x = 0;
            current.angle.y = 0x8000;
            shape_angle.y = 0x8000;

            setBck(BCK_ZAN_FLOAT_WAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);

            player->setPlayerPosAndAngle(&sp34, 0, 0);
            player->changeOriginalDemo();
            player->changeDemoPos0(&sp34);
            player->changeDemoMode(0x17, 1, 4, 0);

            mDemoCamBank = 65.0f;
            mDemoCamEye.set(-225.0f, 344.0f, 382.0f);
            mDemoCamCenter.set(118.0f, 672.0f, 1374.0f);

            Z2GetAudioMgr()->changeBgmStatus(13);
            field_0x77c = 0.0f;

            calcRoomChangeCamera(0);

            if (mFightPhase == PHASE_OI) {
                for (int i = 0; i < 4; i++) {
                    fopAc_ac_c* pmobile;
                    fopAcM_SearchByID(mMobileIDs[i], &pmobile);

                    if (pmobile != NULL) {
                        fopAcM_delete(pmobile);
                        mMobileIDs[i] = fpcM_ERROR_PROCESS_ID_e;
                    }
                }
            }
            field_0x6fc = 1;
        }
        break;
    case 10:
        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        if (mModeTimer == 0) {
            dComIfGp_particle_set(0x88FE, &current.pos, &shape_angle, NULL);
            mSound.startCreatureSound(Z2SE_EN_ZAN_WARP_IN, 0, -1);
            mMode = 11;
        }
        break;
    case 11:
        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        if (calcScale(1)) {
            mMode = 12;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
        }
        break;
    case 12:
        if (mModeTimer == 0) {
            mMode = 13;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 50), 50);
        }
        break;
    case 13:
        calcRoomChangeCamera(0);

        if (mModeTimer == 0) {
            mMode = 14;
            setBck(BCK_ZAN_FLOAT_APPEARSHORT, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f);
            field_0x714 = 1;
            mKankyoBlend = 0.0f;

            static u32 mahojin_se[] = {
                Z2SE_EN_ZAN_MAHOJIN_BB,
                Z2SE_EN_ZAN_MAHOJIN_MG,
                Z2SE_EN_ZAN_MAHOJIN_OI,
                Z2SE_EN_ZAN_MAHOJIN_MK,
                Z2SE_EN_ZAN_MAHOJIN_YO,
                Z2SE_EN_ZAN_MAHOJIN_CTL,
            };

            mSound.startCreatureSound(mahojin_se[mFightPhase], 0, -1);
        }
        break;
    case 14:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mKankyoBlend, 1.0f, 0.02f), cLib_chaseF(&mKankyoBlend, 1.0f, 0.02f));

        if (mpModelMorf->checkFrame(42)) {
            mMahojinAnmMode = 1;
            Z2GetAudioMgr()->changeBgmStatus(mFightPhase + PHASE_MAX);
        }

        if (mpModelMorf->checkFrame(42)) {
            field_0x715 = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(this, 30), 30);
        }

        shape_angle.y = fopAcM_searchPlayerAngleY(this);
        shape_angle.x = -fopAcM_searchPlayerAngleX(this) * 0.5f;

        if (mpModelMorf->isStop()) {
            dComIfGp_getVibration().StopQuake(0x1F);
            setBck(BCK_ZAN_FLOAT_APPEARWAIT, J3DFrameCtrl::EMode_LOOP, 3.0f, 1.0f);

            mFightPhase++;
            if (mFightPhase >= PHASE_MAX) {
                mFightPhase = PHASE_OP;
            }

            mMode = 15;
            mModeTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mMahojinWaitTime), l_HIO.mMahojinWaitTime);

            initNextRoom();
            cXyz pos(0.0f, 0.0f, 0.0f);
            dComIfGs_setRestartRoom(pos, 0, warp_next_room[mFightPhase]);
        }
        break;
    case 15:
        if (mModeTimer == 0 && dComIfGp_roomControl_checkStatusFlag(warp_next_room[mFightPhase], 0x10)) {
            mMode = 16;
            field_0x714 = 0;
            mKankyoBlend = 0.0f;

            setBck(BCK_ZAN_FLOAT_WAITRETURN, J3DFrameCtrl::EMode_NONE, 5.0f, 1.0f);
            mMahojinAnmMode = 4;
        }
        break;
    case 16:
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &mKankyoBlend, 1.0f, 0.02f), cLib_chaseF(&mKankyoBlend, 1.0f, 0.02f));
        player->setPlayerPosAndAngle(&sp34, 0, 0);

        if (mpModelMorf->isStop()) {
            if (mFightPhase != PHASE_LAST) {
                camera->Reset(mDemoCamCenter, mDemoCamEye);
                camera->Start();
                camera->SetTrimSize(0);
                dComIfGp_event_reset();
            }

            field_0x6fc = 0;
            setTgHitBit(TRUE);
            setBaseActionMode(1);
            return;
        }
        break;
    }

    if (field_0x715 != 0) {
        calcRoomChangeCamera(1);
        field_0x715--;
    }

    camera->Set(mDemoCamCenter, mDemoCamEye, mDemoCamBank, 0);
}

void daB_ZANT_c::setBaseActionMode(int param_0) {
    switch (mFightPhase) {
    case PHASE_BB:
        if (param_0 == 0) {
            if (field_0x70c == 0) {
                setActionMode(ACT_FLY, 0);
            } else {
                setActionMode(ACT_FLY_GROUND, 0);
            }
        } else if (param_0 == 1) {
            field_0x70b = 1;
            mFlyWarpPosID = 0;
            mFlyWarpPos = fly_warp_pos[mFlyWarpPosID];
            field_0x711 = 1;
            setActionMode(ACT_WARP, 1);
        } else {
            field_0x70c = 0;
            gravity = 0.0f;
            mFlyWarpPosID = (f32)mFlyWarpPosID + cM_rndF(1.9f) + 1.0f;
            mFlyWarpPos = fly_warp_pos[mFlyWarpPosID % 3];
            field_0x711 = 2;
            setActionMode(ACT_WARP, 1);
        }
        break;
    case PHASE_MG:
        if (param_0 == 0) {
            setActionMode(ACT_SIMA_JUMP, 0);
        } else if (param_0 == 1) {
            gravity = -5.0f;
            mFlyWarpPosID = 0;
            mFlyWarpPos.set(0.0f, 1000.0f, 1100.0f);
            field_0x6b8 = 0x8000;
            field_0x711 = 3;
            setActionMode(ACT_WARP, 0);
        } else {
            setActionMode(ACT_SIMA_JUMP, 11);
        }
        break;
    case PHASE_YO:
        if (param_0 == 0) {
            setActionMode(ACT_ICE_DEMO, 100);
        } else if (param_0 == 1) {
            setActionMode(ACT_ICE_DEMO, 0);
        } else {
            setActionMode(ACT_ICE_DAMAGE, 30);
        }

        mModeTimer = 0;
        break;
    case PHASE_OI:
        if (param_0 == 0) {
            setActionMode(ACT_WATER, 0);
        } else {
            fopAc_ac_c* pmobile;
            fopAcM_SearchByID(mMobileIDs[mCorrectMobileNo], &pmobile);

            s16 var_r27 = shape_angle.y;
            cXyz sp44(current.pos);
            sp44.y = -4000.0f;

            if (pmobile != NULL) {
                var_r27 = pmobile->shape_angle.y;
#if TARGET_PC  // enemy attribute integration
                const f32 mouth_depth = actor_attr::enemy_size_value(this, 300.0f);
                sp44.set(cM_ssin(var_r27) * mouth_depth, mouth_depth, cM_scos(var_r27) * mouth_depth);
#else
                sp44.set(cM_ssin(var_r27) * 300.0f, 300.0f, cM_scos(var_r27) * 300.0f);
#endif
                sp44 += pmobile->current.pos;
            }

            attention_info.flags = 0;
            field_0x6b8 = var_r27;
            mFlyWarpPos = sp44;
            field_0x705 = 1;
            field_0x711 = 1;
            setActionMode(ACT_WARP, 0);
        }
        break;
    case PHASE_MK:
        if (param_0 == 0) {
            setActionMode(ACT_MONKEY, 0);
        } else if (param_0 == 1) {
            setActionMode(ACT_MONKEY, 100);
        } else {
            setNearPillarPos();
            field_0x711 = 0;
            setActionMode(ACT_WARP, 1);
        }
        break;
    case PHASE_LAST:
        if (param_0 == 0) {
            setActionMode(ACT_LAST_ATTACK, field_0x70f);
        } else if (param_0 == 1) {
            if (field_0x718 == 0) {
                setActionMode(ACT_LAST_START_DEMO, 0);
            } else {
                setLastWarp(0, 0);
            }
        }
        break;
    case PHASE_OP:
        setActionMode(ACT_OPENING, 0);
        executeOpening();
        break;
    }
}

void daB_ZANT_c::action() {
#if TARGET_PC  // enemy attribute integration

    // Remember whether this frame began in the forest pillar-jump flight state.
    // executeMonkey() can transition mode 9 -> 1 before the generic movement
    // pass below runs, so checking mMode only after the action switch misses
    // the exact landing frame.
    const bool wasMonkeyPillarJump = mFightPhase == PHASE_MK && mAction == ACT_MONKEY && mMode == 9;

#endif
    if (mFightPhase == PHASE_YO) {
        ice_damage_check();
    } else {
        damage_check();
    }

    switch (mAction) {
    case ACT_SMALL_ATTACK:
        executeSmallAttack();
        break;
    case ACT_WARP:
        executeWarp();
        break;
    case ACT_DAMAGE:
        executeDamage();
        break;
    case ACT_CONFUSE:
        executeConfuse();
        break;
    case ACT_OPENING:
        executeOpening();
        break;
    case ACT_FLY:
        executeFly();
        break;
    case ACT_FLY_GROUND:
        executeFlyGround();
        break;
    case ACT_WATER:
        executeWater();
        break;
    case ACT_HOOK:
        executeHook();
        break;
    case ACT_SWIM:
        executeSwim();
        break;
    case ACT_SIMA_JUMP:
        executeSimaJump();
        break;
    case ACT_ICE_DEMO:
        executeIceDemo();
        break;
    case ACT_ICE_JUMP:
        executeIceJump();
        break;
    case ACT_ICE_STEP:
        executeIceStep();
        break;
    case ACT_ICE_DAMAGE:
        executeIceDamage();
        break;
    case ACT_MONKEY:
        executeMonkey();
        break;
    case ACT_MONKEY_FALL:
        executeMonkeyFall();
        break;
    case ACT_MONKEY_DAMAGE:
        executeMonkeyDamage();
        break;
    case ACT_LAST_START_DEMO:
        executeLastStartDemo();
        break;
    case ACT_LAST_ATTACK:
        executeLastAttack();
        break;
    case ACT_LAST_TIRED:
        executeLastTired();
        break;
    case ACT_LAST_DAMAGE:
        executeLastDamage();
        break;
    case ACT_LAST_END_DEMO:
        executeLastEndDemo();
        break;
    case ACT_ROOM_CHANGE:
        executeRoomChange();
        break;
    }

    if (mAction != ACT_OPENING) {
        daPy_getPlayerActorClass()->onBossRoomWait();
    }

    cXyz sp40(0.0f, 0.0f, 0.0f);

    switch (mFightPhase) {
    case PHASE_BB:
        if (field_0x70c == 0) {
            dComIfGs_onOneZoneSwitch(0, fopAcM_GetRoomNo(this));
        } else {
            dComIfGs_offOneZoneSwitch(0, fopAcM_GetRoomNo(this));
        }
        break;
    case PHASE_MG:
        break;
    case PHASE_YO:
        if (checkBck(BCK_ZAN_FLOAT_WAIT) && mpModelMorf->checkFrame(1)) {
            mSound.startCreatureSound(Z2SE_EN_ZAN_YO_FLOAT_WAIT, 0, -1);
        }

        {
            f32 wall_r = mModelScaleXZ * 50.0f;
            if (wall_r < 100.0f) {
                wall_r = 100.0f;
            }

#if TARGET_PC  // enemy attribute integration
            mAcchCir.SetWall(actor_attr::enemy_size_value(this, 100.0f), actor_attr::enemy_size_value(this, wall_r));
#else
            mAcchCir.SetWall(100.0f, wall_r);
#endif
        }
        dComIfGs_offSaveDunSwitch(20);
        dComIfGs_offSaveDunSwitch(21);
        dComIfGs_offSaveDunSwitch(22);

        switch (field_0x70e) {
        case 1:
            dComIfGs_onSaveDunSwitch(20);
            break;
        case 2:
            dComIfGs_onSaveDunSwitch(21);
            break;
        case 3:
            dComIfGs_onSaveDunSwitch(22);
            break;
        }

        field_0x38b4 = dComIfGp_particle_set(field_0x38b4, 0x87B0, &sp40, &tevStr);
        break;
    case PHASE_OI:
        setWaterBubble();
        checkSwimLinkNear();
        dComIfGs_onOneZoneSwitch(0, fopAcM_GetRoomNo(this));
        break;
    case PHASE_MK:
        checkPillarSwing();
        break;
    case PHASE_LAST:
        break;
    }

    s16 var_r29 = fopAcM_searchPlayerAngleX(this);
    s16 neck_rot_z = 0;
    s16 body_rot_x = 0;

    if (field_0x717 != 0) {
        if (var_r29 > 0x2800) {
            var_r29 = 0x2800;
        }

        if (var_r29 < -0x2800) {
            var_r29 = -0x2800;
        }

        if (abs(var_r29) < 0x1800) {
            neck_rot_z = 0;
            body_rot_x = var_r29;
        } else if (var_r29 > 0) {
            body_rot_x = 0x1800;
            neck_rot_z = (s16)(var_r29 - 0x1800);
        } else {
            body_rot_x = -0x1800;
            neck_rot_z = (s16)(var_r29 + 0x1800);
        }
    }

    var_r29 = fopAcM_searchPlayerAngleY(this);
    s16 neck_rot_x = 0;

    if (field_0x716 != 0) {
        if (var_r29 > 0x2000) {
            var_r29 = 0x2000;
        }

        if (var_r29 < -0x2000) {
            var_r29 = -0x2000;
        }

        neck_rot_x = var_r29;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(this, &mNeckRotZ, neck_rot_z, 8, 0x400, 0x80);
    actor_attr::enemy_add_action_angle(this, &mNeckRotX, neck_rot_x, 8, 0x400, 0x80);
    actor_attr::enemy_add_action_angle(this, &mBackboneRotZ, body_rot_x, 8, 0x400, 0x80);
#else
    cLib_addCalcAngleS(&mNeckRotZ, neck_rot_z, 8, 0x400, 0x80);
    cLib_addCalcAngleS(&mNeckRotX, neck_rot_x, 8, 0x400, 0x80);
    cLib_addCalcAngleS(&mBackboneRotZ, body_rot_x, 8, 0x400, 0x80);
#endif

    calcMahojinAnime();

    dStage_roomControl_c::onNoChangeRoom();
    dStage_roomControl_c::setRoomReadId(warp_next_room[mFightPhase]);

    if (field_0x6fc != 0) {
        dStage_RoomCheck(NULL);
    }

#if TARGET_PC  // enemy attribute integration
    if (mFightPhase == PHASE_LAST) {
        // Final-phase Zant is a ground fighter. Keep vertical physics in vanilla
        // frame units so high action speed does not repeatedly drive him into
        // the floor before background correction, but scale his horizontal
        // locomotion directly from the movement-speed attribute.
        actor_attr::pos_move_resolved_f(this, speedF * actor_attr::enemy_action_speed_multiplier(this), gravity, maxFallSpeed, field_0x9a4.GetCCMoveP());
    } else {
        actor_attr::enemy_action_pos_move_f(this, field_0x9a4.GetCCMoveP());
    }
#else
    fopAcM_posMoveF(this, field_0x9a4.GetCCMoveP());
#endif

    if (field_0x705 == 0) {
        mAcch.CrrPos(dComIfG_Bgsp());
    }

#if TARGET_PC  // enemy attribute integration
    // Forest-phase pillar jumps move X/Z manually in executeMonkey(). At high
    // action speeds, collision/background correction can leave the final frame
    // slightly short even though the scaled travel timer has expired. Once the
    // travel window is complete, keep the selected pillar's X/Z authoritative.
    if (mFightPhase == PHASE_MK && mAction == ACT_MONKEY && mMode == 9 && mModeTimer == 0) {
        current.pos.x = field_0x6ac.x;
        current.pos.z = field_0x6ac.z;
    }

    // More importantly, executeMonkey() can accept the landing and change
    // mode 9 -> 1 before enemy_action_pos_move_f()/CrrPos run.  Reassert the
    // complete pillar landing after those generic movement passes so the same
    // frame cannot pull Zant short/off the pillar again.
    if (wasMonkeyPillarJump && mFightPhase == PHASE_MK && mAction == ACT_MONKEY && mMode == 1) {
        current.pos = field_0x6ac;
        old.pos = current.pos;
        speed.y = 0.0f;
        speedF = 0.0f;
    }

#endif
    u32 sp44 = mModelScaleY * 100.0f;
    mpModelMorf->play(sp44, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

void daB_ZANT_c::mtx_set() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
#if TARGET_PC  // enemy attribute integration
    mDoMtx_stack_c::scaleM(
        (l_HIO.mModelSize * mModelScaleXZ) * actor_attr::enemy_size_multiplier(this),
        (l_HIO.mModelSize * mModelScaleY) * actor_attr::enemy_size_multiplier(this),
        (l_HIO.mModelSize * mModelScaleXZ) * actor_attr::enemy_size_multiplier(this));

#else
    mDoMtx_stack_c::scaleM(l_HIO.mModelSize * mModelScaleXZ, l_HIO.mModelSize * mModelScaleY, l_HIO.mModelSize * mModelScaleXZ);
    
#endif
    J3DModel* model = mpModelMorf->getModel();
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    mpModelMorf->modelCalc();

    if (mDrawSwords) {
        mDoMtx_stack_c::copy(model->getAnmMtx(JNT_ARML3));
        mDoMtx_stack_c::scaleM(mSwordSize, mSwordSize, mSwordSize);
        mpSwordLModel->setBaseTRMtx(mDoMtx_stack_c::get());

        mDoMtx_stack_c::copy(model->getAnmMtx(JNT_ARMR3));
        mDoMtx_stack_c::scaleM(mSwordSize, mSwordSize, mSwordSize);
        mpSwordRModel->setBaseTRMtx(mDoMtx_stack_c::get());
    }

    if (mMahojinAnmMode != 0) {
        cXyz offset(l_HIO.mMahojinOffsetX, l_HIO.mMahojinOffsetY, l_HIO.mMahojinOffsetZ);
        cXyz pos;
        cLib_offsetPos(&pos, &current.pos, shape_angle.y, &offset);

        mDoMtx_stack_c::transS(pos);
        mDoMtx_stack_c::ZXYrotM(shape_angle);
        mDoMtx_stack_c::scaleM(l_HIO.mMahojinSize, l_HIO.mMahojinSize, l_HIO.mMahojinSize);
        mpMahojinModel->setBaseTRMtx(mDoMtx_stack_c::get());

        mpMahojinEndBrk->play();
        mpMahojinBtk->play();
        mpMahojinStartBtk->play();

        if (mMahojin2AnmMode != 0) {
            mDoMtx_stack_c::copy(mpMahojinModel->getBaseTRMtx());
            mDoMtx_stack_c::scaleM(mMahojin2Size, mMahojin2Size, mMahojin2Size);
            mpMahojinModel2->setBaseTRMtx(mDoMtx_stack_c::get());
            mpMahojinBrk2->play();
            mpMahojinStartBtk2->play();
        }
    }
}

void daB_ZANT_c::cc_set() {
    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_NECK));
    mDoMtx_stack_c::multVecZero(&eyePos);
    attention_info.position = eyePos;
    attention_info.position.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(this, mModelScaleY * 100.0f), mModelScaleY * 100.0f);

    cXyz center;
    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_BACKBONE));
    mDoMtx_stack_c::transM(30.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&center);
    mBodySphCc[0].SetC(center);
    mBodySphCc[0].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 70.0f), 70.0f));
    dComIfG_Ccsp()->Set(&mBodySphCc[0]);

    mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_WAIST));
    mDoMtx_stack_c::transM(50.0f, 0.0f, 0.0f);
    mDoMtx_stack_c::multVecZero(&center);
    mBodySphCc[1].SetC(center);
    mBodySphCc[1].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 70.0f), 70.0f));
    dComIfG_Ccsp()->Set(&mBodySphCc[1]);

    if (mFightPhase == PHASE_LAST) {
        mRollCc.SetC(current.pos);
#if TARGET_PC  // enemy attribute integration
        mRollCc.SetR(actor_attr::enemy_size_value(this, 200.0f));
        mRollCc.SetH(actor_attr::enemy_size_value(this, 250.0f));
#else
        mRollCc.SetR(200.0f);
        mRollCc.SetH(250.0f);
#endif
        dComIfG_Ccsp()->Set(&mRollCc);

        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_ARML3));
        mDoMtx_stack_c::transM(100.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::multVecZero(&center);
        mSwordCc[0].SetC(center);
        mSwordCc[0].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 50.0f), 50.0f));
        dComIfG_Ccsp()->Set(&mSwordCc[0]);

        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_ARMR3));
        mDoMtx_stack_c::transM(-100.0f, 0.0f, 0.0f);
        mDoMtx_stack_c::multVecZero(&center);
        mSwordCc[1].SetC(center);
        mSwordCc[1].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 50.0f), 50.0f));
        dComIfG_Ccsp()->Set(&mSwordCc[1]);
    }
}

void daB_ZANT_c::cc_ice_set() {
    class dZantSph_c {
    public:
        int joint_no;
        f32 offset_x;
        f32 radius;
    };

    static dZantSph_c ice_sph_jnt[] = {
        {JNT_FOOTL, 10.0f, 15.0f},
        {JNT_LEGL2, 20.0f, 20.0f},
        {JNT_LEGL2, 0.0f, 25.0f},
        {JNT_LEGL1, 20.0f, 30.0f},
        {JNT_FOOTR, 10.0f, 15.0f},
        {JNT_LEGR2, 20.0f, 20.0f},
        {JNT_LEGR2, 0.0f, 25.0f},
        {JNT_LEGR1, 20.0f, 30.0f},
        {JNT_WAIST, 0.0f, 45.0f},
        {JNT_BACKBONE, 20.0f, 45.0f},
        {JNT_HEAD, 20.0f, 25.0f},
    };

    cXyz sp34;
    cXyz sp40;

    if (mAction == ACT_ICE_STEP || mAction == ACT_ICE_JUMP) {
        if (!dComIfGp_getAttention()->LockonTruth() || dComIfGp_getAttention()->LockonTarget(0) != this) {
            mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTL));
            mDoMtx_stack_c::transM(20.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&sp34);

            mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTR));
            mDoMtx_stack_c::transM(20.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&sp40);

            cXyz player_pos(daPy_getPlayerActorClass()->current.pos);
            s16 player_angle = daPy_getPlayerActorClass()->shape_angle.y;

            if (sp34.absXZ(player_pos) < 700.0f) {
                field_0x70d = 0;
                if (sp40.absXZ(player_pos) < 700.0f && abs((s16)(player_angle - cLib_targetAngleY(&player_pos, &sp40))) < abs((s16)(player_angle - cLib_targetAngleY(&player_pos, &sp34)))) {
                    field_0x70d = 1;
                }
            } else if (sp40.absXZ(player_pos) < 700.0f) {
                field_0x70d = 1;
            } else if (sp34.absXZ(player_pos) < sp40.absXZ(player_pos)) {
                field_0x70d = 0;
            } else {
                field_0x70d = 1;
            }

            if (field_0x70d == 0) {
                eyePos = sp34;
            } else {
                eyePos = sp40;
            }
        } else {
            if (field_0x70d == 0) {
                mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTL));
            } else {
                mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_FOOTR));
            }

            mDoMtx_stack_c::transM(20.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVecZero(&eyePos);
        }

        attention_info.position = eyePos;
        attention_info.position.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 120.0f), 120.0f);
    } else {
        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(JNT_NECK));
        mDoMtx_stack_c::multVecZero(&eyePos);
        attention_info.position = eyePos;
        attention_info.position.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(this, mModelScaleY * 120.0f), mModelScaleY * 120.0f);
    }

    cXyz sp64;
    dZantSph_c sph;

    for (int i = 0; i < 6; i++) {
        sph = ice_sph_jnt[i];

        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(sph.joint_no));
        mDoMtx_stack_c::transM(sph.offset_x, 0.0f, 0.0f);
        mDoMtx_stack_c::multVecZero(&sp64);

        mFootCc[i].SetC(sp64);
        mFootCc[i].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, sph.radius * mModelScaleY), sph.radius * mModelScaleY));
        dComIfG_Ccsp()->Set(&mFootCc[i]);
    }

    for (int i = 0; i < 11; i++) {
        sph = ice_sph_jnt[i];

        mDoMtx_stack_c::copy(mpModelMorf->getModel()->getAnmMtx(sph.joint_no));
        mDoMtx_stack_c::transM(sph.offset_x, 0.0f, 0.0f);
        mDoMtx_stack_c::multVecZero(&sp64);

        mFoot2Cc[i].SetC(sp64);
        mFoot2Cc[i].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, sph.radius * mModelScaleY), sph.radius * mModelScaleY));
        dComIfG_Ccsp()->Set(&mFoot2Cc[i]);

        mCameraCc[i].SetC(sp64);
        mCameraCc[i].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, (sph.radius + 10.0f) * mModelScaleY), (sph.radius + 10.0f) * mModelScaleY));
        dComIfG_Ccsp()->Set(&mCameraCc[i]);
    }
}

int daB_ZANT_c::execute() {
    if (mModeTimer != 0) {
        mModeTimer--;
    }

    if (field_0x6ec != 0) {
        field_0x6ec--;
    }

    if (field_0x6e4 != 0) {
        field_0x6e4--;
    }

    if (field_0x6f0 != 0) {
        field_0x6f0--;
    }

    if (field_0x6f4 != 0) {
        field_0x6f4--;
    }

    switch (field_0x714) {
    case 0:
        dKy_custom_colset(10, 0, mKankyoBlend);
        break;
    case 1:
        dKy_custom_colset(0, 10, mKankyoBlend);
        break;
    }

    action();
    mtx_set();

    if (mFightPhase == PHASE_YO) {
        cc_ice_set();
    } else {
        cc_set();
    }

    return 1;
}

static int daB_ZANT_Execute(daB_ZANT_c* i_this) {
    return i_this->execute();
}

static int daB_ZANT_IsDelete(daB_ZANT_c* i_this) {
    return 1;
}

int daB_ZANT_c::_delete() {
    dComIfG_resDelete(&mPhase, "B_zan");
    
    if (mInitHIO) {
        hio_set = false;
    }

    if (heap != NULL) {
        mSound.deleteObject();
    }

    return 1;
}

static int daB_ZANT_Delete(daB_ZANT_c* i_this) {
    return i_this->_delete();
}

int daB_ZANT_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("B_zan", BMDR_ZAN);
    JUT_ASSERT(0, modelData != NULL);
    mpModelMorf = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", BCK_ZAN_FLOAT_WAIT), 2, DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 1.0f), 1.0f), 0, -1, &mSound, 0, 0x11000084);
    if (mpModelMorf == NULL || mpModelMorf->getModel() == NULL) {
        return 0;
    }

    J3DModel* model = mpModelMorf->getModel();
    model->setUserArea((uintptr_t)this);
    model->getModelData()->getJointNodePointer(1)->setCallBack(JointCallBack);
    model->getModelData()->getJointNodePointer(2)->setCallBack(JointCallBack);

    mpMahojinModel = mDoExt_J3DModel__create((J3DModelData*)dComIfG_getObjectRes("B_zan", BMDR_ZAN_MAHOJIN), 0x80000, 0x11000284);
    if (mpMahojinModel == NULL) {
        return 0;
    }

    mpMahojinEndBrk = JKR_NEW mDoExt_brkAnm();
    if (mpMahojinEndBrk == NULL) {
        return 0;
    }

    if (!mpMahojinEndBrk->init(mpMahojinModel->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("B_zan", BRK_ZAN_MAHOJIN_END), 1, 0, DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    mpMahojinBtk = JKR_NEW mDoExt_btkAnm();
    if (mpMahojinBtk == NULL) {
        return 0;
    }

    if (!mpMahojinBtk->init(mpMahojinModel->getModelData(), (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("B_zan", BTK_ZAN_MAHOJIN), 1, 2, DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    mpMahojinStartBtk = JKR_NEW mDoExt_btkAnm();
    if (mpMahojinStartBtk == NULL) {
        return 0;
    }

    if (!mpMahojinStartBtk->init(mpMahojinModel->getModelData(), (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("B_zan", BTK_ZAN_MAHOJIN_START), 1, 0, DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    mpMahojinModel2 = mDoExt_J3DModel__create((J3DModelData*)dComIfG_getObjectRes("B_zan", BMDR_ZAN_MAHOJIN), 0x80000, 0x11000284);
    if (mpMahojinModel2 == NULL) {
        return 0;
    }

    mpMahojinBrk2 = JKR_NEW mDoExt_brkAnm();
    if (mpMahojinBrk2 == NULL) {
        return 0;
    }

    if (!mpMahojinBrk2->init(mpMahojinModel2->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("B_zan", BRK_ZAN_MAHOJIN_END), 1, 0, 0.0f, 0, -1)) {
        return 0;
    }

    mpMahojinStartBtk2 = JKR_NEW mDoExt_btkAnm();
    if (mpMahojinStartBtk2 == NULL) {
        return 0;
    }

    if (!mpMahojinStartBtk2->init(mpMahojinModel2->getModelData(), (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("B_zan", BTK_ZAN_MAHOJIN_START), 1, 0, DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    modelData = (J3DModelData*)dComIfG_getObjectRes("B_zan", BMDR_ZAN_SWORD_L);
    JUT_ASSERT(0, modelData != NULL);
    mpSwordLModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
    if (mpSwordLModel == NULL) {
        return 0;
    }

    modelData = (J3DModelData*)dComIfG_getObjectRes("B_zan", BMDR_ZAN_SWORD_R);
    JUT_ASSERT(0, modelData != NULL);
    mpSwordRModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
    if (mpSwordRModel == NULL) {
        return 0;
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    return ((daB_ZANT_c*)i_this)->CreateHeap();
}

int daB_ZANT_c::create() {
    fopAcM_ct(this, daB_ZANT_c);
    OS_REPORT("B_ZANT PARAM %x\n", fopAcM_GetParam(this));

#if TARGET_PC
        // Due to our loads being so much faster, Zant can initialize *before* the player
        // This breaks respawning in the final phase of the fight when it tries
        // to load the player's position
        if (daPy_getPlayerActorClass() == NULL) {
            return cPhs_INIT_e;
        }
#endif

    mSwbit = fopAcM_GetParam(this);
    if (mSwbit != 0xFF) {
        if (dComIfGs_isSwitch(mSwbit, fopAcM_GetRoomNo(this))) {
            OS_REPORT("B_ZANT やられ後なので再セットしません\n");
            return cPhs_ERROR_e;
        }
    }

    fopAcM_setStageLayer(this);

    int phase_state = dComIfG_resLoad(&mPhase, "B_zan");
    if (phase_state == cPhs_COMPLEATE_e) {
        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x8F80)) {
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            hio_set = true;
            mInitHIO = true;
            l_HIO.field_0x4 = -1;
        }

        attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        attention_info.distances[fopAc_attn_BATTLE_e] = 24;

        fopAcM_SetMtx(this, mpModelMorf->getModel()->getBaseTRMtx());
        fopAcM_SetMin(this, -200.0f, -200.0f, -200.0f);
        fopAcM_SetMax(this, 200.0f, 200.0f, 200.0f);

        mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir, fopAcM_GetSpeed_p(this), NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        mAcchCir.SetWall(actor_attr::enemy_size_value(this, 100.0f), actor_attr::enemy_size_value(this, 100.0f));
#else
        mAcchCir.SetWall(100.0f, 100.0f);
#endif

#if TARGET_PC  // enemy attribute integration
        health = actor_attr::enemy_health_value(this, 600);
        field_0x560 = health;
#else
        health = 600;
        field_0x560 = 600;
#endif

        field_0x9a4.Init(0xFE, 0, this);
    
        mBodySphCc[0].Set(cc_zant_src);
#if TARGET_PC  // enemy attribute integration

        mBodySphCc[0].SetR(actor_attr::enemy_size_value(this, mBodySphCc[0].GetR()));

        mBodySphCc[0].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mBodySphCc[0].GetAtAtp()));
#endif
        mBodySphCc[0].SetStts(&field_0x9a4);

        mBodySphCc[1].Set(cc_zant_src);
#if TARGET_PC  // enemy attribute integration

        mBodySphCc[1].SetR(actor_attr::enemy_size_value(this, mBodySphCc[1].GetR()));

        mBodySphCc[1].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mBodySphCc[1].GetAtAtp()));
#endif
        mBodySphCc[1].SetStts(&field_0x9a4);

        mRollCc.Set(cc_zant_roll_src);
#if TARGET_PC  // enemy attribute integration

        mRollCc.SetR(actor_attr::enemy_size_value(this, mRollCc.GetR()));

        mRollCc.SetH(actor_attr::enemy_size_value(this, mRollCc.GetH()));

        mRollCc.SetAtAtp(actor_attr::enemy_attack_power_byte(this, mRollCc.GetAtAtp()));
#endif
        mRollCc.SetStts(&field_0x9a4);

        mSwordCc[0].Set(cc_zant_sword_src);
#if TARGET_PC  // enemy attribute integration

        mSwordCc[0].SetR(actor_attr::enemy_size_value(this, mSwordCc[0].GetR()));

        mSwordCc[0].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mSwordCc[0].GetAtAtp()));
#endif
        mSwordCc[0].SetStts(&field_0x9a4);
        mSwordCc[1].Set(cc_zant_sword_src);
#if TARGET_PC  // enemy attribute integration
        mSwordCc[1].SetR(actor_attr::enemy_size_value(this, mSwordCc[1].GetR()));
        mSwordCc[1].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mSwordCc[1].GetAtAtp()));
#endif
        mSwordCc[1].SetStts(&field_0x9a4);

        field_0xc74.Init(0xFE, 0, this);
        for (int i = 0; i < 11; i++) {
            mFoot2Cc[i].Set(cc_zant_foot_src2);
#if TARGET_PC  // enemy attribute integration
            mFoot2Cc[i].SetR(actor_attr::enemy_size_value(this, mFoot2Cc[i].GetR()));
            mFoot2Cc[i].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mFoot2Cc[i].GetAtAtp()));
#endif
            mFoot2Cc[i].SetStts(&field_0xc74);

            mFootCc[i].Set(cc_zant_foot_src);
#if TARGET_PC  // enemy attribute integration

            mFootCc[i].SetR(actor_attr::enemy_size_value(this, mFootCc[i].GetR()));

            mFootCc[i].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mFootCc[i].GetAtAtp()));
#endif
            mFootCc[i].SetStts(&field_0xc74);

            mCameraCc[i].Set(cc_zant_camera_src);
#if TARGET_PC  // enemy attribute integration

            mCameraCc[i].SetR(actor_attr::enemy_size_value(this, mCameraCc[i].GetR()));

            mCameraCc[i].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mCameraCc[i].GetAtAtp()));
#endif
            mCameraCc[i].SetStts(&field_0xc74);
        }

        mSound.init(&current.pos, &eyePos, 3, 1);
        mSound.setEnemyName("B_zant");
        
        mAtInfo.mpSound = &mSound;
        mAtInfo.mPowerType = 0;
        gravity = 0.0f;

        for (int i = 0; i < 9; i++) {
            mPillarIDs[i] = fpcM_ERROR_PROCESS_ID_e;
        }

        for (int i = 0; i < 4; i++) {
            mMobileIDs[i] = fpcM_ERROR_PROCESS_ID_e;
        }

        field_0x724 = -1;

        if (dComIfGp_roomControl_getStayNo() == 60) {
            mFightPhase = PHASE_LAST;
            field_0x718 = 1;
        } else if (dComIfGp_roomControl_getStayNo() == 50) {
            mFightPhase = PHASE_OP;
        } else {
            mFightPhase = dComIfGp_roomControl_getStayNo() - 52;
            if (mFightPhase >= PHASE_MAX) {
                mFightPhase = PHASE_BB;
            }
        }

        if (mFightPhase != PHASE_OP) {
            Z2GetAudioMgr()->bgmStart(Z2BGM_BOSS_ZANT, 0, 0);
            Z2GetAudioMgr()->changeBgmStatus(mFightPhase);
        }

        initNextRoom();
        mKankyoBlend = 1.0f;
        setBaseActionMode(1);

        if (mFightPhase == PHASE_BB) {
            field_0x70b = 0;
        }

        mModelScaleXZ = 1.0f;
        mModelScaleY = 1.0f;

        onWolfNoLock();
        mtx_set();
    }

    return phase_state;
}

static int daB_ZANT_Create(daB_ZANT_c* i_this) {
    return i_this->create();
}

static DUSK_CONST actor_method_class l_daB_ZANT_Method = {
    (process_method_func)daB_ZANT_Create,
    (process_method_func)daB_ZANT_Delete,
    (process_method_func)daB_ZANT_Execute,
    (process_method_func)daB_ZANT_IsDelete,
    (process_method_func)daB_ZANT_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_B_ZANT = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 4,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_B_ZANT_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daB_ZANT_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_B_ZANT_e,
    /* Actor SubMtd */ &l_daB_ZANT_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

AUDIO_INSTANCES;
template<>
JAUSectionHeap* JASGlobalInstance<JAUSectionHeap>::sInstance;

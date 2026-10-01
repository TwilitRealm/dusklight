/**
 * d_a_e_ym.cpp
 * Enemy - Shadow Insect / 闇虫 (Yami Mushi)
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep
#if TARGET_PC  // additional actor attribute integration

#include "dusk/mods/svc/actor_attribute_helpers.hpp"

namespace actor_attr = dusk::mods::svc::actor_attr;
#endif

#include "d/actor/d_a_e_ym.h"
#include "c/c_damagereaction.h"
#include "d/actor/d_a_kago.h"
#include "d/actor/d_a_player.h"
#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_tag_firewall.h"
#include "d/d_com_inf_game.h"
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_camera_mng.h"
#include <cstring>

#include "dusk/settings.h"

class daE_YM_HIO_c: public JORReflexible {
public:
    daE_YM_HIO_c();

    virtual ~daE_YM_HIO_c() {}
    void genMessage(JORMContext*);

    /* 0x04 */ s8 field_0x4;
    /* 0x08 */ f32 mModelSize;
    /* 0x0C */ f32 mElectricInvincibilityTimeExtension;
    /* 0x10 */ f32 mMoveSpeed;
    /* 0x14 */ f32 mFlyMoveSpeed;
    /* 0x18 */ f32 mFlyAttackSpeed;
    /* 0x1C */ f32 mSurpriseDistance;
    /* 0x20 */ f32 mMoveRange;
};

DUSK_GAME_DATA dCcD_SrcSph E_YM_n::cc_sph_src = {
    {
        {0, {{0x400, 1, 13}, {0xD8FBFDFF, 3}, 0x75}},
        {dCcD_SE_METAL, 0, 0, 0, {0}},
        {dCcD_SE_NONE, 0, 0, 0, {2}},
        {0},
    },
    {
        {
            {0.0f, 0.0f, 0.0f},
            40.0f,
        },
    }
};

#define DEFAULT_E_YM_MODEL_SIZE 0.6f

daE_YM_HIO_c::daE_YM_HIO_c() {
    field_0x4 = -1;
    mFlyMoveSpeed = 20.0f;
    mFlyAttackSpeed = 30.0f;
    mModelSize = DEFAULT_E_YM_MODEL_SIZE;
    mElectricInvincibilityTimeExtension = 3.0f;
    mMoveSpeed = 13.0f;
    mSurpriseDistance = 400.0f;
    mMoveRange = 300.0f;
}
#if TARGET_PC  // enemy attribute integration

/* A 4x underground step can cross the tiny path-point sphere in one frame. */
static bool ym_reached_xz_target(daE_YM_c* i_this, const cXyz& target, f32 radius) {

    if (i_this->current.pos.absXZ(target) < radius) {
        return true;
    }

    /* Keep the literal vanilla comparison on the 100% action clock. */
    if (actor_attr::enemy_action_time_speed(i_this) == 1.0f) {
        return false;
    }

    const f32 stepX = i_this->current.pos.x - i_this->old.pos.x;
    const f32 stepZ = i_this->current.pos.z - i_this->old.pos.z;
    const f32 stepLengthSq = stepX * stepX + stepZ * stepZ;
    if (stepLengthSq <= 0.0f) {
        return false;
    }

    // At a fast action clock the insect's finite turn rate can leave it in an
    // orbit which never intersects the vanilla 30-unit sphere.  Treat one
    // frame of actual travel as part of the logical arrival band.  This is a
    // state-transition guard only; it does not change the path or velocity.
    const f32 stepLength = JMAFastSqrt(stepLengthSq);
    const f32 effectiveRadius = radius + stepLength;
    if (i_this->current.pos.absXZ(target) < effectiveRadius) {
        return true;
    }

    const f32 previousX = i_this->old.pos.x - target.x;
    const f32 previousZ = i_this->old.pos.z - target.z;
    const f32 closestTime = -(previousX * stepX + previousZ * stepZ) / stepLengthSq;
    if (closestTime <= 0.0f || closestTime >= 1.0f) {
        return false;
    }

    const f32 closestX = previousX + stepX * closestTime;
    const f32 closestZ = previousZ + stepZ * closestTime;
    return closestX * closestX + closestZ * closestZ < radius * radius;
}
#endif

#if DEBUG
void daE_YM_HIO_c::genMessage(JORMContext* ctext) {
    // 'yami mushi', a.k.a. dark insect
    ctext->genLabel("  闇虫", 0x80000001);
    // basic size
    ctext->genSlider("基本サイズ", &mModelSize, 0.0f, 5.0f);
    ctext->genSlider("放電無敵延長時間", &mElectricInvincibilityTimeExtension, 0.0f, 100.0f);
    // hovering speed
    ctext->genSlider("徘徊速度", &mMoveSpeed, 0.0f, 100.0f);
    // surprising distance
    ctext->genSlider("びっくり距離", &mSurpriseDistance, 0.0f, 1000.0f);
    // movement radius
    ctext->genSlider("移動半径", &mMoveRange, 0.0f, 1000.0f);
    // flight speed
    ctext->genSlider("飛行移動速度", &mFlyMoveSpeed, 0.0f, 100.0f);
    // flight attack speed
    ctext->genSlider("飛行攻撃速度", &mFlyAttackSpeed, 0.0f, 100.0f);
}
#endif


bool daE_YM_c::checkBck(char const* i_arcName, int i_resNo) {
    if (mpMorf->getAnm() == (J3DAnmTransform*)dComIfG_getObjectRes(i_arcName, i_resNo)) {
        return true;
    } else {
        return false;
    }
}

void daE_YM_c::bckSet(int i_resID, u8 i_attribute, f32 i_morf, f32 i_speed) {
    if (mFlyType == true) {
        switch (i_resID) {
        case 6:
            i_resID = 8;
            break;
        case 15:
            i_resID = 9;
            break;
        case 16:
            i_speed = 2.0f;
            i_resID = 10;
            break;
        case 14:
            i_resID = 10;
            break;
        default:
            i_resID = 9;
        }

#if TARGET_PC  // enemy attribute integration
        const actor_attr::ActionAnimationParams params = actor_attr::enemy_animation_params(this, i_morf, i_speed);
#endif
        mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_TM", i_resID), i_attribute,
#if TARGET_PC  // enemy attribute integration
                            params.morph, params.playbackSpeed, 0.0f, -1.0f);
#else
                            i_morf, i_speed, 0.0f, -1.0f);
#endif
    } else {
#if TARGET_PC  // enemy attribute integration
        const actor_attr::ActionAnimationParams params = actor_attr::enemy_animation_params(this, i_morf, i_speed);
#endif
        mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_YM", i_resID), i_attribute,
#if TARGET_PC  // enemy attribute integration
                            params.morph, params.playbackSpeed, 0.0f, -1.0f);
#else
                            i_morf, i_speed, 0.0f, -1.0f);
#endif
    }
}

void daE_YM_c::bckSetFly(int i_resID, u8 i_attribute, f32 i_morf, f32 i_speed) {
#if TARGET_PC  // enemy attribute integration

    const actor_attr::ActionAnimationParams params = actor_attr::enemy_animation_params(this, i_morf, i_speed);
#endif
    mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_TM", i_resID), i_attribute,
#if TARGET_PC  // enemy attribute integration
                        params.morph, params.playbackSpeed, 0.0f, -1.0f);
#else
                        i_morf, i_speed, 0.0f, -1.0f);
#endif
}

int daE_YM_c::draw() {
    if (field_0x71d) {
        return 1;
    }

    if (daPy_getPlayerActorClass()->checkNowWolfEyeUp()) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &field_0x6d4, 255.0f, 1.0f, 30.0f), cLib_addCalc2(&field_0x6d4, 255.0f, 1.0f, 30.0f));
    } else if (mAction == ACT_DOWN) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &field_0x6d4, 255.0f, 1.0f, 30.0f), cLib_addCalc2(&field_0x6d4, 255.0f, 1.0f, 30.0f));
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(this, &field_0x6d4, 0.0f, 1.0f, 30.0f), cLib_addCalc2(&field_0x6d4, 0.0f, 1.0f, 30.0f));
    }

    bool bvar = true;

    if (field_0x710 != 0) {
        if (!field_0x6d4) {
            return 1;
        }

        if (mIsHide && mAction != ACT_RAIL && mAction != ACT_BACK_RAIL) {
            bvar = false;
        }
    } else {
        field_0x710 = 1;
    }

    if (mFlyType == true) {
        J3DShape* shape_p =
            mpMorf->getModel()->getModelData()->getMaterialNodePointer(1)->getShape();

        if (shape_p != NULL) {
            if (field_0x6cc) {
                shape_p->show();
            } else {
                shape_p->hide();
            }
        }
    }

    J3DModel* model_p = mpMorf->getModel();
    g_env_light.settingTevStruct(6, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(model_p, &tevStr);

    J3DModelData* modelData_p = model_p->getModelData();
    for (u16 i = 0; i < modelData_p->getMaterialNum(); i++) {
        J3DMaterial* material = modelData_p->getMaterialNodePointer(i);
        material->getTevKColor(3)->a = field_0x6d4;
    }

    if (bvar) {
        if (field_0x6dc != -70.0f) {
            mpBrk->entry(model_p->getModelData());
            dComIfGd_setListDark();
            mpMorf->entryDL();
            dComIfGd_setList();
        }

        cXyz sp40;
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
        sp40.set(current.pos.x, current.pos.y + 100.0f * sizeMultiplier, current.pos.z);
#else
        sp40.set(current.pos.x, current.pos.y + 100.0f, current.pos.z);
#endif

        field_0x69c = dComIfGd_setShadow(field_0x69c, 1, model_p, &sp40, DUSK_IF_ELSE(400.0f * sizeMultiplier, 400.0f), 0.0f,
                                         current.pos.y, mAcch.GetGroundH(), mAcch.m_gnd,
                                         &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    }

    return 1;
}

static int daE_YM_Draw(daE_YM_c* i_this) {
    return i_this->draw();
}

static u8 lbl_253_bss_8;

static daE_YM_HIO_c l_HIO;

static fopAc_ac_c* m_near_obj;

static f32 m_obj_dist;

static void* s_obj_sub(void* param_0, void* param_1) {
    f32 obj_dist, abs_dist_to_player;
    if (fopAcM_IsActor(param_0) && fopAcM_GetName(param_0) == fpcNm_Obj_Carry_e) {
        if (!fpcM_IsCreating(fopAcM_GetID(param_0))) {
            obj_dist = fopAcM_searchActorDistanceXZ((fopAc_ac_c*)param_0, (fopAc_ac_c*)param_1);

            if (obj_dist < 100.0f && obj_dist < m_obj_dist) {
                abs_dist_to_player = std::abs(fopAcM_searchActorDistanceY((fopAc_ac_c*)param_0, (fopAc_ac_c*)param_1));
                if (abs_dist_to_player < 30.0f) {
                    m_near_obj = (fopAc_ac_c*) param_0;
                    m_obj_dist = obj_dist;
                }
            }
        }
    }

    return 0;
}

// unused function.
void daE_YM_c::setWaterEffect() {
    static u16 w_eff_id[] = {
        ID_ZI_J_DOWNWTRA_A,
        ID_ZI_J_DOWNWTRA_B,
        ID_ZI_J_DOWNWTRA_C,
        ID_ZI_J_DOWNWTRA_D
    }; // unused
}

void daE_YM_c::setDigEffect() {
#if TARGET_PC  // enemy attribute integration
    cXyz sp1C = actor_attr::enemy_size_multiplier(this, field_0x68c);
#else
    cXyz sp1C(field_0x68c, field_0x68c, field_0x68c);
#endif
    cXyz sp28(current.pos);
    sp28.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(this, field_0x6dc), field_0x6dc);

    field_0xad8 = dComIfGp_particle_set(field_0xad8, dPa_RM(ID_ZI_S_YM_DIGA_A), &sp28, &shape_angle, &sp1C);
    field_0xadc = dComIfGp_particle_set(field_0xadc, dPa_RM(ID_ZI_S_YM_DIGA_B), &sp28, &shape_angle, &sp1C);
}

void daE_YM_c::setElecEffect1() {
    f32 fVar3 = (field_0x68c * 10.0f) / 6.0f;
#if TARGET_PC  // enemy attribute integration
    cXyz cStack_2c = actor_attr::enemy_size_multiplier(this, fVar3);
#else
    cXyz cStack_2c(fVar3, fVar3, fVar3);
#endif
    MtxP iVar5;
    if (mFlyType == true) {
        iVar5 = mpMorf->getModel()->getAnmMtx(8);
    } else {
        iVar5 = mpMorf->getModel()->getAnmMtx(0);
    }

    cXyz cStack_38(iVar5[0][3], iVar5[1][3], iVar5[2][3]);
    field_0xad8 = dComIfGp_particle_set(field_0xad8, dPa_RM(ID_ZI_S_YM_ELECAT_A),
                                        &cStack_38, &tevStr, &shape_angle, &cStack_2c,
                                        0xFF, 0, -1, 0, 0, 0);
    field_0xadc = dComIfGp_particle_set(field_0xadc, dPa_RM(ID_ZI_S_YM_ELECAT_B),
                                        &cStack_38, &tevStr, &shape_angle, &cStack_2c,
                                        0xFF, 0, -1, 0, 0, 0);
}


void daE_YM_c::setElecEffect2() {
    f32 fVar3 = (field_0x68c * 10.0f) / 6.0f;
#if TARGET_PC  // enemy attribute integration
    cXyz cStack_2c = actor_attr::enemy_size_multiplier(this, fVar3);
#else
    cXyz cStack_2c(fVar3, fVar3, fVar3);
#endif
    MtxP iVar5;
    if (mFlyType == true) {
        iVar5 = mpMorf->getModel()->getAnmMtx(8);
    } else {
        iVar5 = mpMorf->getModel()->getAnmMtx(0);
    }

    cXyz cStack_38(iVar5[0][3], iVar5[1][3], iVar5[2][3]);
    setElecEffect1();
    field_0xae0 = dComIfGp_particle_set(field_0xae0, dPa_RM(ID_ZI_S_YM_ELECAT_C), &cStack_38, &tevStr, 
                                        &shape_angle, &cStack_2c,
                                        0xFF, 0, -1, 0, 0, 0);
    field_0xae4 = dComIfGp_particle_set(field_0xae4, dPa_RM(ID_ZI_S_YM_ELECAT_D), &cStack_38, &tevStr,
                                        &shape_angle, &cStack_2c,
                                        0xFF, 0, -1, 0, 0, 0);
}


void daE_YM_c::setFireEffect() {
#if TARGET_PC  // enemy attribute integration
    cXyz my_vec_0 = actor_attr::enemy_size_multiplier(this, 1.0f);
#else
    cXyz my_vec_0(1.0f, 1.0f, 1.0f);
#endif
    mParticleKeys[0] = dComIfGp_particle_set(mParticleKeys[0], ID_ZF_J_FIRE00_GLOW, &current.pos, NULL, &my_vec_0);
    mParticleKeys[1] = dComIfGp_particle_set(mParticleKeys[1], ID_ZF_J_FIRE02_FIRE, &current.pos, NULL, &my_vec_0);
    mParticleKeys[2] = dComIfGp_particle_set(mParticleKeys[2], ID_ZF_J_FIRE01_SPARK, &current.pos, NULL, &my_vec_0);
    JPABaseEmitter* emitter;
    if (field_0x6d0 != NULL) {
        for (int idx = 44; idx >= 0; --idx) {
            field_0x6d0[idx+1] = field_0x6d0[idx];
        }

        field_0x6d0[0] = speed;
        for (int idx = 0; idx < 3; ++idx) {
            emitter = dComIfGp_particle_getEmitter(mParticleKeys[idx]);
            if (emitter != NULL) {
                if (speed.abs() > 1.0f) {
                    emitter->setParticleCallBackPtr((JPAParticleCallBack*)&JPTracePCB4);
                    emitter->setUserWork((uintptr_t)field_0x6d0);
                } else {
                    emitter->setParticleCallBackPtr(NULL);
                }
            }
        }
    }
}

bool daE_YM_c::checkWallCrash() {
    if (mType == 1 && (daPy_getPlayerActorClass()->checkFrontRollCrash() ||
                       daPy_getPlayerActorClass()->checkWolfAttackReverse()))
    {
        cXyz tmp = daPy_getPlayerActorClass()->current.pos - mPrevPos;
        if (tmp.absXZ() < 600.0f) {
            if (mTagPosP != NULL) {
                setActionMode(ACT_FLY);
            } else {
                setActionMode(ACT_FALL);
            }

            return true;
        }
    }

    return false;
}

bool daE_YM_c::checkWolfBark() {
    if (mType == 2) {
        return false;
    }

    if (daPy_getPlayerActorClass()->checkWolfBark() && mDistToPlayer < DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, 900.0f), 900.0f)) {
        if (mType == 0) {
            s16 dist_ang = cLib_distanceAngleS(mAngleToPlayer, shape_angle.y);
            cXyz my_vec_0 = current.pos - mPrevPos;
            if (my_vec_0.abs() < l_HIO.mMoveRange && dist_ang < 0x4000 && cM_rndF(10.0f) > 1.0f) {
                setActionMode(ACT_BACK);
            } else {
                if (mTagPosP != NULL) {
                    setActionMode(ACT_FLY);
                } else {
                    setActionMode(ACT_SURPRISE);
                }
            }
        } else if (mType == 1) {
            return false;
        }

        return true;
    }

    return false;
}

bool daE_YM_c::checkSurpriseLock() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    if (mType == 2) {
        return false;
    }

    if (mAction != ACT_WAIT && mAction != ACT_MOVE && mAction != ACT_SURPRISE
        && mAction != ACT_ATTACK && mAction != ACT_ATTACK_WALL) {
        return false;
    }

    if (field_0x6f6 != 0) {
        return false;
    }

    if (field_0x6fc) {
        return false;
    }

    if (mAction != ACT_ATTACK) {
        if (dComIfGp_getAttention()->LockonTruth()) {
            if (dComIfGp_getAttention()->LockonTarget(0) == this && mType == 0) {
                cXyz my_vec_0 = current.pos - mPrevPos;
                cXyz my_vec_1 = player->current.pos - mPrevPos;
                if (mType == 4) {
                    return checkRailSurprise();
                }

                switch (field_0x6a1) {
                    case 0: {
                        if (my_vec_0.abs() > l_HIO.mMoveRange || my_vec_1.abs() > (l_HIO.mMoveRange + 150.0f)) {
                            if (mAction != ACT_SURPRISE) {
                                if (mTagPosP != NULL) {
                                    setActionMode(ACT_FLY);
                                } else {
                                    setActionMode(ACT_SURPRISE);
                                }
                                return true;
                            }

                            return false;
                        } else {
                            setActionMode(ACT_ATTACK);
                            return true;
                        }
                    }
                    // fallthrough
                    case 1: {
                        if (mAction != ACT_SURPRISE) {
                            if (mTagPosP != NULL) {
                                setActionMode(ACT_FLY);
                            } else {
                                setActionMode(ACT_SURPRISE);
                            }
                        }
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

bool daE_YM_c::checkRailSurprise() {
    if (dComIfGp_checkPlayerStatus0(0, 0x8000000)) {
#if TARGET_PC  // enemy attribute integration
        if (mDistToPlayer > actor_attr::enemy_notice_distance(this, 200.0f) || (s16)cLib_distanceAngleS(mAngleToPlayer, shape_angle.y) > 0x2000) {
            field_0x70a = actor_attr::enemy_sync_timer(this, 0x1e);
#else
        if (mDistToPlayer > 200.0f || (s16)cLib_distanceAngleS(mAngleToPlayer, shape_angle.y) > 0x2000) {
            field_0x70a = 0x1e;
#endif
        }
    }

    if (field_0x70a != 0) {
        return false;
    }

    if (field_0x704 == 0) {
        setActionMode(ACT_BACK_RAIL);
    } else {
        setActionMode(ACT_SURPRISE);
    }

    return true;
}

bool daE_YM_c::checkSurpriseNear() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz my_vec_0;
    if (mType == 1) {
        if (field_0x6fa) {
            return false;
        }

        if (player->checkClimbMove()) {
            f32 my_val = field_0x6e0 - 50.0f;
            if (my_val < 50.0f) {
                my_val = 50.0f;
            }

            cXyz my_vec_1 = player->current.pos;
            my_vec_1.y += 100.0f;
            my_vec_0 = mPrevPos - current.pos;
            if (my_vec_0.abs() < my_val) {
                my_vec_0 = my_vec_1 - current.pos;
                if (my_vec_0.abs() < DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, 350.0f), 350.0f)) {
                    dBgS_GndChk gnd_chk;
                    gnd_chk.SetPos(&my_vec_1);
                    f32 gnd_cross = dComIfG_Bgsp().GroundCross(&gnd_chk);
                    if (player->current.pos.y - gnd_cross > 50.0f) {
                        field_0x6a5 = 0;
                        setActionMode(ACT_ATTACK_WALL);
                        return true;
                    }
                }
            } else {
                my_vec_0 = mPrevPos - my_vec_1;
                if (my_vec_0.abs() < DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, 100.0f), 100.0f)) {
                    field_0x6a5 = 1;
                    setActionMode(ACT_ATTACK_WALL);
                    return true;
                }
            }
        }
    } else if (mType == 0 || mType == 4) {
        if (mAcch.ChkGroundHit() == 0) {
            return false;
        }

        f32 my_val;
        if (mType == 4) {
            my_val = l_HIO.mSurpriseDistance + 200.0f;
        } else {
            if (field_0x6a9 != 0) {
                my_val = l_HIO.mSurpriseDistance;
            } else {
                my_val = l_HIO.mSurpriseDistance + 200.0f;
            }
        }

        if (mDistToPlayer < DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, my_val), my_val)) {
            if (mSphCc.ChkCoHit()) {
                cCcD_Obj* hit_obj = mSphCc.GetCoHitObj();
                if (fopAcM_GetName(dCc_GetAc(hit_obj->GetAc())) == fpcNm_ALINK_e) {
                    if (mType == 4) {
                        return checkRailSurprise();
                    }

                    if (mTagPosP != NULL) {
                        setActionMode(ACT_FLY);
                    } else {
                        setActionMode(ACT_SURPRISE);
                    }

                    return true;
                }
            }

            if (mAction == ACT_ATTACK) {
                return false;
            }

            if (player->getSpeedF() >= 16.0f) {
                field_0x6f6 = 0;
            } else {
                if (field_0x6f8) {
                    return false;
                }

#if TARGET_PC  // enemy attribute integration
                if (mDistToPlayer > actor_attr::enemy_notice_distance(this, l_HIO.mSurpriseDistance - 100.0f)) {
                    field_0x6f8 = actor_attr::enemy_sync_timer(this, 0x3c);
#else
                if (mDistToPlayer > l_HIO.mSurpriseDistance - 100.0f) {
                    field_0x6f8 = 0x3c;
#endif
                    return false;
                }
            }

            if (field_0x6f6 == 0) {
                if (mType == 4) {
                    return checkRailSurprise();
                }

                if (mTagPosP != NULL) {
                    setActionMode(ACT_FLY);
                } else {
                    setActionMode(ACT_SURPRISE);
                }

                return true;
            }
        } else {
            field_0x6f8 = 0;
        }
    }

    return false;
}

void daE_YM_c::setNormalCc() {
    mSphCc.SetCoSPrm(0x75);
    mSphCc.SetTgType(0xD8FBFDFF);
}

void daE_YM_c::setAppear() {
    mIsHide = false;

    if (field_0x6a6 != 4) {
        mType = 0;
    }

    field_0x714 = fopAc_AttnFlag_BATTLE_e;
}

void daE_YM_c::setMoveSound(int param_0) {
    if (mFlyType == 1) {
        if (checkBck("E_TM", 10) == 0) {
            return;
        }
    } else if (checkBck("E_YM", 16) == 0) {
        return;
    }

    DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(this, mpMorf, fabsf(speedF) * 0.15f), mpMorf->setPlaySpeed(fabsf(speedF) * 0.15f));
    if (mpMorf->checkFrame(1.0f) || mpMorf->checkFrame(5.0f) || mpMorf->checkFrame(9.0f)) {
        if (param_0) {
            mSound.startCreatureSound(Z2SE_EN_YM_FN_UNDER, 0, -1);
        } else {
            mSound.startCreatureSound(Z2SE_EN_YM_FOOTNOTE, 0, -1);
        }
    }
}

void daE_YM_c::setTurnSound() {
    if (mpMorf->checkFrame(1.0f) || mpMorf->checkFrame(5.0f) || mpMorf->checkFrame(9.0f)) {
        mSound.startCreatureSound(Z2SE_EN_YM_FOOTNOTE, 0, -1);
    }
}

void daE_YM_c::setActionMode(int i_action) {
    mLastAction = mAction;
    mAction = i_action;
    mMode = 0;
    field_0x6f0 = field_0x6f2 = 0;
    current.angle.y = shape_angle.y;
}

void daE_YM_c::executeWait() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz my_vec_0;
    if (mType != 2) {
        mSound.startCreatureSoundLevel(Z2SE_EN_YM_WAIT, 0, -1);
    }

    if (mType != 1) {
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -60.0f, 5.0f), cLib_chaseF(&speed.y, -60.0f, 5.0f));
    }

    if (field_0x6a1 == 1 && mMode != 0 && field_0x6f2 == 0) {
        field_0x708 = field_0x6c9 = 0;
    }

    switch (mMode) {
        case 100: {
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
            mMode = 101;
        }
        // fallthrough
        case 101: {
            current.pos = home.pos;
            setHideType();
            mPrevPos = current.pos;
            if (field_0x6a6 == 1 || field_0x6f0 == 0) {
                field_0x6f0 = 0;
                mMode = 0;
            }
            break;
        }

        case 0: {
            field_0x6f2 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
            checkWall();
            bckSet(0xf, 2, 5.0f, 1.0f);
            if (mType == 2) {
                mMode = 2;
                field_0x6dc = -70.0f;
                field_0x714 = 0;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5), 5);
                field_0x6a5 = 0;
            } else if (mType == 1) {
                if (field_0x6a3 != 0xff) {
                    if (dComIfGs_isSwitch(field_0x6a3, fopAcM_GetRoomNo(this)) == 0) {
                        mMode = 4;
                        break;
                    }
                }
                setNormalCc();
                mMode = 1;
                if (mDistToPlayer < 400.0f) {
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(10.0f) + 30.0f), cM_rndFX(10.0f) + 30.0f);
                } else {
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(30.0f) + 100.0f), cM_rndFX(30.0f) + 100.0f);
                }
            } else {
                setNormalCc();
                mMode = 1;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndFX(30.0f) + 100.0f), cM_rndFX(30.0f) + 100.0f);
            }
            break;
        }

        case 1: {
            if (checkSurpriseNear()) {
                return;
            }

            if (checkWolfBark()) {
                return;
            }

            if (checkWallCrash()) {
                return;
            }

            if (field_0x6f0 == 0) {
                setActionMode(ACT_MOVE);
                return;
            }

            break;
        }

        case 2: {
            checkFrinedSamePos();
            switch (field_0x6a6) {
                case 1:
                    field_0x718 = 1;
                    if (field_0x6d4) {
                        field_0x714 = fopAc_AttnFlag_BATTLE_e;
                    } else {
                        field_0x714 = 0;
                    }
                    m_near_obj = NULL;
                    m_obj_dist = 100.0f;
                    fpcM_Search(s_obj_sub, this);
                    if (m_near_obj == NULL) {
                        setActionMode(ACT_ESCAPE);
                        field_0x718 = 0;
                    }
                    break;
                case 2:
                    break;
                default:
                    if (player->checkWolfDig()) {
                        my_vec_0 = player->getLeftHandPos() - current.pos;
                        if (my_vec_0.abs() < 200.0f) {
                            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x14), 0x14);
                            mMode = 3;
                            field_0x6a6 = 0;
                            fopAcM_OnStatus(this, fopAcStts_UNK_0x4000_e);
                        }
                    }
            }

            break;
        }

        case 3: {
            if (field_0x6f0 == 0) {
                setActionMode(ACT_ESCAPE);
            }
            break;
        }

        case 4: {
            if (field_0x6d4) {
                field_0x714 = fopAc_AttnFlag_BATTLE_e;
            } else {
                field_0x714 = 0;
            }

            if (dComIfGs_isSwitch(field_0x6a3, fopAcM_GetRoomNo(this)) && mType == 1) {
                field_0x714 = fopAc_AttnFlag_BATTLE_e;
                if (mTagPosP != NULL) {
                    setActionMode(ACT_FLY);
                } else {
                    setActionMode(ACT_FALL);
                }
            }

            break;
        }
    }

    current.angle.y = shape_angle.y;
}

void daE_YM_c::executeMove() {
    daPy_py_c* player = daPy_getPlayerActorClass(); // unused
    cXyz my_vec_0;
    cXyz my_vec_1;
    f32 my_val = field_0x6e0 - 50.0f;
    if (my_val < 50.0f) {
        my_val = 50.0f;
    }

    if (!checkSurpriseNear() && !checkWolfBark() && !checkWallCrash() && !setAttackMotion()) {
        if (mType != 1) {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -60.0f, 5.0f), cLib_chaseF(&speed.y, -60.0f, 5.0f));
        }

        switch (mMode) {
            case 0:
                bckSet(0xe, 2, 10.0f, 1.5f);
                checkWall();
                if (mType == 1) {
                    mAcch.SetGroundUpY(fabsf(cM_ssin(field_0x668.x) * 30.0f));
                }
                field_0x6e4 = cM_rndFX(32768.0f);
                if ((s16)cLib_distanceAngleS(field_0x6e4, shape_angle.y) < 0x2000) {
                    field_0x6e4 = shape_angle.y + 0x4000;
                }
                my_vec_0 = mPrevPos - current.pos;
                field_0x6a5 = 0;
                if (my_vec_0.abs() > my_val) {
                    if (mType == 1) {
                        mDoMtx_stack_c::ZrotS(-field_0x668.z);
                        mDoMtx_stack_c::XrotM(-field_0x668.x);
                        mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
                    } else {
                        my_vec_1 = my_vec_0;
                    }
                    field_0x6e4 = cM_atan2s(my_vec_1.x, my_vec_1.z);
                    field_0x6a5 = 1;
                }
                mMode = 1;
                break;
            case 1: {
                setTurnSound();
                s16 my_step = 0x200;
                if (mType == 1 && mDistToPlayer < 400.0f) {
                    my_step = 0x300;
                }
                if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, field_0x6e4, my_step), cLib_chaseAngleS(&shape_angle.y, field_0x6e4, my_step))) {
                    if ((!field_0x6e0 && field_0x6a5 == 0) || checkBeforeBg(field_0x6e4)) {
                        setActionMode(ACT_WAIT);
                    } else {
                        bckSet(0xf, 2, 5.0f, 1.0f);
                        field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(10.0f) + 5.0f), cM_rndF(10.0f) + 5.0f);
                        mMode = 2;
                    }
                }
                break;
            }
            case 2:
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_WAIT, 0, -1);
                if (field_0x6f0 == 0 && mpMorf->checkFrame(1.0f)) {
                    bckSet(0x10, 2, 5.0f, 1.0f);
                    mMode = 3;
                    speedF = 10.0f;
                    if (field_0x6a5 == 0 && speedF > field_0x6e0) {
                        speedF = field_0x6e0;
                    }
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(40.0f) + 10.0f), cM_rndF(40.0f) + 10.0f);
                }
                break;
            case 3: {
                setMoveSound(0);
                bool my_bool = false;
                my_vec_0 = current.pos - mPrevPos;
                if (field_0x6a5 == 0) {
                    if (my_vec_0.abs() > field_0x6e0) {
                        my_bool = true;
                    }
                } else {
                    if (my_vec_0.abs() < my_val) {
                        field_0x6a5 = 0;
                    }
                }
                if (my_bool || field_0x6f0 == 0 || checkBeforeBg(field_0x6e4)) {
                    speedF = 0.0f;
                    setActionMode(ACT_WAIT);
                    return;
                }
                break;
            }
            default:
                break;
        }
        current.angle.y = shape_angle.y;
    }
}

void daE_YM_c::executeEscape() {
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -60.0f, 5.0f), cLib_chaseF(&speed.y, -60.0f, 5.0f));
    daPy_py_c* player = daPy_getPlayerActorClass();
    (void) player; // acquired, but not used.
    cXyz my_vec_0;
    if (field_0x6dc < 0.0f) {
        field_0x6dc += DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 8.0f), 8.0f);
        if (field_0x6dc > 0.0f) {
            field_0x6dc = 0.0f;
        }
    }
    if (!dComIfGp_event_runCheck()) {
        fopAcM_OffStatus(this, fopAcStts_UNK_0x4000_e);
    }
    switch (mMode) {
        case 0:
            if (mType == 2) {
                setAppear();
            }

            if (mLastAction != ACT_WIND) {
                s16 adj_angle = mAngleToPlayer + 0x8000;
                if (mFriendFlag & 0x1) {
                    shape_angle.y = adj_angle;
                } else if (mFriendFlag & 0x2) {
                    if (field_0x6a6 == 0) {
                        shape_angle.y = adj_angle + 0x2800;
                    } else {
                        shape_angle.y = adj_angle + 0x1800;
                    }
                } else if (mFriendFlag & 0x4) {
                    if (field_0x6a6 == 0) {
                        shape_angle.y = adj_angle - 0x2800;
                    } else {
                        shape_angle.y = adj_angle - 0x1800;
                    }
                } else {
                    shape_angle.y = adj_angle + cM_rndFX(8192.0f);
                }
                if (field_0x6a6 == 0 || field_0x6a6 == 4) {
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x28), 0x28);
                    mMode = 3;
                    speedF = 5.0f;
                    speed.y = cM_rndF(5.0f) + 30.0f;
                    field_0x6a5 = 0;
                } else {
                    speedF = 10.0f;
                    speed.y = cM_rndF(5.0f) + 45.0f;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x28), 0x28);
                    mMode = 1;
                }
                field_0x6dc = 0.0f;
                bckSet(0xb, 0, 5.0f, 1.0f);
                mSound.startCreatureSound(Z2SE_EN_YM_JUMP, 0, -1);
            } else {
                field_0x6f2 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                mMode = 2;
            }
            field_0x6a9 = 1;
            field_0x6e4 = shape_angle.y;
            field_0x6e6 = 0x5dc;
            field_0x6e8 = 0;
            break;
        case 1:
            setElecEffect1();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
            if (mAcch.ChkGroundHit()) {
                mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
                bckSet(0xc, 0, 0.0f, 1.0f);
                speedF = 0.0f;
                speed.y = 0.0f;
                mMode = 2;
#if TARGET_PC  // enemy attribute integration
                field_0x6f2 = actor_attr::enemy_sync_timer(this, 10);
                if (field_0x6f0 >= actor_attr::enemy_sync_timer(this, 2)) {
                    field_0x6f0 = actor_attr::enemy_sync_timer(this, 1);
#else
                field_0x6f2 = 10;
                if (field_0x6f0 >= 2) {
                    field_0x6f0 = 1;
#endif
                }
                field_0x70e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x5a), 0x5a);
            }
            break;
        case 2:
            if (field_0x70e) {
                setElecEffect1();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
            }
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_WAIT, 0, -1);
            if (field_0x6f2 == 0) {
                speedF = 10.0f;
                mMode = 4;
                field_0x6e6 = 0x5dc;
                bckSet(0x10, 2, 5.0f, 1.0f);
            }
            break;
        case 3:
            setElecEffect1();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
            if (field_0x6a5 == 0) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, field_0x6e4 + field_0x6e8, 8, 0x1000, 0x100), cLib_addCalcAngleS(&shape_angle.y, field_0x6e4 + field_0x6e8, 8, 0x1000, 0x100));
                if (mAcch.ChkGroundHit()) {
                    mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
                    field_0x6a5 = 1;
                    speedF = 0.0f;
                    bckSet(0xc, 0, 0.0f, 1.0f);
                }
            } else {
                if (mpMorf->isStop()) {
                    mMode = 4;
                    bckSet(0x10, 2, 5.0f, 1.0f);
                }
            }
            break;
        case 4:
            if (field_0x70e) {
                setElecEffect2();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
                field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
                mSphCc.OnAtSetBit();
            }
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 20.0f, 1.0f), cLib_chaseF(&speedF, 20.0f, 1.0f));
            setMoveSound(0);
            if (mAcch.ChkWallHit()) {
                DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0x6e4, 0x800), ANGLE_ADD(field_0x6e4, 0x800));
            }

            if (field_0x6e8 >= 0) {
                DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0x6e6, -200), ANGLE_SUB(field_0x6e6, 200));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0x6e6, 200), ANGLE_ADD(field_0x6e6, 200));
            }

#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0x6e8 , field_0x6e6);
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, field_0x6e4 + field_0x6e8, 8, 0x1000, 0x100);
#else
            field_0x6e8 += field_0x6e6;
            cLib_addCalcAngleS(&shape_angle.y, field_0x6e4 + field_0x6e8, 8, 0x1000, 0x100);
#endif
            my_vec_0 = current.pos - mPrevPos;
            if (my_vec_0.absXZ() > 500.0f) {
                speedF = 0.0f;
                mPrevPos = current.pos;
                field_0x6dc = 0.0f;
                setActionMode(ACT_WAIT);
                setNormalCc();
            }

            break;
        default:
            break;
    }

    if (field_0x6f0 == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 1), 1)) {
        mSphCc.SetCoSPrm(0x75);
    }

    current.angle.y = shape_angle.y;
}

void daE_YM_c::executeDown() {
    field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
    field_0x6dc = 0.0f;
    maxFallSpeed = -60.0f;
    Vec wtr_pos = current.pos;
    wtr_pos.y += 500.0f;
    dBgS_ObjGndChk_Wtr gnd_chk_wtr;
    gnd_chk_wtr.SetPos(&wtr_pos);
    f32 gnd_cross = dComIfG_Bgsp().GroundCross(&gnd_chk_wtr);
    if (current.pos.y > gnd_cross) {
        gravity = -5.0f;
    } else {
        gravity = 0.0f;
    }

    OS_REPORT("YM executeDown %d %f %f \n", mMode, gnd_cross, current.pos.y);
    field_0x6cf = 0;
    switch (mMode) {
        case 0:
            if (mFlyType == 1) {
                bckSet(6, 0, 5.0f, 0.0f);
            } else {
                bckSet(0x10, 2, 5.0f, 1.0f);
            }
            mMode = 1;
            field_0x668.set(0, 0, 0);
            speedF = cM_rndF(5.0f) + 15.0f;
            speed.y = cM_rndF(5.0f) + 40.0f;
            shape_angle.y = mAtInfo.mHitDirection.y + 0x8000;
            shape_angle.x = 0;
            mAcch.ClrGroundHit();
#if TARGET_PC  // enemy attribute integration
            mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
            mAcchCir.SetWall(40.0f, 60.0f);
#endif
            if (mType == 1) {
                cXyz my_vec_0 = daPy_getPlayerActorClass()->current.pos;
                my_vec_0.y += 50.0f;
                cXyz my_vec_1(speedF * cM_ssin(shape_angle.y), speed.y, speedF * cM_scos(shape_angle.y));
                my_vec_1 += current.pos;
                dBgS_LinChk lin_chk;
                lin_chk.Set(&my_vec_0, &my_vec_1, NULL);
                if (dComIfG_Bgsp().LineCross(&lin_chk)) {
                    cM3dGPla plane;
                    dComIfG_Bgsp().GetTriPla(lin_chk, &plane);
                    current.pos = lin_chk.GetCross() + (plane.mNormal * 60.0f);
                    speedF = 0.0f;
                }
            }
            setAppear();
            field_0x714 = 0;
            // Faron Woods
            if (0 == strcmp("R_SP108", dComIfGp_getStartStageName())) {
                if (dComIfGp_getStartStageRoomNo() == 0 && dComIfG_play_c::getLayerNo(0) == 0xe && mSwitchBit != 0xff) {
                    if (dComIfGs_isSwitch(mSwitchBit, fopAcM_GetRoomNo(this)) == 0) {
                        dComIfGs_onSwitch(mSwitchBit, fopAcM_GetRoomNo(this));
                    }
                }
            }
            break;
        case 1:
        case 2:
            if (mMode == 1) {
                if (speed.y < 0.0f) {
                    mAcch.SetGroundUpY(30.0f);
                }
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.x, 0x7800, 8, 0x1000, 0x10), cLib_addCalcAngleS(&shape_angle.x, 0x7800, 8, 0x1000, 0x10));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.x, -0x8000, 8, 0x1000, 0x10), cLib_addCalcAngleS(&shape_angle.x, -0x8000, 8, 0x1000, 0x10));
            }
            if (mAcch.ChkGroundHit()) {
                if (mFlyType != 1) {
                    bckSet(6, 0, 0.0f, DUSK_IF_ELSE(dusk::getSettings().game.fastTears ? 2.0f : 1.0f, 1.0f));
                }
                if (mMode == 1) {
                    mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
                    mMode = 2;
                    speedF = cM_rndF(5.0f) + 5.0f;
                    speed.y = 15.0f;
                } else {
                    mSound.startCreatureSound(Z2SE_EN_YM_MOGAKU, 0, -1);
                    mMode = 3;
                    speedF = 0.0f;
                    shape_angle.x = -0x8000;
                    field_0x6f0 = 0;
                }
                break;
            }

            if (current.pos.y < gnd_cross) {
                mSound.startCreatureSound(Z2SE_CM_BODYFALL_WATER_M, 0, -1);
                mSound.startCreatureSound(Z2SE_EN_YM_MOGAKU, 0, -1);
                bckSet(6, 0, 0.0f, DUSK_IF_ELSE(dusk::getSettings().game.fastTears ? 2.0f : 1.0f, 1.0f));
                speedF = 0.0f;
                speed.y = gravity = 0.0f;
                current.pos.y = gnd_cross;
                shape_angle.x = -0x8000;
                field_0x6f0 = 0;
                mMode = 3;
                break;
            }

            if (speed.y < 0.0f && std::abs(gnd_cross - current.pos.y) > 1000.f) {
                dBgS_GndChk gnd_chk;
                gnd_chk.SetPos(&current.pos);
                f32 gnd_cross_2 = dComIfG_Bgsp().GroundCross(&gnd_chk);
                if (gnd_cross_2 == -G_CM3D_F_INF || std::abs(gnd_cross_2 - current.pos.y) > 1000.0f
                    || dComIfG_Bgsp().GetGroundCode(gnd_chk) == 4 || dComIfG_Bgsp().GetGroundCode(gnd_chk) == 10
                    || dComIfG_Bgsp().GetGroundCode(gnd_chk) == 5)
            {
                    bckSet(6, 0, 0.0f, DUSK_IF_ELSE(dusk::getSettings().game.fastTears ? 2.0f : 1.0f, 1.0f));
                    mMode = 3;
                    speedF = 0.0f;
                    shape_angle.x = -0x8000;
                    field_0x6f0 = 0;
                }
            }
            break;
        case 3:
            if (mpMorf->isStop()) {
                if (mSwitchBit != 0xff && dComIfGs_isSwitch(mSwitchBit, fopAcM_GetRoomNo(this)) == 0) {
                    dComIfGs_onSwitch(mSwitchBit, fopAcM_GetRoomNo(this));
                }
                fopAcM_delete(this);
                fopAcM_createDisappear(this, &current.pos, DUSK_IF_ELSE(static_cast<u8>(10.0f * actor_attr::enemy_size_multiplier(this)), 10), 1, 0xff);
            }
            break;
        default:
            break;
    }
    current.angle.y = shape_angle.y;
}

void daE_YM_c::damage_check() {
    if (field_0x6f4 != 0) {
        return;
    }

    if (mAction == ACT_RIVER || mAction == ACT_DOWN) {
        return;
    }

    mStts.Move();
    if (mSphCc.ChkTgHit()) {
        mAtInfo.mpCollider = mSphCc.GetTgHitObj();
        if (field_0x700 == 0 || !mAtInfo.mpCollider->ChkAtType(AT_TYPE_UNK)) {
            cc_at_check(this, &mAtInfo);
            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_UNK)) {
                field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            } else {
                field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
            }

            if (mAtInfo.mAttackPower <= 1) {
                field_0x6f4 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10 + KREG_S(8)), 10 + KREG_S(8));
            }

            if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG)) {
                if (mIsHide) {
                    setActionMode(ACT_WIND);
                }

                return; // necessary for proper dbg asm flow ctrl
            } else if (mType != 2) {
                mSound.startCreatureSound(Z2SE_EN_YM_DEATH, 0, -1);
                if (mAtInfo.mpCollider->ChkAtType(AT_TYPE_10000000)) {
                    dComIfGp_setHitMark(3, this, &eyePos, NULL, NULL, 0);
                }
                setActionMode(ACT_DOWN);
                executeDown();
            }
        }
    }
}

void daE_YM_c::executeWind() {
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -60.0f, 5.0f), cLib_chaseF(&speed.y, -60.0f, 5.0f));
    switch (mMode) {
        case 0:
            setAppear();
            setNormalCc();
#if TARGET_PC  // enemy attribute integration
            field_0x6f0 = actor_attr::enemy_sync_timer(this, cM_rndFX(5.0f) + 20.0f);
            field_0x6f2 = actor_attr::enemy_sync_timer(this, 0x28);
#else
            field_0x6f0 = cM_rndFX(5.0f) + 20.0f;
            field_0x6f2 = 0x28;
#endif
            mMode = 1;
            field_0x6dc = 0.0f;
        case 1:
            if (speed.y < 0.0f) {
                mAcch.SetGroundUpY(50.0f);
            }
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.z, -0x8000, 0x400), cLib_chaseAngleS(&shape_angle.z, -0x8000, 0x400));
            if (field_0x6f0) {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &field_0x6e6, 4000, 200), cLib_chaseAngleS(&field_0x6e6, 4000, 200));
                speed.y = 25.0f;
            } else {
                if (mAcch.ChkGroundHit()) {
                    mMode = 2;
                    speedF = 0.0f;
                    speed.y = 15.0f;
                }
            }
            break;
        case 2:
            if (mAcch.ChkGroundHit()) {
                speedF = speed.y = 0.0f;
                if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &field_0x6e6, 0, 200), cLib_chaseAngleS(&field_0x6e6, 0, 200))) {
                    mMode = 3;
#if TARGET_PC  // enemy attribute integration
                    field_0x6f0 = actor_attr::enemy_sync_timer(this, 0x32);
                    field_0x6f2 = actor_attr::enemy_sync_timer(this, (u8)(cM_rndF(5.0f) + 5.0f));
#else
                    field_0x6f0 = 0x32;
                    field_0x6f2 = (u8)(cM_rndF(5.0f) + 5.0f);
#endif
                    field_0x6e8 = shape_angle.y;
                    field_0x6a5 = 0;
                }
            }
            break;
        case 3:
            if (field_0x6f0 < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0xf), 0xf)) {
                shape_angle.z = cM_rndF(4096.0f) + 32768.0f;
                shape_angle.y = field_0x6e8 + cM_rndF(2048.0f);
            } else {
                if (field_0x6f2 == 0) {
                    field_0x6f2 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, (u8)(cM_rndF(5.0f) + 5.0f)), (u8)(cM_rndF(5.0f) + 5.0f));
                    field_0x6a5 = field_0x6a5 + 1 & 1;
                }
                if (field_0x6a5 == 0) {
                    shape_angle.z = cM_rndF(2048.0f) + 32768.0f;
                    shape_angle.y = field_0x6e8 + cM_rndF(2048.0f);
                }
            }
            if (field_0x6f0 == 0) {
                speedF = cM_rndF(5.0f) + 5.0f;
                speed.y = 15.0f;
                mMode = 4;
                mAcch.SetGroundUpY(0.0f);
                current.angle.y = shape_angle.y - 0x4000;
            }
            break;
        case 4:
            if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.z, 0, 0x1000), cLib_chaseAngleS(&shape_angle.z, 0, 0x1000)) && mAcch.ChkGroundHit()) {
                speedF = speed.y = 0.0f;
                setActionMode(ACT_ESCAPE);
            }
            break;
        default:
            break;
    }
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_angle_add(this, shape_angle.y , field_0x6e6);
#else
    shape_angle.y += field_0x6e6;
#endif
}

int daE_YM_c::getSurpriseType() {
    cXyz my_vec_0 = mPrevPos - current.pos;
    int ret;
    s16 dist_ang;
    if (my_vec_0.abs() > l_HIO.mMoveRange) {
        ret = 2;
    } else {
        dist_ang = cLib_distanceAngleS(mAngleToPlayer, shape_angle.y);
        ret = 1;
        if (dist_ang < 0x3000 && cM_rndF(10.0f) > 8.0f) {
            ret = 0;
        }
    }
    return ret;
}

void daE_YM_c::setSurpriseAway() {
    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
    mMode = 1;
    bckSet(0x10, 2, 3.0f, speedF / 12.0f);
}

void daE_YM_c::setGoHomeType() {
    s16 tgt_ang = cLib_targetAngleY(&current.pos, &mPrevPos);
    f32 rand_val = cM_rnd();
    field_0x6a8 = 1;
    if (field_0x6a8 == 0 && rand_val < 0.1f && mAcch.ChkGroundHit() && mAcch.ChkWallHit() == 0) {
        field_0x6a8 = 1;
        field_0x6a5 = 2;
        shape_angle.y = tgt_ang + cM_rndFX(4000.0f);
        mMode = 3;
    } else {
        s16 angle_diff = mAngleToPlayer - tgt_ang;
        if (angle_diff < 0) {
            field_0x6a5 = 2;
        } else {
            field_0x6a5 = 3;
        }

        if (field_0x6cb) {
            speedF = 25.0f;
        } else {
            speedF = l_HIO.mMoveSpeed;
        }

        setSurpriseAway();
    }
}

void daE_YM_c::executeSurprise() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    if (mType != 1) {
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -90.0f, 15.0f), cLib_chaseF(&speed.y, -90.0f, 15.0f));
    }

    cXyz my_vec_0 = mPrevPos - current.pos;
    if (daPy_getPlayerActorClass()->checkWolfLock(this) && mAcch.ChkGroundHit()) {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 5.0f);
        actor_attr::enemy_chase_action_angle(this, &shape_angle.y,mAngleToPlayer, 0x400);
#else
        cLib_chaseF(&speedF, 0.0f, 5.0f);
        cLib_chaseAngleS(&shape_angle.y,mAngleToPlayer, 0x400);
#endif
        return;
    }

    switch (mMode) {
        case 0: {
            if (field_0x6a1 == 1 && field_0x708 == 0 && field_0x6c9 == 0) {
                field_0x708 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x78), 0x78);
            }

            int surprise_type = getSurpriseType();
            if (surprise_type == 2 && field_0x6a9) {
                setGoHomeType();
            } else {
                field_0x6a5 = 0;
                field_0x6e4 = mAngleToPlayer + 0x8000;
                s16 sh_ang = player->shape_angle.y;
                s16 dist_ang = cLib_distanceAngleS(field_0x6e4, sh_ang);
                if (sh_ang + dist_ang == field_0x6e4) {
                    field_0x6a5 = 0;
                } else {
                    field_0x6a5 = 1;
                }

                if (field_0x6cb) {
                    speedF = 25.0f;
                    if (field_0x6a9 == 0) {
                        speedF = 28.0f;
                        field_0x6a9 = 1;
                    }
                } else {
                    speedF = 17.0f;
                    if (field_0x6a9 == 0) {
                        speedF = 20.0f;
                        field_0x6a9 = 1;
                    }
                }
                setSurpriseAway();
            }

            break;
        }

        case 1: {
            if (mAcch.ChkWallHit() && field_0x6a5 < 2) {
                setGoHomeType();
                return;
            }

            setMoveSound(0);
            if (field_0x6a5 == 0) {
                field_0x6e4 = mAngleToPlayer - 0x6000;
            } else if (field_0x6a5 == 1) {
                field_0x6e4 = mAngleToPlayer + 0x6000;
            } else {
                s16 sh_val = -0x2000;
                if (field_0x6a5 == 2) {
                    sh_val = 0x2000;
                }

                if (current.pos.absXZ(mPrevPos) < 100.0f) {
                    field_0x6e4 = sh_val + cLib_targetAngleY(&current.pos, &home.pos);
                } else {
                    s16 next_0x6e4 = sh_val + cLib_targetAngleY(&current.pos, &mPrevPos);
                    if (mAcch.ChkWallHit()) {
                        s16 wall_angle = mAcchCir.GetWallAngleY();
                        if (abs((s16)(next_0x6e4 - wall_angle)) > 0x6000) {
                            if ((s16)(cLib_targetAngleY(&current.pos, &mPrevPos) - wall_angle) > 0) {
                                next_0x6e4 = wall_angle + 0x4000;
                            } else {
                                next_0x6e4 = wall_angle - 0x4000;
                            }
                        }
                    }

                    field_0x6e4 = next_0x6e4;
                }
            }

            if (field_0x6a5 <= 1 && my_vec_0.abs() >= l_HIO.mMoveRange + 200.0f) {
                field_0x6f0 = 0;
            }

            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, field_0x6e4, 8, 0x1000, 0x100), cLib_addCalcAngleS(&shape_angle.y, field_0x6e4, 8, 0x1000, 0x100));
            if (field_0x6f0 == 0) {
                if (!field_0x6d4 && cM_rnd() < 0.3f) {
                    if (field_0x6a1 == 1) {
                        setActionMode(ACT_ELECTRIC);
                    } else if (field_0x6a1 == 0) {
                        if (daPy_getPlayerActorClass()->current.pos.abs(mPrevPos) <= l_HIO.mMoveRange + 200.0f) {
                            setActionMode(ACT_ATTACK);
                        }
                    }

                    return;
                }

                if (field_0x6c9 && mDistToPlayer < DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 300.0f), 300.0f)) {
                    setActionMode(ACT_ELECTRIC);
                } else {
                    setActionMode(ACT_WAIT);
                    speedF = 0.0f;
                    field_0x6f6 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5), 5);
                }
            }

            break;
        }

        case 2: {
            field_0x6a5 = getSurpriseType();
            if (mAcch.ChkWallHit()) {
                field_0x6a5 = 2;
            }

            if (field_0x6a5 == 0) {
                shape_angle.y = mAngleToPlayer + cM_rndFX(4000.0f);
            } else if (field_0x6a5 == 1) {
                shape_angle.y = mAngleToPlayer + 0x8000 + cM_rndFX(4000.0f);
            } else {
                shape_angle.y = cLib_targetAngleY(&current.pos, &mPrevPos) + cM_rndFX(4000.0f);
            }
        }
        // fallthrough
        case 3: {
            mMode = 5;
            speedF = 0.0f;
            bckSet(10, 0, 10.0f, 2.0f);
            break;
        }

        case 5: {
            if (mpMorf->isStop()) {
                bckSet(0xb, 0, 5.0f, 1.0f);
                mSound.startCreatureSound(Z2SE_EN_YM_JUMP, 0, -1);
                mMode = 6;
                switch (field_0x6a5 & 3) {
                    case 0:
                        speedF = (mDistToPlayer + 650.0f) / 12.0f;
                        speed.y = 90.0f;
                        break;
                    case 1:
                        speedF = (650.0f - mDistToPlayer) / 10.0f;
                        speed.y = 75.0f;
                        break;
                    case 2:
                        speedF = my_vec_0.abs() / 10.0f;
                        speed.y = 75.0f;
                        break;
                }

                if (speedF < 35.0f) {
                    speedF = 35.0f;
                } else if (speedF >= 50.0f) {
                    speedF = 50.0f;
                }

                if (speed.y < 60.0f) {
                    speed.y = 60.0f;
                } else if (speed.y >= 90.0f) {
                    speed.y = 90.0f;
                }
            }
            break;
        }

        case 6: {
            if (mAcch.ChkGroundHit()) {
                mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
                bckSet(0xc, 0, 0.0f, 1.0f);
                mMode = 7;
                speedF = 0.0f;
            }

            break;
        }

        case 7: {
            int _; // to force b in dbg
            if (mpMorf->isStop() && field_0x6f0 == 0) {
                setActionMode(ACT_WAIT);
                field_0x6f6 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0xf), 0xf);
                field_0x6a7 = 0;
            }

            break;
        }
    }

    current.angle.y = shape_angle.y;
}

void daE_YM_c::executeBack() {
    if (mType != 1) {
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -60.0f, 5.0f), cLib_chaseF(&speed.y, -60.0f, 5.0f));
    }
    switch (mMode) {
        case 0:
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x32), 0x32);
            bckSet(0x10, 2, 5.0f, 1.0f);
            mMode = 1;
            speedF = -5.0f;
            break;
        case 1:
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, mAngleToPlayer, 0x400), cLib_chaseAngleS(&shape_angle.y, mAngleToPlayer, 0x400));
            current.angle.y = shape_angle.y;
            setMoveSound(0);
            if (field_0x6f0 == 0) {
                speedF = 0.0f;
                setActionMode(ACT_WAIT);
            }
            break;
        default:
            break;
    }
}

void daE_YM_c::executeFall() {
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -90.0f, 10.0f), cLib_chaseF(&speed.y, -90.0f, 10.0f));
    switch (mMode) {
        case 0:
            mSound.startCreatureSound(Z2SE_EN_YM_FALL, 0, -1);
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0xc), 0xc);
            mMode = 1;
        case 1:
            if (field_0x6f0 == 0) {
                mType = 0;
                bckSet(0xb, 0, 10.0f, 2.0f);
                mSound.startCreatureSound(Z2SE_EN_YM_JUMP, 0, -1);
                speedF = 30.0f;
                speed.y = 50.0f;
                mMode = 2;
#if TARGET_PC  // enemy attribute integration
                mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
                mAcchCir.SetWall(40.0f, 60.0f);
#endif
                mAcch.SetGroundUpY(0.0f);
                shape_angle.x = field_0x668.x;
                shape_angle.y = field_0x668.z;
                current.angle.y = shape_angle.y + 0x8000;
                if ((s16)cLib_distanceAngleS(current.angle.y, mAngleToPlayer) < 0x1000) {
                    if ((s16)cLib_distanceAngleS(current.angle.y, mAngleToPlayer + 0x1000) < 0x1000) {
                        current.angle.y = shape_angle.y - 0x7000;
                    } else {
                        current.angle.y = shape_angle.y + 0x7000;
                    }
                }
                field_0x668.x = field_0x668.z = 0;
                setElecEffect1();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
            }
            break;
        case 2:
#if TARGET_PC  // enemy attribute integration
            field_0x6fe = actor_attr::enemy_sync_timer(this, 0x1e);
            actor_attr::enemy_chase_action_angle(this, &shape_angle.y, current.angle.y, 0x1000);
            actor_attr::enemy_chase_action_angle(this, &shape_angle.x, 0, 0x800);
#else
            field_0x6fe = 0x1e;
            cLib_chaseAngleS(&shape_angle.y, current.angle.y, 0x1000);
            cLib_chaseAngleS(&shape_angle.x, 0, 0x800);
#endif
            setElecEffect2();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
            mSphCc.OnAtSetBit();
            if (mAcch.ChkGroundHit() == 0) {
                break;
            }
            mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
            speedF = 0.0f;
            setActionMode(ACT_WAIT);
            bckSet(0xf, 2, 2.0f, 1.0f);
            shape_angle.y = current.angle.y;
            shape_angle.x = 0;
            mPrevPos = current.pos;
            break;
        default:
            break;
    }
}

void daE_YM_c::executeAttack() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz my_vec_0 = player->current.pos - mPrevPos;
    if (mType != 1) {
        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -100.0f, 4.0f), cLib_chaseF(&speed.y, -100.0f, 4.0f));
    }
    if (my_vec_0.abs() <= l_HIO.mMoveRange + 200.0f) {
        field_0x688 = mDistToPlayer;
        field_0x6e4 = mAngleToPlayer;
    }
    switch (mMode) {
        case 0:
            bckSet(0xe, 2, 10.0f, 1.5f);
            speedF = 0.0f;
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x50), 0x50);
            mMode = 1;
            field_0x6a5 = 0;
            break;
        case 1:
            if (field_0x6d4 && (dComIfGp_getAttention()->LockonTruth() == 0 || dComIfGp_getAttention()->LockonTarget(0) != this)) {
                if (checkSurpriseNear()) {
                    return;
                }
            }
            setTurnSound();
            if (cLib_distanceAngleS(shape_angle.y, field_0x6e4) < 0x200) {
                if (checkBck("E_YM", 0xf) == 0) {
                    bckSet(0xf, 2, 5.0f, 1.0f);
                }
                field_0x6a5 += (u8) 1;
            } else {
                if (checkBck("E_YM", 0xe) == 0) {
                    bckSet(0xe, 2, 10.0f, 1.5f);
                }
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, field_0x6e4, 0x200), cLib_chaseAngleS(&shape_angle.y, field_0x6e4, 0x200));
                field_0x6a5 = 0;
            }
            if (field_0x6f0 == 0 || field_0x6a5 >= 0x1e || my_vec_0.abs() > l_HIO.mMoveRange + 200.0f) {
                bckSet(0xd, 0, 10.0f, 2.0f);
                mMode = 2;
            }
            break;
        case 2:
            setElecEffect1();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
            if (mpMorf->isStop()) {
                bckSet(10, 0, 10.0f, 2.0f);
                mMode = 3;
            }
            break;
        case 3:
            setElecEffect2();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            if (mpMorf->isStop()) {
                bckSet(0xb, 0, 5.0f, 1.0f);
                mSound.startCreatureSound(Z2SE_EN_YM_JUMP, 0, -1);
                speed.y = 35.0f;
                speedF = (field_0x688 * 4.0f) / 90.0f;
                mMode = 4;
            }
            break;
        case 4:
            setElecEffect2();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
            mSphCc.OnAtSetBit();
            if (mAcch.ChkGroundHit()) {
                mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
                bckSet(0xc, 0, 0.0f, 1.0f);
                speedF = 0.0f;
                mMode = 5;
            }
            break;
        case 5:
            if (mpMorf->isStop()) {
                setActionMode(ACT_WAIT);
#if TARGET_PC  // enemy attribute integration
                field_0x6f6 = actor_attr::enemy_sync_timer(this, 0xf);
                field_0x6fc = actor_attr::enemy_sync_timer(this, 0x32);
#else
                field_0x6f6 = 0xf;
                field_0x6fc = 0x32;
#endif
            }
            break;
        default:
            break;
    }
    current.angle.y = shape_angle.y;
}

bool daE_YM_c::checkAttackEnd() {
    if (daPy_getPlayerActorClass()->checkClimbMove() == 0) {
        setActionMode(ACT_MOVE);
        speedF = 0.0f;
        return 1;
    }

    cXyz my_vec_0 = mPrevPos - current.pos;
    if (my_vec_0.abs() > l_HIO.mMoveRange + 50.0f) {
        setActionMode(ACT_WAIT);
        speedF = 0.0f;
        return 1;
    } else {
        return 0;
    }
}

bool daE_YM_c::setAttackMotion() {
    if (mType != 1) {
        return false;
    }

    if (mSphCc.ChkAtHit()) {
        cCcD_Obj* at_hit_obj = mSphCc.GetAtHitObj();
        if (fopAcM_GetName(dCc_GetAc(at_hit_obj->GetAc())) == fpcNm_ALINK_e) {
            if (mAction != ACT_ATTACK_WALL) {
                setActionMode(ACT_ATTACK_WALL);
            } else if (mMode == 4) {
                return false;
            }

            speedF = 0.0f;
            mMode = 4;
            bckSet(5, 0, 3.0f, 1.0f);
            mSound.startCreatureSound(Z2SE_EN_YM_ATTACK, 0, -1);
            return true;
        }
    }

    return false;
}

void daE_YM_c::executeAttackWall() {
    field_0x6fa = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz my_vec_0;
    cXyz my_vec_1;
    cXyz my_vec_2;
    if (!checkWolfBark() && !checkWallCrash() && !setAttackMotion()) {
        switch (mMode) {
            case 0:
                checkWall();
                if (field_0x6a5 == 0) {
                    my_vec_2 = player->current.pos;
                    my_vec_2.y += 100.0f;
                    my_vec_0 = my_vec_2 - current.pos;
                } else {
                    my_vec_0 = mPrevPos - current.pos;
                }

                mDoMtx_stack_c::ZrotS(-field_0x668.z);
                mDoMtx_stack_c::XrotM(-field_0x668.x);
                mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
                field_0x6e4 = cM_atan2s(my_vec_1.x, my_vec_1.z);
                if (checkBeforeBg(field_0x6e4)) {
                    setActionMode(ACT_MOVE);
                    speedF = 0.0f;
                    return;
                }

                bckSet(0xe, 2, 10.0f, 1.5f);
                speedF = 0.0f;
                mMode = 1;
                break;

            case 1:
                if (checkAttackEnd()) {
                    return;
                }
                setTurnSound();
                if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, field_0x6e4, 0x400), cLib_chaseAngleS(&shape_angle.y, field_0x6e4, 0x400))) {
                    bckSet(0xf, 2, 5.0f, 1.0f);
                    mMode = 2;
                }
                break;

            case 2:
                if (checkAttackEnd()) {
                    return;
                }
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_WAIT, 0, -1);
                if (field_0x6f0 == 0) {
                    bckSet(0x10, 2, 5.0f, 1.0f);
                    mMode = 3;
                    speedF = 20.0f;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(20.0f) + 30.0f), cM_rndF(20.0f) + 30.0f);
                    mSound.startCreatureSound(Z2SE_EN_YM_FIND, 0, -1);
                }
                break;

            case 3: {
                setMoveSound(0);
                u8 do_stuff = false;
                if (field_0x6f0 == 0 || checkBeforeBg(field_0x6e4)) {
                    do_stuff = true;
                } else {
                    my_vec_0 = mPrevPos - current.pos;
                    if (field_0x6a5 == 0) {
                        if (my_vec_0.abs() > l_HIO.mMoveRange + 100.0f) {
                            do_stuff = true;
                        }
                    } else if (my_vec_0.abs() < 30.0f) {
                        do_stuff = true;
                    }
                }

                if (do_stuff) {
                    speedF = 0.0f;
                    mMode = 4;
                    bckSet(5, 0, 3.0f, 1.0f);
                    mSound.startCreatureSound(Z2SE_EN_YM_ATTACK, 0, -1);
                }
                break;
            }

            case 4:
                if (mpMorf->isStop()) {
                    setActionMode(ACT_WAIT);
                    return;
                }
                break;
        }

        current.angle.y = shape_angle.y;
    }
}

void daE_YM_c::executeDefense() {
    switch (mMode) {
        case 0:
            mSound.startCreatureSound(Z2SE_EN_YM_DEFENCE, 0, -1);
            if (mType != 1) {
                bckSet(7, 2, 3.0f, 1.0f);
                mMode = 1;
                shape_angle.y = mAngleToPlayer;
                current.angle.y = mAngleToPlayer + 0x8000;
                if (mAcch.ChkGroundHit()) {
                    speedF = 15.0f;
                    field_0x6a5 = 1;
                } else {
                    speedF = 15.0f;
                    speed.y = 25.0f;
                    field_0x6a5 = 0;
                }
            } else {
                bckSet(7, 0, 3.0f, 1.0f);
                mMode = 2;
                speedF = speed.y = 0.0f;
            }
            break;
        case 1:
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, -60.0f, 5.0f), cLib_chaseF(&speed.y, -60.0f, 5.0f));
            if (mAcch.ChkGroundHit()) {
                if (field_0x6a5 == 0) {
                    mSound.startCreatureSound(Z2SE_EN_YM_LAND, 0, -1);
                    field_0x6a5 = 1;
                }
                if (speedF >= 3.0f) {
                    fopAcM_effSmokeSet2(&field_0xad0, &field_0xad4, &current.pos, NULL, DUSK_IF_ELSE(1.5f * actor_attr::enemy_size_multiplier(this), 1.5f), &tevStr);
                }
                speed.y = 0.0f;
                if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 1.0f), cLib_chaseF(&speedF, 0.0f, 1.0f))) {
                    setActionMode(ACT_WAIT);
                }
            }
            break;
        case 2:
            if (mpMorf->isStop()) {
                setActionMode(ACT_WAIT);
            }
            break;
        default:
            break;
    }
}

bool daE_YM_c::checkFlyTerritory() {
    if (mPrevPos.absXZ(current.pos) > 300.0f) {
        return false;
    }

    if (std::abs(mPrevPos.y - current.pos.y) > 150.0f) {
        return false;
    }

    return true;
}

void daE_YM_c::initFly() {
    attention_info.distances[fopAc_attn_BATTLE_e] = 46;
    gravity = 0.0f;
    field_0x6e4 = 0;
    if (mTagPosP != NULL) {
        mPrevPos = mTagPos;
    } else {
        mPrevPos = home.pos;
    }
    mType = 3;
    shape_angle.x = 0;
}

void daE_YM_c::executeFly() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz player_pos(player->current.pos);
    cXyz my_vec_1 = current.pos - player_pos;
    cXyz my_vec_2 = current.pos - mPrevPos;
    cXyz my_vec_3 = player_pos - mPrevPos;
    mSound.startCreatureSoundLevel(Z2SE_EN_YM_FLY, 0, -1);
    s16 tgt_ang_y;
    switch (mMode) {
        case 0:
            field_0x6f2 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
            bckSetFly(0xd, 0, 0.0f, 1.0f);
            field_0x6a5 = 0;
            mMode = 1;

        case 1:
            if (mType == 1) {
                setElecEffect1();
            }
            if (mpMorf->checkFrame(6.0f)) {
                field_0x6cc = 1;
            }
            if (mpMorf->checkFrame(14.0f)) {
                if (mType != 1) {
                    mMode = 7;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
                    initFly();
                } else {
                    field_0x70e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x28), 0x28);
                    mMode = 10;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x5a), 0x5a);
                    speed.y = -10.0f;
                    initFly();
                    current.angle.y = cLib_targetAngleY(&current.pos, &mPrevPos);
                }
            }
            break;

        case 2:
            field_0x6f2 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
            bckSetFly(0xe, 2, 5.0f, 1.0f);
            initFly();
            field_0x6a5 = 0;
            mMode = 3;
            break;

        case 3: {
            mAcch.SetGroundUpY(70.0f);
            if (field_0x6a5) {
                if (field_0x70c == 0 && cM_rnd() < 0.4f) {
                    field_0x70c = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x5a), 0x5a);
                    field_0x67c = mPrevPos;
                    if (my_vec_3.absXZ() >= 400.0f) {
                        if (checkFlyTerritory()) {
                            field_0x67c.x += cM_rndFX(300.0f);
                            field_0x67c.z += cM_rndFX(300.0f);
                        }
                    } else {
                        if (checkFlyTerritory()) {
                            tgt_ang_y = cLib_targetAngleY(&current.pos, &player_pos) + 0x8000;
                        } else {
                            tgt_ang_y = cLib_targetAngleY(&current.pos, &mPrevPos);
                        }

                        field_0x67c.x += cM_ssin(tgt_ang_y) * 300.0f;
                        field_0x67c.z += cM_scos(tgt_ang_y) * 300.0f;
                    }

                    field_0x67c.y = player_pos.y + cM_rndF(50.0f);
                    if (std::abs(field_0x67c.y - mPrevPos.y) > 300.0f) {
                        field_0x67c.y = mPrevPos.y + cM_rndF(75.0f);
                    }
                    current.angle.y = cLib_targetAngleY(&current.pos, &field_0x67c);
                    current.angle.x = cLib_targetAngleX(&current.pos, &field_0x67c);
                    speedF = l_HIO.mFlyMoveSpeed * cM_scos(s16(current.angle.x));
                    speed.y = l_HIO.mFlyMoveSpeed * cM_ssin(s16(current.angle.x));
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x14), 0x14);
                    mMode = 6;
                    break;
                }
            } else {
                field_0x6a5 += (u8) 1;
            }
            if (my_vec_3.absXZ() >= 400.0f) {
                field_0x67c = mPrevPos;
                if (checkFlyTerritory()) {
                    field_0x67c.x += cM_rndFX(300.0f);
                    if (mPrevPos.y < current.pos.y) {
                        field_0x67c.y -= cM_rndF(75.0f);
                    } else {
                        field_0x67c.y += cM_rndF(75.0f);
                    }
                    field_0x67c.z += cM_rndFX(300.0f);
                }
                current.angle.y = cLib_targetAngleY(&current.pos, &field_0x67c);
                current.angle.x = cLib_targetAngleX(&current.pos, &field_0x67c);
            } else {
                if (checkFlyTerritory()) {
                    current.angle.y = cLib_targetAngleY(&current.pos, &player_pos) + 0x8000;
                    current.angle.x = cM_rndF(4096.0f);
                } else {
                    current.angle.y = cLib_targetAngleY(&current.pos, &mPrevPos);
                    current.angle.x = cLib_targetAngleX(&current.pos, &mPrevPos);
                    if ((s16)(cLib_targetAngleY(&current.pos, &player_pos) - current.angle.y) < 0) {
                        ANGLE_ADD(current.angle.y, 0x3000);
                    } else {
                        ANGLE_SUB(current.angle.y, 0x3000);
                    }
                }
            }

            speed.y = l_HIO.mFlyMoveSpeed * cM_ssin(current.angle.x);
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x14), 0x14);
            mMode = 4;
        }
        // fallthrough
        case 4:
        case 6: {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, l_HIO.mFlyMoveSpeed * std::abs(cM_scos(current.angle.x)), 1.0f), cLib_chaseF(&speedF, l_HIO.mFlyMoveSpeed * std::abs(cM_scos(current.angle.x)), 1.0f));
            if (mpMorf->isStop() && checkBck("E_TM", 0xd)) {
                bckSetFly(0xc, 2, 0.0f, 1.0f);
            }
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &player_pos), 0x400), cLib_chaseAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &player_pos), 0x400));
            s16 ang_diff = current.angle.y - shape_angle.y;
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.z, cM_ssin(ang_diff) * -3072.0f * std::abs(cM_scos(current.angle.x)), 0x100), cLib_chaseAngleS(&shape_angle.z, cM_ssin(ang_diff) * -3072.0f * std::abs(cM_scos(current.angle.x)), 0x100));
            if (std::abs(cM_scos(current.angle.x)) >= std::abs(cM_ssin(current.angle.x))) {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, cM_scos(ang_diff) * 5120.0f * std::abs(cM_scos(current.angle.x)), 0x400), cLib_chaseAngleS(&shape_angle.x, cM_scos(ang_diff) * 5120.0f * std::abs(cM_scos(current.angle.x)), 0x400));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, std::abs(cM_ssin(current.angle.x)) * 5120.0f, 0x400), cLib_chaseAngleS(&shape_angle.x, std::abs(cM_ssin(current.angle.x)) * 5120.0f, 0x400));
            }
            int do_stuff = false;
            if (mMode == 4) {
                if (field_0x6f0 == 0) {
                    do_stuff = true;
                }
            } else if (std::abs(field_0x67c.y - current.pos.y) < 100.0f || mAcch.ChkGroundHit()) {
                do_stuff = true;
                speed.y = 0.0f;
            }
            if (do_stuff) {
                mMode = 5;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
                bckSetFly(0xe, 2, 5.0f, 1.0f);
            }
            break;
        }

        case 7: {
            if (mpMorf->isStop() && checkBck("E_TM", 0xd)) {
                bckSetFly(0xc, 2, 0.0f, 1.0f);
            }
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_float(this, &speedF, 20.0f, 3.0f);
            actor_attr::enemy_chase_action_float(this, &speed.y, 15.0f, 3.0f);
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, mAngleToPlayer + 0x8000, 8, 0x1000, 0x100);
#else
            cLib_chaseF(&speedF, 20.0f, 3.0f);
            cLib_chaseF(&speed.y, 15.0f, 3.0f);
            cLib_addCalcAngleS(&shape_angle.y, mAngleToPlayer + 0x8000, 8, 0x1000, 0x100);
#endif
            current.angle.y = shape_angle.y;
            if (field_0x6f0 == 0) {
                mMode = 5;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
                speedF = speed.y = 0.0f;
                bckSetFly(0xe, 2, 5.0f, 1.0f);
            }
            break;
        }

        case 5: {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &player_pos), 0x800);
            actor_attr::enemy_chase_action_angle(this, &shape_angle.x, 0, 0x400);
            actor_attr::enemy_chase_action_angle(this, &shape_angle.z, 0, 0x400);
            actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 1.0f);
            actor_attr::enemy_chase_action_float(this, &speed.y, 0.0f, 3.0f);
#else
            cLib_chaseAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &player_pos), 0x800);
            cLib_chaseAngleS(&shape_angle.x, 0, 0x400);
            cLib_chaseAngleS(&shape_angle.z, 0, 0x400);
            cLib_chaseF(&speedF, 0.0f, 1.0f);
            cLib_chaseF(&speed.y, 0.0f, 3.0f);
#endif
            if (field_0x6f0 == 0 || my_vec_1.absXZ() < 50.0f
                || (daPy_getPlayerActorClass()->checkWolfBark() && my_vec_1.absXZ() < 100.0f)) {
                speedF = speed.y = 0.0f;
                mMode = 3;
                return;
            }

            if (field_0x6f2 == 0 && my_vec_1.absXZ() < 500.0f
                && my_vec_3.absXZ() < l_HIO.mMoveRange + 150.0f && std::abs(my_vec_2.y) < 150.0f) {
                speedF = speed.y = 0.0f;
                setActionMode(ACT_FLY_ATTACK);
                return;
            }

            break;
        }

        case 10: {
            int _; // to force b in dbg asm
            if (field_0x70e) {
                setElecEffect2();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
                field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
                mSphCc.OnAtSetBit();
            }

            if (field_0x70e) {
                if (mpMorf->isStop() && checkBck("E_TM", 0xd)) {
                    bckSetFly(0xc, 2, 0.0f, 1.0f);
                }
                if (player_pos.y + 100.0f > current.pos.y) {
                    speed.y -= DUSK_IF_ELSE(actor_attr::enemy_move_acceleration(this, 1.0f), 1.0f);
                    if (speed.y < -15.0f) {
                        speed.y = -15.0f;
                    }
                } else {
                    DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speed.y, 0.0f, 1.0f), cLib_chaseF(&speed.y, 0.0f, 1.0f));
                }

                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 30.0f - std::abs(speed.y), 1.0f), cLib_chaseF(&speedF, 30.0f - std::abs(speed.y), 1.0f));
            } else {
                tgt_ang_y = cLib_targetAngleY(&current.pos, &mPrevPos);
                if (s16(cLib_targetAngleY(&current.pos, &player_pos) - tgt_ang_y) < 0) {
                    ANGLE_ADD(tgt_ang_y, 0x3000);
                } else {
                    ANGLE_SUB(tgt_ang_y, 0x3000);
                }

                DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &current.angle.y, tgt_ang_y, 0x400), cLib_chaseAngleS(&current.angle.y, tgt_ang_y, 0x400));
                current.angle.x = cLib_targetAngleX(&current.pos, &mPrevPos);
                speed.y = l_HIO.mFlyMoveSpeed * cM_ssin(current.angle.x);
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, l_HIO.mFlyMoveSpeed * cM_scos(current.angle.x), 1.0f), cLib_chaseF(&speedF, l_HIO.mFlyMoveSpeed * cM_scos(current.angle.x), 1.0f));
            }

            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, current.angle.y, 0x400), cLib_chaseAngleS(&shape_angle.y, current.angle.y, 0x400));
            if (field_0x6f0 == 0) {
                mMode = 3;
            }

            break;
        }
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_angle_add(this, field_0x6e4, 0x800);
    current.pos.y += actor_attr::enemy_action_step(this, cM_ssin(field_0x6e4) * 3.0f);
#else
    ANGLE_ADD(field_0x6e4, 0x800);
    current.pos.y += cM_ssin(field_0x6e4) * 3.0f;
#endif
}

void daE_YM_c::setInclination() {
    s16 tan = cM_atan2s(speedF, speed.y) - 0x4000;
    if (tan < -6000) {
        tan = -6000;
    }
    if (tan > 6000) {
        tan = 6000;
    }
    DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, tan, 0x0180), cLib_chaseAngleS(&shape_angle.x, tan, 0x0180));
}

void daE_YM_c::executeFlyAttack() {
    cXyz my_vec_0;
    mSound.startCreatureSoundLevel(Z2SE_EN_YM_FLY, 0, -1);
    switch (mMode) {
        case 0: {
            field_0x6e4 = 0;
            bckSetFly(5, 0, 0.0f, 1.0f);
            mMode = 1;
            fopAc_ac_c* ply = dComIfGp_getPlayer(0);
            field_0x67c = ply->current.pos;
            field_0x67c.y += 50.0f;
            break;
        }

        case 1:
            setElecEffect1();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_angle(this, &shape_angle.x, 0, 0x400);
            actor_attr::enemy_chase_action_angle(this, &shape_angle.z, 0, 0x400);
#else
            cLib_chaseAngleS(&shape_angle.x, 0, 0x400);
            cLib_chaseAngleS(&shape_angle.z, 0, 0x400);
#endif
            if (mpMorf->isStop()) {
                bckSetFly(6, 2, 0.0f, 1.0f);
                mMode = 2;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 300), 300);
            }
            break;

        case 2: {
            setElecEffect2();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_FLY_ATTACK, 0, -1);
            field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
            mSphCc.OnAtSetBit();
            my_vec_0 = field_0x67c - current.pos;
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x67c), 0x800), cLib_chaseAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &field_0x67c), 0x800));
            current.angle.y = shape_angle.y;
            s16 tan_val = (s16) cM_atan2s(my_vec_0.absXZ(), my_vec_0.y);
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_float(this, &speed.y, l_HIO.mFlyAttackSpeed * cM_scos(tan_val), 5.0f);
            actor_attr::enemy_chase_action_float(this, &speedF, l_HIO.mFlyAttackSpeed * cM_ssin(tan_val), 3.0f);
#else
            cLib_chaseF(&speed.y, l_HIO.mFlyAttackSpeed * cM_scos(tan_val), 5.0f);
            cLib_chaseF(&speedF, l_HIO.mFlyAttackSpeed * cM_ssin(tan_val), 3.0f);
#endif
            if (mSphCc.ChkAtHit()) {
                fopAc_ac_c* hit_actor = dCc_GetAc(mSphCc.GetAtHitObj()->GetAc());
                if (fopAcM_GetName(hit_actor) == fpcNm_ALINK_e) {
                    bckSetFly(7, 0, 0.0f, 1.0f);
                    speedF = speed.y = 0.0f;
                    mMode = 4;
                    current.angle.y = cLib_targetAngleY(&current.pos, &mPrevPos);
                    shape_angle.x = 0;
                    return;
                }
            }
            if (my_vec_0.abs() < 100.0f || field_0x6f0 == 0) {
                mMode = 5;
                field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
                bckSetFly(0xe, 2, 5.0f, 1.0f);
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
            }
            setInclination();
            break;
        }

        case 4:
        case 5: {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0x6e4, 0x800);
            current.pos.y += actor_attr::enemy_action_step(this, cM_ssin(field_0x6e4) * 3.0f);
            actor_attr::enemy_chase_action_float(this, &speed.y, 0.0f, 3.0f);
            actor_attr::enemy_chase_action_float(this, &speedF, 0.0f, 3.0f);
#else
            ANGLE_ADD(field_0x6e4, 0x800);
            current.pos.y += cM_ssin(field_0x6e4) * 3.0f;
            cLib_chaseF(&speed.y, 0.0f, 3.0f);
            cLib_chaseF(&speedF, 0.0f, 3.0f);
#endif
            int do_stuff = 0;
            if (mMode == 4) {
                if (mpMorf->isStop()) {
                    do_stuff = true;
                }
            } else if (field_0x6f0 == 0) {
                do_stuff = true;
            }
            if (do_stuff) {
                mMode = 3;
                bckSetFly(0xe, 2, 5.0f, 1.0f);
                speed.y = speedF = 0.0f;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, cM_rndF(10.0f) + 30.0f), cM_rndF(10.0f) + 30.0f);
            }
            setInclination();
            break;
        }

        case 3: {
            s16 mang = mAngleToPlayer;
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_angle(this, &current.angle.y, mang + 0x8000, 4, 0x1000, 0x100);
            actor_attr::enemy_chase_action_angle(this, &shape_angle.y, mang, 0x400);
            actor_attr::enemy_chase_action_float(this, &speed.y, 10.0f, 3.0f);
            actor_attr::enemy_chase_action_float(this, &speedF, 10.0f, 3.0f);
#else
            cLib_addCalcAngleS(&current.angle.y, mang + 0x8000, 4, 0x1000, 0x100);
            cLib_chaseAngleS(&shape_angle.y, mang, 0x400);
            cLib_chaseF(&speed.y, 10.0f, 3.0f);
            cLib_chaseF(&speedF, 10.0f, 3.0f);
#endif
            if (field_0x6f0 == 0) {
                setActionMode(ACT_FLY);
                mMode = 2;
            }
            s16 tan = cM_atan2s(speedF, speed.y) - 0x4000;
            if (tan < -6000) {
                tan = -6000;
            }
            if (tan > 6000) {
                tan = 6000;
            }
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.x, tan, 0x180), cLib_chaseAngleS(&shape_angle.x, tan, 0x180));
            setInclination();
            break;
        }
    }
}

void daE_YM_c::setNextPathPoint() {
    cXyz my_vec_0;
    ++mCurrentPntNo;
    if (mCurrentPntNo >= mpPath->m_num) {
        mCurrentPntNo = 0;
    }
    my_vec_0 = dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
    shape_angle.y = current.angle.y = cLib_targetAngleY(&current.pos, &my_vec_0);
    mMode = 2;
    bckSet(0x10, 2, 5.0f, 1.0f);
}

bool daE_YM_c::checkRailDig() {
    daPy_py_c* player = daPy_getPlayerActorClass();
    cXyz my_vec_0;
    if (player->checkWolfDig()) {
        my_vec_0 = player->getLeftHandPos() - current.pos;
        if (my_vec_0.abs() < 200.0f) {
            field_0x714 &= ~fopAc_AttnFlag_ETC_e;
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x14), 0x14);
            mMode = 3;
#if TARGET_PC  // enemy attribute integration
            mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
            mAcchCir.SetWall(40.0f, 60.0f);
#endif
            fopAcM_OnStatus(this, fopAcStts_UNK_0x4000_e);
            return true;
        }
    }

    return false;
}

void daE_YM_c::executeRail() {
    daPy_py_c* player = daPy_getPlayerActorClass();;
    cXyz my_vec_0;
    switch (mMode) {
        case 0: {
            mIsHide = 1;
            field_0x6a6 = 4;
            field_0x714 |= fopAc_AttnFlag_BATTLE_e;
            field_0x6dc = -70.0f;
            dPnt* point_p = dPath_GetPnt(mpPath, mCurrentPntNo);
            current.pos = old.pos = point_p->m_position;
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
            field_0x714 |= fopAc_AttnFlag_ETC_e;
            mSphCc.SetCoSPrm(0x145);
            mSphCc.SetTgType(0x10000);
            setNextPathPoint();
        }
        // fallthrough
        case 1: {
            if (!checkRailDig()) {
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_WAIT, 0, -1);
                if (player->checkWolfDig() == 0 && field_0x6f0 == 0) {
                    setNextPathPoint();
                }
            }
            break;
        }

        case 2: {
            if (!checkRailDig()) {
                speedF = 9.0f;
                setMoveSound(1);
                my_vec_0 = dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
                if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_pos(this, &current.pos, my_vec_0, speedF), cLib_chasePos(&current.pos, my_vec_0, speedF)) || current.pos.absXZ(my_vec_0) < 10.0f) {
                    mMode = 1;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 60), 60);
                    bckSet(0xf, 2, 5.0f, 1.0f);
                }
                speedF = 0.0f;
            }
            break;
        }

        case 3: {
            if (field_0x6f0 == 0) {
                setAppear();
                mPrevPos = current.pos;
                setActionMode(ACT_ESCAPE);
#if TARGET_PC  // enemy attribute integration
                field_0x704 = actor_attr::enemy_sync_timer(this, 0x1c2);
                field_0x70e = actor_attr::enemy_sync_timer(this, 0x5a);
#else
                field_0x704 = 0x1c2;
                field_0x70e = 0x5a;
#endif
                if (mTagPosP != NULL) {
                    mType = 0;
                }
            }
            break;
        }
    }
}

void daE_YM_c::executeBackRail() {
    static f32 YM_DIG_POS_Y[] = {-15.0f, -30.0f, -70.0f};

    cXyz my_vec_0 = (Vec)dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
    switch (mMode) {
        case 0: {
            if (current.pos.abs(my_vec_0) < 150.0f) {
                if ((s16)cLib_distanceAngleS(shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0)) >= 0x2000) {
                    ++mCurrentPntNo;
                    if (mCurrentPntNo >= mpPath->m_num) {
                        mCurrentPntNo = 0;
                    }
                    my_vec_0 = dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
                }
            }

            bckSet(8, 2, 5.0f, 1.0f);
            mIsHide = 1;
            field_0x6a5 = 0;
            field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 7), 7);
            mMode = 1;
#if TARGET_PC  // enemy attribute integration
            mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
            mAcchCir.SetWall(40.0f, 60.0f);
#endif
        }
        // fallthrough
        case 1: {
            setDigEffect();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_DIG, 0, -1);
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_chase_action_float(this, &field_0x6dc, YM_DIG_POS_Y[field_0x6a5], 3.0f);
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0), 8, 0x2000, 0x100);
#else
            cLib_chaseF(&field_0x6dc, YM_DIG_POS_Y[field_0x6a5], 3.0f);
            cLib_addCalcAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0), 8, 0x2000, 0x100);
#endif
            current.angle.y = shape_angle.y;
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 5.0f, 1.0f), cLib_chaseF(&speedF, 5.0f, 1.0f));
            if (field_0x6f0 == 0) {
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                mMode = 2;
                speedF = 0.0f;
            }

            break;
        }

        case 2: {
            setDigEffect();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_DIG, 0, -1);
            if (field_0x6f0 == 0) {
                ++field_0x6a5;
                if (field_0x6a5 >= 3) {
                    mMode = 3;
                    field_0x6a5 = 0;
#if TARGET_PC  // enemy attribute integration
                    // The three visible dig stages are complete.  On a
                    // non-vanilla action clock, commit the already-requested
                    // buried offset so the moving-under-the-ground state
                    // cannot remain visibly stuck in the digging pose.
                    if (actor_attr::enemy_action_time_speed(this) != 1.0f) {
                        field_0x6dc = -70.0f;
                    }
#endif
                    mSphCc.SetCoSPrm(0x145);
                    mSphCc.SetTgType(0x10000);
                    field_0x714 &= ~fopAc_AttnFlag_BATTLE_e;
                    bckSet(0x10, 2, 3.0f, 0.0f);
                } else {
                    if (field_0x6a5 == 2) {
                        field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0xf), 0xf);
                        mSphCc.SetCoSPrm(0x145);
                        mSphCc.SetTgType(0x10000);
                        field_0x714 &= ~fopAc_AttnFlag_BATTLE_e;
                    } else {
                        field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 7), 7);
                    }
                    mMode = 1;
                }
            }

            break;
        }

        case 3: {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6dc, -70.0f, 2.0f), cLib_chaseF(&field_0x6dc, -70.0f, 2.0f));
            setMoveSound(1);
            if (current.pos.absXZ(my_vec_0) < 100.0f) {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0), 0x1000), cLib_chaseAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0), 0x1000));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0), 8, 0x2000, 0x100), cLib_addCalcAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0), 8, 0x2000, 0x100));
            }

            current.angle.y = shape_angle.y;
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &speedF, 20.0f, 1.0f), cLib_chaseF(&speedF, 20.0f, 1.0f));
            if (mAcch.ChkWallHit()) {
                field_0x6a5 += (u8) 2;
                if (field_0x6a5 >= DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20) && checkBeforeGround()) {
                    field_0x6a5 = 0;
                }
            } else if (field_0x6a5) {
                --field_0x6a5;
            }

            if (DUSK_IF_ELSE(ym_reached_xz_target(this, my_vec_0, 30.0f), current.pos.absXZ(my_vec_0) < 30.0f) && field_0x6dc == -70.0f) {
                speedF = 0.0f;
                mMode = 4;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
                bckSet(0xf, 2, 5.0f, 1.0f);
            }

            break;
        }

        case 4: {
            // following needed for both (a) dbg stack reg to use r31, and (b) force b instruction in asm:
            int _;
            if (field_0x6f0 == 0) {
                field_0x714 |= fopAc_AttnFlag_BATTLE_e;
                setActionMode(ACT_RAIL);
            }

            break;
        }
    }
}

void daE_YM_c::checkElectricStart() {
    if (field_0x6a1 == 1) {
        if (mAction != ACT_WAIT && mAction != ACT_SURPRISE) {
            field_0x708 = field_0x6c9 = 0;
        } else if (field_0x708 != 0) {
            field_0x708--;

            if (field_0x708 == 0) {
                field_0x6c9 = 1;
            }
        }
    } else {
        field_0x708 = field_0x6c9 = 0;
    }
}

void daE_YM_c::executeElectric() {
    switch (mMode) {
        case 0: {
            field_0x6c9 = 0;
            bckSet(9, 0, 3.0f, 1.0f);
            speedF = 0.0f;
            mMode = 1;
            break;
        }

        case 1: {
            if (mpMorf->getFrame() < 36.0f) {
                setElecEffect1();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC, 0, -1);
            } else {
                setElecEffect2();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            }

            if (mpMorf->checkFrame(40.0f)) {
                mMode = 2;
            }

            break;
        }

        case 2: {
            setElecEffect2();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
            mSphCc.OnAtSetBit();
            if (mpMorf->isStop()) {
                mMode = 3;
                field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x1e), 0x1e);
                bckSet(0xf, 2, 5.0f, 1.0f);
            }

            break;
        }

        case 3: {
            if (field_0x6f0 == 0) {
                setActionMode(ACT_WAIT);
                return;
            }

            break;
        }
    }
}

void daE_YM_c::executeSwitch() {
    cXyz my_vec_0;
    switch (mMode) {
        case 0:
            mSphCc.SetCoSPrm(0x145);
            mSphCc.SetTgType(0);
            field_0x6dc = -70.0f;
            mIsHide = 1;
            if (mpPath != NULL) {
                if (mpPath->field_0x4 == 1) {
                    field_0x714 = fopAc_AttnFlag_BATTLE_e;
                } else {
                    field_0x714 = 0;
                    field_0x6cf = 0;
                }
            } else {
                field_0x714 = 0;
            }
            if (dComIfGs_isSwitch(field_0x6a3, fopAcM_GetRoomNo(this))) {
                if (mpPath == NULL) {
                    mPrevPos = current.pos;
                    mMode = 1;
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                    field_0x6e4 = shape_angle.y;
                    field_0x6e6 = 0x5dc;
                    field_0x6e8 = 0;
                } else {
                    dPnt* pnt = dPath_GetPnt(mpPath, mCurrentPntNo);
                    mCurrentPntNo = 0;
                    ++mCurrentPntNo;
                    current.pos = old.pos = pnt->m_position;
                    mMode = 2;
                }
                if (field_0x6cb) {
                    speedF = 25.0f;
                } else {
                    speedF = 17.0f;
                }
                setAppear();
                setNormalCc();
                field_0x70e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 0x5a), 0x5a);
                bckSet(0x10, 2, 5.0f, 1.0f);
            }
            break;
        case 1:
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6dc, 0.0f, 15.0f), cLib_chaseF(&field_0x6dc, 0.0f, 15.0f));
            setMoveSound(0);
            if (mAcch.ChkWallHit()) {
                DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0x6e4, 0x800), ANGLE_ADD(field_0x6e4, 0x800));
            }
            if (field_0x6e8 >= 0) {
                DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0x6e6, -200), ANGLE_SUB(field_0x6e6, 200));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_angle_add(this, field_0x6e6, 200), ANGLE_ADD(field_0x6e6, 200));
            }
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0x6e8 , field_0x6e6);
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, field_0x6e4 + field_0x6e8, 4, 0x1000, 0x100);
#else
            field_0x6e8 += field_0x6e6;
            cLib_addCalcAngleS(&shape_angle.y, field_0x6e4 + field_0x6e8, 4, 0x1000, 0x100);
#endif
            if (current.pos.absXZ(mPrevPos) > 500.0f) {
                speedF = 0.0f;
                mPrevPos = current.pos;
                field_0x6dc = 0.0f;
                setActionMode(ACT_WAIT);
            }
            break;
        case 2:
#if TARGET_PC  // enemy attribute integration
            mAcchCir.SetWall((0.0f) * actor_attr::enemy_size_multiplier(this), (0.0f) * actor_attr::enemy_size_multiplier(this));
#else
            mAcchCir.SetWall(0.0f, 0.0f);
#endif
            if (field_0x70e) {
                setElecEffect2();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
                field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
                mSphCc.OnAtSetBit();
            }
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6dc, 0.0f, 3.0f), cLib_chaseF(&field_0x6dc, 0.0f, 3.0f));
            setMoveSound(0);
            my_vec_0 = dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_angle(this, &shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0),
#else
            cLib_addCalcAngleS(&shape_angle.y, cLib_targetAngleY(&current.pos, &my_vec_0),
#endif
                               4, 0x2000, 0x100);
            if (current.pos.absXZ(my_vec_0) < 50.0f) {
                ++mCurrentPntNo;
                if (mCurrentPntNo >= mpPath->m_num) {
                    if (mpPath->field_0x4 == 2) {
                        field_0x71d = 1;
                        field_0x714 = 0;
                    } else {
                        speedF = 0.0f;
                        mPrevPos = current.pos;
                        field_0x6dc = 0.0f;
#if TARGET_PC  // enemy attribute integration
                        mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
                        mAcchCir.SetWall(40.0f, 60.0f);
#endif
                        setActionMode(ACT_WAIT);
                    }
                }
            }
            break;
        case 10:
            setAppear();
            setNormalCc();
            current.pos = old.pos = dPath_GetPnt(mpPath, mpPath->m_num-1)->m_position;
            mPrevPos = current.pos;
            field_0x6dc = 0.0f;
            speedF = 0.0f;
#if TARGET_PC  // enemy attribute integration
            mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
            mAcchCir.SetWall(40.0f, 60.0f);
#endif
            setActionMode(ACT_WAIT);
            break;
        default:
            break;
    }
    current.angle.y = shape_angle.y;
}

void daE_YM_c::initFireFly(int param_0) {
    field_0x6e6 = cM_rndFX(512.0f);
    if (param_0 && mAcch.ChkWallHit()) {
        s16 wall_ang = mAcchCir.GetWallAngleY();
        wall_ang = (wall_ang * 2) - (current.angle.y + 0x8000);
        current.angle.y = wall_ang + cM_rndFX(4096.0);
#if TARGET_PC  // enemy attribute integration
        speed.y += actor_attr::enemy_move_step(this, cM_rndFX(15.0f));
        field_0x70e = actor_attr::enemy_sync_timer(this, cM_rndFX(5.0f) + 10.0f);
#else
        speed.y += cM_rndFX(15.0f);
        field_0x70e = cM_rndFX(5.0f) + 10.0f;
#endif
    }
    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
}

void daE_YM_c::executeFire() {
    if (current.pos.y >= 550.0f) {
        current.pos.y = 550.0f;
        mAcch.SetRoofHit();
    }
    switch (mMode) {
        case 0:
            if (field_0x6f0) {
                setElecEffect2();
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            } else if (daPy_getPlayerActorClass()->checkFrontRollCrash() || daPy_getPlayerActorClass()->checkWolfAttackReverse()) {
                cXyz pos_diff = daPy_getPlayerActorClass()->current.pos - current.pos;
                if (pos_diff.abs() < 250.0f) {
                    field_0x6f0 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 10), 10);
                }
            }
            if (field_0x6d4) {
                field_0x714 = fopAc_AttnFlag_BATTLE_e;
            } else {
                field_0x714 = 0;
            }
            mSphCc.SetCoSPrm(0x145);
            mSphCc.SetTgType(0);
            field_0x6dc = -70.0f;
            mIsHide = 1;
            field_0x71c = 1;
            current.pos = home.pos;
            if (field_0x6a3 == 0xff || dComIfGs_isSwitch(field_0x6a3, fopAcM_GetRoomNo(this))) {
                field_0x71c = 0;
                mPrevPos = current.pos;
                field_0x6e4 = 0;
                setAppear();
                setNormalCc();
                mSphCc.SetTgType(0);
                field_0x6dc = 0.0f;
                mMode = 1;
                bckSetFly(0xd, 0, 5.0f, 1.0f);
                field_0x6a5 = 0;
            }
            break;
        case 1:
            if (mpMorf->checkFrame(6.0f)) {
                field_0x6cc = 1;
            }
            if (mpMorf->checkFrame(14.0f)) {
                attention_info.distances[fopAc_attn_BATTLE_e] = 0x2e;
                gravity = 0.0f;
                fopAcM_SearchByName(0x1f7, &field_0x660);
                if (field_0x660 != NULL) {
                    for (int idx = 0; idx < 20; ++idx) {
                        if (((daTag_FWall_c*)field_0x660)->getTagNo(idx) == getTagNo()) {
                            field_0x67c = ((daTag_FWall_c*)field_0x660)->getPos(idx);
                            break;
                        }
                    }
                    mMode = 2;
                } else {
                    initFireFly(0);
                    mMode = 3;
                }
            }
        case 2:
            if (fopAcM_CheckCondition(this, 4) == 0) {
                setFireEffect();
                setElecEffect2();
            }
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
            field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
            mSphCc.OnAtSetBit();
            current.angle.y = cLib_targetAngleY(&current.pos, &field_0x67c);
            current.angle.x = cLib_targetAngleX(&current.pos, &field_0x67c);
            speedF = cM_scos(s16(current.angle.x)) * 40.0f;
            speed.y = cM_ssin(s16(current.angle.x)) * 40.0f;
            if (field_0x67c.abs(current.pos) < 200.0f && mAcch.ChkWallHit()) {
                mMode = 3;
                initFireFly(1);
                for (int idx = 0; idx < 20; ++idx) {
                    if (((daTag_FWall_c*)field_0x660)->getTagNo(idx) == getTagNo()) {
                        ((daTag_FWall_c*)field_0x660)->setFire(idx);
                    }
                }
            }
            shape_angle.y = current.angle.y;
            break;
        case 3:
            if (!dComIfGp_event_runCheck()) {
                mSphCc.OnCoSetBit();
            } else {
                mSphCc.OffCoSetBit();
            }
            if (field_0x6a5 == 0 && mpMorf->isStop()) {
                bckSetFly(0xc, 2, 0.0f, 1.0f);
                ++field_0x6a5;
            }
            if (fopAcM_CheckCondition(this, 4) == 0) {
                setFireEffect();
            }
            if (field_0x70e) {
                if (fopAcM_CheckCondition(this, 4) == 0) {
                    setElecEffect2();
                }
                mSound.startCreatureSoundLevel(Z2SE_EN_YM_ELECTRIC_LOOP, 0, -1);
                field_0x700 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, l_HIO.mElectricInvincibilityTimeExtension), l_HIO.mElectricInvincibilityTimeExtension);
                mSphCc.OnAtSetBit();
            }
            if (current.pos.y > 400.0f) {
                speed.y -= DUSK_IF_ELSE(actor_attr::enemy_move_acceleration(this, 1.0f), 1.0f);
                if (speed.y < -15.0f) {
                    speed.y = -15.0f;
                }
            } else if (current.pos.y < 200.0f) {
                if (speed.y < 0.0f) {
                    speed.y = 0.0f;
                }
            } else {
                speed.y += DUSK_IF_ELSE(actor_attr::enemy_move_acceleration(this, 1.0f), 1.0f);
                if (speed.y > 15.0f) {
                    speed.y = 15.0f;
                }
            }
            speedF = 40.0f - std::abs(speed.y);
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, current.angle.y , field_0x6e6);
#else
            current.angle.y += field_0x6e6;
#endif
            if (mAcch.ChkWallHit()) {
                initFireFly(1);
            } else if (field_0x6f0 == 0) {
                initFireFly(0);
            }
            shape_angle.y = current.angle.y;
            break;
        default:
            break;
    }
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_angle_add(this, field_0x6e4, 0x2000);
    current.pos.y += actor_attr::enemy_action_step(this, cM_ssin(field_0x6e4) * 3.0f);
#else
    ANGLE_ADD(field_0x6e4, 0x2000);
    current.pos.y += cM_ssin(field_0x6e4) * 3.0f;
#endif
    if (mMode) {
        mSound.startCreatureSoundLevel(Z2SE_EN_YM_FLY, 0, -1);
    }
}

void daE_YM_c::setRiverAttention() {
    cXyz player_pos = daPy_getPlayerActorClass()->current.pos;
    attention_info.distances[fopAc_attn_BATTLE_e] = 60;
    field_0x714 = 0;
    if (current.pos.abs(mpKago->current.pos) > 2000.0f) {
        return;
    }
    cXyz my_vec_0;
    cXyz my_vec_1;
    mDoMtx_stack_c::YrotS(-shape_angle.y);
    mDoMtx_stack_c::XrotM(-shape_angle.x);
    mDoMtx_stack_c::transM(current.pos - mpKago->current.pos);
    mDoMtx_stack_c::multVecZero(&my_vec_0);
    my_vec_1.set(my_vec_0.x, my_vec_0.z, my_vec_0.y);
    if (abs(cM_atan2s(my_vec_1.y, my_vec_1.absXZ()) - 0x4000) < 0x2000) {
        field_0x714 = fopAc_AttnFlag_BATTLE_e;
    }
}

void daE_YM_c::setLockByCargo() {
    field_0x6f2 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 90), 90);
}

void daE_YM_c::executeRiver() {
    cXyz player_pos = daPy_getPlayerActorClass()->current.pos;
    cXyz pnt_pos = (Vec)dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
    cXyz my_vec_0;
    f32 next_path;
    switch (mMode) {
        case 0: {
            field_0x714 = 0;
            field_0x6d4 = 0.0f;
            field_0x6dc = 0.0f;
            gravity = 0.0f;
            speed.y = speedF = 0.0f;
            field_0x6ec = 0.0f;
            field_0x6f2 = 0;
            if (mpKago == NULL) {
                fopAcM_SearchByName(0xf4, (fopAc_ac_c**)&mpKago);
            } else if ( mpKago->getPathDir() ) {
                if (dComIfGs_isLightDropGetFlag(dComIfGp_getStartStageDarkArea()) == 0) {
                    field_0x71d = 1;
                } else {
                    if (mpKago->getPathDir() > 0) {
                        if (mpPath != NULL) {
                            mMode = 1;
                        } else {
                            field_0x71d = 1;
                            break;
                        }
                    } else {
                        if (field_0x6b0 != NULL) {
                            mMode = 1;
                            mpPath = field_0x6b0;
                        } else {
                            field_0x71d = 1;
                            break;
                        }
                    }
                    if (mMode == 1) {
                        bckSetFly(0xe, 2, 5.0f, 1.0f);
                        field_0x6cc = 1;
                        mCurrentPntNo = 0;
                        pnt_pos = dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
                        current.pos = old.pos = pnt_pos;
                        ++mCurrentPntNo;
                        pnt_pos = dPath_GetPnt(mpPath, mCurrentPntNo)->m_position;
                        shape_angle.y = current.angle.y = cLib_targetAngleY(&current.pos, &pnt_pos);
                        setAppear();
                        setNormalCc();
                        field_0x6f0 = 0;
                    }
                }
            }
            break;
        }

        case 1: {
            setElecEffect1();
            mSound.startCreatureSoundLevel(Z2SE_EN_YM_FLY, 0, -1);
            if (mpKago->isFlying()) {
                if (field_0x6a3 != 0xFF) {
                    if (dComIfGs_isSwitch(field_0x6a3, fopAcM_GetRoomNo(this))) {
                        speed.y = speedF = 0.0f;
                        mMode = 2;
                        bckSetFly(0xb, 2, 5.0f, 1.0f);
                    }
                } else {
                    next_path = mpKago->checkNextPath(current.pos);
                    if (next_path < 2000.0f) {
                        speed.y = speedF = 0.0f;
                        mMode = 2;
                        bckSetFly(0xb, 2, 5.0f, 1.0f);
                    }
                }
            }

            break;
        }

        case 2: {
            setRiverAttention();
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, field_0x6e8, 0x200);
            current.pos.y += actor_attr::enemy_move_step(this, cM_ssin(field_0x6e8 << 1) * 15.0f);
#else
            ANGLE_ADD(field_0x6e8, 0x200);
            current.pos.y += cM_ssin(field_0x6e8 << 1) * 15.0f;
#endif
            f32 my_float_val = cM_scos(field_0x6e8) * 15.0f;
#if TARGET_PC  // enemy attribute integration
            current.pos.x += actor_attr::enemy_move_step(this, my_float_val * cM_ssin(shape_angle.y));
            current.pos.z += actor_attr::enemy_move_step(this, my_float_val * cM_scos(shape_angle.y));
#else
            current.pos.x += my_float_val * cM_ssin(shape_angle.y);
            current.pos.z += my_float_val * cM_scos(shape_angle.y);
#endif
            shape_angle.y = current.angle.y = cLib_targetAngleY(&current.pos, &pnt_pos);
            shape_angle.x = current.angle.x = cLib_targetAngleX(&current.pos, &pnt_pos);
            s16 ang_diff = abs(mAngleToPlayer - shape_angle.y); // unused
            next_path = mpKago->checkNextPath(current.pos);
            if (mpKago->isFlying()) {
                if (field_0x6f2) {
                    field_0x714 = fopAc_AttnFlag_BATTLE_e;
                    if (next_path < 200.0f) {
                        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 70.0f, 1.0f), cLib_chaseF(&field_0x6ec, 70.0f, 1.0f));
                    } else if (next_path > 1000.0f) {
                        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 10.0f, 1.0f), cLib_chaseF(&field_0x6ec, 10.0f, 1.0f));
                    } else {
                        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 30.0f, 1.0f), cLib_chaseF(&field_0x6ec, 30.0f, 1.0f));
                    }
                } else {
                    if (next_path < 0.0f) {
#if TARGET_PC  // enemy attribute integration
                        field_0x6f0 = actor_attr::enemy_sync_timer(this, 0x96);
                        actor_attr::enemy_chase_action_float(this, &field_0x6ec, 70.0f, 1.0f);
#else
                        field_0x6f0 = 0x96;
                        cLib_chaseF(&field_0x6ec, 70.0f, 1.0f);
#endif
                    } else if (field_0x6f0) {
                        DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 70.0f, 1.0f), cLib_chaseF(&field_0x6ec, 70.0f, 1.0f));
                    } else {
                        if (next_path < 4000.0f) {
                            if (next_path < 1500.0f) {
                                f32 abs_val = mpKago->speed.abs() - 10.0f;
                                if (abs_val < 30.0f) {
                                    abs_val = 30.0f;
                                }

                                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, abs_val, 1.0f), cLib_chaseF(&field_0x6ec, abs_val, 1.0f));
                            } else {
                                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 30.0f, 1.0f), cLib_chaseF(&field_0x6ec, 30.0f, 1.0f));
                            }
                        } else {
                            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 0.0f, 1.0f), cLib_chaseF(&field_0x6ec, 0.0f, 1.0f));
                        }
                    }
                }
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x6ec, 0.0f, 1.0f), cLib_chaseF(&field_0x6ec, 0.0f, 1.0f));
                field_0x6d4 = 0.0f;
            }

            speed.y = field_0x6ec * cM_ssin(current.angle.x);
            speedF = field_0x6ec * cM_scos(current.angle.x);
            if (pnt_pos.abs(current.pos) < 500.0f) {
                ++mCurrentPntNo;
                if (mCurrentPntNo >= mpPath->m_num) {
                    speedF = 0.0f;
                    mPrevPos = pnt_pos;
                    field_0x6dc = 0.0f;
#if TARGET_PC  // enemy attribute integration
                    mAcchCir.SetWall((40.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
#else
                    mAcchCir.SetWall(40.0f, 60.0f);
#endif
                    shape_angle.x = current.angle.x = 0;
                    setActionMode(ACT_WAIT);
                    break;
                }
            }

            if (current.pos.abs(mpKago->current.pos) > 2000.0f) {
                field_0x68c = 2.4f;
                setElecEffect2();
                field_0x68c = 1.5f;
            }

            mSound.startCreatureSoundLevel(Z2SE_EN_YM_FLY, 0, -1);
            if (mSphCc.ChkTgHit()) {
                cCcD_Obj * tg_hit_obj = mSphCc.GetTgHitObj();
                if (fopAcM_GetName(dCc_GetAc(tg_hit_obj->GetAc())) == fpcNm_KAGO_e) {
                    if (mpKago == tg_hit_obj->GetAc()) {
                        if (mpKago->getLockActor() == this) {
                            mpKago->setLockActor(NULL);
                        }

                        setAppear();
                        speed.y = speedF = 0.0f;
                        field_0x714 = 0;
                        bckSet(0x10, 2, 5.0f, 1.0f);
                        mpKago->setEatYm();
                        mMode = 3;
                        field_0x6ce = 1;
                    }
                }
            }

            break;
        }

        case 3: {
            int _; // force extra b instruction in dbg asm
            setElecEffect1();
            if (mpKago != NULL) {
                mDoMtx_stack_c::copy(mpKago->getMouthMtx());
                mDoMtx_stack_c::multVecZero(&my_vec_0);
                current.pos += mpKago->speed;
                current.pos = my_vec_0;
                if (DUSK_IF_ELSE(actor_attr::enemy_chase_action_pos(this, &current.pos, my_vec_0, 50.0f), cLib_chasePos(&current.pos, my_vec_0, 50.0f))) {
                    if (mSwitchBit != 0xff && dComIfGs_isSwitch(mSwitchBit, fopAcM_GetRoomNo(this)) == 0) {
                        dComIfGs_onSwitch(mSwitchBit, fopAcM_GetRoomNo(this));
                    }

                    dComIfGp_setHitMark(3, this, &current.pos, NULL, NULL, 0);
                    fopAcM_delete(this);
                    fopAcM_createDisappear(this, &current.pos, DUSK_IF_ELSE(static_cast<u8>(20.0f * actor_attr::enemy_size_multiplier(this)), 0x14), 1, 0xff);
                }
            }

            break;
        }
    }
}

static void* s_ym_sub(void* main_p, void* other_p) {
    if (fopAcM_IsActor(main_p) && fopAcM_GetName(main_p) == fpcNm_E_YM_e) {
        if (!fpcM_IsCreating(fopAcM_GetID(main_p)) && main_p != other_p &&
            ((daE_YM_c*)main_p)->current.pos == ((daE_YM_c*)other_p)->current.pos)
        {
            u8 friend_flag = ((daE_YM_c*)other_p)->getFriendFlag();
            if (friend_flag & 1) {
                ((daE_YM_c*)main_p)->setFriendFlag(0x84);
            } else {
                ((daE_YM_c*)main_p)->setFriendFlag(0x82);
            }

            ((daE_YM_c*)other_p)->setFriendFlag(0x81);
        }
    }

    return 0;
}

void daE_YM_c::checkFrinedSamePos() {
    if (mFriendFlag == 0) {
        setFriendFlag(0x80);
        fpcM_Search(s_ym_sub, this);
    }
}

void daE_YM_c::action() {
    int reg_r27 = 0; // set but not used
    if (daPy_getPlayerActorClass()->getCutType() == daPy_py_c::CUT_TYPE_WOLF_LOCK) {
        reg_r27 = 1;
        field_0x700 = 0;
    }

    mDistToPlayer = fopAcM_searchPlayerDistance(this);
    mAngleToPlayer = fopAcM_searchPlayerAngleY(this);
    cXyz my_vec_0;
    cXyz my_vec_1;
    mSphCc.OnTgSetBit();
    if (field_0x6fe) {
        mSphCc.OffTgSetBit();
    }

    if (!field_0x6d4) {
        mSphCc.OffTgSetBit();
    }

    mSphCc.SetTgSpl(dCcG_Tg_Spl_UNK_0);
    mSphCc.SetAtMtrl(0);
    if (field_0x700) {
        mSphCc.SetAtMtrl(7);
    }

    if (mType == 1) {
        mSphCc.OnAtSetBit();
    } else {
        mSphCc.OffAtSetBit();
    }

    damage_check();
    checkSurpriseLock();
    field_0x6cf = 1;
    int do_stuff = false;
    switch (mAction) {
        case ACT_WAIT:
            executeWait();
            break;
        case ACT_MOVE:
            executeMove();
            break;
        case ACT_ESCAPE:
            executeEscape();
            break;
        case ACT_DOWN:
            executeDown();
            break;
        case ACT_WIND:
            executeWind();
            break;
        case ACT_SURPRISE:
            executeSurprise();
            break;
        case ACT_BACK:
            executeBack();
            break;
        case ACT_FALL:
            executeFall();
            break;
        case ACT_ATTACK:
            do_stuff = true;
            executeAttack();
            break;
        case ACT_ATTACK_WALL:
            do_stuff = true;
            executeAttackWall();
            break;
        case ACT_DEFENSE:
            executeDefense();
            break;
        case ACT_FLY:
            executeFly();
            break;
        case ACT_FLY_ATTACK:
            do_stuff = true;
            executeFlyAttack();
            break;
        case ACT_RAIL:
            executeRail();
            break;
        case ACT_BACK_RAIL:
            executeBackRail();
            break;
        case ACT_ELECTRIC:
            do_stuff = true;
            executeElectric();
            break;
        case ACT_SWITCH:
            executeSwitch();
            break;
        case ACT_FIRE:
            executeFire();
            break;
        case ACT_RIVER:
            executeRiver();
            break;
    }

    mSound.setLinkSearch(do_stuff);
    if (field_0x6cf) {
        mSound.startCreatureSoundLevel(Z2SE_EN_YM_NEAR, 0, -1);
    }

    if (mType != 1) {
        f32 reg_f31 = speedF; // set but not used
        if (mAcch.ChkGroundHit()) {
            cM3dGPla plane;
            dComIfG_Bgsp().GetTriPla(mAcch.m_gnd, &plane);
            cXyz* p_vec = plane.GetNP();
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_angle(this, &field_0x668.x, (s16)cM_atan2s(p_vec->z, p_vec->y), 8, 0x800, 0x100);
            actor_attr::enemy_add_action_angle(this, &field_0x668.y, (s16)cM_atan2s(p_vec->x, p_vec->y), 8, 0x800, 0x100);
#else
            cLib_addCalcAngleS(&field_0x668.x, (s16)cM_atan2s(p_vec->z, p_vec->y), 8, 0x800, 0x100);
            cLib_addCalcAngleS(&field_0x668.y, (s16)cM_atan2s(p_vec->x, p_vec->y), 8, 0x800, 0x100);
#endif
            reg_f31 *= p_vec->y;
        } else {
            field_0x668.set(0, 0, 0);
        }

        DUSK_IF_ELSE(actor_attr::enemy_pos_move_f(this, mStts.GetCCMoveP()), fopAcM_posMoveF(this, mStts.GetCCMoveP()));
        if (field_0x71c == 0) {
            mAcch.CrrPos(dComIfG_Bgsp());
        }
    } else {
        mDoMtx_stack_c::XrotS(field_0x668.x);
        mDoMtx_stack_c::ZrotM(field_0x668.z);
        mDoMtx_stack_c::YrotM(current.angle.y);
        my_vec_0.set(0.0f, 0.0f, DUSK_IF_ELSE(actor_attr::enemy_move_step(this, speedF), speedF));
        mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
        speed = my_vec_1;
        current.pos += speed;
    }

    if (!field_0x6d4) {
        attention_info.flags = 0;
        if (daPy_getPlayerActorClass()->checkWolfLock(this)) {
            daPy_getPlayerActorClass()->cancelWolfLock(this);
        }

        onWolfNoLock();
    } else {
        attention_info.flags = field_0x714;
        offWolfNoLock();
        if (mType == 1 && fopAcM_CheckCondition(this, 4) == 0) {
            dBgS_LinChk lin_chk;
            lin_chk.Set(&dComIfGp_getCamera(0)->view.lookat.eye, &attention_info.position, this);
            if (dComIfG_Bgsp().LineCross(&lin_chk)) {
                onWolfNoLock();
            }
        }
    }

    cXyz unused_vec(field_0x68c, field_0x68c, field_0x68c);
    cXyz my_pos = current.pos;
#if TARGET_PC  // enemy attribute integration
    cXyz bindEffectScale = actor_attr::enemy_size_multiplier(this, scale);
    setMidnaBindEffect(this, &mSound, &my_pos, &bindEffectScale);
#else
    setMidnaBindEffect(this, &mSound, &my_pos, &scale);
#endif
    mpMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    mpBrk->play();
}

void daE_YM_c::mtx_set() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y + field_0x6dc * sizeMultiplier, current.pos.z);
#else
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y + field_0x6dc, current.pos.z);
#endif
    mDoMtx_stack_c::ZXYrotM(field_0x668);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
#if TARGET_PC  // enemy attribute integration
    const f32 modelScale = field_0x68c * sizeMultiplier;
    mDoMtx_stack_c::scaleM(modelScale, modelScale, modelScale);
#else
    mDoMtx_stack_c::scaleM(field_0x68c, field_0x68c, field_0x68c);
#endif

    J3DModel* model_p = mpMorf->getModel();
    model_p->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->modelCalc();
}

void daE_YM_c::cc_set() {
    cXyz my_vec_0;
    cXyz my_vec_1;
    J3DModel* model = mpMorf->getModel();
    cXyz my_vec_2(0.0f, 0.0f, 0.0f);
    if (mIsHide) {
        my_vec_2.y = 40.0f;
    }

    if (field_0x718 == 0) {
        if (mType != 1) {
            MTXCopy(model->getAnmMtx(0), mDoMtx_stack_c::get());
            my_vec_0.set(30.0f, 0.0f, 0.0f);
            mDoMtx_stack_c::multVec(&my_vec_0, &eyePos);
            attention_info.position = eyePos;
            attention_info.position.y += DUSK_IF_ELSE(field_0x68c * 50.0f * actor_attr::enemy_size_multiplier(this), field_0x68c * 50.0f);
        } else {
            mDoMtx_stack_c::XrotS(field_0x668.x);
            mDoMtx_stack_c::ZrotM(field_0x668.z);
            my_vec_0.set(0.0f, -30.0f, 50.0f);
            IF_DUSK(my_vec_0 *= actor_attr::enemy_size_multiplier(this);)
            mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
            eyePos = current.pos + my_vec_1;
            mDoMtx_stack_c::XrotS(field_0x668.x);
            mDoMtx_stack_c::ZrotM(field_0x668.z);
            my_vec_0.set(0.0f, 30.0f, 70.0f);
            IF_DUSK(my_vec_0 *= actor_attr::enemy_size_multiplier(this);)
            mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
            attention_info.position = current.pos + my_vec_1;
        }
    } else if (field_0x718 == 1) {
        eyePos = current.pos;
        attention_info.position = eyePos;
        attention_info.position.y += DUSK_IF_ELSE(70.0f * actor_attr::enemy_size_multiplier(this), 70.0f);
    }

    MTXCopy(model->getAnmMtx(0), mDoMtx_stack_c::get());
    if (mType != 1) {
        my_vec_0.set(10.0f, -20.0f, 0.0f);
    } else {
        my_vec_0.set(10.0f, 40.0f, 0.0f);
    }

    mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
    mSphCc.SetC(my_vec_1 + my_vec_2);
    if (mAction == ACT_RIVER) {
        mSphCc.SetR(DUSK_IF_ELSE((100.0f) * actor_attr::enemy_size_multiplier(this), 100.0f));
    } else if (field_0x700) {
        mSphCc.SetR(DUSK_IF_ELSE((40.0f) * actor_attr::enemy_size_multiplier(this), 40.0f));
    } else {
        mSphCc.SetR(DUSK_IF_ELSE((30.0f) * actor_attr::enemy_size_multiplier(this), 30.0f));
    }

    dComIfG_Ccsp()->Set(&mSphCc);
}

int daE_YM_c::execute() {
#if DEBUG
    if (DEFAULT_E_YM_MODEL_SIZE != l_HIO.mModelSize) {
        field_0x68c = l_HIO.mModelSize;
    }
#endif

    if (field_0x71d) {
        return 1;
    }

    if (field_0x6f0 != 0) {
        field_0x6f0--;
    }

    if (field_0x6f2 != 0) {
        field_0x6f2--;
    }

    if (field_0x6f4 != 0) {
        field_0x6f4--;
    }

    if (field_0x6f6 != 0) {
        field_0x6f6--;
    }

    if (field_0x6f8 != 0) {
        field_0x6f8--;
    }

    if (field_0x6fc != 0) {
        field_0x6fc--;
    }

    if (field_0x6fe != 0) {
        field_0x6fe--;
    }

    if (field_0x700 != 0) {
        field_0x700--;
    }

    if (field_0x6fa != 0) {
        field_0x6fa--;
    }

    if (field_0x704 != 0) {
        field_0x704--;
    }

    if (field_0x70a != 0) {
        field_0x70a--;
    }

    if (field_0x70c != 0) {
        field_0x70c--;
    }

    if (field_0x70e != 0) {
        field_0x70e--;
    }

    checkElectricStart();
    action();
    mtx_set();
    cc_set();
    return 1;
}

static int daE_YM_Execute(daE_YM_c* i_this) {
    return i_this->execute();
}

static int daE_YM_IsDelete(daE_YM_c*) {
    return 1;
}

int daE_YM_c::_delete() {
    if (mFlyType == true) {
        dComIfG_resDelete(&mPhase, "E_TM");
    } else {
        dComIfG_resDelete(&mPhase, "E_YM");
    }

    if (field_0xaf4) {
        lbl_253_bss_8 = 0;
        mDoHIO_DELETE_CHILD(l_HIO.field_0x4);
    }

    if (heap != NULL) {
        mSound.deleteObject();
    }

    return 1;
}

static int daE_YM_Delete(daE_YM_c* i_this) {
    fopAcM_RegisterDeleteID(i_this, "E_YM");
    return i_this->_delete();
}

int daE_YM_c::CreateHeap() {
    if (mFlyType == 1) {
        J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_TM", 0x11);
        JUT_ASSERT(0x1094, modelData != NULL);
        mpMorf = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("E_TM", 10),
                                      0, 1.0f, 0, -1, &mSound, 0x80000,0x11000084);
        if (mpMorf == NULL || mpMorf->getModel() == NULL) {
            return 0;
        }

        mpBrk = JKR_NEW mDoExt_brkAnm();
        if (mpBrk == NULL) {
            return 0;
        }

        if (!mpBrk->init(mpMorf->getModel()->getModelData(),
                         (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_TM", 0x14),
                         TRUE, 2, 1.0f, 0, -1)) {
            return 0;
        }
    } else {
        J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_YM", 0x13);
        JUT_ASSERT(0x10bb, modelData != NULL);
        mpMorf = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("E_YM", 0x10),
                                      0, 1.0f, 0, -1, &mSound, 0x80000,0x11000084);
        if (mpMorf == NULL || mpMorf->getModel() == NULL) {
            return 0;
        }

        mpBrk = JKR_NEW mDoExt_brkAnm();
        if (mpBrk == NULL) {
            return 0;
        }

        if (!mpBrk->init(mpMorf->getModel()->getModelData(),
                         (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_YM", 0x16),
                         TRUE, 2, 1.0f, 0, -1)) {
            return 0;
        }
    }

    if (mType == 6) {
        field_0x6d0 = JKR_NEW_ARRAY(cXyz, 45);
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    daE_YM_c* a_this = static_cast<daE_YM_c*>(i_this);
    return a_this->CreateHeap();
}

bool daE_YM_c::checkBeforeBg(s16 i_rot_val) {
    dBgS_LinChk lin_chk;
    cXyz my_vec_0;
    cXyz my_vec_1;
    cXyz my_vec_2;
    cXyz my_vec_3;
    if (!speedF) {
        return false;
    }

    if (mType != 1) {
        if (mAcch.ChkWallHit()) {
            return true;
        }
    } else {
        mDoMtx_stack_c::XrotS(field_0x668.x);
        mDoMtx_stack_c::ZrotM(field_0x668.z);
        mDoMtx_stack_c::YrotM(i_rot_val);
        my_vec_0.set(0.0f, 30.0f, 0.0f);
        mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_3);
        my_vec_3 += current.pos;
        mDoMtx_stack_c::XrotS(field_0x668.x);
        mDoMtx_stack_c::ZrotM(field_0x668.z);
        mDoMtx_stack_c::YrotM(i_rot_val);
        my_vec_0.set(0.0f, 0.0f, 60.0f);
        mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
        my_vec_0.set(my_vec_1.x, 0.0f, my_vec_1.z);
        my_vec_2 = my_vec_3 + my_vec_0;
        lin_chk.Set(&my_vec_3, &my_vec_2, NULL);
        if (dComIfG_Bgsp().LineCross(&lin_chk)) {
            return true;
        }

        if (my_vec_1.y > 0.0f) {
            my_vec_0.set(0.0f, 60.0f, 0.0f);
        } else {
            my_vec_0.set(0.0f, -200.0f, 0.0f);
        }

        my_vec_2 = my_vec_3 + my_vec_0;
        lin_chk.Set(&my_vec_3, &my_vec_2, NULL);
        if (dComIfG_Bgsp().LineCross(&lin_chk)) {
            return true;
        }

        mDoMtx_stack_c::XrotS(field_0x668.x);
        mDoMtx_stack_c::ZrotM(field_0x668.z);
        mDoMtx_stack_c::YrotM(i_rot_val);
        my_vec_0.set(0.0f, 30.0f, 80.0f);
        mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
        my_vec_3 = current.pos + my_vec_1;
        cXyz inside_vec(0.0f, 0.0f, 80.0f);
        cLib_offsetPos(&my_vec_2, &my_vec_3, (s16)field_0x668.z, &inside_vec);
        lin_chk.Set(&my_vec_3, &my_vec_2, NULL);
        if (dComIfG_Bgsp().LineCross(&lin_chk) == 0) {
            return true;
        }

        cM3dGPla plane;
        dComIfG_Bgsp().GetTriPla(lin_chk, &plane);
        cXyz* p_vec = plane.GetNP();
        s16 tan_val = cM_atan2s(p_vec->absXZ(), p_vec->y) + -0x8000;
        if (abs(tan_val + 0x4000) <= 0x1555) {
            if (field_0x668.z != (s16)(cM_atan2s(p_vec->x, p_vec->z) + 0x8000)) {
                return true;
            }
        }
    }

    return false;
}

bool daE_YM_c::checkBeforeGround() {
    cXyz my_vec_0;
    cXyz my_vec_1;
    dBgS_GndChk gnd_chk;
    cXyz adj_pos = current.pos;
    adj_pos.y += 300.0f;
    mDoMtx_stack_c::YrotS(current.angle.y);
    my_vec_0.set(0.0f, 00.0f, 60.0f);
    mDoMtx_stack_c::multVec(&my_vec_0, &my_vec_1);
    adj_pos.x += my_vec_1.x;
    adj_pos.z += my_vec_1.z;
    gnd_chk.SetPos(&adj_pos);
    f32 gnd_cross = dComIfG_Bgsp().GroundCross(&gnd_chk);
    if (gnd_cross > current.pos.y) {
        current.pos.set(adj_pos.x, gnd_cross, adj_pos.z);
        old.pos = current.pos;
        return true;
    } else {
        return false;
    }
}

void daE_YM_c::checkInitialWall() {
    cXyz my_vec_0;
    cXyz my_vec_1(0.0f, 0.0f, 200.0f);
    dBgS_LinChk lin_chk;
    for (int idx = 0; idx < 4; ++idx) {
        cLib_offsetPos(&my_vec_0, &current.pos, shape_angle.y + (idx << 0xe), &my_vec_1);
        lin_chk.Set(&current.pos, &my_vec_0, NULL);
        if (dComIfG_Bgsp().LineCross(&lin_chk)) {
            cM3dGPla plane;
            dComIfG_Bgsp().GetTriPla(lin_chk, &plane);
            cXyz* p_vec = plane.GetNP();
            s16 tan_val = cM_atan2s(p_vec->absXZ(), p_vec->y) + -0x8000;
            if (abs(tan_val + 0x4000) <= 0x1555) {
                current.pos = lin_chk.GetCross();
                field_0x668.z = cM_atan2s(p_vec->x, p_vec->z) + 0x8000;
                field_0x668.x = tan_val;
#if TARGET_PC  // enemy attribute integration
                mAcchCir.SetWall((20.0f) * actor_attr::enemy_size_multiplier(this), (10.0f) * actor_attr::enemy_size_multiplier(this));
#else
                mAcchCir.SetWall(20.0f, 10.0f);
#endif
                return;
            }
        }
    }
    mType = 0;
}

u8 daE_YM_c::checkWall() {
    cXyz my_vec_0;
    cXyz my_vec_1;
    cXyz my_vec_2(0.0f, 0.0f, 100.0f);
    dBgS_LinChk lin_chk;
    if (mType != 1) {
        return 0;
    }

    cLib_offsetPos(&my_vec_1, &current.pos, s16(field_0x668.z), &my_vec_2);
    my_vec_2.z = -50.0;
    cLib_offsetPos(&my_vec_0, &current.pos, s16(field_0x668.z), &my_vec_2);
    lin_chk.Set(&my_vec_0, &my_vec_1, NULL);
    if (dComIfG_Bgsp().LineCross(&lin_chk)) {
        cM3dGPla plane;
        dComIfG_Bgsp().GetTriPla(lin_chk, &plane);
        cXyz* p_vec = plane.GetNP();
        s16 tan_val = cM_atan2s(p_vec->absXZ(), p_vec->y) + -0x8000;
        if (abs(tan_val + 0x4000) <= 0x1555) {
            current.pos = lin_chk.GetCross();
            field_0x668.z = cM_atan2s(p_vec->x, p_vec->z) + 0x8000;
            field_0x668.x = cM_atan2s(p_vec->absXZ(), p_vec->y) + -0x8000;
            return 1;
        }
    }
    return 0;
}

void daE_YM_c::setHideType() {
    mIsHide = true;
    mSphCc.SetCoSPrm(0x145);
    mSphCc.SetTgType(0x10000);

    m_near_obj = NULL;
    m_obj_dist = 100.0f;

    fpcM_Search(s_obj_sub, this);

    if (m_near_obj != NULL) {
        cXyz obj_pos(m_near_obj->current.pos);

        current.pos.x = obj_pos.x;
        current.pos.z = obj_pos.z;
        field_0x6a6 = 1;
    } else {
        field_0x6a6 = 3;
    }
}

int daE_YM_c::create() {
    fopAcM_ct(this, daE_YM_c);

    mType = fopAcM_GetParam(this) & 0xFF;
    if (mType == 0xFF) {
        mType = 0;
    }

    mSwitchBit = (fopAcM_GetParam(this) & 0xFF000000) >> 24;

    u8 prm2 = (fopAcM_GetParam(this) & 0xFF0000) >> 16;
    if (prm2 == 0xFF) {
        prm2 = 0;
    }

    field_0x6e0 = prm2 * 100.0f;

    u8 prm1 = (fopAcM_GetParam(this) & 0xFF00) >> 8;
    u8 tmp0 = (current.angle.z & 0xFF00) >> 8;
    mTagNo = current.angle.z & 0xFF;
    field_0x6a1 = 1;
    if ((current.angle.x & 0x3) != 0) {
        field_0x6a1 = 0;
    }

    if ((current.angle.x & 0xC) != 0) {
        field_0x6cb = 1;
    }

    field_0x6a3 = (current.angle.x & 0xFF00) >> 8;

    if (mSwitchBit != 0xFF && dComIfGs_isSwitch(mSwitchBit, fopAcM_GetRoomNo(this))) {
        // E_YM won't reset because it's already been defeated.
        OS_REPORT("E_YM やられ後なので再セットしません\n");
        return cPhs_ERROR_e;
    }

    if ((mTagNo != 0 && mTagNo != 0xFF) || (mType == 6 || mType == 3 || mType == 7)) {
        mFlyType = true;
    } else {
        mFlyType = false;
    }

    int phase;
    if (mFlyType == true) {
        phase = dComIfG_resLoad(&mPhase, "E_TM");
    } else {
        phase = dComIfG_resLoad(&mPhase, "E_YM");
    }

    if (phase == cPhs_COMPLEATE_e) {
        OS_REPORT("E_YM PARAM %x %x %x \n", fopAcM_GetParam(this), current.angle.z, current.angle.x);
        current.angle.x = current.angle.z = 0;
        shape_angle.x = shape_angle.z = 0;

        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x28B0)) {
            return cPhs_ERROR_e;
        }

        if (!lbl_253_bss_8) {
            field_0xaf4 = 1;
            lbl_253_bss_8 = 1;
            l_HIO.field_0x4 = mDoHIO_CREATE_CHILD("闇の虫", &l_HIO);
        }

        field_0x714 = fopAc_AttnFlag_BATTLE_e;
        attention_info.distances[fopAc_attn_BATTLE_e] = 58;
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
        fopAcM_SetMin(this, -200.0f * sizeMultiplier, -100.0f * sizeMultiplier, -200.0f * sizeMultiplier);
        fopAcM_SetMax(this, 200.0f * sizeMultiplier, 100.0f * sizeMultiplier, 200.0f * sizeMultiplier);
#else
        fopAcM_SetMin(this, -200.0f, -100.0f, -200.0f);
        fopAcM_SetMax(this, 200.0f, 100.0f, 200.0f);
#endif

        mAcch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &mAcchCir,
                  fopAcM_GetSpeed_p(this), NULL, NULL);
        mAcch.OnLineCheck();
#if TARGET_PC  // enemy attribute integration
        mAcchCir.SetWall((30.0f) * actor_attr::enemy_size_multiplier(this), (60.0f) * actor_attr::enemy_size_multiplier(this));
        health = actor_attr::enemy_health_value(this, 10.0f);
        field_0x560 = health;
#else
        mAcchCir.SetWall(30.0f, 60.0f);
        field_0x560 = health = 10;
#endif

        mStts.Init(100, 0, this);
        mSphCc.Set(E_YM_n::cc_sph_src);
        mSphCc.SetStts(&mStts);
        IF_DUSK(mSphCc.SetAtAtp(actor_attr::enemy_attack_power_byte(this, 1.0f));)

        mSound.init(&current.pos, &eyePos, 3, 1);
        mSound.setEnemyName("E_ym");
        mAtInfo.mpSound = &mSound;
        mAtInfo.mPowerType = 1;
        field_0x68c = DEFAULT_E_YM_MODEL_SIZE;

        switch (mType) {
        case 0:
            mPrevPos = current.pos;
            setActionMode(ACT_WAIT);
            break;
        case 1:
            checkInitialWall();
            mPrevPos = current.pos;
            setActionMode(ACT_WAIT);
            break;
        case 2:
            setActionMode(ACT_WAIT);
            mMode = 100;
            break;
        case 3:
            setActionMode(ACT_FLY);
            mMode = 2;
            break;
        case 4:
            if (tmp0 != 0xFF) {
                mpPath = dPath_GetRoomPath(tmp0, fopAcM_GetRoomNo(this));

                if (mpPath != NULL) {
                    attention_info.distances[fopAc_attn_ETC_e] = 31;
                    field_0x6a6 = 4;
                    mPrevPos = current.pos;
                    setActionMode(ACT_WAIT);
                    break;
                }
            }

            mType = 0;
            mPrevPos = current.pos;
            setActionMode(ACT_WAIT);
            break;
        case 5:
            if (field_0x6a3 == 0xFF) {
                mType = 0;
                mPrevPos = current.pos;
                setActionMode(ACT_WAIT);
            } else {
                if (tmp0 != 0xFF) {
                    mpPath = dPath_GetRoomPath(tmp0, fopAcM_GetRoomNo(this));
                }

                setActionMode(ACT_SWITCH);
                if (dComIfGs_isSwitch(field_0x6a3, fopAcM_GetRoomNo(this))) {
                    mMode = 10;
                }
            }
            break;
        case 6:
            setActionMode(ACT_FIRE);
            break;
        case 7:
            if (tmp0 != 0xFF) {
                mpPath = dPath_GetRoomPath(tmp0, fopAcM_GetRoomNo(this));
            }

            if (prm1 != 0xFF) {
                field_0x6b0 = dPath_GetRoomPath(prm1, fopAcM_GetRoomNo(this));
            }

            if (mpPath != NULL || field_0x6b0 != NULL) {
                field_0x68c = 1.5f;
                setActionMode(ACT_RIVER);
                break;
            }

            return cPhs_ERROR_e;
        }

        field_0x6cc = 1;
        if (mFlyType == true && mType != 3) {
            field_0x6cc = 0;
        }

        daE_YM_Execute(this);
    }

    return phase;
}

static int daE_YM_Create(daE_YM_c* i_this) {
    return i_this->create();
}

static DUSK_CONST actor_method_class l_daE_YM_Method = {
    (process_method_func)daE_YM_Create,
    (process_method_func)daE_YM_Delete,
    (process_method_func)daE_YM_Execute,
    (process_method_func)daE_YM_IsDelete,
    (process_method_func)daE_YM_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_YM = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_YM_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daE_YM_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_YM_e,
    /* Actor SubMtd */ &l_daE_YM_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_CULL_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

#include "d/dolzel_rel.h" // IWYU pragma: keep

#if !TARGET_PC
#include "d/actor/d_a_b_ob.h"
#endif
#include "JSystem/J3DGraphBase/J3DMaterial.h"
#include "SSystem/SComponent/c_math.h"
#if TARGET_PC  // additional actor attribute integration
#include "Z2AudioLib/Z2Instances.h"
#endif
#include "c/c_damagereaction.h"
#if TARGET_PC  // additional actor attribute integration
#include "d/actor/d_a_b_ob.h"
#include "d/actor/d_a_b_oh.h"
#include "d/actor/d_a_obj_lv3WaterB.h"
#include "d/actor/d_a_obj_ystone.h"
#endif
#include "d/actor/d_a_player.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"
#endif
#include "f_op/f_op_camera_mng.h"
#include "f_op/f_op_msg_mng.h"
#include "m_Do/m_Do_graphic.h"
#if TARGET_PC  // additional actor attribute integration

namespace actor_attr = dusk::mods::svc::actor_attr;
#else
#include "d/actor/d_a_b_oh.h"
#include "d/actor/d_a_obj_lv3WaterB.h"
#include "d/actor/d_a_obj_ystone.h"
#include "Z2AudioLib/Z2Instances.h"
#endif

enum B_oh_RES_File_ID {
    /* BCK */
    /* 0x08 */ BCK_AL_FOIFINISH = 0x8,
    /* 0x09 */ BCK_AL_OICUT,
    /* 0x0A */ BCK_AL_OICUTB,
    /* 0x0B */ BCK_AL_OIFINISH,
    /* 0x0C */ BCK_AL_OIHANG,
    /* 0x0D */ BCK_AL_OIHANGMISS,
    /* 0x0E */ BCK_AL_OIHANGWAIT,
    /* 0x0F */ BCK_AL_OIHANGWAITB,
    /* 0x10 */ BCK_AL_OISPIT,
    /* 0x11 */ BCK_OH_CORE_OPEN,
    /* 0x12 */ BCK_OI_APPEAR,
    /* 0x13 */ BCK_OI_APPEAR02,
    /* 0x14 */ BCK_OI_DEAD,
    /* 0x15 */ BCK_OI_EAT,
    /* 0x16 */ BCK_OI_EAT02,
    /* 0x17 */ BCK_OI_LASTDAMAGE,
    /* 0x18 */ BCK_OI_OPENMOUTH,
    /* 0x19 */ BCK_OI_OPENMOUTHWAIT,
    /* 0x1A */ BCK_OI_SWALLOW,
    /* 0x1B */ BCK_OI_SWIMWAIT,
    /* 0x1C */ BCK_OI_TENTACLE_END,
    /* 0x1D */ BCK_OI_THROWUP,
    /* 0x1E */ BCK_OI_WAIT,

    /* BMDR */
    /* 0x21 */ BMDR_EF_OISUI = 0x21,
    /* 0x22 */ BMDR_OI_BODY,
    /* 0x23 */ BMDR_OI_FINA,
    /* 0x24 */ BMDR_OI_FINB,
    /* 0x25 */ BMDR_OI_FINC,
    /* 0x26 */ BMDR_OI_TAIL,

    /* BMDV */
    /* 0x29 */ BMDV_OH = 0x29,
    /* 0x2A */ BMDV_OH_CORE,
    /* 0x2B */ BMDV_OI_HEAD,

    /* BRK */
    /* 0x2E */ BRK_EF_OISUI = 0x2E,
    /* 0x2F */ BRK_OH_LOOP,

    /* BTK */
    /* 0x32 */ BTK_AL_FOICUT = 0x32,
    /* 0x33 */ BTK_AL_FOICUTB,
    /* 0x34 */ BTK_AL_FOIFINISH,
    /* 0x35 */ BTK_EF_OISUI,
    /* 0x36 */ BTK_OH_LOOP,

    /* BTP */
    /* 0x39 */ BTP_AL_FOIFINISH = 0x39,
};

class daB_OB_HIO_c {
public:
public:
    daB_OB_HIO_c();
    virtual ~daB_OB_HIO_c() {}

    /* 0x04 */ s8 field_0x04;
    /* 0x08 */ f32 mCoreSize;
    /* 0x0C */ f32 mCoreMoveSpeed;
    /* 0x10 */ f32 mBodySize;
    /* 0x14 */ s16 mLightR;
    /* 0x16 */ s16 mLightG;
    /* 0x18 */ s16 mLightB;
    /* 0x1C */ f32 mRange;
};

daB_OB_HIO_c::daB_OB_HIO_c() {
    field_0x04 = -1;
    mCoreSize = 1.0f;
    mCoreMoveSpeed = 0.2f;
    mBodySize = 1.0f;
    mLightR = 0xA3;
    mLightG = 0xFF;
    mLightB = 0xFF;
    mRange = 3.0f;
}
#if TARGET_PC  // enemy attribute integration

/* Phase two has two nested native damage budgets: sword hits during one
 * mount, then the number of mounts.  Split the health multiplier between
 * both dimensions so each responds to health without multiplying the total
 * fight length by the health roll twice. */
static f32 fish_health_budget_scale(b_ob_class* i_this) {

    return JMAFastSqrt(actor_attr::enemy_health_amount(i_this, 1.0f));
}

static s16 fish_hits_per_mount(b_ob_class* i_this) {

    return actor_attr::positive_s16(4.0f * fish_health_budget_scale(i_this));
}

// Keep Morpheel's ride window proportional to the number of stabs Link is
// allowed to perform during that mount.  Vanilla is 4 hits, so this leaves
// at least vanilla timing so low health still allows a stab, and expands
// the window for higher hit counts by the same ratio as fish_hits_per_mount().
static s16 fish_hang_time_for_hits(b_ob_class* i_this, f32 vanillaFrames) {

    return actor_attr::positive_s16(vanillaFrames * std::max((f32)fish_hits_per_mount(i_this) / 4.0f, 1.0f));
}

static s16 fish_mount_count(b_ob_class* i_this) {

    return actor_attr::positive_s16(3.0f * fish_health_budget_scale(i_this));
}

static s16 fish_down_mount(b_ob_class* i_this) {

    const s16 mountCount = fish_mount_count(i_this);
    return mountCount > 1 ? mountCount - 1 : 1;
}
#endif

static void anm_init(b_ob_class* i_this, int i_anmID, f32 i_morf, u8 i_mode, f32 i_speed) {
#if TARGET_PC  // enemy attribute integration

    const actor_attr::ActionAnimationParams actorAttributeAnimation = actor_attr::enemy_animation_params(i_this, i_morf, i_speed);
    i_morf = actorAttributeAnimation.morph;
    i_speed = actorAttributeAnimation.playbackSpeed;
#endif
    J3DAnmTransform* pbck = (J3DAnmTransform*)dComIfG_getObjectRes("B_oh", i_anmID);
    i_this->mBodyParts[0].mpMorf->setAnm(pbck, i_mode, i_morf, i_speed, 0.0f, -1.0f, NULL);
    i_this->mAnmID = i_anmID;
}

static int HeadCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int joint_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        b_ob_class* a_this = (b_ob_class*)model->getUserArea();

        if (a_this != NULL) {
            if (joint_no >= 2 && joint_no <= 4) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, a_this->field_0x47ae);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            } else if (joint_no >= 5 && joint_no <= 7) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, a_this->field_0x47ae);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return 1;
}

static int TailCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int joint_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        b_ob_class* a_this = (b_ob_class*)model->getUserArea();

        if (a_this != NULL) {
            if (joint_no == 3) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, a_this->field_0x47aa);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            } else if (joint_no == 6) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, -a_this->field_0x47aa);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            } else if (joint_no == 2 || joint_no == 4 || joint_no == 5) {
                int idx = joint_no;
                if (joint_no == 2) {
                    idx = 0;
                } else if (joint_no >= 4) {
                    idx = joint_no - 3;
                }

                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, a_this->field_0x47a4[idx] + a_this->field_0x47ac);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return 1;
}

static u8 data_8061B4FD;

static BOOL moveSW;

static BOOL moveSW2;

static daB_OB_HIO_c l_HIO;

static int pno;

static int fin;

static int FinACallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int joint_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        b_ob_class* a_this = (b_ob_class*)model->getUserArea();

        if (a_this != NULL) {
            if (joint_no == 2 || joint_no == 1) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);

                if (fin == 0) {
                    cMtx_YrotM(*calc_mtx, a_this->mBodyParts[pno].mFinARot.y);
                    cMtx_XrotM(*calc_mtx, a_this->mBodyParts[pno].mFinARot.y);
                } else {
                    cMtx_YrotM(*calc_mtx, -a_this->mBodyParts[pno].mFinARot.y);
                    cMtx_XrotM(*calc_mtx, -a_this->mBodyParts[pno].mFinARot.y);
                }

                cMtx_ZrotM(*calc_mtx, a_this->mBodyParts[pno].mFinARot.x);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return 1;
}

static int FinBCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int joint_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        b_ob_class* a_this = (b_ob_class*)model->getUserArea();

        if (a_this != NULL) {
            if (joint_no == 2 || joint_no == 3 || joint_no == 5 || joint_no == 6) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, a_this->mBodyParts[pno].mFinBRot.y);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return 1;
}

static int FinCCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int joint_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        b_ob_class* a_this = (b_ob_class*)model->getUserArea();

        if (a_this != NULL) {
            if (joint_no == 1) {
                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_YrotM(*calc_mtx, a_this->mBodyParts[pno].mFinCRot.y);
                cMtx_XrotM(*calc_mtx, -a_this->mBodyParts[pno].mFinCRot.y);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return 1;
}

static int sui_nodeCallBack(J3DJoint* i_joint, int param_1) {
    if (param_1 == 0) {
        int joint_no = i_joint->getJntNo();
        J3DModel* model = j3dSys.getModel();
        b_ob_class* a_this = (b_ob_class*)model->getUserArea();

        if (a_this != NULL) {
            if (joint_no >= 2 && joint_no <= 4) {
                f32 rot = a_this->field_0x47c4 * (TREG_F(8) + 1.0f);

                MTXCopy(model->getAnmMtx(joint_no), *calc_mtx);
                cMtx_ZrotM(*calc_mtx, rot);
                model->setAnmMtx(joint_no, *calc_mtx);
                MTXCopy(*calc_mtx, J3DSys::mCurrentMtx);
            }
        }
    }

    return 1;
}

static void dark(J3DModelData* i_modelData, s16 i_color) {
    if (i_color != 0) {
        for (u16 i = 0; i < i_modelData->getMaterialNum(); i++) {
            J3DMaterial* material = i_modelData->getMaterialNodePointer(i);
            material->getTevColor(0)->r = i_color;
            material->getTevColor(0)->g = i_color;
            material->getTevColor(0)->b = i_color;
        }
    }
}

static int daB_OB_Draw(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;

    g_env_light.settingTevStruct(0, &a_this->current.pos, &a_this->tevStr);
    BOOL var_r28 = true;

    if (i_this->mFishBattleMode == 2) {
        camera_class* camera = (camera_class*)dComIfGp_getCamera(0);
        var_r28 = false;

        for (int i = 0; i < 19; i++) {
            if (!i_this->mBodyParts[i].mHide) {
                if ((i_this->mBodyParts[i].mPos - camera->view.lookat.eye).abs() >
                    i_this->mBodyParts[i].mSize * (JREG_F(17) + 500.0f))
                {
                    g_env_light.setLightTevColorType_MAJI(i_this->mBodyParts[i].mpMorf->getModel(),
                                                          &a_this->tevStr);
                    dark(i_this->mBodyParts[i].mpMorf->getModel()->getModelData(),
                         i_this->field_0x5d14);
                    i_this->mBodyParts[i].mpMorf->entryDL();

                    if (i == 0) {
                        var_r28 = true;
                    }

                    if (i_this->mBodyParts[i].mpFinMorf != NULL) {
                        g_env_light.setLightTevColorType_MAJI(
                            i_this->mBodyParts[i].mpFinMorf->getModel(), &a_this->tevStr);
                        dark(i_this->mBodyParts[i].mpFinMorf->getModel()->getModelData(),
                             i_this->field_0x5d14);
                        i_this->mBodyParts[i].mpFinMorf->entryDL();
                    }

                    if (i_this->mBodyParts[i].mpFinUnkMorf != NULL) {
                        g_env_light.setLightTevColorType_MAJI(
                            i_this->mBodyParts[i].mpFinUnkMorf->getModel(), &a_this->tevStr);
                        dark(i_this->mBodyParts[i].mpFinUnkMorf->getModel()->getModelData(),
                             i_this->field_0x5d14);
                        i_this->mBodyParts[i].mpFinUnkMorf->entryDL();
                    }

                    if (i_this->mBodyParts[i].mpFinBMorf != NULL) {
                        g_env_light.setLightTevColorType_MAJI(
                            i_this->mBodyParts[i].mpFinBMorf->getModel(), &a_this->tevStr);
                        dark(i_this->mBodyParts[i].mpFinBMorf->getModel()->getModelData(),
                             i_this->field_0x5d14);
                        i_this->mBodyParts[i].mpFinBMorf->entryDL();
                    }

                    if (i_this->mBodyParts[i].mpFinCMorf != NULL) {
                        g_env_light.setLightTevColorType_MAJI(
                            i_this->mBodyParts[i].mpFinCMorf->getModel(), &a_this->tevStr);
                        dark(i_this->mBodyParts[i].mpFinCMorf->getModel()->getModelData(),
                             i_this->field_0x5d14);
                        i_this->mBodyParts[i].mpFinCMorf->entryDL();
                    }
                }
            }
        }

        if (!i_this->mHideCore) {
            g_env_light.setLightTevColorType_MAJI(i_this->mpCoreMorf->getModel(), &a_this->tevStr);
            i_this->mpCoreMorf->entryDL();
        }
    }

    if (i_this->mCoreBattleMode != 0 && var_r28) {
        g_env_light.setLightTevColorType_MAJI(i_this->mpCoreMorf->getModel(), &a_this->tevStr);

        if (i_this->mAction == OB_ACTION_CORE_START || i_this->mAction == OB_ACTION_CORE_HAND_MOVE)
        {
            i_this->mpCoreMorf->getModel()
                ->getModelData()
                ->getMaterialNodePointer(1)
                ->getShape()
                ->hide();
        } else {
            i_this->mpCoreMorf->getModel()
                ->getModelData()
                ->getMaterialNodePointer(1)
                ->getShape()
                ->show();
        }

        i_this->mpCoreMorf->entryDL();

        if (i_this->mDrawShadow) {
            cXyz pos;
            pos.set(a_this->current.pos.x, a_this->current.pos.y + 100.0f, a_this->current.pos.z);

            i_this->mShadowKey = dComIfGd_setShadow(
                i_this->mShadowKey, 1, i_this->mpCoreMorf->getModel(), &pos, BREG_F(19) + 1300.0f, 0.0f,
                a_this->current.pos.y, i_this->mAcch.GetGroundH(), i_this->mAcch.m_gnd,
                &a_this->tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
        }

        g_env_light.settingTevStruct(0, &a_this->home.pos, &a_this->tevStr);
        g_env_light.setLightTevColorType_MAJI(i_this->mBodyParts[0].mpMorf->getModel(),
                                              &a_this->tevStr);
        i_this->mBodyParts[0].mpMorf->entryDL();
    }

    if (i_this->mSuiBrkFrame > 0.0f) {
        g_env_light.setLightTevColorType_MAJI(i_this->mpSuiModel, &a_this->tevStr);
        i_this->mpSuiBrk->entry(i_this->mpSuiModel->getModelData());
        i_this->mpSuiBtk->entry(i_this->mpSuiModel->getModelData());
        mDoExt_modelUpdateDL(i_this->mpSuiModel);
    }

    if (i_this->mBlureRate > 2) {
        mDoGph_gInf_c::setBlureRate(i_this->mBlureRate);
        mDoGph_gInf_c::onBlure();
    } else {
        mDoGph_gInf_c::offBlure();
    }

    return 1;
}

static void core_start(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz sp28;
    cXyz sp34;

    i_this->mDistToPlayer = fopAcM_searchPlayerDistance(i_this);

    switch (i_this->mMode) {
    case 0:
        if (i_this->mTimers[0] == 0) {
            b_oh_class* tentacle =
                (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i_this->mCoreHandNo]);
            if (tentacle != NULL) {
                i_this->mMode = 1;
                i_this->field_0x478c = 0;

                anm_init(i_this, BCK_OI_APPEAR, 0.0f, 0, 0.0f);
                a_this->home.pos.y = (i_this->field_0x47a0 - 200.0f) + KREG_F(11);
                a_this->current.pos.y = a_this->home.pos.y - 500.0f;
            }
        }
        break;
    case 1:
#if TARGET_PC  // enemy attribute integration
    {
#endif
#if DEBUG
        if (mDoCPd_c::getTrigY(PAD_2)) {
            i_this->mMode = 2;
            i_this->mDemoAction = 30;
        }
#endif

#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
        const f32 openingTriggerScale = sizeMultiplier > 1.0f ? sizeMultiplier : 1.0f;
        if (i_this->mDistToPlayer < 1600.0f * openingTriggerScale) {
#else
        if (i_this->mDistToPlayer < 1600.0f) {
#endif
            i_this->mMode = 2;
            i_this->mDemoAction = 30;
        }
        break;
#if TARGET_PC  // enemy attribute integration
    }
#endif
    case 2: {
        b_oh_class* tentacle =
            (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i_this->mCoreHandNo]);
        MTXCopy(tentacle->mpMorf->getModel()->getAnmMtx(i_this->field_0x478c),
                mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp28);

        MTXCopy(
            tentacle->mpMorf->getModel()->getAnmMtx(i_this->field_0x478c + i_this->field_0x4790),
            mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp34);

        a_this->current.pos = sp28 + ((sp34 - sp28) * i_this->field_0x4798);

        int var_r4;
        if (i_this->mTimers[0] == 0) {
            if (i_this->field_0x4790 > 0) {
                i_this->field_0x4798 += DUSK_IF_ELSE(actor_attr::enemy_action_step(i_this, l_HIO.mCoreMoveSpeed), l_HIO.mCoreMoveSpeed);
                var_r4 = i_this->field_0x478c + i_this->field_0x4790;
#if TARGET_PC  // enemy attribute integration
                actor_attr::enemy_angle_add(i_this, a_this->current.angle.y , 0x200);
#else
                a_this->current.angle.y += 0x200;
#endif
            } else {
                i_this->field_0x4798 += DUSK_IF_ELSE(actor_attr::enemy_action_step(i_this, l_HIO.mCoreMoveSpeed * 2.0f), l_HIO.mCoreMoveSpeed * 2.0f);
                var_r4 = i_this->field_0x478c + i_this->field_0x4790 + i_this->field_0x4790;

                if (var_r4 < tentacle->field_0xca8) {
                    var_r4 = tentacle->field_0xca8;
                }
            }

            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x9dc[var_r4], BREG_F(9) + 2.0f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f)), cLib_addCalc2(&tentacle->field_0x9dc[var_r4], BREG_F(9) + 2.0f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f)));
        } else {
            int var_r27 = i_this->field_0x478c;
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x9dc[var_r27], BREG_F(9) + 1.5f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f)), cLib_addCalc2(&tentacle->field_0x9dc[var_r27], BREG_F(9) + 1.5f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f)));

            if (i_this->field_0x4790 > 0) {
                var_r27 -= i_this->field_0x4790;
            } else {
                var_r27 += i_this->field_0x4790;
            }

            if (var_r27 < tentacle->field_0xca8) {
                var_r27 = tentacle->field_0xca8;
            }

#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x9dc[var_r27], BREG_F(9) + 1.5f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f));
            actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.y, i_this->mYAngleToPlayer, 2, 0x800);
            actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.x, i_this->mXAngleToPlayer, 2, 0x800);
#else
            cLib_addCalc2(&tentacle->field_0x9dc[var_r27], BREG_F(9) + 1.5f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f));
            cLib_addCalcAngleS2(&a_this->current.angle.y, i_this->mYAngleToPlayer, 2, 0x800);
            cLib_addCalcAngleS2(&a_this->current.angle.x, i_this->mXAngleToPlayer, 2, 0x800);
#endif
        }

        if (i_this->field_0x4798 >= 1.0f) {
            i_this->field_0x4798--;
            i_this->field_0x478c += i_this->field_0x4790;

            if (i_this->field_0x478c >= 28) {
                i_this->field_0x4790 *= -1;
                i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 110), 110);
            } else if (i_this->field_0x478c <= tentacle->field_0xca8) {
                i_this->mMode = 3;
            }
        }
        break;
    }
    case 3:
        break;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_angle_add(i_this, a_this->shape_angle.z , 0xA00);
#else
    a_this->shape_angle.z += 0xA00;
#endif
}

static void core_hand_move(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz sp38;
    cXyz sp44;

    switch (i_this->mMode) {
    case 0:
        if (i_this->mTimers[0] == 0) {
            b_oh_class* tentacle =
                (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i_this->mCoreHandNo]);
            if (tentacle == NULL || (tentacle != NULL && tentacle->mAction == OH_ACTION_END)) {
                i_this->mCoreHandNo += cM_rndF(7.0f) + 1.0f;
                if (i_this->mCoreHandNo >= 8) {
                    i_this->mCoreHandNo -= 8;
                }
            } else {
                i_this->mMode = 1;
                i_this->field_0x478c = tentacle->field_0xca8;
            }
        }
        break;
    case 1: {
        b_oh_class* tentacle =
            (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i_this->mCoreHandNo]);
        if (tentacle == NULL || (tentacle != NULL && tentacle->mAction == OH_ACTION_END)) {
            i_this->mMode = 0;
            return;
        }

        MTXCopy(tentacle->mpMorf->getModel()->getAnmMtx(i_this->field_0x478c),
                mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp38);

        MTXCopy(
            tentacle->mpMorf->getModel()->getAnmMtx(i_this->field_0x478c + i_this->field_0x4790),
            mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp44);

        a_this->current.pos = sp38 + ((sp44 - sp38) * i_this->field_0x4798);

        if (tentacle->mAction == OH_ACTION_ATTACK || tentacle->mAction == OH_ACTION_CAUGHT) {
            if (i_this->field_0x4790 > 0) {
                i_this->field_0x4790 = -1;
            }

            i_this->field_0x4798 += DUSK_IF_ELSE(actor_attr::enemy_action_step(i_this, l_HIO.mCoreMoveSpeed * 2.0f), l_HIO.mCoreMoveSpeed * 2.0f);
        } else {
            i_this->field_0x4798 += DUSK_IF_ELSE(actor_attr::enemy_action_step(i_this, l_HIO.mCoreMoveSpeed), l_HIO.mCoreMoveSpeed);
        }

        int var_r4;
        if (i_this->field_0x4790 > 0) {
            var_r4 = i_this->field_0x478c + i_this->field_0x4790;
        } else {
            var_r4 = i_this->field_0x478c + i_this->field_0x4790 + i_this->field_0x4790;
            if (var_r4 < tentacle->field_0xca8) {
                var_r4 = tentacle->field_0xca8;
            }
        }

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x9dc[var_r4], BREG_F(9) + 2.0f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f)), cLib_addCalc2(&tentacle->field_0x9dc[var_r4], BREG_F(9) + 2.0f, 0.5f, l_HIO.mCoreMoveSpeed * (BREG_F(12) + 2.0f)));

        if (i_this->field_0x4798 >= 1.0f) {
            i_this->field_0x4798--;
            i_this->field_0x478c += i_this->field_0x4790;

            if (i_this->field_0x478c >= 29) {
                i_this->field_0x4790 *= -1;
            } else if (i_this->field_0x478c <= tentacle->field_0xca8) {
                i_this->mMode = 2;
            }
        }

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x479c, 1.0f, 10.0f), cLib_addCalc0(&i_this->field_0x479c, 1.0f, 10.0f));
        break;
    }
    case 2:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x479c, -200.0f, 1.0f, 10.0f), cLib_addCalc2(&i_this->field_0x479c, -200.0f, 1.0f, 10.0f));

        if (i_this->mTimers[0] == 0) {
            i_this->mMode = 0;
            i_this->field_0x478c = 0;
            i_this->field_0x4790 = 1;

            i_this->mCoreHandNo += cM_rndF(7.0f) + 1.0f;
            if (i_this->mCoreHandNo >= 8) {
                i_this->mCoreHandNo -= 8;
            }
        }
        break;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_angle_add(i_this, a_this->shape_angle.z , 0xA00);
#else
    a_this->shape_angle.z += 0xA00;
#endif
}

static int bf_ct;

static void* s_bf_sub(void* i_this, void* i_data) {
    if (fopAcM_IsActor(i_this) && fopAcM_GetName(i_this) == fpcNm_E_OctBg_e) {
        bf_ct++;
    }

    return NULL;
}

static void* s_bfdel_sub(void* i_this, void* i_data) {
    if (fopAcM_IsActor(i_this) && fopAcM_GetName(i_this) == fpcNm_E_OctBg_e) {
        fopAcM_delete((fopAc_ac_c*)i_this);
    }

    return NULL;
}

static void* s_kaisoudel_sub(void* i_this, void* i_data) {
    if (fopAcM_IsActor(i_this) && fopAcM_GetName(i_this) == fpcNm_Obj_Kaisou_e) {
        fopAcM_delete((fopAc_ac_c*)i_this);
    }

    return NULL;
}

static void bombfishset(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz pos;
    i_this->mHitIFrameTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 20), 20);

    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, BCK_OI_WAIT, 5.0f, J3DFrameCtrl::EMode_LOOP, 3.0f);
        i_this->mMode = 1;

        if (a_this->field_0x567 == 0 && i_this->mDemoAction == 0) {
            i_this->mDemoAction = 60;
            a_this->field_0x567 = 1;
        }

        i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 150), 150);
        break;
    case 1:
#if TARGET_PC  // enemy attribute integration
    {
        const f32 actionSpeed = actor_attr::enemy_action_time_speed(i_this);
        const long currentBoundary = static_cast<long>(std::ceil(i_this->mTimers[0] * actionSpeed / 16.0f));
        const long previousBoundary = static_cast<long>(std::ceil((i_this->mTimers[0] + 1) * actionSpeed / 16.0f));
        for (long boundary = currentBoundary; boundary < previousBoundary; ++boundary) {
            const long remainingActionFrames = boundary * 16;
            if (remainingActionFrames <= 0 || remainingActionFrames >= 100) {
                continue;
            }
#else
        if (i_this->mTimers[0] < 100 && !(i_this->mTimers[0] & 15)) {
#endif
            bf_ct = 0;
            fpcM_Search(s_bf_sub, i_this);

            if (bf_ct < 10) {
                pos = a_this->home.pos;
                pos.y += (JREG_F(6) + 350.0f);

                csXyz angle(-0x3800, 0, 0);
                angle.y = cM_rndF(0x10000);

                fopAcM_createChild(fpcNm_E_OctBg_e, fopAcM_GetID(a_this), 0xFFFFFFFF, &pos,
                                   fopAcM_GetRoomNo(a_this), &angle, NULL, -1, NULL);
                i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_BG_APPEAR, -1);
            }
        }
#if TARGET_PC  // enemy attribute integration
        }
#endif

        if (i_this->mTimers[0] == 0) {
            if (i_this->mDemoAction != 0) {
                i_this->mDemoAction = 101;
            }

            i_this->mAction = OB_ACTION_CORE_HAND_MOVE;
            i_this->mMode = 0;
            anm_init(i_this, BCK_OI_WAIT, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        }
        break;
    }
}

static void core_hook(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    i_this->mHitIFrameTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 20), 20);

    switch (i_this->mMode) {
    case 0:
        i_this->mMode = 1;
        a_this->speedF = 0.0f;
        i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 3), 3);
        break;
    case 1:
        break;
    }

    if (i_this->mTimers[0] == 0 && !fopAcM_checkHookCarryNow(a_this)) {
        i_this->mAction = OB_ACTION_CORE_CHANCE;
        i_this->mMode = 0;
    }

    a_this->shape_angle.z += DUSK_IF_ELSE(actor_attr::enemy_action_s16_step(i_this, 0xA00), 0xA00);
}

static int core_chance(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz sp68;
    cXyz sp74;
    int var_r30 = 1;

    switch (i_this->mMode) {
    case 0:
        a_this->current.angle.x = 0;
        a_this->speedF = 0.0f;
        a_this->speed.y = 0.0f;

        sp68.x = a_this->home.pos.x - a_this->current.pos.x;
        sp68.z = a_this->home.pos.z - a_this->current.pos.z;
        if (JMAFastSqrt(sp68.x * sp68.x + sp68.z * sp68.z) < (NREG_F(4) + 500.0f)) {
            i_this->mMode = 2;
        } else {
            i_this->mMode = 1;
            i_this->mTimers[0] = 150;
        }
        break;
    case 1:
        if (i_this->mAcch.ChkGroundHit()) {
            if (i_this->mTimers[0] == 0) {
                i_this->mMode = 2;
            } else {
                a_this->speed.y = VREG_F(1) + cM_rndF(10.0f) + 10.0f + 10.0f;
                a_this->current.angle.y = cM_rndF(0x10000);
                a_this->speedF = VREG_F(2) + cM_rndF(3.0f) + 3.0f + 5.0f;
            }
        }

        if (a_this->speedF >= 20.0f) {
            i_this->mTimers[0] = 2;
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &a_this->speedF, 6.0f, 1.0f, 1.0f), cLib_addCalc2(&a_this->speedF, 6.0f, 1.0f, 1.0f));
        }
        break;
    case 2:
        i_this->mHitIFrameTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 20), 20);
        var_r30 = 2;

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &a_this->speedF, 50.0f, 1.0f, 5.0f), cLib_addCalc2(&a_this->speedF, 50.0f, 1.0f, 5.0f));
        i_this->mTargetMovePos = a_this->home.pos;
        i_this->mTargetMovePos.y += (VREG_F(6) + 350.0f);
        sp68 = i_this->mTargetMovePos - a_this->current.pos;

        f32 var_f31 = JMAFastSqrt(sp68.x * sp68.x + sp68.z * sp68.z);

        f32 var_f30 = var_f31 * 10.0f;
        if (var_f30 > 7000.0f) {
            var_f30 = 7000.0f;
        }

        f32 var_f29 = var_f31 * 20.0f;
        if (var_f29 > 10000.0f) {
            var_f29 = 10000.0f;
        }

#if TARGET_PC  // enemy attribute integration
        s16 sp58 = var_f30 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 1000));
        s16 sp50 = var_f30 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 800)) - var_f29;
#else
        s16 sp58 = var_f30 * cM_ssin(i_this->field_0x4750 * 1000);
        s16 sp50 = var_f30 * cM_ssin(i_this->field_0x4750 * 800) - var_f29;
#endif

        a_this->current.angle.y = sp58 + cM_atan2s(sp68.x, sp68.z);
        a_this->current.angle.x = sp50 - cM_atan2s(sp68.y, var_f31);

        if (sp68.abs() < 100.0f) {
            if (i_this->field_0x4744 != 0) {
                i_this->mAction = OB_ACTION_CORE_END;
                i_this->mMode = 0;
            } else {
                if (i_this->field_0x5d13 && i_this->mDemoAction == 0) {
                    i_this->mAction = OB_ACTION_BOMBFISH_SET;
                    i_this->mMode = 0;
                    i_this->field_0x5d13 = 1;
                } else {
                    i_this->mAction = OB_ACTION_CORE_HAND_MOVE;
                    i_this->mMode = 0;
                }

                i_this->field_0x478c = 0;
                i_this->field_0x4790 = 1;
                i_this->field_0x479c = -200.0f;

                i_this->mCoreHandNo += (cM_rndF(7.0f) + 1.0f);
                if (i_this->mCoreHandNo >= 8) {
                    i_this->mCoreHandNo -= 8;
                }
            }
        }
        break;
    }

    a_this->shape_angle.z += DUSK_IF_ELSE(actor_attr::enemy_action_s16_step(i_this, 0xA00), 0xA00);
    return var_r30;
}

static void core_end(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    int frame = i_this->mBodyParts[0].mpMorf->getFrame();
    i_this->mHitIFrameTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10), 10);

    cXyz particle_pos;
    switch (i_this->mMode) {
    case 0:
#if TARGET_PC  // enemy attribute integration
    {
#endif
        i_this->mMode = 1;
        a_this->gravity = 0.0f;
        i_this->mDemoAction = 20;
        i_this->field_0x479c = -1000.0f;
        anm_init(i_this, BCK_OI_TENTACLE_END, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);

        particle_pos.set(0.0f, 0.0f, 0.0f);
#if TARGET_PC  // enemy attribute integration
        cXyz effectScale = actor_attr::enemy_size_multiplier(i_this, 1.0f);
        dComIfGp_particle_set(dPa_RM(ID_ZI_S_OI_ISO_DISAPP_A), &particle_pos, NULL, &effectScale);
#else
        dComIfGp_particle_set(dPa_RM(ID_ZI_S_OI_ISO_DISAPP_A), &particle_pos, NULL, NULL);
#endif
        mDoAud_seStart(Z2SE_EN_OI_DEMO_APPEAR2, NULL, 0, 0);
        break;
#if TARGET_PC  // enemy attribute integration
    }
#endif
    case 1:
        if (DUSK_IF_ELSE(i_this->mBodyParts[0].mpMorf->checkFrame(100.0f), frame == 100)) {
            dComIfGp_getVibration().StartQuake(3, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (frame >= 100) {
            i_this->field_0x5ce4 = WREG_F(11) + 6.0f;
        }

        if (i_this->mBodyParts[0].mpMorf->isStop()) {
            i_this->mMode = 2;
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120), 120);
            dComIfGp_getVibration().StopQuake(31);
            a_this->home.pos.y = i_this->field_0x47a0 - 900.0f;
        }
        break;
    case 2:
        if (i_this->mTimers[0] == DUSK_IF_ELSE(actor_attr::enemy_sync_countdown_milestone(i_this, 120, 60), 60)) {
            for (int i = 0; i < 8; i++) {
                fopAc_ac_c* tentacle = fopAcM_SearchByID(i_this->mTentacleActorIDs[i]);
                fopAcM_delete(tentacle);
            }
        }

        if (i_this->mTimers[0] <= DUSK_IF_ELSE(actor_attr::enemy_sync_countdown_milestone(i_this, 120, 50), 50)) {
            i_this->field_0x5ce4 = WREG_F(11) + 6.0f;

            if (i_this->mTimers[0] == DUSK_IF_ELSE(actor_attr::enemy_sync_countdown_milestone(i_this, 120, 50), 50)) {
                dComIfGp_getVibration().StartQuake(4, 31, cXyz(0.0f, 1.0f, 0.0f));
            }
        }

        if (i_this->mTimers[0] == 0) {
            i_this->mCoreBattleMode = 0;
            i_this->mFishBattleMode = 1;
            i_this->mZoneSwTimer = 0;
            a_this->home.pos.y = i_this->field_0x47a0;
            a_this->health = 2;

            MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getBaseTRMtx(),
                    mDoMtx_stack_c::get());
            mDoMtx_stack_c::multVecZero(&a_this->current.pos);
            a_this->current.angle.y = 0;
            a_this->current.angle.x = -0x4000;
            a_this->shape_angle.x = -0x4000;

            i_this->mAction = OB_ACTION_FISH_NORMAL;
            i_this->mMode = 1;

            i_this->mMoveAngle.y = a_this->current.angle.y;
            i_this->mMoveAngle.x = -0x4000;
            i_this->field_0x4750 = 0;
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 110), 110);
            anm_init(i_this, BCK_OI_APPEAR02, 1.0f, J3DFrameCtrl::EMode_NONE, 1.0f);

            fopAcM_OffStatus(a_this, 0x80000);
            fopAcM_OnStatus(a_this, 0x200000);
            i_this->mTimers[3] = DUSK_IF_ELSE(actor_attr::enemy_sync_terminal_timer(i_this, VREG_S(8) + 219), VREG_S(8) + 220);

            for (int i = 0; i < 8; i++) {
                i_this->mTentacleActorIDs[i] =
                    fopAcM_createChild(fpcNm_B_OH2_e, fopAcM_GetID(a_this), i, &a_this->current.pos,
                                       fopAcM_GetRoomNo(a_this), NULL, NULL, -1, NULL);
            }
        }
        break;
    }
}

static void core_action(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz sp30;
    cXyz sp3C;

    if (i_this->mCoreAnm != 0) {
        anm_init(i_this, i_this->mCoreAnm, 10.0f, i_this->mCoreAnmMode, 1.0f);
        i_this->mCoreAnm = 0;
    }

    BOOL attn_ON = true;
    BOOL look_at_player = true;
    int var_r26 = 0;
    BOOL check_eat = false;
    BOOL play_move_sound = true;

    switch (i_this->mAction) {
    case OB_ACTION_CORE_START:
        core_start(i_this);
        look_at_player = false;
        break;
    case OB_ACTION_CORE_HAND_MOVE:
        core_hand_move(i_this);
        check_eat = true;
        break;
    case OB_ACTION_BOMBFISH_SET:
        bombfishset(i_this);
        play_move_sound = false;
        break;
    case OB_ACTION_CORE_HOOK:
        core_hook(i_this);
        look_at_player = false;
        play_move_sound = false;
        break;
    case OB_ACTION_CORE_CHANCE:
        var_r26 = core_chance(i_this);
        look_at_player = false;
        break;
    case OB_ACTION_CORE_END:
        core_end(i_this);
        var_r26 = 0;
        look_at_player = false;
        attn_ON = false;
        break;
    }

    if (check_eat && i_this->mDemoAction == 0) {
        sp30 = dComIfGp_getPlayer(0)->current.pos - a_this->home.pos;
#if TARGET_PC
		if (JMAFastSqrt(sp30.x * sp30.x + sp30.z * sp30.z) < DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, VREG_F(17) + 300.0f), VREG_F(17) + 300.0f) && sp30.y < DUSK_IF_ELSE((VREG_F(18) + 800.0f) * (1.0f + (actor_attr::enemy_size_multiplier(i_this) - 1.0f) * 0.5f), VREG_F(18) + 800.0f)) {
#else
        if (JMAFastSqrt(sp30.x * sp30.x + sp30.z * sp30.z) < (VREG_F(17) + 300.0f) && sp30.y < (VREG_F(18) + 800.0f)) {
#endif
            i_this->mDemoAction = 50;
        }
    }

    if (attn_ON && i_this->field_0x479c > -1.0f && a_this->eyePos.y >= a_this->home.pos.y) {
        fopAcM_OnStatus(a_this, 0);
        a_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
    } else {
        fopAcM_OffStatus(a_this, 0);
        a_this->attention_info.flags = 0;
    }

    if (look_at_player) {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.y, i_this->mYAngleToPlayer, 2, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.x, i_this->mXAngleToPlayer, 2, 0x800);
#else
        cLib_addCalcAngleS2(&a_this->current.angle.y, i_this->mYAngleToPlayer, 2, 0x800);
        cLib_addCalcAngleS2(&a_this->current.angle.x, i_this->mXAngleToPlayer, 2, 0x800);
#endif
    }

    if (var_r26 == 1) {
        cMtx_YrotS(*calc_mtx, a_this->current.angle.y);

        sp30.x = 0.0f;
        sp30.y = 0.0f;
        sp30.z = a_this->speedF;
        MtxPosition(&sp30, &sp3C);
        a_this->speed.x = sp3C.x;
        a_this->speed.z = sp3C.z;

#if TARGET_PC  // enemy attribute integration
        a_this->current.pos += actor_attr::enemy_move_step(i_this, a_this->speed);
        a_this->speed.y += actor_attr::enemy_velocity_gravity_step(i_this, -(VREG_F(0) + 3.0f));
#else
        a_this->current.pos += a_this->speed;
        a_this->speed.y -= (VREG_F(0) + 3.0f);
#endif

        f32 temp_y_offset = VREG_F(11) + 20.0f;
        a_this->current.pos.y -= temp_y_offset;
        a_this->old.pos.y -= temp_y_offset;

        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        a_this->current.pos.y += temp_y_offset;
        a_this->old.pos.y += temp_y_offset;
    } else if (var_r26 == 2) {
        cMtx_YrotS(*calc_mtx, a_this->current.angle.y);
        cMtx_XrotM(*calc_mtx, a_this->current.angle.x);

        sp30.x = 0.0f;
        sp30.y = 0.0f;
        sp30.z = a_this->speedF;
        MtxPosition(&sp30, &a_this->speed);
        a_this->current.pos += DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, a_this->speed), a_this->speed);
    }

    if (play_move_sound) {
        f32 dist_to_oldpos = (a_this->current.pos - a_this->old.pos).abs();
        i_this->mSound.startCreatureSoundLevel(Z2SE_EN_OI_CORE_MOVE, dist_to_oldpos, -1);
    }
}

static void core_damage_check(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;

    if (fopAcM_CheckStatus(a_this, 0x100000)) {
        i_this->field_0x5d38 =
            dComIfGp_particle_set(i_this->field_0x5d38, dPa_RM(ID_ZI_S_OI_ISO_COREHIT_B), &a_this->current.pos, NULL, NULL);
    }

    i_this->mCoreCcStts.Move();

    if (i_this->mHitIFrameTimer == 0 && i_this->mCcSph.ChkTgHit()) {
        i_this->mAtInfo.mpCollider = i_this->mCcSph.GetTgHitObj();
        cc_at_check(a_this, &i_this->mAtInfo);

        dComIfGp_particle_set(dPa_RM(ID_ZI_S_OI_ISO_COREHIT_A), &a_this->current.pos, NULL, NULL);

        if (i_this->mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
            i_this->mAction = OB_ACTION_CORE_HOOK;
            i_this->mMode = 0;
            i_this->mSound.startCreatureSound(Z2SE_EN_OI_CORE_PULLOUT, 0, -1);
            dComIfGs_onOneZoneSwitch(7, -1);
        } else {
            a_this->current.angle.y = i_this->mYAngleToPlayer + 0x8000;
            a_this->speedF = 30.0f;
            a_this->speed.y = 7.0f;

            i_this->mSound.startCreatureVoice(Z2SE_EN_OI_V_CORE_DAMAGE, -1);

            if (a_this->health <= 0 && !dComIfGp_event_runCheck()) {
                i_this->field_0x4744 = 1;
            } else {
                i_this->field_0x5d13 = 1;
            }
        }

        i_this->mHitIFrameTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10), 10);
    }
}

static void fish_damage_check(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);

    i_this->mBodyCcStts.Move();

    if (i_this->mHitIFrameTimer == 0) {
        for (int i = 0; i < 19; i++) {
            if (i_this->mBodyParts[i].mSph.ChkTgHit()) {
                i_this->mHitIFrameTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10), 10);
                def_se_set(&i_this->mOISound, i_this->mBodyParts[i].mSph.GetTgHitObj(), 0x2A, NULL);
            }
        }

        if (i_this->field_0x4756 == 0 && i_this->mCoreSph.ChkTgHit()) {
            i_this->mAtInfo.mpCollider = i_this->mCoreSph.GetTgHitObj();
            cc_at_check(a_this, &i_this->mAtInfo);

            if (i_this->mAtInfo.mpCollider->ChkAtType(AT_TYPE_HOOKSHOT)) {
                i_this->mTimers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 40), 40);
                i_this->field_0x4756 = 1;

                if (i_this->mAction != OB_ACTION_FISH_NORMAL) {
                    i_this->mAction = OB_ACTION_FISH_NORMAL;
                    i_this->mMode = 0;
                    OS_REPORT("B_OB HOOK VAC OFF !!\n");
                }
            }
        }

        fopEn_enemy_c* e_this = (fopEn_enemy_c*)a_this;
        if (e_this->checkCutDownHitFlg()) {
            e_this->offCutDownHitFlg();
            i_this->field_0x5d18 = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(i_this, 125), 125);

            JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->field_0x5d38);
            if (emitter != NULL) {
                emitter->becomeDeleteEmitter();
            }

            i_this->field_0x5d38 = 0;
            dComIfGp_getVibration().StartShock(5, 31, cXyz(0.0f, 1.0f, 0.0f));
            i_this->mHangHitCount++;
            dComIfGs_onOneZoneSwitch(9, -1);

            if (player->getCutType() == daPy_py_c::CUT_TYPE_FINISH_STAB) {
#if TARGET_PC  // enemy attribute integration
                i_this->mHangHitCount = fish_hits_per_mount(i_this);
                i_this->mHangFinishCount = fish_mount_count(i_this);
#else
                i_this->mHangHitCount = 4;
                i_this->mHangFinishCount = 3;
#endif
                i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_TODOME, -1);
                i_this->mSound.startCollisionSE(Z2SE_HIT_SWORD_STAB, 0x20);
            } else {
                i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_DAMAGE, -1);
                i_this->mSound.startCollisionSE(Z2SE_HIT_SWORD_STAB, 0x1F);
            }

            if (i_this->mHangHitCount >= DUSK_IF_ELSE(fish_hits_per_mount(i_this), 4)) {
                i_this->mHangFinishCount++;
                if (i_this->mHangFinishCount >= DUSK_IF_ELSE(fish_down_mount(i_this), 2)) {
                    e_this->onDownFlg();
                }

                i_this->mTimers[2] = 100;

#if TARGET_PC  // enemy attribute integration
                if (i_this->mHangFinishCount >= fish_mount_count(i_this)) {
                    i_this->mHangTimer = fish_hang_time_for_hits(i_this, 250.0f);
#else
                if (i_this->mHangFinishCount >= 3) {
                    i_this->mHangTimer = 250;
#endif
                } else {
                    i_this->mHangTimer = DUSK_IF_ELSE(fish_hang_time_for_hits(i_this, 210.0f), 210);
                }
            }

            i_this->field_0x47b0 = (JREG_S(2) + 20);
        }
    }
}

static BOOL pl_check(b_ob_class* i_this, f32 i_range, s16 i_maxAngle) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);

    if (fopAcM_searchPlayerDistance(a_this) < DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, i_range), i_range)) {
        s16 y_diff = a_this->current.angle.y - i_this->mYAngleToPlayer;
        s16 x_diff = a_this->current.angle.x - i_this->mXAngleToPlayer;

        if (y_diff < i_maxAngle && y_diff > (s16)-i_maxAngle && x_diff < i_maxAngle &&
            x_diff > (s16)-i_maxAngle)
        {
            if (!fopAcM_otherBgCheck(a_this, player)) {
                return true;
            }
        }
    }

    return false;
}

static int fish_normal(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);

    cXyz sp64;
    f32 dist_to_ground = fabsf(i_this->mAcch.GetGroundH() - a_this->current.pos.y);
    BOOL throw_player = false;
    f32 dist_to_player = (a_this->eyePos - player->current.pos).abs();

    if (i_this->field_0x4756 == 1) {
        if (dist_to_player < DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, KREG_F(11) + 150.0f), KREG_F(11) + 150.0f)) {
            Z2GetAudioMgr()->changeBgmStatus(2);
            i_this->field_0x4756 = 2;
        }
    } else if (i_this->field_0x4756 == 2) {
        if (dist_to_player >= DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, KREG_F(11) + 150.0f), KREG_F(11) + 150.0f)) {
            i_this->field_0x4756 = 0;
            Z2GetAudioMgr()->changeBgmStatus(1);
        }
    }

    BOOL player_hang_on;
    if (dist_to_player < DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, KREG_F(11) + 150.0f), KREG_F(11) + 150.0f)) {
        player_hang_on = true;
        i_this->field_0x47b4 = 0;
        moveSW2 = false;
    } else {
        player_hang_on = false;
    }

    int ret = 0;
    BOOL in_opening_demo = false;
    if (i_this->mDemoAction >= 20 && i_this->mDemoAction < 30) {
        in_opening_demo = true;
    }

    static cXyz sdemo_p[4] = {
        cXyz(2000.0f, 3500.0f, 0.0f),
        cXyz(-1000.0f, 2000.0f, 1000.0f),
        cXyz(0.0f, 3500.0f, 2000.0f),
        cXyz(1000.0f, 2000.0f, -1000.0f),
    };

    switch (i_this->mMode) {
    case 0:
        if (in_opening_demo) {
            i_this->mTargetMovePos = a_this->home.pos + sdemo_p[i_this->mDemoMovePosIdx];
            i_this->mDemoMovePosIdx++;
            i_this->mDemoMovePosIdx &= 3;

            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30), 30);
            i_this->field_0x47b4 = 0;
        } else {
            i_this->mTargetMovePos.x = a_this->home.pos.x + cM_rndFX(2000.0f);
            i_this->mTargetMovePos.y = a_this->home.pos.y + 1000.0f + cM_rndF(2000.0f);
            i_this->mTargetMovePos.z = a_this->home.pos.z + cM_rndFX(2000.0f);
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(30.0f) + 30.0f), cM_rndF(30.0f) + 30.0f);
        }

        sp64 = i_this->mTargetMovePos - a_this->current.pos;
        i_this->mMoveAngle.y = cM_atan2s(sp64.x, sp64.z);
        i_this->mMoveAngle.x = -cM_atan2s(sp64.y, JMAFastSqrt(sp64.x * sp64.x + sp64.z * sp64.z));

        if (i_this->mMoveAngle.x > 0x1000) {
            i_this->mMoveAngle.x = 0x1000;
        } else if (i_this->mMoveAngle.x < -0x1000) {
            i_this->mMoveAngle.x = -0x1000;
        }

        i_this->mMode = 1;
        i_this->field_0x476a = 0;
        /* fallthrough */
    case 1: {
        if (in_opening_demo && i_this->mTimers[0] > DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 60), 60)) {
            ret = 4;
        }

        if (i_this->mAnmID != BCK_OI_SWIMWAIT) {
            anm_init(i_this, BCK_OI_SWIMWAIT, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        }

        if (dist_to_ground < l_HIO.mBodySize * (ZREG_F(8) + 1200.0f)) {
            if (i_this->mMoveAngle.x > 0) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &i_this->mMoveAngle.x, 0, 4, 0x40), cLib_addCalcAngleS2(&i_this->mMoveAngle.x, 0, 4, 0x40));
            }

            if (a_this->current.angle.x > 0) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.x, 0, 4, 0x40), cLib_addCalcAngleS2(&a_this->current.angle.x, 0, 4, 0x40));
            }
        }

        s16 y_diff = a_this->current.angle.y - i_this->mMoveAngle.y;
        s16 x_diff = a_this->current.angle.x - i_this->mMoveAngle.x;
        if (i_this->mTimers[0] == 0 && y_diff <= 0x100 && y_diff >= (s16)-0x100 &&
            x_diff <= 0x100 && x_diff >= (s16)-0x100)
        {
            i_this->mMode = 0;
            i_this->field_0x47b4++;
        }

        if (!player_hang_on && i_this->field_0x47b4 >= 3 && pl_check(i_this, 5000.0f, 0x2000)) {
            i_this->mMode = 2;
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(50.0f) + 200.0f), cM_rndF(50.0f) + 200.0f);
            i_this->field_0x47b4 = 0;
            i_this->field_0x476a = 0;
            i_this->mTimers[4] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30), 30);
        }
        break;
    }
    case 2:
        ret = 1;
        i_this->mTargetMovePos.x = player->current.pos.x;

        if (dist_to_ground < l_HIO.mBodySize * (ZREG_F(8) + 1200.0f)) {
            i_this->mTargetMovePos.y = player->current.pos.y + 300.0f + AREG_F(7);
        } else {
            i_this->mTargetMovePos.y = player->current.pos.y;
        }

        i_this->mTargetMovePos.z = player->current.pos.z;

        sp64 = i_this->mTargetMovePos - a_this->current.pos;
        i_this->mMoveAngle.y = cM_atan2s(sp64.x, sp64.z);
        i_this->mMoveAngle.x = -cM_atan2s(sp64.y, JMAFastSqrt(sp64.x * sp64.x + sp64.z * sp64.z));
        sp64.abs();

        if (!daPy_getPlayerActorClass()->checkHookshotShootReturnMode() &&
            i_this->mTimers[4] == 0 && pl_check(i_this, BREG_F(17) + 3000.0f, 0x3000))
        {
            i_this->mAction = OB_ACTION_FISH_VACUME;
            i_this->mMode = 0;
            moveSW2 = true;
            return 1;
        }

        if (player_hang_on || daPy_getPlayerActorClass()->checkEquipHeavyBoots() ||
            !pl_check(i_this, 10000.0f, 0x3000) || i_this->mTimers[0] == 0)
        {
            i_this->mMode = 1;

            if (dist_to_ground < l_HIO.mBodySize * (ZREG_F(8) + 1200.0f)) {
                i_this->mMoveAngle.x = -0x2000;
                i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(30.0f) + 30.0f), cM_rndF(30.0f) + 30.0f);
            } else {
                i_this->mMoveAngle.x = -0x1000;
                i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(10.0f) + 20.0f), cM_rndF(10.0f) + 20.0f);
            }
        }
        break;
    case 10:
        ret = 1;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_angle_add(i_this, i_this->mMoveAngle.y , 0x200);
#else
        i_this->mMoveAngle.y += 0x200;
#endif
        i_this->field_0x476a = 0x200;
        i_this->mMoveAngle.x = -0x1000;

        if (i_this->mTimers[2] == 0) {
            i_this->mMode = 1;
        }
        break;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.y, i_this->mMoveAngle.y, 0x10, i_this->field_0x476a);
    actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.x, i_this->mMoveAngle.x, 0x10, i_this->field_0x476a);
#else
    cLib_addCalcAngleS2(&a_this->current.angle.y, i_this->mMoveAngle.y, 0x10, i_this->field_0x476a);
    cLib_addCalcAngleS2(&a_this->current.angle.x, i_this->mMoveAngle.x, 0x10, i_this->field_0x476a);
#endif

    if (in_opening_demo) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x476a, 0x100, 1, 2), cLib_addCalcAngleS2(&i_this->field_0x476a, 0x100, 1, 2));
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x476a, 0x180, 1, 8), cLib_addCalcAngleS2(&i_this->field_0x476a, 0x180, 1, 8));
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc0(i_this, &i_this->mSuiBrkFrame, 1.0f, 0.5f);
    actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5d04, 0.7f, 1.0f, 0.05f);
#else
    cLib_addCalc0(&i_this->mSuiBrkFrame, 1.0f, 0.5f);
    cLib_addCalc2(&i_this->field_0x5d04, 0.7f, 1.0f, 0.05f);
#endif

    a_this->speedF = TREG_F(0) + 100.0f;

    if (player_hang_on) {
        ret = 3;
        i_this->mHangTimer++;

        if (i_this->mHangTimer == (JREG_S(5) + DUSK_IF_ELSE(fish_hang_time_for_hits(i_this, 210.0f), 210))) {
            i_this->field_0x47b0 = (JREG_S(3) + 35);
        }

        if (i_this->mHangTimer == (JREG_S(6) + DUSK_IF_ELSE(fish_hang_time_for_hits(i_this, 230.0f), 230))) {
            player->setOctaIealWildHang();
        }

        if (i_this->mHangTimer == (JREG_S(7) + DUSK_IF_ELSE(fish_hang_time_for_hits(i_this, 284.0f), 284))) {
            throw_player = true;

            if (i_this->mTimers[2] != 0) {
                if (i_this->mHangFinishCount >= DUSK_IF_ELSE(fish_mount_count(i_this), 3)) {
                    i_this->mAction = OB_ACTION_FISH_END;
                    i_this->mMode = 0;
                    i_this->mDemoAction = 40;
                } else {
                    i_this->mMode = 10;
                    i_this->mTimers[2] = 80;
                }
            }
        }

        if (DUSK_IF_ELSE(i_this->mHangTimer >= fish_hang_time_for_hits(i_this, 230.0f) || i_this->mHangHitCount >= fish_hits_per_mount(i_this), i_this->mHangTimer >= 230 || i_this->mHangHitCount >= 4)) {
            i_this->field_0x47b0 = (JREG_S(3) + 25);
        }
    } else {
        i_this->mHangTimer = 0;
        i_this->mHangHitCount = 0;
    }

    if (i_this->mTimers[3] != 0) {
        i_this->field_0x5ce4 = JREG_F(11) + 6.0f;

        cXyz sp70;
        MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0), mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp70);

        i_this->field_0x5d24[0] =
            dComIfGp_particle_set(i_this->field_0x5d24[0], dPa_RM(ID_ZI_S_OI_FISH_APP_A), &sp70, NULL, NULL);

        sp64.set(0.0f, 0.0f, 0.0f);
        i_this->field_0x5d24[1] =
            dComIfGp_particle_set(i_this->field_0x5d24[1], dPa_RM(ID_ZI_S_OI_FISH_APP_B), &sp64, NULL, NULL);

        if (i_this->mTimers[3] == 1) {
            dComIfGp_getVibration().StopQuake(31);
        }
    }

    if (i_this->mAnmID == BCK_OI_APPEAR02 && i_this->mBodyParts[0].mpMorf->isStop()) {
        anm_init(i_this, BCK_OI_SWIMWAIT, 10.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
    }

    if (throw_player) {
        player->cancelOctaIealHang();
        daPy_getPlayerActorClass()->setThrowDamage(a_this->current.angle.y + 0x8000, 40.0f, KREG_F(14) + 50.0f,
                                                   0, 0, 2);
        i_this->mAttnOffTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_u8_timer(i_this, 70), 70);
    }

    return ret;
}

static int fish_vacume(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);

    cXyz sp3C;
    int var_r30 = 0;

    switch (i_this->mMode) {
    case 0:
        anm_init(i_this, BCK_OI_OPENMOUTH, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
        i_this->mMode = 1;
        i_this->field_0x476a = 0;
        break;
    case 1:
        if (i_this->mBodyParts[0].mpMorf->isStop()) {
            anm_init(i_this, BCK_OI_OPENMOUTHWAIT, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
            i_this->mMode = 2;
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 200), 200);
        }
        break;
    case 2:
        i_this->mMoveAngle.y = i_this->mYAngleToPlayer;
        i_this->mMoveAngle.x = 0;
        var_r30 = 1;

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, 0.0f, 0.05f, 5.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, 0.0f, 0.05f, 5.0f);
#else
        cLib_addCalc2(&a_this->current.pos.x, 0.0f, 0.05f, 5.0f);
        cLib_addCalc2(&a_this->current.pos.z, 0.0f, 0.05f, 5.0f);
#endif

        if (i_this->mTimers[0] == 0) {
            i_this->mAction = OB_ACTION_FISH_NORMAL;
            i_this->mMode = 0;
        } else if ((a_this->eyePos - player->current.pos).abs() > (KREG_F(11) + 150.0f) &&
                   i_this->mTimers[0] < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 100), 100))
        {
            if (!daPy_getPlayerActorClass()->checkEquipHeavyBoots() &&
                pl_check(i_this, 3000.0f, 0x4000))
            {
                i_this->mDemoAction = 10;
                i_this->mMode = 3;
            }
        }
        break;
    case 3:
        if (i_this->mDemoActionTimer < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30), 30)) {
            var_r30 = 1;
        }

        i_this->mBodyParts[0].mSph.OffCoSetBit();

        if (i_this->mBodyParts[0].mpMorf->isStop()) {
            if (i_this->mAnmID == BCK_OI_SWALLOW) {
                anm_init(i_this, BCK_OI_EAT02, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
            } else if (i_this->mAnmID == BCK_OI_EAT02) {
                anm_init(i_this, BCK_OI_THROWUP, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
                i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_THROWUP, -1);
            } else if (i_this->mAnmID == BCK_OI_THROWUP) {
                i_this->mAction = OB_ACTION_FISH_NORMAL;
                i_this->mMode = 0;
            }
        }
        break;
    }

    if (var_r30) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->mSuiBrkFrame, 29.0f, 1.0f, 0.5f), cLib_addCalc2(&i_this->mSuiBrkFrame, 29.0f, 1.0f, 0.5f));
        i_this->mOISound.startCreatureVoiceLevel(Z2SE_EN_OI_V_DRAWIN, -1);
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc0(i_this, &i_this->mSuiBrkFrame, 1.0f, 0.5f), cLib_addCalc0(&i_this->mSuiBrkFrame, 1.0f, 0.5f));
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.y, i_this->mMoveAngle.y, 0x10, i_this->field_0x476a);
    actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.x, i_this->mMoveAngle.x, 0x10, i_this->field_0x476a);
    actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x476a, 0x180, 1, 8);
    actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5d04, 0.0f, 1.0f, 0.05f);
#else
    cLib_addCalcAngleS2(&a_this->current.angle.y, i_this->mMoveAngle.y, 0x10, i_this->field_0x476a);
    cLib_addCalcAngleS2(&a_this->current.angle.x, i_this->mMoveAngle.x, 0x10, i_this->field_0x476a);
    cLib_addCalcAngleS2(&i_this->field_0x476a, 0x180, 1, 8);
    cLib_addCalc2(&i_this->field_0x5d04, 0.0f, 1.0f, 0.05f);
#endif
    return 2;
}

static int fish_end(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    cXyz sp68;

    f32 speed_f = 100.0f;
    s16 angleX_max_step = 0x300;

    switch (i_this->mMode) {
    case 0:
        i_this->mMode = 1;
        i_this->mTargetMovePos.set(0.0f, -16000.0f, 6500.0f);
        i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 100), 100);
        i_this->field_0x5d04 = KREG_F(8) + 1.5f;
        i_this->mBlureRateTarget = 150;
        i_this->field_0x5dd8 = 3;
        anm_init(i_this, BCK_OI_LASTDAMAGE, 0.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
        break;
    case 1:
        if (i_this->mBodyParts[0].mpMorf->isStop()) {
            anm_init(i_this, BCK_OI_SWIMWAIT, 5.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        }

        if (i_this->mTimers[0] == 0) {
            i_this->mMode = 2;
            i_this->mTargetMovePos.set(0.0f, -19000.0f, -6500.0f);
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 150), 150);
            i_this->mDemoCamEye.set(-6000.0f, -22000.0f, 2000.0f);
            anm_init(i_this, BCK_OI_LASTDAMAGE, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
        }
        break;
    case 2:
        sp68.set(VREG_F(0) + -1325.0f, -24000.0f, VREG_F(1) + 9178.0f);
        player->setPlayerPosAndAngle(&sp68, VREG_S(1) + 10000, 0);

        if (i_this->mTimers[0] == 0) {
            i_this->mMode = 3;
            i_this->mTargetMovePos.set(0.0f, TREG_F(14) + -23550.0f, 10200.0f);
            i_this->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120), 120);
            i_this->mDemoCamEye.set(2130.0f, -23071.0f, 7383.0f);
            i_this->mDemoAction = 42;
            i_this->mDemoActionTimer = 0;
            i_this->mBlureRateTarget = 0;
            anm_init(i_this, BCK_OI_LASTDAMAGE, 5.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
        }
        break;
    case 3:
        if (i_this->mAcch.ChkWallHit() && i_this->mTimers[0] == 0) {
            anm_init(i_this, BCK_OI_DEAD, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
            dComIfGp_getVibration().StartShock(8, 31, cXyz(0.0f, 1.0f, 0.0f));
            mDoAud_seStart(Z2SE_OBJ_BOMB_EXPLODE, NULL, 0, 0);

            i_this->mMode = 10;
            i_this->field_0x5ce4 = WREG_F(14) + 20.0f;
            i_this->mTimers[0] = 0;
            a_this->current.angle.x = 0x1000;
            i_this->mMoveAngle.x = 0;
            i_this->mMoveAngle.y = 0;
            i_this->field_0x5d08 = 50;

            moveSW2 = true;
            i_this->mDemoAction = 43;
            i_this->mDemoActionTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 1), 1);

            obj_lv3WaterB_class* water =
                (obj_lv3WaterB_class*)fopAcM_SearchByName(fpcNm_OBJ_LV3WATERB_e);
            water->mAction = LV3WATERB_ACT_REMOVE;
            a_this->home.pos.set(0.0f, -23580.0f, 10300.0f);

            for (int i = 0; i < 5; i++) {
                static u16 iso_eff[] = {dPa_RM(ID_ZI_S_OI_FISH_SYOTOTSU_A), dPa_RM(ID_ZI_S_OI_FISH_SYOTOTSU_B), dPa_RM(ID_ZI_S_OI_FISH_SYOTOTSU_C), dPa_RM(ID_ZI_S_OI_FISH_SYOTOTSU_D), dPa_RM(ID_ZI_S_OI_FISH_SYOTOTSU_E)};
                dComIfGp_particle_set(iso_eff[i], &a_this->home.pos, NULL, NULL);
            }

            mDoAud_seStart(Z2SE_EN_OI_HIT_WALL, &a_this->home.pos, 0, 0);
            speed_f = 0.0f;
        }
        break;
    case 10:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->mTargetMovePos.x, 0.1f, 10.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, VREG_F(7) + -23550.0f, 0.1f, 10.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, (VREG_F(8) + 9128.0f) - 500.0f, 0.1f, 10.0f);
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->mTargetMovePos.x, 0.1f, 10.0f);
        cLib_addCalc2(&a_this->current.pos.y, VREG_F(7) + -23550.0f, 0.1f, 10.0f);
        cLib_addCalc2(&a_this->current.pos.z, (VREG_F(8) + 9128.0f) - 500.0f, 0.1f, 10.0f);
#endif
        speed_f = 0.0f;

        i_this->field_0x4750--;
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.z, 0x4000, 0x20, 0x80), cLib_addCalcAngleS2(&a_this->current.angle.z, 0x4000, 0x20, 0x80));
        break;
    }

    s16 angleY_offset = 0;
    s16 angleX_offset = 0;

    if (speed_f > 1.0f) {
        sp68 = i_this->mTargetMovePos - a_this->current.pos;
        f32 var_f30 = JMAFastSqrt(sp68.x * sp68.x + sp68.z * sp68.z);

        i_this->mMoveAngle.y = cM_atan2s(sp68.x, sp68.z);
        i_this->mMoveAngle.x = -cM_atan2s(sp68.y, var_f30);

        f32 temp_f2 = var_f30 * 2.0f;
        if (temp_f2 > 12000.0f) {
            temp_f2 = 12000.0f;
        }

#if TARGET_PC  // enemy attribute integration
        angleY_offset = temp_f2 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 1300));
        angleX_offset = (temp_f2 / 2) * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 1100));
#else
        angleY_offset = temp_f2 * cM_ssin(i_this->field_0x4750 * 1300);
        angleX_offset = (temp_f2 / 2) * cM_ssin(i_this->field_0x4750 * 1100);
#endif
    } else {
        angleX_max_step = 0x20;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.y, i_this->mMoveAngle.y + angleY_offset, 0x10, 0x300);
    actor_attr::enemy_add_action_angle(i_this, &a_this->current.angle.x, i_this->mMoveAngle.x + angleX_offset, 0x20, angleX_max_step);
#else
    cLib_addCalcAngleS2(&a_this->current.angle.y, i_this->mMoveAngle.y + angleY_offset, 0x10, 0x300);
    cLib_addCalcAngleS2(&a_this->current.angle.x, i_this->mMoveAngle.x + angleX_offset, 0x20, angleX_max_step);
#endif
    a_this->speedF = speed_f;
    return 4;
}

static f32 buf_p[] = {
    0.0f, 1.0f,  2.0f,  3.0f,  4.0f,  5.0f,  6.0f,  7.0f,  8.0f,  8.8f,
    9.6f, 10.4f, 11.2f, 12.0f, 12.6f, 13.2f, 13.8f, 14.4f, 15.0f,
};

static f32 size_p[] = {1.0f, 1.05f, 1.075f, 1.1f,  1.1f,  1.075f, 1.05f,
                                    1.0f, 0.9f,  0.85f,  0.8f,  0.75f, 0.7f,   0.65f,
                                    0.6f, 0.55f, 0.5f,   0.45f, 1.0f};

static void dmcalc(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    int i;
    ob_part_s* part;

    cXyz sp2C;
    cXyz sp20;
    cXyz sp14;

    f32 var_f29;
    f32 var_f28 = 0.0f;

    cMtx_YrotS(*calc_mtx, a_this->shape_angle.y);
    cMtx_XrotM(*calc_mtx, a_this->shape_angle.x);
    cMtx_YrotM(*calc_mtx, i_this->field_0x47c4);

    sp2C.x = 0.0f;
    sp2C.y = 0.0f;

    if (i_this->mAction == OB_ACTION_FISH_END) {
        sp2C.z = WREG_F(3) + -40.0f;
        var_f28 = NREG_F(11) + -5.0f;
    } else {
        sp2C.z = AREG_F(15) + -20.0f;
    }

    MtxPosition(&sp2C, &sp14);
    sp2C.x = 0.0f;
    sp2C.y = 0.0f;
    f32 temp_f30 = 1.0f;

    part = i_this->mBodyParts;

    i_this->mBodyParts[0].field_0x26 = i_this->mBodyParts[0].mAngle.y;
    i_this->mBodyParts[0].mPos = a_this->current.pos;
    i_this->mBodyParts[0].mAngle = a_this->shape_angle;
    i_this->mBodyParts[0].mAngle.z = i_this->field_0x47c4;
    part++;

    if (i_this->field_0x5d08 != 0) {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_tick_visual_timer(i_this, i_this->field_0x5d08, i_this->field_0x4750);
#else
        i_this->field_0x5d08--;
#endif
    }

    f32 var_f27 = (f32)i_this->field_0x5d08 * 0.6f;

    f32 var_f26;
    if (a_this->speedF < 1.0f) {
        var_f26 = JREG_F(19) + -10.0f;
    } else {
        var_f26 = 0.0f;
    }

    for (i = 1; i < 20; i++, part++) {
        // this block is almost certainly fake, but cant figure out a better way to fix instruction
        // order
        int var_r27 = i * (JREG_S(6) + 5000);
        int temp_r3 = (JREG_S(4) + i_this->field_0x5d08 * (JREG_S(5) + 1200));
        int temp_r0 = (temp_r3 + var_r27);
        temp_r0 = temp_r0;

        f32 temp_f2 = var_f27 * cM_ssin(temp_r0 + 3000);
        f32 temp_f3 = var_f26 * cM_ssin(JREG_S(7) + var_r27 + 3000);

        f32 temp_f4;
        if (i == (KREG_S(7) + 18)) {
            sp2C.z = DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, part->mSize * (WREG_F(17) + 250.0f)), part->mSize * (WREG_F(17) + 250.0f));
            temp_f4 = part->mSize * (WREG_F(9) + 0.4f);
        } else {
            sp2C.z = DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, part->mSize * (WREG_F(14) + 450.0f)), part->mSize * (WREG_F(14) + 450.0f));
            temp_f4 = part->mSize;
        }

        f32 temp_f1 = part->mPos.x;
        temp_f1 -= part[-1].mPos.x;

        var_f29 = temp_f3 + (temp_f1 + (sp14.x * temp_f30));
        f32 temp_f2_2 = temp_f2 + (var_f28 + (part->mPos.y + (sp14.y * temp_f30)));

        if (temp_f2_2 < i_this->field_0x47a0 + (temp_f4 * (NREG_F(12) + 350.0f))) {
            temp_f2_2 = i_this->field_0x47a0 + (temp_f4 * (NREG_F(12) + 350.0f));
        }

        temp_f30 *= (WREG_F(15) + 1.0f);

        f32 var_f24 = temp_f2_2 - part[-1].mPos.y;
        f32 var_f23 = (part->mPos.z - part[-1].mPos.z) + (sp14.z * temp_f30);
        part[-1].field_0x26 = part[-1].mAngle.y;
        part->mAngle.y = cM_atan2s(var_f29, var_f23);
        part->mAngle.x =
            -cM_atan2s(var_f24, JMAFastSqrt(var_f29 * var_f29 + var_f23 * var_f23));

        cMtx_YrotS(*calc_mtx, part->mAngle.y);
        cMtx_XrotM(*calc_mtx, part->mAngle.x);
        MtxPosition(&sp2C, &sp20);
        part->mPos = part[-1].mPos + sp20;
        part->mAngle.y += -0x8000;
        part->mAngle.x *= -1;
    }
}

#if TARGET_PC  // enemy attribute integration
static s16 trail_angle_lerp(s16 from, s16 to, f32 fraction) {
#endif

#if TARGET_PC  // enemy attribute integration
    const s16 delta = static_cast<s16>(to - from);
    return static_cast<s16>(static_cast<s32>(from) + static_cast<s32>(delta * fraction));
}
#endif

#if TARGET_PC  // enemy attribute integration
static csXyz trail_angles_lerp(const csXyz& from, const csXyz& to, f32 fraction) {

    return csXyz(trail_angle_lerp(from.x, to.x, fraction), trail_angle_lerp(from.y, to.y, fraction), trail_angle_lerp(from.z, to.z, fraction));
}

static void sample_body_trail(b_ob_class* i_this, f32 lag, cXyz& pos, csXyz& angle) {

    // Never wrap an oversized segment delay back to the head. Beyond the
    // finite history, extend its oldest tangent instead of overlapping parts.
    const f32 boundedLag = std::clamp(lag, 0.0f, 510.0f);
    const int whole = static_cast<int>(boundedLag);
    const int recent = (i_this->field_0x2320 - whole) & 0x1FF;
    const int older = (recent - 1) & 0x1FF;
    const f32 fraction = std::max(lag, 0.0f) - whole;
    pos = i_this->field_0x2324[recent] + (i_this->field_0x2324[older] - i_this->field_0x2324[recent]) * fraction;
    angle = trail_angles_lerp(i_this->field_0x3b24[recent], i_this->field_0x3b24[older], std::min(fraction, 1.0f));
}

#endif

static void fish_move(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz sp84;
    cXyz sp90;

    if (moveSW) {
        int var_r16 = 0;
        BOOL attn_ON = true;

        i_this->mBodyParts[0].mSph.OnCoSetBit();

        switch (i_this->mAction) {
        case OB_ACTION_FISH_NORMAL:
            var_r16 = fish_normal(i_this);
            break;
        case OB_ACTION_FISH_VACUME:
            var_r16 = fish_vacume(i_this);
            break;
        case OB_ACTION_FISH_END:
            var_r16 = fish_end(i_this);
            attn_ON = false;
            break;
        }

        if (i_this->mAttnOffTimer != 0) {
            i_this->mAttnOffTimer--;
        }

        if (attn_ON && i_this->mAttnOffTimer == 0) {
            fopAcM_OnStatus(a_this, 0);
            a_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        } else {
            fopAcM_OffStatus(a_this, 0);
            a_this->attention_info.flags = 0;
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, a_this->current.angle.y, 2, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, a_this->current.angle.x, 2, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, a_this->current.angle.z, 2, 0x800);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.y, a_this->current.angle.y, 2, 0x800);
        cLib_addCalcAngleS2(&a_this->shape_angle.x, a_this->current.angle.x, 2, 0x800);
        cLib_addCalcAngleS2(&a_this->shape_angle.z, a_this->current.angle.z, 2, 0x800);
#endif

        s16 var_r17_2 = 0;
        f32 temp_f1 = 0.0f;

        if (var_r16 == 0) {
            var_r17_2 = VREG_S(5) + 700;
            temp_f1 = 30.0f;
        } else if (var_r16 == 1) {
            var_r17_2 = VREG_S(6) + 1600;
            temp_f1 = 30.0f;
        } else if (var_r16 == 2) {
            var_r17_2 = VREG_S(5) + 1000;
            temp_f1 = 15.0f;
        } else if (var_r16 == 3) {
            var_r17_2 = VREG_S(7) + 1600;
            temp_f1 = VREG_F(18) + 30.0f;
        } else if (var_r16 == 4) {
            var_r17_2 = VREG_S(5) + 700;
        } else if (var_r16 == 5) {
            var_r17_2 = VREG_S(8) + 1000;
            temp_f1 = KREG_F(18) + 30.0f;
        } else if (var_r16 == 6) {
            var_r17_2 = VREG_S(8) + 1200;
            temp_f1 = KREG_F(18) + 40.0f;
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x47c0, temp_f1, 0.5f, 1.0f);
        actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x47bc, var_r17_2, 1, 20);
        actor_attr::enemy_angle_add(i_this, i_this->field_0x47ba , i_this->field_0x47bc);
#else
        cLib_addCalc2(&i_this->field_0x47c0, temp_f1, 0.5f, 1.0f);
        cLib_addCalcAngleS2(&i_this->field_0x47bc, var_r17_2, 1, 20);
        i_this->field_0x47ba += i_this->field_0x47bc;
#endif

        s16 temp_r16 = i_this->field_0x47c0 * (a_this->speedF * cM_ssin(i_this->field_0x47ba));
        cMtx_YrotS(*calc_mtx, a_this->current.angle.y);
        cMtx_XrotM(*calc_mtx, a_this->current.angle.x);
        cMtx_YrotM(*calc_mtx, temp_r16);

        sp84.x = 0.0f;
        sp84.y = 0.0f;
        sp84.z = i_this->field_0x5d04 * (a_this->speedF * 0.6f * l_HIO.mBodySize);
        MtxPosition(&sp84, &a_this->speed);
        a_this->current.pos += DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, a_this->speed), a_this->speed);

        i_this->mOISound.startCreatureSoundLevel(
            Z2SE_EN_OI_MOVE, i_this->field_0x5d04 * (a_this->speedF * 0.6f * l_HIO.mBodySize),
            dComIfGp_getReverb(fopAcM_GetRoomNo(a_this)));
        i_this->field_0x3b24[i_this->field_0x2320].z = temp_r16;
        i_this->field_0x47c4 = temp_r16;
    }

#if TARGET_PC  // enemy attribute integration
    const f32 trailSpeed = actor_attr::enemy_action_time_speed(i_this);
    const f32 trailSize = actor_attr::enemy_size_multiplier(i_this);
    if (moveSW && trailSpeed != 1.0f) {
        // Store one sample per action frame, not per Execute. Otherwise a
        // faster head stretches the body and a slower one collapses it.
        const int samples = actor_attr::enemy_action_event_count(i_this, static_cast<u16>(i_this->field_0x4750), 1);
        const int previous = (i_this->field_0x2320 - 1) & 0x1FF;
        const cXyz from = i_this->field_0x2324[previous];
        const csXyz fromAngle = i_this->field_0x3b24[previous];
        const csXyz toAngle(a_this->current.angle.x, a_this->current.angle.y, i_this->field_0x47c4);
        for (int sample = 0; sample < samples; ++sample) {
            const int index = (i_this->field_0x2320 + sample) & 0x1FF;
            const f32 fraction = static_cast<f32>(sample + 1) / samples;
            i_this->field_0x2324[index] = from + (a_this->current.pos - from) * fraction;
            i_this->field_0x3b24[index] = trail_angles_lerp(fromAngle, toAngle, fraction);
        }
        // The existing increment at the end advances to the next free slot.
        i_this->field_0x2320 = (i_this->field_0x2320 + samples - 1) & 0x1FF;
    } else {

#endif
    i_this->field_0x2324[i_this->field_0x2320] = a_this->current.pos;
    i_this->field_0x3b24[i_this->field_0x2320].y = a_this->current.angle.y;
    i_this->field_0x3b24[i_this->field_0x2320].x = a_this->current.angle.x;
#if TARGET_PC  // enemy attribute integration
    }
#endif

    ob_part_s* part = i_this->mBodyParts;
    int var_r22 = 0;
    s16 var_r17_2;

    f32 var_f31;
    if (!moveSW2) {
        var_f31 = (TREG_F(1) + 7.5f) / i_this->field_0x5d04;
    } else {
        dmcalc(i_this);
    }

    for (int i = 0; i < 19 && !part->mHide; i++, part++) {
        s16 var_r25;

        if (!moveSW2) {
            if (i >= 1) {
                var_r22 = XREG_S(0) + 4;
                var_r17_2 = 0;
            } else {
                if (i_this->field_0x47b0 == 100) {
                    var_r17_2 = cM_ssin(i_this->field_0x47b0 * 3000) * JREG_F(15);
                } else {
                    var_r17_2 =
#if TARGET_PC  // enemy attribute integration
                        i_this->field_0x47b0 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 3000)) * (JREG_F(12) + 170.0f);
#else
                        i_this->field_0x47b0 * cM_ssin(i_this->field_0x4750 * 3000) * (JREG_F(12) + 170.0f);
#endif
                    if (i_this->field_0x47b0 != 0) {
#if TARGET_PC  // enemy attribute integration
                        actor_attr::enemy_tick_visual_timer(i_this, i_this->field_0x47b0, i_this->field_0x4750);
#else
                        i_this->field_0x47b0--;
#endif
                    }
                }
            }

            int sp48 = var_f31 * buf_p[i];
            int temp_r4 = ((i_this->field_0x2320 - (s16)var_r22) - sp48) & 0x1FF;
            IF_NOT_DUSK(part->mPos = i_this->field_0x2324[temp_r4];)
            part->field_0x26 = part->mAngle.y + part->mAngle.z;
#if TARGET_PC  // enemy attribute integration
            if (trailSpeed == 1.0f && trailSize == 1.0f) {
                part->mPos = i_this->field_0x2324[temp_r4];
#endif
            part->mAngle = i_this->field_0x3b24[temp_r4];
#if TARGET_PC  // enemy attribute integration
            } else if (i == 0) {
                part->mPos = a_this->current.pos;
                part->mAngle.set(a_this->current.angle.x, a_this->current.angle.y, i_this->field_0x47c4);
            } else {
                sample_body_trail(i_this, (var_f31 * buf_p[i] + var_r22) * trailSize, part->mPos, part->mAngle);
            }
#endif
            part->mSize = l_HIO.mBodySize * size_p[i];
            var_r25 = (s16)(part->field_0x26 - (part->mAngle.y + part->mAngle.z));
        } else {
            var_r17_2 = 0;
            var_r25 = (s16) - (part->field_0x26 - part->mAngle.y);
        }

        mDoMtx_stack_c::transS(part->mPos.x, part->mPos.y, part->mPos.z);
#if TARGET_PC  // enemy attribute integration
        mDoMtx_stack_c::scaleM(actor_attr::enemy_size_multiplier(i_this, part->mSize));
#else
        mDoMtx_stack_c::scaleM(part->mSize, part->mSize, part->mSize);
#endif
        mDoMtx_stack_c::YrotM(part->mAngle.y);
        mDoMtx_stack_c::XrotM(part->mAngle.x + var_r17_2);
        mDoMtx_stack_c::YrotM(part->mAngle.z);
        mDoMtx_stack_c::ZrotM(a_this->shape_angle.z);

        if (i == 0) {
            mDoMtx_stack_c::push();
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x47ae, var_r25 * (VREG_S(4) + 7), 4, 0x400), cLib_addCalcAngleS2(&i_this->field_0x47ae, var_r25 * (VREG_S(4) + 7), 4, 0x400));
            mDoMtx_stack_c::pop();
        } else if (i == 18) {
            f32 temp_f28 = YREG_F(0) + 1000.0f;
            f32 temp_f27 = YREG_F(1) + 4000.0f;

            for (int j = 0; j < 3; j++) {
                i_this->field_0x47a4[j] =
#if TARGET_PC  // enemy attribute integration
                    temp_f28 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , YREG_S(1) + 1000, j * (YREG_S(2) + -12000)));
#else
                    temp_f28 * cM_ssin(i_this->field_0x4750 * (YREG_S(1) + 1000) + (j * (YREG_S(2) + -12000)));
#endif
            }

#if TARGET_PC  // enemy attribute integration
            i_this->field_0x47aa = temp_f27 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , YREG_S(2) + 1200));
            actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x47ac, var_r25 * (VREG_S(3) + 4), 4, 0x400);
#else
            i_this->field_0x47aa = temp_f27 * cM_ssin(i_this->field_0x4750 * (YREG_S(2) + 1200));
            cLib_addCalcAngleS2(&i_this->field_0x47ac, var_r25 * (VREG_S(3) + 4), 4, 0x400);
#endif
        }

        if (!moveSW2) {
            if (i == 18) {
                mDoMtx_stack_c::transM(0.0f, 0.0f, XREG_F(1) + 250.0f);
            } else if (i != 0) {
                mDoMtx_stack_c::transM(0.0f, 0.0f, XREG_F(0) + 600.0f);
            }
        } else if (i == 18) {
            mDoMtx_stack_c::transM(0.0f, 0.0f, (XREG_F(1) + 250.0f) - 150.0f);
        } else if (i != 0) {
            mDoMtx_stack_c::transM(0.0f, 0.0f, (XREG_F(0) + 600.0f) - 250.0f);
        }

        if (i == 0) {
            part->mpMorf->play(NULL, 0, 0);

            if (i_this->mAnmID == BCK_OI_EAT02 && i_this->mBodyParts[0].mpMorf->checkFrame(1.0f)) {
                i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_EAT02, -1);
            }
        }

        part->mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
        part->mpMorf->modelCalc();

        if (i == 0 && i_this->mSuiBrkFrame > 0.0f) {
            i_this->mpSuiBrk->setFrame(i_this->mSuiBrkFrame);
            i_this->mpSuiBtk->play();

            MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0), mDoMtx_stack_c::get());
            mDoMtx_stack_c::XrotM(VREG_S(1) + 0x4000);
            mDoMtx_stack_c::transM(0.0f, VREG_F(0) + 1300.0f, 0.0f);
            i_this->mpSuiModel->setBaseTRMtx(mDoMtx_stack_c::get());
        }

        if (i >= 4 && i <= 17) {
            pno = i;
            fin = 0;

            if (part->field_0x3e == 0) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &part->mFinARot.x, -4000, 0x20, 0x28), cLib_addCalcAngleS2(&part->mFinARot.x, -4000, 0x20, 0x28));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &part->mFinARot.x, 0, 8, 100), cLib_addCalcAngleS2(&part->mFinARot.x, 0, 8, 100));
            }

            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &part->mFinARot.y, var_r25 * (TREG_S(9) + 7), 4, 0x400), cLib_addCalcAngleS2(&part->mFinARot.y, var_r25 * (TREG_S(9) + 7), 4, 0x400));

            mDoMtx_stack_c::push();
            mDoMtx_stack_c::transM(KREG_F(0), KREG_F(1) + 510.00003f, KREG_F(2) + -390.00003f);
            part->mpFinMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
            part->mpFinMorf->modelCalc();
            mDoMtx_stack_c::pop();
        }

        if (i >= 7 && i <= 15) {
            pno = i;
            fin = 1;

            mDoMtx_stack_c::push();
            mDoMtx_stack_c::transM(ZREG_F(0), ZREG_F(1) + -500.00003f, ZREG_F(2) + -290.00003f);
            mDoMtx_stack_c::ZrotM(-0x8000);
            part->mpFinUnkMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
            part->mpFinUnkMorf->modelCalc();
            mDoMtx_stack_c::pop();
        }

        if (i == 8) {
            pno = i;

            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &part->mFinBRot.y, var_r25 * (VREG_S(8) + 7), 4, 0x400), cLib_addCalcAngleS2(&part->mFinBRot.y, var_r25 * (VREG_S(8) + 7), 4, 0x400));

            mDoMtx_stack_c::push();
            mDoMtx_stack_c::transM(KREG_F(3), KREG_F(4) + -360.0f, KREG_F(5) + -360.0f);
            part->mpFinBMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
            part->mpFinBMorf->modelCalc();
            mDoMtx_stack_c::pop();
        }

        if (i == 17) {
            pno = i;

            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &part->mFinCRot.y, var_r25 * (TREG_S(8) + 13), 4, 0x400), cLib_addCalcAngleS2(&part->mFinCRot.y, var_r25 * (TREG_S(8) + 13), 4, 0x400));

            mDoMtx_stack_c::push();
            mDoMtx_stack_c::transM(KREG_F(6), KREG_F(7) + -480.00003f, KREG_F(8) + -120.00001f);
            part->mpFinCMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
            part->mpFinCMorf->modelCalc();
            mDoMtx_stack_c::pop();
        }

        if (i == 0) {
            MTXCopy(part->mpMorf->getModel()->getBaseTRMtx(), *calc_mtx);

            sp84.set(0.0f, 0.0f, VREG_F(2) + 700.0f);
            MtxPosition(&sp84, &sp90);
            part->mSph.SetC(sp90);
            part->mSph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, part->mSize * (TREG_F(16) + 400.0f)), part->mSize * (TREG_F(16) + 400.0f)));

            sp84.set(0.0f, 0.0f, VREG_F(1));
            MtxPosition(&sp84, &sp90);
            i_this->mFishHeadSph.SetC(sp90);
            i_this->mFishHeadSph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, part->mSize * (TREG_F(16) + 450.0f)), part->mSize * (TREG_F(16) + 450.0f)));

            dComIfG_Ccsp()->Set(&i_this->mFishHeadSph);
        } else {
            part->mSph.SetC(part->mPos);

            if (i != 18) {
                part->mSph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, part->mSize * (TREG_F(18) + 450.0f)), part->mSize * (TREG_F(18) + 450.0f)));
            } else {
                part->mSph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, part->mSize * (TREG_F(17) + 200.0f)), part->mSize * (TREG_F(17) + 200.0f)));
            }
        }

        dComIfG_Ccsp()->Set(&part->mSph);
    }

    if (moveSW) {
        i_this->field_0x2320++;
        i_this->field_0x2320 &= 0x1FF;
    }

    f32 var_f31_2 = DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, l_HIO.mBodySize * ((VREG_F(17) + 600.0f) * 0.6f)), l_HIO.mBodySize * ((VREG_F(17) + 600.0f) * 0.6f));
    a_this->current.pos.y -= var_f31_2;
    a_this->old.pos.y -= var_f31_2;
    i_this->mAcch.CrrPos(dComIfG_Bgsp());
    a_this->current.pos.y += var_f31_2;
    a_this->old.pos.y += var_f31_2;

    MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getBaseTRMtx(), mDoMtx_stack_c::get());
    mDoMtx_stack_c::transM(XREG_F(5), XREG_F(6) + 390.0f, XREG_F(7) + 400.0f + 11.0f);
    mDoMtx_stack_c::multVecZero(&sp90);
    a_this->eyePos = sp90;
    a_this->attention_info.position = a_this->eyePos;
    a_this->attention_info.position.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, 50.0f), 50.0f);

    if (i_this->mDemoAction == 21 || i_this->mDemoAction == 22) {
        sp84 = dComIfGp_getCamera(0)->view.lookat.eye - sp90;
    } else {
        sp84 = player->eyePos - sp90;
    }

    s16 temp_r15_2 = -cM_atan2s(sp84.y, sp84.z);
    int temp_r16_2 = cM_atan2s(sp84.x, JMAFastSqrt(sp84.y * sp84.y + sp84.z * sp84.z));

    mDoMtx_stack_c::transS(sp90.x, sp90.y, sp90.z);
    IF_DUSK(mDoMtx_stack_c::scaleM(actor_attr::enemy_size_multiplier(i_this, l_HIO.mCoreSize));)
    mDoMtx_stack_c::XrotM(temp_r15_2);
    mDoMtx_stack_c::YrotM(temp_r16_2);

    i_this->field_0x4740 = cM_ssin(DUSK_IF_ELSE(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 1000), i_this->field_0x4750 * 1000)) * 9.5f + 9.5f;
    i_this->mpCoreMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    i_this->mpCoreMorf->setFrame(i_this->field_0x4740);
    i_this->mpCoreMorf->modelCalc();

    i_this->mCoreSph.SetC(sp90);
    i_this->mCoreSph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, l_HIO.mCoreSize * 80.0f), l_HIO.mCoreSize * 80.0f));
    dComIfG_Ccsp()->Set(&i_this->mCoreSph);
    i_this->mFishBattleMode = 2;
}

static void cam_3d_morf(b_ob_class* i_this, f32 i_scale) {
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, i_this->mDemoCamCenterTarget.x, i_scale,
#else
    cLib_addCalc2(&i_this->mDemoCamCenter.x, i_this->mDemoCamCenterTarget.x, i_scale,
#endif
                  i_this->field_0x5cc0 * i_this->field_0x5cd0);
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, i_this->mDemoCamCenterTarget.y, i_scale,
#else
    cLib_addCalc2(&i_this->mDemoCamCenter.y, i_this->mDemoCamCenterTarget.y, i_scale,
#endif
                  i_this->field_0x5cc4 * i_this->field_0x5cd0);
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, i_this->mDemoCamCenterTarget.z, i_scale,
#else
    cLib_addCalc2(&i_this->mDemoCamCenter.z, i_this->mDemoCamCenterTarget.z, i_scale,
#endif
                  i_this->field_0x5cc8 * i_this->field_0x5cd0);

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.x, i_this->mDemoCamEyeTarget.x, i_scale,
#else
    cLib_addCalc2(&i_this->mDemoCamEye.x, i_this->mDemoCamEyeTarget.x, i_scale,
#endif
                  i_this->field_0x5cb4 * i_this->field_0x5cd0);
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.y, i_this->mDemoCamEyeTarget.y, i_scale,
#else
    cLib_addCalc2(&i_this->mDemoCamEye.y, i_this->mDemoCamEyeTarget.y, i_scale,
#endif
                  i_this->field_0x5cb8 * i_this->field_0x5cd0);
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.z, i_this->mDemoCamEyeTarget.z, i_scale,
#else
    cLib_addCalc2(&i_this->mDemoCamEye.z, i_this->mDemoCamEyeTarget.z, i_scale,
#endif
                  i_this->field_0x5cbc * i_this->field_0x5cd0);
}

static void* s_hasidel_sub(void* i_this, void* i_data) {
    if (fopAcM_IsActor(i_this) && fopAcM_GetName(i_this) == fpcNm_OCTHASHI_e) {
        fopAcM_delete((fopAc_ac_c*)i_this);
    }

    return NULL;
}

static void demo_camera(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
    camera_process_class* camera = dComIfGp_getCamera(dComIfGp_getPlayerCameraID(0));
    camera_process_class* camera0 = dComIfGp_getCamera(0);

    cXyz sp58;
    cXyz sp64;
    cXyz sp70;
    cXyz sp7C;
    cXyz sp88;

    b_oh_class* tentacle =
        (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i_this->field_0x5ce8]);
    int var_r27 = 0;

    switch (i_this->mDemoAction) {
    case 1:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFEF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 2;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 55.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();
        daPy_getPlayerActorClass()->changeDemoMode(46, 0, 0, 0);

        i_this->mDemoCamEye = camera0->view.lookat.eye;
        i_this->mDemoCamCenter = camera0->view.lookat.center;
        sp58 = i_this->mDemoCamEye - tentacle->current.pos;

        i_this->field_0x5ce0 = cM_atan2s(sp58.x, sp58.z);

        tentacle->field_0x1f68 = player->current.pos;
        tentacle->field_0x1f74 = player->shape_angle;
        tentacle->field_0x1f7c = 0.1f;

        i_this->field_0x5cd8 = 300.0f;
        i_this->field_0x5cdc = 1500.0f;
        /* fallthrough */
    case 2:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, player->current.pos.x, 0.3f,
#else
        cLib_addCalc2(&i_this->mDemoCamCenter.x, player->current.pos.x, 0.3f,
#endif
                      i_this->field_0x5cd0 * 200.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, player->current.pos.y + 150.0f, 0.3f,
#else
        cLib_addCalc2(&i_this->mDemoCamCenter.y, player->current.pos.y + 150.0f, 0.3f,
#endif
                      i_this->field_0x5cd0 * 200.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, player->current.pos.z, 0.3f,
#else
        cLib_addCalc2(&i_this->mDemoCamCenter.z, player->current.pos.z, 0.3f,
#endif
                      i_this->field_0x5cd0 * 200.0f);

        cMtx_YrotS(*calc_mtx, i_this->field_0x5ce0);

        sp58.x = 0.0f;
        sp58.y = i_this->field_0x5cd8;
        sp58.z = i_this->field_0x5cdc;
        MtxPosition(&sp58, &sp64);

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer > actor_attr::enemy_sync_timer(i_this, 30)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd8, 1050.0f, 0.2f, 15.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cdc, 1000.0f, 0.2f, 10.0f);
#else
        if (i_this->mDemoActionTimer > 30) {
            cLib_addCalc2(&i_this->field_0x5cd8, 1050.0f, 0.2f, 15.0f);
            cLib_addCalc2(&i_this->field_0x5cdc, 1000.0f, 0.2f, 10.0f);
#endif
        }

        sp64 += tentacle->current.pos;

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.x, sp64.x, 0.1f, i_this->field_0x5cd0 * 200.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.y, sp64.y, 0.1f, i_this->field_0x5cd0 * 200.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.z, sp64.z, 0.1f, i_this->field_0x5cd0 * 200.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, 1.0f, 1.0f, TREG_F(7) + 0.1f);
#else
        cLib_addCalc2(&i_this->mDemoCamEye.x, sp64.x, 0.1f, i_this->field_0x5cd0 * 200.0f);
        cLib_addCalc2(&i_this->mDemoCamEye.y, sp64.y, 0.1f, i_this->field_0x5cd0 * 200.0f);
        cLib_addCalc2(&i_this->mDemoCamEye.z, sp64.z, 0.1f, i_this->field_0x5cd0 * 200.0f);
        cLib_addCalc2(&i_this->field_0x5cd0, 1.0f, 1.0f, TREG_F(7) + 0.1f);
#endif

        MTXCopy(tentacle->mpMorf->getModel()->getAnmMtx(29), mDoMtx_stack_c::get());

        mDoMtx_stack_c::transM(ZREG_F(0) + 50.0f, ZREG_F(1) + -100.0f,
                               -(ZREG_F(2) + 80.0f) * (f32)tentacle->field_0xc88);

        if (tentacle->mAction == OH_ACTION_CAUGHT) {
            player->setPlayerPosAndAngle(mDoMtx_stack_c::get());
        } else {
            csXyz sp174;

            mDoMtx_MtxToRot(mDoMtx_stack_c::get(), &sp174);
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &tentacle->field_0x1f74.y, sp174.y, 1, 0x800), cLib_addCalcAngleS2(&tentacle->field_0x1f74.y, sp174.y, 1, 0x800));
            mDoMtx_stack_c::multVecZero(&sp64);

#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x1f7c, 1.0f, 1.0f, 0.1f);
            actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x1f68.x, sp64.x, 1.0f, tentacle->field_0x1f7c * 100.0f);
            actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x1f68.y, sp64.y, 1.0f, tentacle->field_0x1f7c * 100.0f);
            actor_attr::enemy_add_action_calc2(i_this, &tentacle->field_0x1f68.z, sp64.z, 1.0f, tentacle->field_0x1f7c * 100.0f);
#else
            cLib_addCalc2(&tentacle->field_0x1f7c, 1.0f, 1.0f, 0.1f);
            cLib_addCalc2(&tentacle->field_0x1f68.x, sp64.x, 1.0f, tentacle->field_0x1f7c * 100.0f);
            cLib_addCalc2(&tentacle->field_0x1f68.y, sp64.y, 1.0f, tentacle->field_0x1f7c * 100.0f);
            cLib_addCalc2(&tentacle->field_0x1f68.z, sp64.z, 1.0f, tentacle->field_0x1f7c * 100.0f);
#endif
            player->setPlayerPosAndAngle(&tentacle->field_0x1f68, &tentacle->field_0x1f74);
        }
        break;
    case 3:
        player->setPlayerPosAndAngle(&a_this->home.pos, &tentacle->field_0x1f74);

        cMtx_YrotS(*calc_mtx, i_this->field_0x5ce0);
        sp58.x = 0.0f;
        sp58.y = i_this->field_0x5cd8;
        sp58.z = i_this->field_0x5cdc;
        MtxPosition(&sp58, &sp64);

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd8, 800.0f, 0.2f, 10.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cdc, 1000.0f, 0.2f, 10.0f);
#else
        cLib_addCalc2(&i_this->field_0x5cd8, 800.0f, 0.2f, 10.0f);
        cLib_addCalc2(&i_this->field_0x5cdc, 1000.0f, 0.2f, 10.0f);
#endif

        sp64 += tentacle->current.pos;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.x, sp64.x, 0.1f, i_this->field_0x5cd0 * 200.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.y, sp64.y, 0.1f, i_this->field_0x5cd0 * 200.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.z, sp64.z, 0.1f, i_this->field_0x5cd0 * 200.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, 1.0f, 1.0f, TREG_F(7) + 0.1f);
#else
        cLib_addCalc2(&i_this->mDemoCamEye.x, sp64.x, 0.1f, i_this->field_0x5cd0 * 200.0f);
        cLib_addCalc2(&i_this->mDemoCamEye.y, sp64.y, 0.1f, i_this->field_0x5cd0 * 200.0f);
        cLib_addCalc2(&i_this->mDemoCamEye.z, sp64.z, 0.1f, i_this->field_0x5cd0 * 200.0f);
        cLib_addCalc2(&i_this->field_0x5cd0, 1.0f, 1.0f, TREG_F(7) + 0.1f);
#endif

        if (DUSK_IF_ELSE(actor_attr::enemy_action_event_count(i_this, i_this->field_0x4750 , 16) > 0, !(i_this->field_0x4750 & 15))) {
            dComIfGp_getVibration().StartShock(4, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (i_this->mAnmID == BCK_OI_EAT && i_this->mBodyParts[0].mpMorf->checkFrame(153.0f)) {
            i_this->mDemoAction = 4;
            i_this->mDemoActionTimer = 0;

            sp64 = a_this->home.pos;
            sp64.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, KREG_F(13) + 400.0f), KREG_F(13) + 400.0f);
            player->setPlayerPosAndAngle(&sp64, &tentacle->field_0x1f74);

            daPy_getPlayerActorClass()->setThrowDamage(a_this->home.angle.y, DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, 10.0f) * actor_attr::enemy_action_time_speed(i_this), 10.0f),
                                                       DUSK_IF_ELSE((KREG_F(14) + 30.0f) * JMAFastSqrt(actor_attr::enemy_size_multiplier(i_this)), KREG_F(14) + 30.0f), 4, 1, 2);
            daPy_getPlayerActorClass()->changeDemoMode(1, 0, 0, 0);
        }
        break;
    case 4:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, player->current.pos.x, 0.4f, 300.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, player->current.pos.y, 0.4f, 300.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, player->current.pos.z, 0.4f, 300.0f);
#else
        cLib_addCalc2(&i_this->mDemoCamCenter.x, player->current.pos.x, 0.4f, 300.0f);
        cLib_addCalc2(&i_this->mDemoCamCenter.y, player->current.pos.y, 0.4f, 300.0f);
        cLib_addCalc2(&i_this->mDemoCamCenter.z, player->current.pos.z, 0.4f, 300.0f);
#endif

        if (i_this->mBodyParts[0].mpMorf->isStop() || DUSK_IF_ELSE(actor_attr::enemy_action_time_speed(i_this) < 1.0f && i_this->mDemoActionTimer >= i_this->mBodyParts[0].mpMorf->getEndFrame() - 153.0f, false)) {
            i_this->mDemoAction = 100;
            i_this->mCoreAnm = BCK_OI_WAIT;
            i_this->mCoreAnmMode = J3DFrameCtrl::EMode_LOOP;
            i_this->field_0x4794 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 100), 100);
        }
        break;
    case 10:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFEF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 11;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 55.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();

        i_this->field_0x5cf8 = player->shape_angle.y;
        i_this->field_0x5cfc = 0;
        i_this->field_0x5d00 = ZREG_F(3) + 1000.0;

        anm_init(i_this, BCK_OI_OPENMOUTHWAIT, 3.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);

        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x5cd4 = 500.0f;
        } else {
            i_this->field_0x5cd4 = -500.0f;
        }
        /* fallthrough */
    case 11: {
        f32 temp_f31 = (a_this->eyePos - player->current.pos).abs();
        if (temp_f31 < DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, KREG_F(11) + 150.0f), KREG_F(11) + 150.0f)) {
            i_this->mDemoAction = 100;
            break;
        }

        MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0x13), mDoMtx_stack_c::get());

        sp58.x = i_this->field_0x5d00;
        sp58.y = 0.0;
        sp58.z = 0.0;
        mDoMtx_stack_c::multVec(&sp58, &i_this->field_0x5cec);

        if (i_this->field_0x5cec.y < a_this->home.pos.y) {
            i_this->field_0x5cec.y = a_this->home.pos.y;
        }

        if (i_this->mDemoActionTimer >= DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 118), 118)) {
            daPy_getPlayerActorClass()->changeDemoMode(46, 0, 0, 0);
        }

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer < actor_attr::enemy_sync_timer(i_this, 75)) {
            if (actor_attr::enemy_action_event_count(i_this, i_this->field_0x4750 , 8) > 0) {
#else
        if (i_this->mDemoActionTimer < 75) {
            if (!(i_this->field_0x4750 & 7)) {
#endif
                dComIfGp_getVibration().StartShock(1, 0x1f, cXyz(0.0f, 1.0f, 0.0f));
            }
        } else if (DUSK_IF_ELSE(i_this->mDemoActionTimer < actor_attr::enemy_sync_timer(i_this, 146) && actor_attr::enemy_action_event_count(i_this, i_this->field_0x4750 , 16) > 0, i_this->mDemoActionTimer < 146 && !(i_this->field_0x4750 & 15))) {
            dComIfGp_getVibration().StartShock(4, 0x1f, cXyz(0.0f, 1.0f, 0.0f));
        }

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 138)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5d00, 700.0f, 0.1f, 200.0f);
#else
        if (i_this->mDemoActionTimer >= 138) {
            cLib_addCalc2(&i_this->field_0x5d00, 700.0f, 0.1f, 200.0f);
#endif

            if (i_this->mDemoActionTimer > DUSK_IF_ELSE(std::min<int>(actor_attr::enemy_sync_timer(i_this, 170), actor_attr::enemy_sync_timer(i_this, 138) + 32), 170)) {
                i_this->mDemoAction = 100;
            }

            if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 138), 138)) {
                daPy_getPlayerActorClass()->setThrowDamage(a_this->current.angle.y, 0.0f, 0.0f, 4,
                                                           1, 2);
                daPy_getPlayerActorClass()->changeDemoMode(1, 0, 0, 0);
            }
        } else {
            i_this->field_0x5cf8 += 0x300;

#if TARGET_PC  // enemy attribute integration
            if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 15)) {
                if (i_this->mDemoActionTimer == actor_attr::enemy_sync_timer(i_this, 30)) {
#else
            if (i_this->mDemoActionTimer >= 15) {
                if (i_this->mDemoActionTimer == 30) {
#endif
                    anm_init(i_this, BCK_OI_SWALLOW, 2.0f, J3DFrameCtrl::EMode_NONE, 1.0f);
                    i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_SWALLOW, -1);
                }

#if TARGET_PC  // enemy attribute integration
                actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x5d00, 1.0f, i_this->field_0x5cfc);
                actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cfc, 100.0f, 1.0f, 2.0f);
#else
                cLib_addCalc0(&i_this->field_0x5d00, 1.0f, i_this->field_0x5cfc);
                cLib_addCalc2(&i_this->field_0x5cfc, 100.0f, 1.0f, 2.0f);
#endif
            }
        }

        player->setPlayerPosAndAngle(&i_this->field_0x5cec, i_this->field_0x5cf8, 0);

        cMtx_YrotS(*calc_mtx, a_this->current.angle.y);
        sp58.x = i_this->field_0x5cd4;
        sp58.y = ZREG_F(12) + 500.0f;
        sp58.z = ZREG_F(13) + 2000.0f;
        MtxPosition(&sp58, &i_this->mDemoCamEye);
        i_this->mDemoCamEye += a_this->current.pos;

        if (i_this->mDemoActionTimer == 0) {
            i_this->mDemoCamCenter = i_this->field_0x5cec;
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, player->current.pos.x, 0.4f, 300.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, player->current.pos.y, 0.4f, 300.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, player->current.pos.z, 0.4f, 300.0f);
#else
            cLib_addCalc2(&i_this->mDemoCamCenter.x, player->current.pos.x, 0.4f, 300.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.y, player->current.pos.y, 0.4f, 300.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.z, player->current.pos.z, 0.4f, 300.0f);
#endif
        }
        break;
    }
    case 20:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFFF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 21;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 55.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();
        daPy_getPlayerActorClass()->changeDemoMode(0x17, 0, 0, 0);

        i_this->mDemoCamCenter.x = -371.0f;
        i_this->mDemoCamCenter.y = KREG_F(11) + -23869.0;
        i_this->mDemoCamCenter.z = -1247.0f;
        i_this->mDemoCamEye.x = -473.0f;
        i_this->mDemoCamEye.y = -23869.0f;
        i_this->mDemoCamEye.z = -1581.0f;
        i_this->field_0x5cd0 = 0;

        Z2GetAudioMgr()->bgmStop(30, 0);
        Z2GetAudioMgr()->subBgmStart(Z2BGM_BOSS_OCTAEEL_D02);

        fpcM_Search(s_bfdel_sub, i_this);
        dComIfGs_onOneZoneSwitch(8, -1);
        /* fallthrough */
    case 21:
        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 290), 290)) {
            i_this->mBlureRateTarget = 150;
            i_this->field_0x5dd8 = 2;
        }

        sp64.set(-204.0f, -24000.0f, -1307.0f);
        player->setPlayerPosAndAngle(&sp64, 1642, 0);

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer > actor_attr::enemy_sync_timer(i_this, KREG_S(6) + 320)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, KREG_F(12) + -23169.0f, 0.1f,
#else
        if (i_this->mDemoActionTimer > KREG_S(6) + 320) {
            cLib_addCalc2(&i_this->mDemoCamCenter.y, KREG_F(12) + -23169.0f, 0.1f,
#endif
                          i_this->field_0x5cd0 * 40.0f);
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, 1.0f, 1.0f, KREG_F(7) + 0.02f), cLib_addCalc2(&i_this->field_0x5cd0, 1.0f, 1.0f, KREG_F(7) + 0.02f));
        }

        if (i_this->mDemoActionTimer < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, KREG_S(7) + 430), KREG_S(7) + 430)) {
            break;
        }

        i_this->mDemoAction = 22;
        i_this->mDemoActionTimer = 0;
        i_this->field_0x5cd8 = -2000.0f;
        i_this->mBlureRateTarget = 0;
        /* fallthrough */
    case 22:
        sp88 = a_this->eyePos;
        sp88.y += i_this->field_0x5cd8;

#if TARGET_PC  // enemy attribute integration
        {
            const int eventCount = actor_attr::enemy_action_event_count(i_this, i_this->mDemoActionTimer, 8);
            const int actionFrame = static_cast<int>(std::floor(i_this->mDemoActionTimer * actor_attr::enemy_action_time_speed(i_this)));
            const int lastIndex = actionFrame / 8;
            for (int event = eventCount - 1; event >= 0; --event) {
                const int index = lastIndex - event;
                if (index >= 0 && index <= 13) {
                    i_this->mBodyParts[4 + index].field_0x3e = 1;
                }
            }
#else
        if (!(i_this->mDemoActionTimer & 7) && (i_this->mDemoActionTimer >> 3) <= 13) {
            i_this->mBodyParts[4 + (i_this->mDemoActionTimer >> 3)].field_0x3e = 1;
#endif
        }

        if (i_this->mDemoActionTimer == 0) {
            i_this->mDemoCamCenter = sp88;
            i_this->mDemoCamEye.x = 7180.0f;
            i_this->mDemoCamEye.y = BREG_F(8) + -16200.0f + 100.0f;
            i_this->mDemoCamEye.z = 100.0f;
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, sp88.x, 0.4f, 200.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, sp88.y, 0.4f, 200.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, sp88.z, 0.4f, 200.0f);
            actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x5cd8, 0.1f, 15.0f);
#else
            cLib_addCalc2(&i_this->mDemoCamCenter.x, sp88.x, 0.4f, 200.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.y, sp88.y, 0.4f, 200.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.z, sp88.z, 0.4f, 200.0f);
            cLib_addCalc0(&i_this->field_0x5cd8, 0.1f, 15.0f);
#endif
        }

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 250)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamEye.y, -16500.0f, 0.1f, BREG_F(9) + 2.0f);
#else
        if (i_this->mDemoActionTimer >= 250) {
            cLib_addCalc2(&i_this->mDemoCamEye.y, -16500.0f, 0.1f, BREG_F(9) + 2.0f);
#endif
        }

        if (i_this->mDemoActionTimer != DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 430), 430)) {
            break;
        }

        i_this->mDemoAction = 23;
        i_this->mDemoActionTimer = 0;
        /* fallthrough */
    case 23:
        i_this->mDemoCamEye.set(-1236.0f, -23800.0f, -3470.0f);
        sp88 = a_this->eyePos;

        if (i_this->mDemoActionTimer == 0) {
            i_this->mDemoCamCenter = sp88;
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, sp88.x, 0.2f, 200.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, sp88.y, 0.2f, 200.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, sp88.z, 0.2f, 200.0f);
#else
            cLib_addCalc2(&i_this->mDemoCamCenter.x, sp88.x, 0.2f, 200.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.y, sp88.y, 0.2f, 200.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.z, sp88.z, 0.2f, 200.0f);
#endif
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120), 120)) {
            i_this->mDemoAction = 101;
            Z2GetAudioMgr()->bgmStart(Z2BGM_BOSS_OCTAEEL_1, 0, 0);
        }
        break;
    case 30:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFFF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 31;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 55.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();
        daPy_getPlayerActorClass()->changeDemoMode(0x17, 0, 0, 0);

        i_this->mDemoCamEye.set(-1705.0f, -23870.0f, -762.0f);
        Z2GetAudioMgr()->subBgmStart(Z2BGM_BOSS_OCTAEEL_D01);
        mDoAud_seStart(Z2SE_EN_OI_DEMO_APPEAR, NULL, 0, 0);
        dComIfGs_onOneZoneSwitch(5, -1);
        /* fallthrough */
    case 31:
        if (i_this->mDemoActionTimer < DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 40), 40)) {
            sp64.set(-1193.0f, -24000.0f, -770.0f);
            player->setPlayerPosAndAngle(&sp64, 0x2A02, 0);
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30), 30)) {
            dComIfGp_getEvent()->startCheckSkipEdge(a_this);
        }

        sp88 = a_this->current.pos;
        if (sp88.y < i_this->field_0x47a0 + NREG_F(1) + 200.0f) {
            sp88.y = i_this->field_0x47a0 + NREG_F(1) + 200.0f;
        }

        if (i_this->mDemoActionTimer == 0) {
            i_this->mDemoCamCenter = sp88;
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, sp88.x, 0.1f, 50.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, sp88.y, 0.1f, 50.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, sp88.z, 0.1f, 50.0f);
#else
            cLib_addCalc2(&i_this->mDemoCamCenter.x, sp88.x, 0.1f, 50.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.y, sp88.y, 0.1f, 50.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.z, sp88.z, 0.1f, 50.0f);
#endif
        }

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 90) && i_this->mDemoActionTimer <= actor_attr::enemy_sync_timer(i_this, 260)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamFovy, 30.0f, 0.2f, i_this->field_0x5cd0);
#else
        if (i_this->mDemoActionTimer >= 90 && i_this->mDemoActionTimer <= 260) {
            cLib_addCalc2(&i_this->mDemoCamFovy, 30.0f, 0.2f, i_this->field_0x5cd0);
#endif
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamFovy, 55.0f, 0.2f, i_this->field_0x5cd0), cLib_addCalc2(&i_this->mDemoCamFovy, 55.0f, 0.2f, i_this->field_0x5cd0));
        }

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, 1.0f, 1.0f, 0.05f), cLib_addCalc2(&i_this->field_0x5cd0, 1.0f, 1.0f, 0.05f));

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 390), 390)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[1]);
            tentacle->mActionPhase = 1;
            dComIfGp_getVibration().StartQuake(2, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (DUSK_IF_ELSE(i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 390) && i_this->mDemoActionTimer <= actor_attr::enemy_sync_timer(i_this, 525), i_this->mDemoActionTimer >= 390 && i_this->mDemoActionTimer <= 525)) {
            i_this->field_0x5ce4 = WREG_F(12) + 3.0f;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 410), 410)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[5]);
            tentacle->mActionPhase = 1;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 430), 430)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[3]);
            tentacle->mActionPhase = 1;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 445), 445)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[2]);
            tentacle->mActionPhase = 1;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 460), 460)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[6]);
            tentacle->mActionPhase = 1;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 470), 470)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[4]);
            tentacle->mActionPhase = 1;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 475), 475)) {
            tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[7]);
            tentacle->mActionPhase = 1;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 570), 570)) {
            i_this->mDemoAction = 32;
            i_this->mDemoActionTimer = 0;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation_play_speed(i_this, i_this->mBodyParts[0].mpMorf, 1.0f), i_this->mBodyParts[0].mpMorf->setPlaySpeed(1.0f));

            sp58.set(0.0f, 0.0f, 0.0f);
            dComIfGp_particle_set(dPa_RM(ID_ZI_S_OI_ISO_APP_A), &sp58, NULL, NULL);
            dComIfGp_getVibration().StartQuake(3, 31, cXyz(0.0f, 1.0f, 0.0f));

            i_this->field_0x5cd0 = 0;
            i_this->mAction = 1;
            i_this->mMode = 0;

            for (int i = 1; i < 7; i++) {
                tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i]);
                tentacle->mActionPhase = 2;
            }

            i_this->mBlureRateTarget = 150;
            i_this->field_0x5dd8 = 1;
            i_this->mZoneSwTimer = 0;
        }
        break;
    case 32:
#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 30)) {
            if (i_this->mDemoActionTimer == actor_attr::enemy_sync_timer(i_this, 30)) {
#else
        if (i_this->mDemoActionTimer >= 30) {
            if (i_this->mDemoActionTimer == 30) {
#endif
                i_this->mDemoCamCenterTarget.set(80.0f, -24093.0f, 160.0f);
                i_this->mDemoCamEyeTarget.set(-447.0f, -22850.0f, -718.0f);

                i_this->field_0x5cb4 = fabsf(i_this->mDemoCamEyeTarget.x - i_this->mDemoCamEye.x);
                i_this->field_0x5cb8 = fabsf(i_this->mDemoCamEyeTarget.y - i_this->mDemoCamEye.y);
                i_this->field_0x5cbc = fabsf(i_this->mDemoCamEyeTarget.z - i_this->mDemoCamEye.z);
                i_this->field_0x5cc0 = fabsf(i_this->mDemoCamCenterTarget.x - i_this->mDemoCamCenter.x);
                i_this->field_0x5cc4 = fabsf(i_this->mDemoCamCenterTarget.y - i_this->mDemoCamCenter.y);
                i_this->field_0x5cc8 = fabsf(i_this->mDemoCamCenterTarget.z - i_this->mDemoCamCenter.z);
                i_this->field_0x5cd0 = 0.0f;

                daPy_getPlayerActorClass()->changeDemoMode(0x17, 0, 0, 0);
            }

            cam_3d_morf(i_this, BREG_F(17) + 0.1f);
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, BREG_F(18) + 0.04f, 1.0f, BREG_F(19) + 0.0005f), cLib_addCalc2(&i_this->field_0x5cd0, BREG_F(18) + 0.04f, 1.0f, BREG_F(19) + 0.0005f));
        }

        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &a_this->home.pos.y, i_this->field_0x47a0, 1.0f, 2.0f), cLib_addCalc2(&a_this->home.pos.y, i_this->field_0x47a0, 1.0f, 2.0f));

        if (i_this->mDemoActionTimer <= DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 70), 70)) {
            i_this->field_0x5ce4 = WREG_F(10) + 6.0f;
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 70), 70)) {
            dComIfGp_getVibration().StopQuake(31);
            i_this->mBlureRateTarget = 0;
        }

        if (i_this->mBodyParts[0].mpMorf->isStop()) {
            anm_init(i_this, BCK_OI_WAIT, 0.0f, J3DFrameCtrl::EMode_LOOP, 1.0f);
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120), 120)) {
            fopMsgM_messageSetDemo(1153);  // Twilit Aquatic MORPHEEL
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 200), 200)) {
            i_this->mDemoAction = 100;
            for (int i = 0; i < 8; i++) {
                tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i]);
                tentacle->mAction = 1;
                tentacle->mActionPhase = 1;
                tentacle->mTimers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(50.0f) + 100.0f), cM_rndF(50.0f) + 100.0f);
            }

            Z2GetAudioMgr()->bgmStart(Z2BGM_BOSS_OCTAEEL_0, 0, 0);
        }
        break;
    case 40:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFFF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 41;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 55.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();
        i_this->mDemoCamEye.set(5000.0f, -17000.0f, 5000.0f);

        Z2GetAudioMgr()->bgmStop(30, 0);
        Z2GetAudioMgr()->bgmStreamPrepare(0x2000020);
        Z2GetAudioMgr()->bgmStreamPlay();
        mDoAud_seStart(Z2SE_EN_OI_ENDDEMO, 0, 0, 0);
        i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_ENDDEMO, -1);
        /* fallthrough */
    case 41:
    case 42:
    case 43:
    case 44:
        if (i_this->mDemoAction >= 43) {
            var_r27 = 1;
        }

        if (i_this->mDemoAction == 43 && i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 60), 60)) {
            i_this->mDemoCamEye.set(4716.0f, -22925.0f, 1873.0f);
            i_this->mDemoAction = 44;
            i_this->mDemoActionTimer = 0;
        }

        if (i_this->mDemoAction == 42 || i_this->mDemoAction == 43) {
            sp88 = a_this->eyePos;
        } else {
            MTXCopy(i_this->mBodyParts[BREG_S(5) + 8].mpMorf->getModel()->getAnmMtx(0),
                    mDoMtx_stack_c::get());
            mDoMtx_stack_c::multVecZero(&sp88);
        }

        if (i_this->mDemoActionTimer == 0) {
            i_this->mDemoCamCenter = sp88;
        } else {
            f32 var_f31;
            if (a_this->speedF < 1.0f) {
                var_f31 = 0.1f;
            } else {
                var_f31 = 0.3f;
            }

#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, sp88.x, var_f31, 400.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, sp88.y, var_f31, 400.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, sp88.z, var_f31, 400.0f);
#else
            cLib_addCalc2(&i_this->mDemoCamCenter.x, sp88.x, var_f31, 400.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.y, sp88.y, var_f31, 400.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.z, sp88.z, var_f31, 400.0f);
#endif
        }

        if (i_this->mDemoAction != 44 || i_this->mDemoActionTimer != DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120), 120)) {
            break;
        }

        i_this->mDemoCamCenter.set(4264.0f, -25096.0f, 12114.0f);
        i_this->mDemoCamEye.set(-1377.0f, -22866.0f, 9105.0f);
        i_this->mDemoAction = 45;
        i_this->mDemoActionTimer = 0;
        fpcM_Search(s_hasidel_sub, i_this);
        fpcM_Search(s_kaisoudel_sub, i_this);
        /* fallthrough */
    case 45: {
        var_r27 = 1;
        if (i_this->mDemoActionTimer != DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 80), 80)) {
            break;
        }

        i_this->mDemoCamCenter.set(-560.0f, -16612.0f, -836.0f);
        i_this->mDemoCamEye.set(-600.0f, -16245.0f, -1331.0f);
        i_this->mDemoCamCenterTarget.set(-6378.0f, -21886.0f, 7150.0f);
        i_this->mDemoCamEyeTarget.set(-6961.0f, -21727.0f, 7278.0f);

        i_this->field_0x5cb4 = fabsf(i_this->mDemoCamEyeTarget.x - i_this->mDemoCamEye.x);
        i_this->field_0x5cb8 = fabsf(i_this->mDemoCamEyeTarget.y - i_this->mDemoCamEye.y);
        i_this->field_0x5cbc = fabsf(i_this->mDemoCamEyeTarget.z - i_this->mDemoCamEye.z);
        i_this->field_0x5cc0 = fabsf(i_this->mDemoCamCenterTarget.x - i_this->mDemoCamCenter.x);
        i_this->field_0x5cc4 = fabsf(i_this->mDemoCamCenterTarget.y - i_this->mDemoCamCenter.y);
        i_this->field_0x5cc8 = fabsf(i_this->mDemoCamCenterTarget.z - i_this->mDemoCamCenter.z);
        i_this->field_0x5cd0 = 0.0f;

        i_this->mDemoAction = 46;
        i_this->mDemoActionTimer = 0;
        i_this->field_0x5dd8 = 4;

        obj_lv3WaterB_class* water = (obj_lv3WaterB_class*)fopAcM_SearchByName(fpcNm_OBJ_LV3WATERB_e);
        water->mAction = LV3WATERB_ACT_END;
        water->field_0x586 = 0;
        var_r27 = 2;
        /* fallthrough */
    }
    case 46:
        sp58.set(VREG_F(0) + -1225.0f, -24000.0f, VREG_F(1) + 8678.0f);
        player->setPlayerPosAndAngle(&sp58, VREG_S(1) + 10000, 0);

#if TARGET_PC  // enemy attribute integration
        {
            const int eventCount = actor_attr::enemy_action_event_count(i_this, i_this->mDemoActionTimer, 8);
            const int actionFrame = static_cast<int>(std::floor(i_this->mDemoActionTimer * actor_attr::enemy_action_time_speed(i_this)));
            const int lastIndex = actionFrame / 8;
            for (int event = eventCount - 1; event >= 0; --event) {
                const int index = lastIndex - event;
                if (index >= 0 && index <= 13) {
                    i_this->mBodyParts[index + 4].field_0x3e = 0;
                }
            }
#else
        if (!(i_this->mDemoActionTimer & 7) && (i_this->mDemoActionTimer >> 3) <= 13) {
            i_this->mBodyParts[(i_this->mDemoActionTimer >> 3) + 4].field_0x3e = 0;
#endif
        }

        if (i_this->mDemoActionTimer > 0) {
            cam_3d_morf(i_this, BREG_F(17) + 0.05f);
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, BREG_F(18) + 0.002f, 1.0f, BREG_F(19) + 0.0001f), cLib_addCalc2(&i_this->field_0x5cd0, BREG_F(18) + 0.002f, 1.0f, BREG_F(19) + 0.0001f));
        }

        if (i_this->mDemoActionTimer != DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 400), 400)) {
            break;
        }

        i_this->mDemoCamCenter.set(-1031.0f, -23787.0f, 8845.0f);
        i_this->mDemoCamEye.set(-1629.0f, -23891.0f, 8958.0f);
        i_this->mDemoAction = 47;
        i_this->mDemoActionTimer = 0;
        /* fallthrough */
    case 47:
#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 50)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5d14, TREG_F(11) + -20.0f, 1.0f, TREG_F(12) + 0.25f);
#else
        if (i_this->mDemoActionTimer >= 50) {
            cLib_addCalc2(&i_this->field_0x5d14, TREG_F(11) + -20.0f, 1.0f, TREG_F(12) + 0.25f);
#endif
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 50), 50)) {
            i_this->mHideCore = true;
            dComIfGp_particle_set(dPa_RM(ID_ZI_S_FM_ENDDEMO_CORECRASH_A), &a_this->eyePos, NULL, NULL);
            fopAcM_createDisappear(a_this, &a_this->eyePos, 15, 0, 0xFF);
            mDoAud_seStart(Z2SE_EN_BQ_EYE_EXPLODE, NULL, 0, 0);

            cXyz sp94(1.0f, 1.0f, 1.0f);
            csXyz sp17C(player->shape_angle);
            sp17C.y += KREG_S(3) + -0x7060;

            fopAcM_createItemForBoss(&a_this->eyePos, dItemNo_UTAWA_HEART_e, fopAcM_GetRoomNo(a_this), &sp17C,
                                     NULL, KREG_F(19) + 10.0f, 10.0f, -1);
            dComIfGp_getVibration().StartShock(5, 1, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 100), 100)) {
            i_this->mDemoCamCenter.set(-2785.0f, -22626.0f, 7614.0f);
            i_this->mDemoCamEye.set(-3295.0f, -22459.0f, 7307.0f);
            i_this->mDemoCamCenterTarget.set(-2933.0f, -22626.0f, 6829.0f);
            i_this->mDemoCamEyeTarget.set(-3295.0f, -22459.0f, 7307.0f);

            i_this->field_0x5cb4 = fabsf(i_this->mDemoCamEyeTarget.x - i_this->mDemoCamEye.x);
            i_this->field_0x5cb8 = fabsf(i_this->mDemoCamEyeTarget.y - i_this->mDemoCamEye.y);
            i_this->field_0x5cbc = fabsf(i_this->mDemoCamEyeTarget.z - i_this->mDemoCamEye.z);
            i_this->field_0x5cc0 = fabsf(i_this->mDemoCamCenterTarget.x - i_this->mDemoCamCenter.x);
            i_this->field_0x5cc4 = fabsf(i_this->mDemoCamCenterTarget.y - i_this->mDemoCamCenter.y);
            i_this->field_0x5cc8 = fabsf(i_this->mDemoCamCenterTarget.z - i_this->mDemoCamCenter.z);
            i_this->field_0x5cd0 = 0.0f;

            i_this->mDemoAction = 48;
            i_this->mDemoActionTimer = 0;
        }
        break;
    case 48:
        if (i_this->mDemoActionTimer >= DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 60), 60)) {
            i_this->field_0x5cd0 = 0.02f;
            cam_3d_morf(i_this, BREG_F(17) + 0.1f);

#if TARGET_PC  // enemy attribute integration
            const int eventCount = actor_attr::enemy_action_event_count(i_this, i_this->mDemoActionTimer, 4, -60);
            const int actionFrame = static_cast<int>(std::floor(i_this->mDemoActionTimer * actor_attr::enemy_action_time_speed(i_this)));
            const int lastIndex = (actionFrame - 60) / 4;
            for (int event = eventCount - 1; event >= 0; --event) {
                int idx = lastIndex - event;
                if (idx >= 0 && idx < 19) {
#else
            if (!(i_this->mDemoActionTimer & 3)) {
                int idx = ((i_this->mDemoActionTimer - 60) >> 2);
                if (idx < 19) {
#endif
                    i_this->mBodyParts[idx].mHide = true;

                    cXyz spA0(i_this->mBodyParts[idx].mSize,
                              i_this->mBodyParts[idx].mSize,
                              i_this->mBodyParts[idx].mSize);

                    if (idx == 0) {
                        mDoAud_seStart(Z2SE_EN_OI_EXPLODE, NULL, 0, 0);
                        for (int i = 0; i < 8; i++) {
                            fopAc_ac_c* tentacle = fopAcM_SearchByID(i_this->mTentacleActorIDs[i]);
                            fopAcM_delete(tentacle);
                        }

                        i_this->field_0x5d40[0][idx] = dComIfGp_particle_set(
                            dPa_RM(ID_ZI_S_OI_DEADEXP_POLY_HEAD), &i_this->mBodyParts[idx].mPos, NULL, &spA0);
                        i_this->field_0x5d40[1][idx] = dComIfGp_particle_set(
                            dPa_RM(ID_ZI_S_OI_DEADEXP_SMOKE_HEAD), &i_this->mBodyParts[idx].mPos, NULL, &spA0);
                    } else if (idx == 18) {
                        i_this->field_0x5d40[0][idx] = dComIfGp_particle_set(
                            dPa_RM(ID_ZI_S_OI_DEADEXP_POLY_TAIL), &i_this->mBodyParts[idx].mPos, NULL, &spA0);
                        i_this->field_0x5d40[1][idx] = dComIfGp_particle_set(
                            dPa_RM(ID_ZI_S_OI_DEADEXP_SMOKE_TAIL), &i_this->mBodyParts[idx].mPos, NULL, &spA0);
                    } else {
                        i_this->field_0x5d40[0][idx] = dComIfGp_particle_set(
                            dPa_RM(ID_ZI_S_OI_DEADEXP_POLY_BODY), &i_this->mBodyParts[idx].mPos, NULL, &spA0);
                        i_this->field_0x5d40[1][idx] = dComIfGp_particle_set(
                            dPa_RM(ID_ZI_S_OI_DEADEXP_SMOKE_BODY), &i_this->mBodyParts[idx].mPos, NULL, &spA0);
                    }
                }
            }
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 180), 180)) {
            i_this->mDemoAction = 49;
            i_this->mDemoActionTimer = 0;

            for (int i = 0; i < 19; i++) {
                for (int j = 0; j < 2; j++) {
                    if (i_this->field_0x5d40[j][i] != NULL) {
                        i_this->field_0x5d40[j][i]->becomeDeleteEmitter();
                    }
                }
            }

            static cXyz room_pos(0.0f, 0.0f, 0.0f);
            static cXyz sc(10.0f, 10.0f, 10.0f);

            for (int i = 0; i < 5; i++) {
                static u16 ex_eff[] = {dPa_RM(ID_ZI_S_OI_CONVERGE_FILTER), dPa_RM(ID_ZI_S_OI_CONVERGE_FILTEROUT), dPa_RM(ID_ZI_S_OI_CONVERGE_HIDE), dPa_RM(ID_ZI_S_OI_CONVERGE_POLYGON_A), dPa_RM(ID_ZI_S_OI_CONVERGE_POLYGON_B)};

#if TARGET_PC  // enemy attribute integration
                if (i == 0) {
                    static const cXyz effWideScale = {
                        mDoGph_gInf_c::getAspect() * 10.0f, 10.0f, 10.0f};
                    dComIfGp_particle_set(ex_eff[i], &room_pos, NULL, &effWideScale);
                } else
#endif
                {
                    dComIfGp_particle_set(ex_eff[i], &room_pos, NULL, &sc);
                }
            }

            i_this->mDemoCamEye.set(-4820.0f, -18600.0f, -510.0f);
            i_this->mDemoCamCenter.set(-2630.0f, -21620.0f, 4900.0f);
            i_this->mDemoCamFovy = 45.0f;
            i_this->field_0x5cd0 = 0.0f;

            i_this->mSound.startCreatureSound(Z2SE_EN_BOSS_CONVERGE, 0, 0);
        }
        break;
    case 49:
#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer >= actor_attr::enemy_sync_timer(i_this, 100)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamFovy, 30.0f, 0.2f, i_this->field_0x5cd0);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, 1.0f, 1.0f, 0.05f);
#else
        if (i_this->mDemoActionTimer >= 100) {
            cLib_addCalc2(&i_this->mDemoCamFovy, 30.0f, 0.2f, i_this->field_0x5cd0);
            cLib_addCalc2(&i_this->field_0x5cd0, 1.0f, 1.0f, 0.05f);
#endif
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 100), 100)) {
            cMtx_YrotS(*calc_mtx, player->shape_angle.y);
            sp58.set(0.0f, JREG_F(8) + 300.0f, JREG_F(9) + 100.0f);
            MtxPosition(&sp58, &sp64);

            sp64 += player->current.pos;
            OS_REPORT("///YSTONE POS  %d,%d,%d\n", sp64.x, sp64.y, sp64.z);
            fopAcM_create(fpcNm_OBJ_YSTONE_e, 0, &sp64, fopAcM_GetRoomNo(a_this), NULL, NULL, -1);
        }

        if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120), 120)) {
            obj_ystone_class* ystone = (obj_ystone_class*)fopAcM_SearchByName(fpcNm_OBJ_YSTONE_e);
            if (ystone != NULL) {
                ystone->field_0x59b = 0;
            }
        }

        if (i_this->mDemoActionTimer >= DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 180), 180)) {
            i_this->mDemoCamFovy = 45.0f;
            if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 180), 180)) {
                i_this->field_0x5cd8 = JREG_F(8) + 300.0f;
            }

            cMtx_YrotS(*calc_mtx, player->shape_angle.y);

            sp58.x = ZREG_F(1);
            sp58.y = ZREG_F(2) + 100.0f;
            sp58.z = ZREG_F(3) + -200.0f;
            MtxPosition(&sp58, &i_this->mDemoCamEye);

            i_this->mDemoCamEye += player->current.pos;
            i_this->mDemoCamCenter = player->current.pos;
            i_this->mDemoCamCenter.y += ZREG_F(4) + 200.0f;

            sp58.set(0.0f, i_this->field_0x5cd8, JREG_F(9) + 100.0f);
            MtxPosition(&sp58, &sp64);
            sp64 += player->current.pos;

            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd8, 210.0f, 0.05f, 0.5f), cLib_addCalc2(&i_this->field_0x5cd8, 210.0f, 0.05f, 0.5f));

            obj_ystone_class* ystone = (obj_ystone_class*)fopAcM_SearchByName(fpcNm_OBJ_YSTONE_e);
            if (ystone != NULL) {
                ystone->setCurrentPos(sp64);
            }

            if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 279), 279)) {
                cXyz spAC(-1520.0f, -23960.0f, 7100.0f);
                csXyz sp184(0, 0, 0);
                fopAcM_createWarpHole(&spAC, &sp184, fopAcM_GetRoomNo(a_this), 0, 1, 2);
            }

            if (i_this->mDemoActionTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 290), 290)) {
                i_this->mDemoAction = 100;
                dComIfGs_onStageBossEnemy();
                dComIfGs_onSwitch(a_this->home.angle.z & 0xFF, fopAcM_GetRoomNo(a_this));
                fopAcM_delete(a_this);
            }
        }
        break;
    case 50:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFFF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 51;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 55.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();

        i_this->field_0x5cf8 = player->shape_angle.y;
        i_this->field_0x5cfc = 0;
        i_this->field_0x5d00 = ZREG_F(3) + 300.0f;

        anm_init(i_this, BCK_OI_EAT, 3.0f, J3DFrameCtrl::EMode_NONE, 1.0f);

        i_this->mOISound.startCreatureVoice(Z2SE_EN_OI_V_EAT, -1);

        MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0), mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp64);
        dComIfGp_particle_set(dPa_RM(ID_ZI_S_OI_ISO_HAKI_A), &sp64, NULL, NULL);
        dComIfGp_particle_set(dPa_RM(ID_ZI_S_OI_ISO_HAKI_B), &sp64, NULL, NULL);
        /* fallthrough */
    case 51:
        MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0x13), mDoMtx_stack_c::get());
        sp58.x = i_this->field_0x5d00;
        sp58.y = 0.0;
        sp58.z = 0.0;
        mDoMtx_stack_c::multVec(&sp58, &i_this->field_0x5cec);

        i_this->field_0x5cf8 += 0x1000;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x5d00, 1.0f, i_this->field_0x5cfc);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cfc, 100.0f, 1.0f, 5.0f);
#else
        cLib_addCalc0(&i_this->field_0x5d00, 1.0f, i_this->field_0x5cfc);
        cLib_addCalc2(&i_this->field_0x5cfc, 100.0f, 1.0f, 5.0f);
#endif

        player->setPlayerPosAndAngle(&i_this->field_0x5cec, i_this->field_0x5cf8, 0);

        cMtx_YrotS(*calc_mtx, a_this->home.angle.y + -0x2000);
        sp58.x = 0.0;
        sp58.y = ZREG_F(12) + 1000.0f;
        sp58.z = ZREG_F(13) + 900.0f;
        MtxPosition(&sp58, &i_this->mDemoCamEye);
        i_this->mDemoCamEye += a_this->home.pos;

        if (i_this->mAnmID == BCK_OI_EAT) {
            if (i_this->mBodyParts[0].mpMorf->checkFrame(120.0f)) {
                daPy_getPlayerActorClass()->changeDemoMode(46, 0, 0, 0);
            }

            if (i_this->mBodyParts[0].mpMorf->checkFrame(153.0f)) {
                i_this->mDemoAction = 52;
                i_this->mDemoActionTimer = 0;

                sp64 = a_this->home.pos;
                sp64.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, KREG_F(13) + 400.0f), KREG_F(13) + 400.0f);
                player->setPlayerPosAndAngle(&sp64, 0, 0);

                daPy_getPlayerActorClass()->setThrowDamage(a_this->home.angle.y, DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, 10.0f) * actor_attr::enemy_action_time_speed(i_this), 10.0f),
                                                           DUSK_IF_ELSE((KREG_F(14) + 30.0f) * JMAFastSqrt(actor_attr::enemy_size_multiplier(i_this)), KREG_F(14) + 30.0f), 4, 1, 2);
                daPy_getPlayerActorClass()->changeDemoMode(1, 0, 0, 0);
            }
        }
    case 52:
        if (i_this->mDemoActionTimer == 0) {
            i_this->mDemoCamCenter = i_this->field_0x5cec;
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.x, player->current.pos.x, 0.4f, 300.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.y, player->current.pos.y, 0.4f, 300.0f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamCenter.z, player->current.pos.z, 0.4f, 300.0f);
#else
            cLib_addCalc2(&i_this->mDemoCamCenter.x, player->current.pos.x, 0.4f, 300.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.y, player->current.pos.y, 0.4f, 300.0f);
            cLib_addCalc2(&i_this->mDemoCamCenter.z, player->current.pos.z, 0.4f, 300.0f);
#endif
        }

#if TARGET_PC  // enemy attribute integration
        if (i_this->mDemoActionTimer > actor_attr::enemy_sync_timer(i_this, 25)) {
            actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamFovy, 40.0f, 0.05f, i_this->field_0x5cd0);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x5cd0, 1.0f, 1.0f, 0.05f);
#else
        if (i_this->mDemoActionTimer > 25) {
            cLib_addCalc2(&i_this->mDemoCamFovy, 40.0f, 0.05f, i_this->field_0x5cd0);
            cLib_addCalc2(&i_this->field_0x5cd0, 1.0f, 1.0f, 0.05f);
#endif
        }

        if (i_this->mDemoAction == 52 && (i_this->mBodyParts[0].mpMorf->isStop() || DUSK_IF_ELSE(actor_attr::enemy_action_time_speed(i_this) < 1.0f && i_this->mDemoActionTimer >= i_this->mBodyParts[0].mpMorf->getEndFrame() - 153.0f, false))) {
            i_this->mDemoAction = 100;
            i_this->mCoreAnm = BCK_OI_WAIT;
            i_this->mCoreAnmMode = J3DFrameCtrl::EMode_LOOP;
            i_this->field_0x4794 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 100), 100);
        }
        break;
    case 60:
        if (!a_this->eventInfo.checkCommandDemoAccrpt()) {
            fopAcM_orderPotentialEvent(a_this, 2, 0xFFFF, 0);
            a_this->eventInfo.onCondition(2);
            return;
        }

        camera->mCamera.Stop();
        i_this->mDemoAction = 61;
        i_this->mDemoActionTimer = 0;
        i_this->mDemoCamFovy = 40.0f;
        i_this->field_0x5cd0 = 0.0f;
        camera->mCamera.SetTrimSize(3);

        daPy_getPlayerActorClass()->changeOriginalDemo();
        /* fallthrough */
    case 61:
        cMtx_YrotS(*calc_mtx, a_this->home.angle.y + 0x2000);
        sp58.x = 0.0f;
        sp58.y = ZREG_F(15) + 100.0f;
        sp58.z = ZREG_F(16) + 1700.0f;
        MtxPosition(&sp58, &i_this->mDemoCamEye);
        i_this->mDemoCamEye += a_this->home.pos;

        i_this->mDemoCamCenter = a_this->home.pos;
        i_this->mDemoCamCenter.y += ZREG_F(17) + 700.0f;
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->mDemoCamFovy, 55.0f, 0.05f, 0.2f), cLib_addCalc2(&i_this->mDemoCamFovy, 55.0f, 0.05f, 0.2f));
    }

    if (i_this->mDemoAction != 0) {
        if (i_this->mDemoAction >= 100) {
            if (i_this->mDemoAction == 101) {
                cMtx_YrotS(*calc_mtx, player->shape_angle.y);
                sp58.x = 0.0f;
                sp58.y = JREG_F(1) + 100.0f;
                sp58.z = JREG_F(2) + -250.0f;
                MtxPosition(&sp58, &i_this->mDemoCamEye);
                i_this->mDemoCamEye += player->current.pos;

                i_this->mDemoCamCenter = player->current.pos;
                i_this->mDemoCamCenter.y += 120.0f;
            }

            camera->mCamera.Reset(i_this->mDemoCamCenter, i_this->mDemoCamEye);
            camera->mCamera.Start();
            camera->mCamera.SetTrimSize(0);
            dComIfGp_event_reset();
            daPy_getPlayerActorClass()->cancelOriginalDemo();
            i_this->mDemoAction = 0;
        }

        cXyz cam_center;
        cXyz cam_eye;

        cam_center = i_this->mDemoCamCenter;
        cam_eye = i_this->mDemoCamEye;

        s16 cam_bank = 0;
        if (i_this->field_0x5ce4 > 0.0f) {
#if TARGET_PC  // enemy attribute integration
            int sp38 = i_this->field_0x5ce4 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->mDemoActionTimer , 0x1B00)) * 25.0f;
            int sp40 = i_this->field_0x5ce4 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->mDemoActionTimer , 0x1900)) * 25.0f;
#else
            int sp38 = i_this->field_0x5ce4 * cM_ssin(i_this->mDemoActionTimer * 0x1B00) * 25.0f;
            int sp40 = i_this->field_0x5ce4 * cM_ssin(i_this->mDemoActionTimer * 0x1900) * 25.0f;
#endif
            sp58 = cam_center - cam_eye;

            s16 temp_r5 = sp38 + cM_atan2s(sp58.x, sp58.z);
            s16 temp_r4 = sp40 - cM_atan2s(sp58.y, JMAFastSqrt(sp58.x * sp58.x + sp58.z * sp58.z));
            cMtx_YrotS(*calc_mtx, temp_r5);
            cMtx_XrotM(*calc_mtx, temp_r4);

            sp58.x = 0.0;
            sp58.y = 0.0;
            sp58.z = sp58.abs();
            MtxPosition(&sp58, &cam_center);
            cam_center += cam_eye;

            cam_bank = (BREG_F(4) + 30.0f) *
#if TARGET_PC  // enemy attribute integration
                      (i_this->field_0x5ce4 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->mDemoActionTimer , 0x1E00)));
            actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x5ce4, 1.0f, BREG_F(5) + 0.5f);
#else
                      (i_this->field_0x5ce4 * cM_scos(i_this->mDemoActionTimer * 0x1E00));
            cLib_addCalc0(&i_this->field_0x5ce4, 1.0f, BREG_F(5) + 0.5f);
#endif
        }

        camera->mCamera.Set(cam_center, cam_eye, cam_bank, i_this->mDemoCamFovy);

        i_this->mDemoActionTimer++;
#if TARGET_PC  // enemy attribute integration
        i_this->field_0x4794 = actor_attr::enemy_sync_timer(i_this, 90);
        actor_attr::enemy_add_action_angle(i_this, &i_this->mBlureRate, i_this->mBlureRateTarget, 1, 8);
#else
        i_this->field_0x4794 = 90;
        cLib_addCalcAngleS2(&i_this->mBlureRate, i_this->mBlureRateTarget, 1, 8);
#endif

        if (i_this->mDemoAction >= 31 && i_this->mDemoAction < 40) {
            if (dComIfGp_getEvent()->checkSkipEdge()) {
                cDmr_SkipInfo = JREG_S(8) + 30;
                dStage_changeScene(1, 0.0f, 0, fopAcM_GetRoomNo(a_this), 0, -1);
            }
        }
    }

    if (var_r27 != 0) {
        sp64.set(0.0f, -23580.0f, 10300.0f);

        for (int i = 0; i < 4; i++) {
            static u16 iso_eff[] = {dPa_RM(ID_ZI_S_OI_HAISUI_A), dPa_RM(ID_ZI_S_OI_HAISUI_B), dPa_RM(ID_ZI_S_OI_HAISUI_C), dPa_RM(ID_ZI_S_OI_HAISUI_D)};
            i_this->field_0x5d24[i] =
                dComIfGp_particle_set(i_this->field_0x5d24[i], iso_eff[i], &sp64, NULL, NULL);

            if (var_r27 == 2) {
                JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->field_0x5d24[i]);
                if (emitter != NULL) {
                    emitter->becomeDeleteEmitter();
                }
            }
        }
    }
}

static int daB_OB_Execute(b_ob_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;

    if (cDmrNowMidnaTalk()) {
        return 1;
    }

    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    cXyz sp70;
    cXyz sp7C;

    if (cDmr_SkipInfo != 0) {
        cDmr_SkipInfo--;
    }

    i_this->field_0x4750++;
    i_this->mYAngleToPlayer = fopAcM_searchPlayerAngleY(a_this);
    i_this->mXAngleToPlayer = -fopAcM_searchPlayerAngleX(a_this);

    for (int i = 0; i < 6; i++) {
        if (i_this->mTimers[i] != 0) {
            i_this->mTimers[i]--;
        }
    }

    if (i_this->mHitIFrameTimer != 0) {
        i_this->mHitIFrameTimer--;
    }

    if (i_this->field_0x4794 != 0) {
        i_this->field_0x4794--;
    }

    if (i_this->mFishBattleMode != 0) {
        i_this->attention_info.distances[fopAc_attn_BATTLE_e] = 60;
        i_this->mDistToPlayer = fopAcM_searchPlayerDistance(a_this);

        fish_damage_check(i_this);
        fish_move(i_this);

        if (i_this->mHideCore) {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc0(i_this, &i_this->mBossLightScale, 1.0f, 0.05f), cLib_addCalc0(&i_this->mBossLightScale, 1.0f, 0.05f));
        } else {
            i_this->mBossLightScale = 1.0f;
        }

        if (i_this->mBossLightScale > 0.0f) {
            GXColor color;
            color.r = l_HIO.mLightR;
            color.g = l_HIO.mLightG;
            color.b = l_HIO.mLightB;
            color.a = 0xFF;

            cXyz pos(a_this->current.pos);
            dKy_BossLight_set(&pos, &color, l_HIO.mRange * i_this->mBossLightScale, 0);
        }

        if (!dComIfGs_isOneZoneSwitch(9, -1) && !dComIfGp_event_runCheck()) {
            i_this->mZoneSwTimer++;

            if (i_this->mZoneSwTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 600), 600)) {
                dComIfGs_onOneZoneSwitch(11, -1);
            }
        }
    }

    if (i_this->mCoreBattleMode != 0) {
        if (i_this->mAction == OB_ACTION_CORE_START) {
            if (player->current.pos.y > -11000.0f) {
                if (!dComIfGp_event_runCheck()) {
                    i_this->mZoneSwTimer++;
                }

                if (i_this->mZoneSwTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 300), 300)) {
                    dComIfGs_onOneZoneSwitch(4, -1);
                }
            } else {
                i_this->mZoneSwTimer = 0;
            }
        } else if (dComIfGs_isOneZoneSwitch(5, -1) && !dComIfGs_isOneZoneSwitch(7, -1)) {
            if (player->current.pos.y > -20500.0f) {
                dComIfGs_onOneZoneSwitch(6, -1);
                i_this->mZoneSwTimer = 0;
            } else {
                dComIfGs_offOneZoneSwitch(6, -1);
                i_this->mZoneSwTimer++;

                if (i_this->mZoneSwTimer == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 600), 600)) {
                    dComIfGs_onOneZoneSwitch(10, -1);
                }
            }
        }

        i_this->attention_info.distances[fopAc_attn_BATTLE_e] = 4;
        i_this->mDistToPlayer = fopAcM_searchPlayerDistanceXZ(a_this);

        core_damage_check(i_this);
        core_action(i_this);

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, a_this->current.angle.y, 2, 0x400);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, a_this->current.angle.x, 2, 0x400);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.y, a_this->current.angle.y, 2, 0x400);
        cLib_addCalcAngleS2(&a_this->shape_angle.x, a_this->current.angle.x, 2, 0x400);
#endif

        mDoMtx_stack_c::transS(a_this->current.pos.x, a_this->current.pos.y + i_this->field_0x479c,
                               a_this->current.pos.z);
        mDoMtx_stack_c::YrotM(a_this->shape_angle.y);
        mDoMtx_stack_c::XrotM(a_this->shape_angle.x);
#if TARGET_PC  // enemy attribute integration
        mDoMtx_stack_c::scaleM(actor_attr::enemy_size_multiplier(i_this, l_HIO.mCoreSize));
#else
        mDoMtx_stack_c::scaleM(l_HIO.mCoreSize, l_HIO.mCoreSize, l_HIO.mCoreSize);
#endif
        mDoMtx_stack_c::ZrotM(a_this->shape_angle.z);
        mDoMtx_stack_c::scaleM(0.9f, 1.1f, 0.9f);
        mDoMtx_stack_c::ZrotM(-a_this->shape_angle.z);

        i_this->field_0x4740 = cM_ssin(DUSK_IF_ELSE(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x4750 , 1000), i_this->field_0x4750 * 1000)) * 9.5f + 9.5f;
        i_this->mpCoreMorf->setFrame(i_this->field_0x4740);
        i_this->mpCoreMorf->modelCalc();

        i_this->mpCoreMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp7C);
        i_this->eyePos = sp7C;

        if (a_this->eyePos.y >= a_this->home.pos.y) {
            a_this->attention_info.position = a_this->eyePos;
            a_this->attention_info.position.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, 50.0f), 50.0f);
            i_this->mDrawShadow = true;
        } else {
            i_this->mDrawShadow = false;
        }

        sp7C = a_this->current.pos;

        if (i_this->mHitIFrameTimer != 0) {
            sp7C.x += 22222.0f;
        }

        i_this->mCcSph.SetC(sp7C);
        i_this->mCcSph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, l_HIO.mCoreSize * 50.0f), l_HIO.mCoreSize * 50.0f));

        dComIfG_Ccsp()->Set(&i_this->mCcSph);
        i_this->mCcSph.OffAtSetBit();

        if (!dComIfGp_event_runCheck()) {
            i_this->mCcSph.OnCoSetBit();
            i_this->mBodySph.OnCoSetBit();
        } else {
            i_this->mCcSph.OffCoSetBit();
            i_this->mBodySph.OffCoSetBit();
        }

        if (i_this->mCoreBattleMode != 0) {
#if TARGET_PC  // enemy attribute integration
            /* The head model starts below the arena floor. Keep the same
               surface line when randomized scaling changes its height by
               scaling the burial depth around home.pos.y as well. */
            const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
            const f32 burialDepth = (KREG_F(17) + 750.0f) * sizeMultiplier;
            mDoMtx_stack_c::transS(a_this->home.pos.x, a_this->home.pos.y - burialDepth,
                                   a_this->home.pos.z);
#else
            mDoMtx_stack_c::transS(a_this->home.pos.x, a_this->home.pos.y - (KREG_F(17) + 750.0f),
                                   a_this->home.pos.z);
#endif
            mDoMtx_stack_c::XrotM(-0x4000);
#if TARGET_PC  // enemy attribute integration
            const f32 bodyScale = l_HIO.mBodySize * sizeMultiplier;
            mDoMtx_stack_c::scaleM(bodyScale, bodyScale, bodyScale);
#else
            mDoMtx_stack_c::scaleM(l_HIO.mBodySize, l_HIO.mBodySize, l_HIO.mBodySize);
#endif

            i_this->mBodyParts[0].mpMorf->play(NULL, 0, 0);
            i_this->mBodyParts[0].mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
            i_this->mBodyParts[0].mpMorf->modelCalc();

            i_this->mBodySph.SetC(a_this->home.pos);
            i_this->mBodySph.SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, TREG_F(17) + 410.0f), TREG_F(17) + 410.0f));
            dComIfG_Ccsp()->Set(&i_this->mBodySph);
        }
    }

    if (i_this->mAnmID == BCK_OI_APPEAR && i_this->mBodyParts[0].mpMorf->getPlaySpeed() >= DUSK_IF_ELSE(actor_attr::enemy_action_step(i_this, 0.5f), 0.5f)) {
        J3DModel* model = i_this->mBodyParts[0].mpMorf->getModel();
        for (int i = 0; i < 5; i++) {
            int joint_no = 1;
            if (i >= 2) {
                joint_no = 0x13;
            }

            MTXCopy(model->getAnmMtx(joint_no), mDoMtx_stack_c::get());
            mDoMtx_stack_c::multVecZero(&sp7C);

            static u16 iso_eff[] = {dPa_RM(ID_ZI_S_OI_ISO_APP_B), dPa_RM(ID_ZI_S_OI_ISO_APP_C), dPa_RM(ID_ZI_S_OI_ISO_APP_D), dPa_RM(ID_ZI_S_OI_ISO_APP_E), dPa_RM(ID_ZI_S_OI_ISO_APP_F)};
            i_this->field_0x5d24[i] =
                dComIfGp_particle_set(i_this->field_0x5d24[i], iso_eff[i], &sp7C, NULL, NULL);
        }
    } else if (i_this->mAnmID == BCK_OI_EAT) {
        J3DModel* model = i_this->mBodyParts[0].mpMorf->getModel();
        MTXCopy(model->getAnmMtx(0x13), mDoMtx_stack_c::get());
        mDoMtx_stack_c::multVecZero(&sp7C);

        for (int i = 0; i < 3; i++) {
            static u16 iso_eff[] = {dPa_RM(ID_ZI_S_OI_ISO_HAKI_C), dPa_RM(ID_ZI_S_OI_ISO_HAKI_D), dPa_RM(ID_ZI_S_OI_ISO_HAKI_E)};
            i_this->field_0x5d24[i] =
                dComIfGp_particle_set(i_this->field_0x5d24[i], iso_eff[i], &sp7C, NULL, NULL);
        }
    } else if (i_this->mAnmID == BCK_OI_THROWUP) {
        J3DModel* model = i_this->mBodyParts[0].mpMorf->getModel();
        for (int i = 0; i < 4; i++) {
            static u16 iso_eff[] = {dPa_RM(ID_ZI_S_OI_FISH_HAKI_A), dPa_RM(ID_ZI_S_OI_FISH_HAKI_B), dPa_RM(ID_ZI_S_OI_FISH_HAKI_C), dPa_RM(ID_ZI_S_OI_FISH_HAKI_D)};
            i_this->field_0x5d24[i] = dComIfGp_particle_set(i_this->field_0x5d24[i], iso_eff[i],
                                                            &a_this->current.pos, NULL, NULL);

            JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->field_0x5d24[i]);
            if (emitter != NULL) {
                emitter->setGlobalRTMatrix(model->getAnmMtx(0x13));
            }
        }
    }

    demo_camera(i_this);

    f32 blend_target = 0.0f;
    if (i_this->field_0x5dd8 == 0) {
        dKy_custom_colset(0, 1, i_this->mColsetBlend);
    } else if (i_this->field_0x5dd8 == 1) {
        dKy_custom_colset(0, 1, i_this->mColsetBlend);
        blend_target = 1.0f;
    } else if (i_this->field_0x5dd8 == 2) {
        dKy_custom_colset(2, 1, i_this->mColsetBlend);
    } else if (i_this->field_0x5dd8 == 3) {
        dKy_custom_colset(2, 3, i_this->mColsetBlend);
        blend_target = 1.0f;
    } else if (i_this->field_0x5dd8 == 4) {
        dKy_custom_colset(4, 4, blend_target);
    }

    DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->mColsetBlend, blend_target, 1.0f, 0.05f), cLib_addCalc2(&i_this->mColsetBlend, blend_target, 1.0f, 0.05f));

    if (i_this->field_0x5d18 != 0) {
        i_this->field_0x5d18--;
        i_this->field_0x5d38 =
            dComIfGp_particle_set(i_this->field_0x5d38, dPa_RM(ID_ZI_S_OI_FISH_CORESASHI_A), &a_this->current.pos, NULL, NULL);

        JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->field_0x5d38);
        if (emitter != 0) {
            emitter->setGlobalRTMatrix(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0));
        }
    }

    MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(0x13), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&i_this->field_0x5be4[0]);

    MTXCopy(i_this->mBodyParts[0].mpMorf->getModel()->getAnmMtx(1), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&i_this->field_0x5be4[1]);

    MTXCopy(i_this->mBodyParts[2].mpMorf->getModel()->getBaseTRMtx(), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&i_this->field_0x5be4[2]);

    MTXCopy(i_this->mBodyParts[5].mpMorf->getModel()->getBaseTRMtx(), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&i_this->field_0x5be4[3]);

    MTXCopy(i_this->mBodyParts[18].mpMorf->getModel()->getBaseTRMtx(), mDoMtx_stack_c::get());
    mDoMtx_stack_c::multVecZero(&i_this->field_0x5be4[4]);

    for (int i = 0; i < 8; i++) {
        if (i_this->mCoreBattleMode != 0) {
            b_oh_class* tentacle = (b_oh_class*)fopAcM_SearchByID(i_this->mTentacleActorIDs[i]);
            if (tentacle != NULL) {
                MTXCopy(tentacle->mpMorf->getModel()->getAnmMtx(0x19), mDoMtx_stack_c::get());
                sp70 = i_this->field_0x5be4[5 + i];
                mDoMtx_stack_c::multVecZero(&i_this->field_0x5be4[5 + i]);
                sp70 = i_this->field_0x5be4[5 + i] - sp70;

                i_this->mOISound.startTentacleSoundLevel(
                    Z2SE_EN_OI_TENT_MOVE, i, sp70.abs(), 0,
                    dComIfGp_getReverb(fopAcM_GetRoomNo(a_this)));
            }
        }
    }

    i_this->mOISound.framework(0, dComIfGp_getReverb(fopAcM_GetRoomNo(a_this)));
    return 1;
}

static int daB_OB_IsDelete(b_ob_class* i_this) {
    return 1;
}

static int daB_OB_Delete(b_ob_class* i_this) {
    dComIfG_resDelete(&i_this->mPhase, "B_oh");

    if (i_this->mHIOInit) {
        data_8061B4FD = 0;
    }

    if (i_this->heap != NULL) {
        i_this->mSound.stopAnime();
        i_this->mOISound.deleteObject();
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    b_ob_class* a_this = (b_ob_class*)i_this;

    a_this->mpCoreMorf =
        JKR_NEW mDoExt_McaMorfSO((J3DModelData*)dComIfG_getObjectRes("B_oh", BMDV_OH_CORE), NULL, NULL,
                             (J3DAnmTransform*)dComIfG_getObjectRes("B_oh", BCK_OH_CORE_OPEN), 2, DUSK_IF_ELSE(actor_attr::enemy_action_step(a_this, 1.0f), 1.0f), 0, -1,
                             &a_this->mSound, 0, 0x11000084);

    for (int i = 0; i < 19; i++) {
        static int p_bmd[] = {BMDV_OI_HEAD, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY,
                              BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_BODY, BMDR_OI_TAIL};

        u32 var_r31 = 0x80000;
        if (p_bmd[i] == 43) {
            var_r31 = 0;
        }

        a_this->mBodyParts[i].mpMorf =
            JKR_NEW mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("B_oh", p_bmd[i]), NULL, NULL,
                               NULL, 2, 1.0f, 0, -1, 1, NULL, var_r31, 0x11000084);

        if (a_this->mBodyParts[i].mpMorf == NULL ||
            a_this->mBodyParts[i].mpMorf->getModel() == NULL)
        {
            return 0;
        }

        a_this->mBodyParts[i].mpMorf->getModel()->setUserArea((uintptr_t)i_this);

        for (u16 j = 0; j < a_this->mBodyParts[i].mpMorf->getModel()->getModelData()->getJointNum();
             j++)
        {
            if (p_bmd[i] == 0x2B) {
                a_this->mBodyParts[i]
                    .mpMorf->getModel()
                    ->getModelData()
                    ->getJointNodePointer(j)
                    ->setCallBack(HeadCallBack);
            } else if (p_bmd[i] == 0x26) {
                a_this->mBodyParts[i]
                    .mpMorf->getModel()
                    ->getModelData()
                    ->getJointNodePointer(j)
                    ->setCallBack(TailCallBack);
            }
        }

        if (i >= 4 && i <= 17) {
            a_this->mBodyParts[i].mpFinMorf =
                JKR_NEW mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("B_oh", BMDR_OI_FINA), NULL, NULL,
                                   NULL, 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000084);

            if (a_this->mBodyParts[i].mpFinMorf == NULL ||
                a_this->mBodyParts[i].mpFinMorf->getModel() == NULL)
            {
                return 0;
            }

            a_this->mBodyParts[i].mpFinMorf->getModel()->setUserArea((uintptr_t)i_this);

            for (u16 j = 0;
                 j < a_this->mBodyParts[i].mpFinMorf->getModel()->getModelData()->getJointNum();
                 j++)
            {
                a_this->mBodyParts[i]
                    .mpFinMorf->getModel()
                    ->getModelData()
                    ->getJointNodePointer(j)
                    ->setCallBack(FinACallBack);
            }

            if (dComIfGs_isSwitch(0x1C, fopAcM_GetRoomNo(i_this))) {
                a_this->mBodyParts[i].field_0x3e = 1;
            } else {
                a_this->mBodyParts[i].mFinARot.x = 0xF060;
            }
        }

        if (i >= 7 && i <= 15) {
            a_this->mBodyParts[i].mpFinUnkMorf =
                JKR_NEW mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("B_oh", BMDR_OI_FINA), NULL, NULL,
                                   NULL, 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000084);

            if (a_this->mBodyParts[i].mpFinUnkMorf == NULL ||
                a_this->mBodyParts[i].mpFinUnkMorf->getModel() == NULL)
            {
                return 0;
            }

            a_this->mBodyParts[i].mpFinUnkMorf->getModel()->setUserArea((uintptr_t)i_this);

            for (u16 j = 0;
                 j < a_this->mBodyParts[i].mpFinUnkMorf->getModel()->getModelData()->getJointNum();
                 j++)
            {
                a_this->mBodyParts[i]
                    .mpFinUnkMorf->getModel()
                    ->getModelData()
                    ->getJointNodePointer(j)
                    ->setCallBack(FinACallBack);
            }
        }

        if (i == 8) {
            a_this->mBodyParts[i].mpFinBMorf =
                JKR_NEW mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("B_oh", BMDR_OI_FINB), NULL, NULL,
                                   NULL, 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000084);

            if (a_this->mBodyParts[i].mpFinBMorf == NULL ||
                a_this->mBodyParts[i].mpFinBMorf->getModel() == NULL)
            {
                return 0;
            }

            a_this->mBodyParts[i].mpFinBMorf->getModel()->setUserArea((uintptr_t)i_this);

            for (u16 j = 0;
                 j < a_this->mBodyParts[i].mpFinBMorf->getModel()->getModelData()->getJointNum();
                 j++)
            {
                a_this->mBodyParts[i]
                    .mpFinBMorf->getModel()
                    ->getModelData()
                    ->getJointNodePointer(j)
                    ->setCallBack(FinBCallBack);
            }
        }

        if (i == 17) {
            a_this->mBodyParts[i].mpFinCMorf =
                JKR_NEW mDoExt_McaMorf((J3DModelData*)dComIfG_getObjectRes("B_oh", BMDR_OI_FINC), NULL, NULL,
                                   NULL, 2, 1.0f, 0, -1, 1, NULL, 0x80000, 0x11000084);

            if (a_this->mBodyParts[i].mpFinCMorf == NULL ||
                a_this->mBodyParts[i].mpFinCMorf->getModel() == NULL)
            {
                return 0;
            }

            a_this->mBodyParts[i].mpFinCMorf->getModel()->setUserArea((uintptr_t)i_this);

            for (u16 j = 0;
                 j < a_this->mBodyParts[i].mpFinCMorf->getModel()->getModelData()->getJointNum();
                 j++)
            {
                a_this->mBodyParts[i]
                    .mpFinCMorf->getModel()
                    ->getModelData()
                    ->getJointNodePointer(j)
                    ->setCallBack(FinCCallBack);
            }
        }
    }

    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("B_oh", BMDR_EF_OISUI);
    JUT_ASSERT(0, modelData != NULL);

    a_this->mpSuiModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000284);
    if (a_this->mpSuiModel == NULL) {
        return 0;
    }

    a_this->mpSuiModel->setUserArea((uintptr_t)i_this);

    for (u16 j = 0; j < modelData->getJointNum(); j++) {
        modelData->getJointNodePointer(j)->setCallBack(sui_nodeCallBack);
    }

    a_this->mpSuiBrk = JKR_NEW mDoExt_brkAnm();
    if (a_this->mpSuiBrk == NULL) {
        return 0;
    }

    if (!a_this->mpSuiBrk->init(modelData, (J3DAnmTevRegKey*)dComIfG_getObjectRes("B_oh", BRK_EF_OISUI), TRUE, J3DFrameCtrl::EMode_LOOP, DUSK_IF_ELSE(actor_attr::enemy_action_step(a_this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    a_this->mpSuiBtk = JKR_NEW mDoExt_btkAnm();
    if (a_this->mpSuiBtk == NULL) {
        return 0;
    }

    if (!a_this->mpSuiBtk->init(modelData, (J3DAnmTextureSRTKey*)dComIfG_getObjectRes("B_oh", BTK_EF_OISUI), TRUE, J3DFrameCtrl::EMode_LOOP, DUSK_IF_ELSE(actor_attr::enemy_action_step(a_this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    return 1;
}

static int daB_OB_Create(fopAc_ac_c* i_this) {
    b_ob_class* a_this = (b_ob_class*)i_this;
    fopAcM_ct(i_this, b_ob_class);

    OS_REPORT("B_OB//////////////B_OB SET 0 !!\n");

    int phase_state = dComIfG_resLoad(&a_this->mPhase, "B_oh");
    if (phase_state == cPhs_COMPLEATE_e) {
        if (dComIfGs_isStageBossEnemy()) {
            // "After B_OB defeated, so not re-setting\n"
            OS_REPORT("B_OB やられ後なので再セットしません\n");
            cXyz pos(-1520.0f, -23960.0f, 7100.0f);
            csXyz angle(0, 0, 0);

            fopAcM_createWarpHole(&pos, &angle, fopAcM_GetRoomNo(a_this), 0, 0, 2);

            if (!dComIfGs_isStageLife()) {
                cXyz scale(1.0f, 1.0f, 1.0f);
                pos.set(-916.0f, -23954.0f, 8916.0f);
                fopAcM_createItemForBoss(&pos, dItemNo_UTAWA_HEART_e, fopAcM_GetRoomNo(a_this),
                                         &a_this->shape_angle, &scale, 0.0f, 0.0f, -1);
            }

            return cPhs_ERROR_e;
        }

        OS_REPORT("B_OB PARAM %x\n", fopAcM_GetParam(a_this));
        a_this->shape_angle.z = 0;
        a_this->current.angle.z = 0;
        OS_REPORT("B_OB//////////////B_OB SET 1 !!\n");

        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, 0x1F6A0)) {
            OS_REPORT("//////////////B_OB SET NON !!\n");
            return cPhs_ERROR_e;
        }

        OS_REPORT("//////////////B_OB SET 2 !!\n");

        if (!data_8061B4FD) {
            a_this->mHIOInit = 1;
            data_8061B4FD = true;
            l_HIO.field_0x04 = -1;
        }

#if TARGET_PC  // enemy attribute integration
        a_this->field_0x560 = a_this->health = actor_attr::enemy_health_value(a_this, 30.0f);
        a_this->mTimers[0] = actor_attr::enemy_sync_timer(a_this, 30);
#else
        a_this->health = 30;
        a_this->field_0x560 = 30;
        a_this->mTimers[0] = 30;
#endif
        a_this->field_0x4790 = 1;

        static dCcD_SrcSph cc_sph_src = {
            {
                {0x0, {{AT_TYPE_CSTATUE_SWING, 0x2, 0xd}, {0xd8fbfdff, 0x3}, 0x75}},  // mObj
                {dCcD_SE_METAL, 0x0, 0x2, 0x0, 0x0},                                  // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},                                   // mGObjTg
                {0x0},                                                                // mGObjCo
            },                                                                        // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 50.0f}  // mSph
            }  // mSphAttr
        };

        static dCcD_SrcSph body_sph_src = {
            {
                {0x0, {{0x0, 0x0, 0x0}, {0xd8fbfdff, 0x3}, 0x75}},  // mObj
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                 // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x3},                 // mGObjTg
                {0x0},                                              // mGObjCo
            },                                                      // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 60.0f}  // mSph
            }  // mSphAttr
        };

        static dCcD_SrcSph parts_sph_src = {
            {
                {0x0, {{0x0, 0x0, 0x0}, {0xd8fbfdff, 0x3}, 0x75}},  // mObj
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                 // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x3},                 // mGObjTg
                {0x0},                                              // mGObjCo
            },                                                      // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 60.0f}  // mSph
            }  // mSphAttr
        };

        static dCcD_SrcSph core_sph_src = {
            {
                {0x0, {{0x0, 0x0, 0x0}, {0x4002, 0x3}, 0x75}},  // mObj
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},             // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},             // mGObjTg
                {0x0},                                          // mGObjCo
            },                                                  // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 50.0f}  // mSph
            }  // mSphAttr
        };

        a_this->mCoreCcStts.Init(100, 0, i_this);
        a_this->mCcSph.Set(cc_sph_src);
#if TARGET_PC  // enemy attribute integration
        a_this->mCcSph.SetR(actor_attr::enemy_size_value(a_this, a_this->mCcSph.GetR()));
        a_this->mCcSph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, a_this->mCcSph.GetAtAtp()));
#endif
        a_this->mCcSph.SetStts(&a_this->mCoreCcStts);
        IF_DUSK(a_this->mCcSph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, 2.0f));)

        a_this->mCoreSph.Set(core_sph_src);
#if TARGET_PC  // enemy attribute integration

        a_this->mCoreSph.SetR(actor_attr::enemy_size_value(a_this, a_this->mCoreSph.GetR()));

        a_this->mCoreSph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, a_this->mCoreSph.GetAtAtp()));
#endif
        a_this->mCoreSph.SetStts(&a_this->mCoreCcStts);

        a_this->mBodyCcStts.Init(0xFF, 0, i_this);
        a_this->mBodySph.Set(body_sph_src);
#if TARGET_PC  // enemy attribute integration
        a_this->mBodySph.SetR(actor_attr::enemy_size_value(a_this, a_this->mBodySph.GetR()));
        a_this->mBodySph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, a_this->mBodySph.GetAtAtp()));
#endif
        a_this->mBodySph.SetStts(&a_this->mBodyCcStts);

        for (int i = 0; i < 19; i++) {
            a_this->mBodyParts[i].mSph.Set(parts_sph_src);
#if TARGET_PC  // enemy attribute integration
            a_this->mBodyParts[i].mSph.SetR(actor_attr::enemy_size_value(a_this, a_this->mBodyParts[i].mSph.GetR()));
            a_this->mBodyParts[i].mSph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, a_this->mBodyParts[i].mSph.GetAtAtp()));
#endif
            a_this->mBodyParts[i].mSph.SetStts(&a_this->mBodyCcStts);
            a_this->mBodyParts[i].mSph.OnTgShield();
            a_this->mBodyParts[i].mSph.SetTgHitMark(CcG_Tg_UNK_MARK_2);
        }

        a_this->mFishHeadSph.Set(parts_sph_src);
#if TARGET_PC  // enemy attribute integration

        a_this->mFishHeadSph.SetR(actor_attr::enemy_size_value(a_this, a_this->mFishHeadSph.GetR()));

        a_this->mFishHeadSph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, a_this->mFishHeadSph.GetAtAtp()));
#endif
        a_this->mFishHeadSph.SetStts(&a_this->mBodyCcStts);
        a_this->mFishHeadSph.OnTgShield();
        a_this->mFishHeadSph.SetTgHitMark(CcG_Tg_UNK_MARK_2);

        a_this->mAcch.Set(fopAcM_GetPosition_p(i_this), fopAcM_GetOldPosition_p(i_this), i_this, 1,
                          &a_this->mAcchCir, fopAcM_GetSpeed_p(i_this), NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        a_this->mAcchCir.SetWall(actor_attr::enemy_size_value(a_this, 300.0f), actor_attr::enemy_size_value(a_this, KREG_F(12) + 950.0f));
#else
        a_this->mAcchCir.SetWall(300.0f, KREG_F(12) + 950.0f);
#endif
        a_this->mCoreBattleMode = 1;

        if (cDmr_SkipInfo != 0) {
            a_this->mAction = OB_ACTION_CORE_HAND_MOVE;
            Z2GetAudioMgr()->bgmStart(Z2BGM_BOSS_OCTAEEL_0, 0, 0);
            dComIfGs_onOneZoneSwitch(5, -1);
        } else {
            a_this->mAction = OB_ACTION_CORE_START;
        }

        for (int i = 0; i < 8; i++) {
            a_this->mTentacleActorIDs[i] =
                fopAcM_createChild(fpcNm_B_OH_e, fopAcM_GetID(i_this), i, &i_this->current.pos,
                                   fopAcM_GetRoomNo(i_this), NULL, NULL, -1, NULL);
        }

        for (int i = 0; i < 0x200; i++) {
            a_this->field_0x2324[i].set(0.0f, i_this->home.pos.y - 10000.0f, 0.0f);
        }

        moveSW = true;
        moveSW2 = false;
        a_this->field_0x47a0 = i_this->home.pos.y;

        a_this->mSound.init(&i_this->current.pos, &i_this->eyePos, 3, 1);
        a_this->mAtInfo.mpSound = &a_this->mSound;
        a_this->mOISound.init(
            &a_this->field_0x5be4[0], &a_this->field_0x5be4[1], &a_this->field_0x5be4[2],
            &a_this->field_0x5be4[3], &a_this->field_0x5be4[4], &a_this->field_0x5be4[5],
            &a_this->field_0x5be4[6], &a_this->field_0x5be4[7], &a_this->field_0x5be4[8],
            &a_this->field_0x5be4[9], &a_this->field_0x5be4[10], &a_this->field_0x5be4[11],
            &a_this->field_0x5be4[12]);
        a_this->field_0x5d04 = 0.7f;

        daB_OB_Execute(a_this);
    }

    return phase_state;
}

AUDIO_INSTANCES;

static DUSK_CONST actor_method_class l_daB_OB_Method = {
    (process_method_func)daB_OB_Create,
    (process_method_func)daB_OB_Delete,
    (process_method_func)daB_OB_Execute,
    (process_method_func)daB_OB_IsDelete,
    (process_method_func)daB_OB_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_B_OB = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 4,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_B_OB_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(b_ob_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_B_OB_e,
    /* Actor SubMtd */ &l_daB_OB_Method,
    /* Status       */ fopAcStts_UNK_0x80000_e | fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

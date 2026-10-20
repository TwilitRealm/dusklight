/**
 * @file d_a_e_yh.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_e_yh.h"
#include "d/d_cc_uty.h"
#include "d/actor/d_a_e_yd_leaf.h"
#include "d/d_cc_d.h"
#include "f_op/f_op_actor_enemy.h"
#include "f_op/f_op_kankyo_mng.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"

#include <cmath>
#endif

class daE_YH_HIO_c : public JORReflexible {
public:
    daE_YH_HIO_c();
    virtual ~daE_YH_HIO_c() {}

    void genMessage(JORMContext* ctx);

    /* 0x04 */ s8 mId;
    /* 0x08 */ f32 mBasicSize;
    /* 0x0C */ f32 mFlowerSeasonSize;
    /* 0x10 */ f32 mDisplaySize;
    /* 0x14 */ s16 mCeilingReappearanceInterval;
    /* 0x16 */ s16 mAttackFreq;
};
#if TARGET_PC  // additional actor attribute integration

namespace actor_attr = dusk::mods::svc::actor_attr;

namespace dusk::mods::svc::actor_attr {

template <>
struct EnemyActorAccessor<e_yh_class> {
    static fopAc_ac_c* get(e_yh_class* i_this) {

        return &i_this->enemy;

}
};

template <>
struct EnemyActionTimePolicy<e_yh_class> {
    static bool uses(e_yh_class* i_this) {

        if (i_this->field_0x66e == 4 && i_this->field_0x670 == 5) {
            return false;
        }

        return i_this->field_0x66e != 20 || i_this->field_0x670 != 20;

}
};

}  // namespace dusk::mods::svc::actor_attr
#endif

daE_YH_HIO_c::daE_YH_HIO_c() {
    mId = -1;
    mBasicSize = 1.0f;
    mFlowerSeasonSize = 0.5f;
    mDisplaySize = 1.2f;
    mCeilingReappearanceInterval = 70;
    mAttackFreq = 30;
}

#if DEBUG
void daE_YH_HIO_c::genMessage(JORMContext* ctx) {
    ctx->genLabel("  デグババ", 0x80000001);
    ctx->genSlider("基本サイズ", &mBasicSize, 0.0f, 5.0f);
    ctx->genSlider("花時サイズ", &mFlowerSeasonSize, 0.0f, 3.0f);
    ctx->genSlider("出現サイズ", &mDisplaySize, 0.0f, 3.0f);
    ctx->genSlider("天井再出現間隔", &mCeilingReappearanceInterval, 0, 300);
    ctx->genSlider("攻撃頻度", &mAttackFreq, 0, 300);
}
#endif

static void yh_disappear(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz acStack_30;
    cXyz cStack_3c;
#if TARGET_PC  // enemy attribute integration
    const f32 effectScale = 1.5f * actor_attr::enemy_size_multiplier(i_this);
    cXyz cStack_48(effectScale, effectScale, effectScale);
#else
    cXyz cStack_48(1.5f, 1.5f, 1.5f);
#endif
    dComIfGp_particle_set(dPa_RM(ID_ZI_S_YAMIDEAD_NORMAL_A), &a_this->current.pos, 0, &cStack_48);
    dComIfGp_particle_set(dPa_RM(ID_ZI_S_YAMIDEAD_NORMAL_B), &a_this->current.pos, 0, &cStack_48);
    fopAcM_seStart(a_this, Z2SE_DARK_VANISH, 0);
    fopAcM_createItemFromEnemyID(0x2f, &a_this->current.pos, -1, -1, NULL,
                                 NULL, NULL, NULL);
    fopAcM_delete(a_this);
    a_this->field_0x567 = 1;
    for (int i = 0; i <= 2; i++) {
        JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->mParticleIds[i]);
        if (emitter != NULL) {
            emitter->deleteAllParticle();
            dComIfGp_particle_levelEmitterOnEventMove(i_this->mParticleIds[i]);
        }
    }
}

static void anm_init(e_yh_class* i_this, int param_2, f32 param_3, u8 param_4, f32 param_5) {
#if TARGET_PC  // enemy attribute integration

    const actor_attr::ActionAnimationParams params = actor_attr::action_animation_params(param_3, param_5, actor_attr::enemy_action_time_speed(i_this));
#endif
    i_this->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_yd", param_2), param_4,
#if TARGET_PC  // enemy attribute integration
                           params.morph, params.playbackSpeed, 0.0f, -1.0f);
#else
                           param_3, param_5, 0.0f, -1.0f);
#endif
    i_this->field_0x664 = param_2;
}

static BOOL leaf_anm_init(e_yh_class* i_this, int param_2, f32 param_3, u8 param_4, f32 param_5) {
    e_yd_leaf_class* leaf = (e_yd_leaf_class*)fopAcM_SearchByID(i_this->mChildID);
    if (leaf != NULL) {
#if TARGET_PC  // enemy attribute integration
        const actor_attr::ActionAnimationParams params = actor_attr::action_animation_params(param_3, param_5, actor_attr::enemy_action_time_speed(i_this));
#endif
        leaf->mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_yd", param_2), param_4,
#if TARGET_PC  // enemy attribute integration
                               params.morph, params.playbackSpeed, 0.0f, -1.0f, NULL);
        leaf->speedF = actor_attr::enemy_action_time_speed(i_this);
#else
                               param_3, param_5, 0.0f, -1.0f, NULL);
#endif
        return TRUE;
    }

    return FALSE;
}

static int daE_YH_Draw(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif

    g_env_light.settingTevStruct(2, &a_this->current.pos, &a_this->tevStr);

    J3DModel* model = i_this->mpMorf->getModel();
    g_env_light.setLightTevColorType_MAJI(model, &a_this->tevStr);

    i_this->mBrk->entry(model->getModelData());
    i_this->mpMorf->entryDL();

    if (i_this->field_0x66e != 10) {
        cXyz shadowPos;
        shadowPos.set(a_this->current.pos.x, a_this->current.pos.y + DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f), a_this->current.pos.z);
        i_this->mShadowKey = dComIfGd_setShadow(
            i_this->mShadowKey, 1, model, &shadowPos, DUSK_IF_ELSE(600.0f * sizeMultiplier, 600.0f), 0.0f, a_this->current.pos.y,
            fopAcM_gc_c::getGroundY(), *fopAcM_gc_c::getGroundCheck(), &a_this->tevStr, 0, 1.0f,
            dDlst_shadowControl_c::getSimpleTex());
    }

    static GXColor l_color = {
        0x14,
        0x0F,
        0x00,
        0xFF,
    };

    i_this->mLine.update(12, l_color, &a_this->tevStr);
    dComIfGd_set3DlineMat(&i_this->mLine);

    for (int i = 1; i < 11; i++) {
        if (i_this->mModels[i] != NULL) {
            g_env_light.setLightTevColorType_MAJI(i_this->mModels[i], &a_this->tevStr);
            mDoExt_modelUpdateDL(i_this->mModels[i]);
        }
    }

    return 1;
}

static BOOL pl_check(e_yh_class* i_this, f32 param_2) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (i_this->field_0x68c < param_2 && !fopAcM_otherBgCheck(a_this, player)) {
        return 1;
    }
    return 0;
}

static void damage_check(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    if (i_this->field_0x69e != 0) {
        return;
    }

    i_this->mStts.Move();
    if (i_this->mAtSph.ChkAtShieldHit()) {
        if (i_this->field_0x66e == 20) {
            a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 10.0f), 10.0f);
            a_this->speedF = -15.0f;
            i_this->field_0x864 = 0;
            a_this->shape_angle.x = 0xf000;
            i_this->field_0x69e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 5.0f), 5);
        } else {
            i_this->field_0x69e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 6.0f), 6);
            i_this->field_0x66e = 7;
            i_this->field_0x670 = 0;
            i_this->field_0x684 = player->shape_angle.y;
            i_this->field_0x123c = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10), 10);
        }
        
        dComIfGp_getVibration().StartShock(4, 0x1f, cXyz(0.0f, 1.0f, 0.0f));
    } else {
        s8 bVar5 = FALSE;
        if (i_this->field_0x66e < 20) {
            for (int i = 0; i < 4; i++) {
                if (i_this->mKukiSphs[i].ChkTgHit()) {
                    i_this->field_0x69e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10.0f), 10);
                    i_this->mAtInfo.mpCollider = i_this->mKukiSphs[i].GetTgHitObj();
                    if (i_this->mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOOMERANG)) {
                        bVar5 = TRUE;
                    } else {
                        if (i_this->mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOMB |
                                                                    AT_TYPE_MIDNA_LOCK))
                        {
                            i_this->field_0x66e = 21;
                            i_this->mAtInfo.mpActor =
                                dCc_GetAc(i_this->mAtInfo.mpCollider->GetAc());
                            i_this->field_0x684 =
                                cM_atan2s(-(a_this->current.pos.x -
                                            i_this->mAtInfo.mpActor->current.pos.x),
                                            -(a_this->current.pos.z -
                                            i_this->mAtInfo.mpActor->current.pos.z));
                            i_this->field_0x670 = 0;
                            return;
                        }
                        at_power_check(&i_this->mAtInfo);
                        bVar5 = TRUE;
                    }
                    break;
                }
            }
        }

        if (i_this->mCcSph.ChkTgHit()) {
            i_this->field_0x123c = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10), 10);
            i_this->mAtInfo.mpCollider = i_this->mCcSph.GetTgHitObj();
            cc_at_check(a_this, &i_this->mAtInfo);
            if (i_this->mAtInfo.mpCollider->ChkAtType(AT_TYPE_BOMB | AT_TYPE_MIDNA_LOCK)) {
                i_this->field_0x66e = 21;
                i_this->mAtInfo.mpActor = dCc_GetAc(i_this->mAtInfo.mpCollider->GetAc());
                i_this->field_0x684 = cM_atan2s(
                    -(a_this->current.pos.x - i_this->mAtInfo.mpActor->current.pos.x),
                    -(a_this->current.pos.z - i_this->mAtInfo.mpActor->current.pos.z));
                i_this->field_0x670 = 0;
                return;
            }

            i_this->field_0x69e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 6.0f), 6);
            if (i_this->mAtInfo.mHitType == HIT_TYPE_STUN) {
                if (i_this->field_0x66e == 20) {
                    i_this->field_0x85c = 60.0f;
                    i_this->field_0x860 = i_this->mAtInfo.mHitDirection.y;
                    i_this->field_0x670 = 10;
                } else {
                    i_this->field_0x66e = 7;
                    i_this->field_0x670 = 0;
                    i_this->field_0x684 = player->shape_angle.y;
                }
            } else if (i_this->field_0x66e < 20) {
                i_this->field_0x66e = 7;
                i_this->field_0x670 = 0;
                i_this->field_0x684 = i_this->mAtInfo.mHitDirection.y;
                a_this->health = DUSK_IF_ELSE(actor_attr::enemy_health_value(i_this, 30.0f), 30);
            } else if (i_this->field_0x66e == 20) {
                if (a_this->health <= 0) {
                    i_this->field_0x684 = i_this->mAtInfo.mHitDirection.y;
                    i_this->field_0x670 = 0;
                    i_this->field_0x66e = 21;
                    return;
                }

                i_this->field_0x85c = 60.0f;
                i_this->field_0x860 = i_this->mAtInfo.mHitDirection.y;
                i_this->field_0x670 = 10;
            }
        }

        if (a_this->health <= DUSK_IF_ELSE(actor_attr::enemy_health_value(i_this, 10.0f), 10)) {
            a_this->health = 0;
            i_this->mCcSph.SetTgHitMark(CcG_Tg_UNK_MARK_3);
        }

        if (bVar5) {
            if (i_this->field_0x66e == 20) {
                i_this->field_0x85c = 30.0f;
                i_this->field_0x860 = -player->shape_angle.y;
                i_this->field_0x670 = 10;
                a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 5.0f), 5.0f);
            } else {
                i_this->field_0x66e = 20;
                i_this->field_0x670 = 0;
                i_this->field_0x852 = 5000;
                i_this->field_0x854 = 0;
                a_this->speedF = 0.0f;
                i_this->field_0x698[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 45.0f), 45);
                i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_CUT, -1);
                def_se_set(&i_this->mSound, i_this->mAtInfo.mpCollider, 0x21,
                                        NULL);
                if (i_this->field_0x5b4 == 1) {
                    i_this->field_0x868 = 20.0f;
#if TARGET_PC  // enemy attribute integration
                    a_this->speed.y = actor_attr::enemy_move_step(i_this, 5.0f);
                    a_this->field_0x560 = actor_attr::enemy_health_value(i_this, 10.0f);
                    a_this->health = a_this->field_0x560;
#else
                    a_this->speed.y = 5.0f;
                    a_this->field_0x560 = 10;
                    a_this->health = 10;
#endif
                } else if (i_this->field_0x5b4 == 2) {
                    i_this->field_0x868 = 20.0f;
                    a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 5.0f), 5.0f);
                } else {
                    i_this->field_0x868 = -10.0f;
                    a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 30.0f), 30.0f);
                }
            }
        }
    }
}

static bool hio_set;

static daE_YH_HIO_c l_HIO;
#if TARGET_PC  // enemy attribute integration
static f32 yh_effective_size(e_yh_class* i_this) {

    return l_HIO.mBasicSize * i_this->field_0x694 * actor_attr::enemy_size_multiplier(i_this);
}
#endif

static void e_yh_stay(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz cStack_1c;
    i_this->field_0x69e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 15.0f), 15);

    switch (i_this->field_0x670) {
    case 0:
        i_this->field_0x678.y = 0.0f;
        // fallthrough
    case 1:
        anm_init(i_this, 14, 10.0f, 0, 1.0f);
        if (leaf_anm_init(i_this, 21, 10.0f, 2, 0.0f)) {
            i_this->field_0x670 = 5;
            i_this->field_0x690 = 0.0f;
            i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 50.0f), 50);
        }
        break;
    case 2:
        if (i_this->field_0x854 != 0) {
            i_this->field_0x678.y = DUSK_IF_ELSE(-170.0f * sizeMultiplier, -170.0f);
        } else {
            i_this->field_0x678.y = DUSK_IF_ELSE(170.0f * sizeMultiplier, 170.0f);
        }
        i_this->field_0x670 = 1;
        break;
    case 5:
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, a_this->home.pos.x, 0.2f,
#else
        cLib_addCalc2(&a_this->current.pos.x, a_this->home.pos.x, 0.2f,
#endif
                      i_this->field_0x690 * 10.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, a_this->home.pos.y + i_this->field_0x678.y, 0.2f,
#else
        cLib_addCalc2(&a_this->current.pos.y, a_this->home.pos.y + i_this->field_0x678.y, 0.2f,
#endif
                      i_this->field_0x690 * 10.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, a_this->home.pos.z, 0.2f,
#else
        cLib_addCalc2(&a_this->current.pos.z, a_this->home.pos.z, 0.2f,
#endif
                      i_this->field_0x690 * 10.0f);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.1f), cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.1f));
        cStack_1c = a_this->current.pos - a_this->home.pos;
        cStack_1c.y = 0.0f;
#if TARGET_PC  // enemy attribute integration
        if (cStack_1c.abs() < 20.0f * sizeMultiplier) {
            actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x678.y, 1.0f, 4.0f);
#else
        if (cStack_1c.abs() < 20.0f) {
            cLib_addCalc0(&i_this->field_0x678.y, 1.0f, 4.0f);
#endif
            if (i_this->field_0x854 != 0) {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0x4000, 4, 0x200), cLib_addCalcAngleS2(&a_this->shape_angle.x, 0x4000, 4, 0x200));
            } else {
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, -0x4000, 4, 0x200), cLib_addCalcAngleS2(&a_this->shape_angle.x, -0x4000, 4, 0x200));
            }
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x848, 0.4f, 1.0f, 0.02f);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x694, l_HIO.mFlowerSeasonSize, 1.0f, 0.05f);
#else
            cLib_addCalc2(&i_this->field_0x848, 0.4f, 1.0f, 0.02f);
            cLib_addCalc2(&i_this->field_0x694, l_HIO.mFlowerSeasonSize, 1.0f, 0.05f);
#endif
        }
        if (i_this->field_0x698[0] == 0 &&
            (i_this->field_0x5b7 == 1 || pl_check(i_this, DUSK_IF_ELSE(actor_attr::enemy_notice_distance(i_this, i_this->field_0x674), i_this->field_0x674))))
        {
            if (i_this->field_0x854 != 0) {
                i_this->field_0x66e = 2;
            } else {
                i_this->field_0x66e = 1;
            }
            i_this->field_0x670 = 0;
        }
        break;
    }
}

static void e_yh_appear(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    f32 frame = i_this->mpMorf->getFrame();
    int frameInt = (int)frame;
    
    switch (i_this->field_0x670) {
    case 0:
        anm_init(i_this, 6, 10.0f, 0, 1.0f);
        leaf_anm_init(i_this, 20, 10.0f, 2, 1.0f);
        i_this->field_0x670 = 1;
        
#if TARGET_PC  // enemy attribute integration
        i_this->field_0x678.x = a_this->home.pos.x + cM_rndFX(100.0f) * sizeMultiplier;
        i_this->field_0x678.z = a_this->home.pos.z + cM_rndFX(100.0f) * sizeMultiplier;
#else
        i_this->field_0x678.x = a_this->home.pos.x + cM_rndFX(100.0f);
        i_this->field_0x678.z = a_this->home.pos.z + cM_rndFX(100.0f);
#endif
        
        if (i_this->field_0x854 != 0) {
            i_this->field_0x678.y = a_this->home.pos.y - DUSK_IF_ELSE((170.0f + cM_rndFX(50.0f)) * sizeMultiplier, (170.0f + cM_rndFX(50.0f)));
        } else {
            i_this->field_0x678.y = DUSK_IF_ELSE(a_this->home.pos.y + 170.0f * sizeMultiplier + cM_rndFX(50.0f) * sizeMultiplier, a_this->home.pos.y + 170.0f + cM_rndFX(50.0f));
        }
        i_this->field_0x690 = 0.2f;
        
        i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        i_this->mSound.startCreatureSound(Z2SE_EN_DB_APPEAR, 0, -1);
        break;
    case 1:
        if (15 <= frameInt && frameInt <= 35) {
            i_this->field_0x123d = 2;
        }
        i_this->field_0xb18 = 1;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 0.1f, i_this->field_0x690 * 30.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 0.1f, i_this->field_0x690 * 30.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 0.1f, i_this->field_0x690 * 30.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.05f);
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 0.1f, i_this->field_0x690 * 30.0f);
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 0.1f, i_this->field_0x690 * 30.0f);
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 0.1f, i_this->field_0x690 * 30.0f);
        cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.05f);
#endif
        
        if (i_this->mpMorf->isStop()) {
            i_this->field_0x66e = 3;
            i_this->field_0x670 = 0;
        }
        break;
        
    
    }
    
    if (i_this->field_0x854 != 0) {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, -0x8000, 8, 0x400);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800);
        cLib_addCalcAngleS2(&a_this->shape_angle.x, -0x8000, 8, 0x400);
#endif
    } else {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688, 8, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0, 8, 0x400);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688, 8, 0x800);
        cLib_addCalcAngleS2(&a_this->shape_angle.x, 0, 8, 0x400);
#endif
    }
    
    DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x694, l_HIO.mDisplaySize, 1.0f, 0.1f), cLib_addCalc2(&i_this->field_0x694, l_HIO.mDisplaySize, 1.0f, 0.1f));
}

static void e_yh_appear_v(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_34;
    f32 dVar6 = DUSK_IF_ELSE(60.0f * sizeMultiplier, 60.0f);
    switch (i_this->field_0x670) {
    case 0:
        anm_init(i_this, 6, 5.0f, 0, 1.0f);
        leaf_anm_init(i_this, 0x14, 5.0f, 2, 1.0f);
        i_this->field_0x670 = 1;
        i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30.0f), 30);
        i_this->field_0x690 = 0.0f;
        i_this->mpMorf->setFrame(15.0f);
        // fallthrough
    case 1:
#if TARGET_PC  // enemy attribute integration
        local_34.x = dVar6 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 0x4b0));
        local_34.z = dVar6 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 0x5dc));
#else
        local_34.x = dVar6 * cM_ssin(i_this->field_0x66c * 0x4b0);
        local_34.z = dVar6 * cM_scos(i_this->field_0x66c * 0x5dc);
#endif

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, a_this->home.pos.x + local_34.x, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.x, a_this->home.pos.x + local_34.x, 0.1f,
#endif
                      i_this->field_0x690 * 40.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, a_this->home.pos.z + local_34.z, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.z, a_this->home.pos.z + local_34.z, 0.1f,
#endif
                      i_this->field_0x690 * 40.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, a_this->home.pos.y - 400.0f * sizeMultiplier, 0.5f,
#else
        cLib_addCalc2(&a_this->current.pos.y, a_this->home.pos.y - 400.0f, 0.5f,
#endif
                      i_this->field_0x690 * 40.0f);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.2f), cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.2f));

        if (i_this->field_0x698[0] == 0) {
            i_this->field_0x66e = 3;
            i_this->field_0x670 = 0;
            i_this->field_0x84c = 0.0f;
        }
        break;
    }

#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0x4000, 4, 0x400);
    actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x848, 0.5f, 1.0f, 0.05f);
    actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x694, l_HIO.mDisplaySize, 1.0f, 0.15f);
#else
    cLib_addCalcAngleS2(&a_this->shape_angle.x, 0x4000, 4, 0x400);
    cLib_addCalc2(&i_this->field_0x848, 0.5f, 1.0f, 0.05f);
    cLib_addCalc2(&i_this->field_0x694, l_HIO.mDisplaySize, 1.0f, 0.15f);
#endif
    i_this->field_0xb18 = 1;
}

static void e_yh_wait(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_48;
    f32 dVar10 = DUSK_IF_ELSE(30.0f * sizeMultiplier, 30.0f);

    if (DUSK_IF_ELSE(actor_attr::action_event_count(i_this->field_0x66c , 16, actor_attr::enemy_action_time_speed(i_this) ) > 0, (i_this->field_0x66c & 0xf) == 0) && cM_rndF(1.0f) < 0.75f) {
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_WAIT, -1);
    }

    switch (i_this->field_0x670) {
    case -1:
        if (i_this->mpMorf->isStop()) {
            i_this->field_0x670 = 0;
        } else {
#if TARGET_PC  // enemy attribute integration
            cXyz cStack_54(0.0f, 90.0f * sizeMultiplier, -100.0f * sizeMultiplier);
#else
            cXyz cStack_54(0.0f, 90.0f, -100.0f);
#endif
            cXyz cStack_60;
            mDoMtx_YrotS(*calc_mtx, a_this->shape_angle.y);
            MtxPosition(&cStack_54, &cStack_60);
            i_this->field_0x678 = a_this->home.pos + cStack_60;

#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 0.3f, 30.0f);
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 0.3f, 30.0f);
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 0.3f, 30.0f);
#else
            cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 0.3f, 30.0f);
            cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 0.3f, 30.0f);
            cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 0.3f, 30.0f);
#endif
            break;
        }
        // fallthrough
    case 0:
        anm_init(i_this, 19, 10.0f, 2, 1.0f);
        leaf_anm_init(i_this, 21, 10.0f, 2, 1.0f);
        i_this->field_0x670 = 1;

        if (i_this->field_0x5b6 == 1) {
#if TARGET_PC  // enemy attribute integration
            const s16 vanillaAttackDelay = cM_rndF(20.0f) + 30.0f;
            i_this->field_0x698[1] = actor_attr::enemy_sync_timer(i_this, vanillaAttackDelay);
#else
            i_this->field_0x698[1] = cM_rndF(20.0f) + 30.0f;
#endif
        } else {
#if TARGET_PC  // enemy attribute integration
            const s16 vanillaAttackDelay = l_HIO.mAttackFreq + cM_rndF(l_HIO.mAttackFreq);
            i_this->field_0x698[1] = actor_attr::enemy_sync_timer(i_this, vanillaAttackDelay);
#else
            i_this->field_0x698[1] = l_HIO.mAttackFreq + cM_rndF(l_HIO.mAttackFreq);
#endif
        }
        i_this->field_0x698[0] = 0;
        break;
    case 1:
        if (i_this->field_0x698[0] == 0) {
#if TARGET_PC  // enemy attribute integration
            i_this->field_0x698[0] = actor_attr::enemy_sync_timer(i_this, cM_rndF(30.0f) + 10.0f);
            i_this->field_0x678.x = a_this->home.pos.x + cM_rndFX(100.0f) * sizeMultiplier;
            i_this->field_0x678.z = a_this->home.pos.z + cM_rndFX(100.0f) * sizeMultiplier;
#else
            i_this->field_0x698[0] = cM_rndF(30.0f) + 10.0f;
            i_this->field_0x678.x = a_this->home.pos.x + cM_rndFX(100.0f);
            i_this->field_0x678.z = a_this->home.pos.z + cM_rndFX(100.0f);
#endif
            if (i_this->field_0x854 != 0) {
                i_this->field_0x678.y = a_this->home.pos.y - DUSK_IF_ELSE((170.0f + cM_rndFX(50.0f)) * sizeMultiplier, (170.0f + cM_rndFX(50.0f)));
            } else {
                i_this->field_0x678.y = DUSK_IF_ELSE(a_this->home.pos.y + 170.0f * sizeMultiplier + cM_rndFX(50.0f) * sizeMultiplier, a_this->home.pos.y + 170.0f + cM_rndFX(50.0f));
            }
            i_this->field_0x690 = 0.0f;
        }

#if TARGET_PC  // enemy attribute integration
        local_48.x = dVar10 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 1900));
        local_48.y = dVar10 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 2200));
        local_48.z = dVar10 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 2000));
#else
        local_48.x = dVar10 * cM_ssin(i_this->field_0x66c * 1900);
        local_48.y = dVar10 * cM_ssin(i_this->field_0x66c * 2200);
        local_48.z = dVar10 * cM_scos(i_this->field_0x66c * 2000);
#endif

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x + local_48.x, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x + local_48.x, 0.1f,
#endif
                      i_this->field_0x690 * 20.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z + local_48.z, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z + local_48.z, 0.1f,
#endif
                      i_this->field_0x690 * 20.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y + local_48.y, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y + local_48.y, 0.1f,
#endif
                      i_this->field_0x690 * 20.0f);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.05f), cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.05f));

        if (i_this->field_0x5b7 != 1 && !pl_check(i_this, DUSK_IF_ELSE(actor_attr::enemy_notice_distance(i_this, i_this->field_0x674 + 100.0f), i_this->field_0x674 + 100.0f))) {
            i_this->field_0x66e = 0;
            i_this->field_0x670 = 2;
            i_this->mSound.startCreatureSound(Z2SE_EN_DB_HIKKOMU, 0, -1);
        } else {
            daPy_py_c* player = (daPy_py_c*)dComIfGp_getPlayer(0);
            if (player->getDkCaught() == 0) {
                if (!player->getDkCaught2() && i_this->field_0x698[1] == 0 &&
                    pl_check(i_this, DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, 700.0f), 700.0f)))
                {
                    i_this->field_0x66e = 4;
                    i_this->field_0x670 = 0;
                }
            }
        }
        break;
    }

    if (i_this->field_0x854 != 0) {
        fopAc_ac_c* player = dComIfGp_getPlayer(0);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800), cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800));
        cXyz local_6c = player->eyePos - a_this->current.pos;
        s16 angle = (s16)cM_atan2s(local_6c.y, JMAFastSqrt(local_6c.x * local_6c.x + local_6c.z * local_6c.z));
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, angle + 0x8000, 8, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, angle + 0x8000, 8, 0x400));
        i_this->field_0xb18 = 1;
    } else {
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688, 8, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0, 8, 0x400);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688, 8, 0x800);
        cLib_addCalcAngleS2(&a_this->shape_angle.x, 0, 8, 0x400);
#endif
    }
}

static void e_yh_mk_roof(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_34;
    f32 dVar7 = DUSK_IF_ELSE(30.0f * sizeMultiplier, 30.0f);
    
    if (DUSK_IF_ELSE(actor_attr::action_event_count(i_this->field_0x66c , 16, actor_attr::enemy_action_time_speed(i_this) ) > 0, (i_this->field_0x66c & 0xf) == 0) && cM_rndF(1.0f) < 0.75f) {
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_WAIT, -1);
    }
    
    switch (i_this->field_0x670) {
    case 0:
        if (i_this->field_0x698[0] == 0) {
            anm_init(i_this, 19, 10.0f, 2, 1.0f);
            leaf_anm_init(i_this, 21, 10.0f, 2, 1.0f);
            i_this->field_0x670 = 1;
            i_this->field_0x698[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(50.0f) + 50.0f), cM_rndF(50.0f) + 50.0f);
            i_this->field_0x698[0] = 0;
            i_this->field_0x694 = l_HIO.mDisplaySize;
        } else {
            i_this->field_0x694 = l_HIO.mFlowerSeasonSize;
        }
        break;
    case 1:
        if (i_this->field_0x698[0] == 0) {
#if TARGET_PC  // enemy attribute integration
            i_this->field_0x698[0] = actor_attr::enemy_sync_timer(i_this, cM_rndF(30.0f) + 10.0f);
            i_this->field_0x678.x = a_this->home.pos.x + cM_rndFX(70.0f) * sizeMultiplier;
            i_this->field_0x678.z = a_this->home.pos.z + cM_rndFX(70.0f) * sizeMultiplier;
            i_this->field_0x678.y = a_this->home.pos.y - 250.0f * sizeMultiplier;
#else
            i_this->field_0x698[0] = cM_rndF(30.0f) + 10.0f;
            i_this->field_0x678.x = a_this->home.pos.x + cM_rndFX(70.0f);
            i_this->field_0x678.z = a_this->home.pos.z + cM_rndFX(70.0f);
            i_this->field_0x678.y = a_this->home.pos.y - 250.0f;
#endif
            i_this->field_0x690 = 0.0f;
        }
        
#if TARGET_PC  // enemy attribute integration
        local_34.x = dVar7 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 1900));
        local_34.z = dVar7 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 2000));
#else
        local_34.x = dVar7 * cM_ssin(i_this->field_0x66c * 1900);
        local_34.z = dVar7 * cM_scos(i_this->field_0x66c * 2000);
#endif
        
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x + local_34.x, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x + local_34.x, 0.1f,
#endif
                      i_this->field_0x690 * 20.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z + local_34.z, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z + local_34.z, 0.1f,
#endif
                      i_this->field_0x690 * 20.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 0.1f,
#else
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 0.1f,
#endif
                      i_this->field_0x690 * 20.0f);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.05f), cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.05f));
        break;
    }
    
    a_this->shape_angle.x = 0x4000;
    a_this->shape_angle.z = 0;
}

static void e_yh_attack(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_68;
    cXyz cStack_74;
    s16 local_e6 = 0;
    
    if (i_this->field_0x854 != 0) {
        local_68 = player->eyePos - a_this->current.pos;
        local_e6 = (s16)cM_atan2s(local_68.y, JMAFastSqrt(local_68.x * local_68.x + local_68.z * local_68.z));
    }
    
    switch (i_this->field_0x670) {
    case 0:
        i_this->field_0x670 = 1;
        i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 17.0f), 17);
        i_this->field_0x690 = 0.0f;
        i_this->field_0xb19 = 0;
        
        i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_ATTACK, -1);
        leaf_anm_init(i_this, 20, 5.0f, 2, 1.0f);
        anm_init(i_this, 7, 5.0f, 0, 1.0f);
        // fallthrough
    case 1:
        if (i_this->field_0x854 != 0) {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800), cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800));
            local_68.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
            local_68.y = -170.0f * sizeMultiplier;
            local_68.z = 100.0f * sizeMultiplier;
#else
            local_68.y = -170.0f;
            local_68.z = 100.0f;
#endif
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688, 8, 0x800), cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688, 8, 0x800));
            local_68.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
            local_68.y = 170.0f * sizeMultiplier;
            local_68.z = -100.0f * sizeMultiplier;
#else
            local_68.y = 170.0f;
            local_68.z = -100.0f;
#endif
        }
        
        mDoMtx_YrotS(*calc_mtx, a_this->shape_angle.y);
        MtxPosition(&local_68, &cStack_74);
        i_this->field_0x678 = a_this->home.pos + cStack_74;
        
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 0.1f, i_this->field_0x690 * 20.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 0.1f, i_this->field_0x690 * 20.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 0.1f, i_this->field_0x690 * 20.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.1f);
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 0.1f, i_this->field_0x690 * 20.0f);
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 0.1f, i_this->field_0x690 * 20.0f);
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 0.1f, i_this->field_0x690 * 20.0f);
        cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.1f);
#endif
        
        if (i_this->field_0x698[0] == 0) {
            i_this->field_0x670 = 2;
            i_this->field_0x698[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10.0f), 10);
            
            if (i_this->field_0x854 != 0) {
                mDoMtx_YrotS(*calc_mtx, a_this->shape_angle.y + 0x8000);
                mDoMtx_XrotM(*calc_mtx, -local_e6);
                local_68.x = 0.0f;
                local_68.y = 0.0f;
                local_68.z = DUSK_IF_ELSE(350.0f * sizeMultiplier, 350.0f);
                MtxPosition(&local_68, &cStack_74);
                i_this->field_0x678 = a_this->current.pos + cStack_74;
            } else {
                mDoMtx_YrotS(*calc_mtx, a_this->shape_angle.y);
                local_68.x = 0.0f;
                local_68.y = 0.0f;
                local_68.z = DUSK_IF_ELSE(350.0f * sizeMultiplier, 350.0f);
                MtxPosition(&local_68, &cStack_74);
                i_this->field_0x678 = a_this->home.pos + cStack_74;
                i_this->field_0x678.y = player->current.pos.y + DUSK_IF_ELSE(100.0f * sizeMultiplier, 100.0f);
            }
            
            a_this->speed.x = fabsf(i_this->field_0x678.x - a_this->current.pos.x);
            a_this->speed.y = fabsf(i_this->field_0x678.y - a_this->current.pos.y);
            a_this->speed.z = fabsf(i_this->field_0x678.z - a_this->current.pos.z);
            i_this->field_0x690 = 0.0f;
            
            i_this->mAtSph.StartCAt(a_this->current.pos);
            
            if (i_this->field_0x854 == 0) {
                if (cM_rndF(1.0f) < 0.5f) {
                    i_this->field_0x864 = 0x4000;
                } else {
                    i_this->field_0x864 = -0x4000;
                }
            }
            
            i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_BITE, -1);
        }
        break;

    case 2: {
        if (i_this->field_0x698[1] != 0) {
            i_this->field_0xb18 = 1;
        }

#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 0.8f,
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 0.8f,
#endif
                      a_this->speed.x * i_this->field_0x690);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 0.8f,
#else
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 0.8f,
#endif
                      a_this->speed.z * i_this->field_0x690);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 0.8f,
#else
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 0.8f,
#endif
                      a_this->speed.y * i_this->field_0x690);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 0.2f, 1.0f, 0.05f);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, i_this->field_0x864, 2, 0x2000);
#else
        cLib_addCalc2(&i_this->field_0x690, 0.2f, 1.0f, 0.05f);
        cLib_addCalcAngleS2(&a_this->shape_angle.z, i_this->field_0x864, 2, 0x2000);
#endif

        daPy_py_c* pdVar6 = (daPy_py_c*)daPy_getPlayerActorClass();
        if (!pdVar6->getDkCaught()) {
            if (!pdVar6->getDkCaught2() && i_this->mAtSph.ChkAtHit()) {
                cCcD_Obj* hitObj = i_this->mAtSph.GetAtHitObj();

                if (player == dCc_GetAc(hitObj->GetAc())) {
                    OS_REPORT("E_DB//////////////AT  SET 1 !!\n");
                    i_this->field_0x670 = 5;
                    i_this->field_0x690 = 20.0f;
                    anm_init(i_this, 8, 2.0f, 2, 1.0f);
                    i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120.0f), 120);

                    daPy_py_c* pyPlayer = (daPy_py_c*)daPy_getPlayerActorClass();
                    pyPlayer->setDkCaught(a_this);

                    dComIfGp_getVibration().StartShock(6, 0x1f, cXyz(0.0f, 1.0f, 0.0f));
                    break;
                }
            }
        }

        if (i_this->mpMorf->isStop()) {
            i_this->field_0x670 = 3;
            i_this->field_0x698[0] = 0;
        }
        break;
    }
    case 3:
        if (i_this->field_0x698[0] == 0) {
            i_this->field_0x66e = 3;
            i_this->field_0x670 = 0;
        }
        break;
        
    case 5:
        i_this->field_0x855 = 1;
        i_this->mSound.startCreatureVoiceLevel(Z2SE_EN_DB_V_BITING, -1);
        i_this->mCcSph.OffCoSetBit();
        local_e6 = 0;
        
        if (daPy_getPlayerActorClass()->getDkCaught()) {
#if TARGET_PC  // enemy attribute integration
            if (i_this->field_0x698[0] == actor_attr::enemy_sync_timer(i_this, 30.0f)) {
                dComIfGp_setItemLifeCount(-actor_attr::enemy_attack_damage(i_this, 1.0f), 0);
#else
            if (i_this->field_0x698[0] == 30) {
                dComIfGp_setItemLifeCount(-1.0f, 0);
#endif
            }
            
            if (DUSK_IF_ELSE(actor_attr::action_event_count(i_this->field_0x66c , 16, actor_attr::enemy_action_time_speed(i_this) ) > 0, (i_this->field_0x66c & 0xf) == 0)) {
                dComIfGp_getVibration().StartShock(3, 0x1f, cXyz(0.0f, 1.0f, 0.0f));
            }
            
            local_68 = player->current.pos - a_this->home.pos;
            s16 sVar12_2 = cM_atan2s(local_68.x, local_68.z);
            local_68.y = 0.0f;
            f32 dVar15 = local_68.abs();
            
            if (dVar15 > 100.0f) {
                f32 pushDist = 0.1f * (dVar15 - 100.0f);
                if (pushDist > 100.0f) {
                    pushDist = 100.0f;
                }
                
                daPy_getPlayerActorClass()->setOutPower(pushDist, sVar12_2 + 0x8000, 0);
            }
            
            if (i_this->field_0x854 == 0) {
                if (i_this->field_0x864 == 0) {
                    if (cM_rndF(1.0f) < 0.5f) {
                        i_this->field_0x864 = 0x4000;
                    } else {
                        i_this->field_0x864 = -0x4000;
                    }
                }
                
                cMtx_YrotS(*calc_mtx, sVar12_2);
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, i_this->field_0x864 + 2000.0f * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 12000)), 2, 0x2000), cLib_addCalcAngleS2(&a_this->shape_angle.z, i_this->field_0x864 + 2000.0f * cM_ssin(i_this->field_0x66c * 12000), 2, 0x2000));
                
                local_68.x = 0.0f;
                local_68.y = + 100.0f - 30.0f;
                local_68.z = - 50.0f - 20.0f;
                MtxPosition(&local_68, &i_this->field_0x678);
                i_this->field_0x678 += player->current.pos;
                
                DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, sVar12_2, 2, 0x1000), cLib_addCalcAngleS2(&a_this->shape_angle.y, sVar12_2, 2, 0x1000));
            } else {
                local_e6 = -0x4000;
                i_this->field_0x678 = player->eyePos;
                i_this->field_0x678.y += 80.0f;
            }
            
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 1.0f, i_this->field_0x690);
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 1.0f, i_this->field_0x690);
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 1.0f, i_this->field_0x690);
            actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 200.0f, 1.0f, 10.0f);
#else
            cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 1.0f, i_this->field_0x690);
            cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 1.0f, i_this->field_0x690);
            cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 1.0f, i_this->field_0x690);
            cLib_addCalc2(&i_this->field_0x690, 200.0f, 1.0f, 10.0f);
#endif
        } else {
            i_this->field_0x66e = 3;
            i_this->field_0x670 = -1;
            anm_init(i_this, 17, 2.0f, 0, 1.0f);
            i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_RELEASE, -1);
            i_this->field_0x85c = 20.0f;
            i_this->field_0x860 = a_this->shape_angle.y;
        }
        break;
    }
    
    if (i_this->field_0xb18 != 0) {
        f32 dVar15 = DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f);
        a_this->current.pos.y -= dVar15;
        a_this->old.pos.y -= dVar15;
        
        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        
        a_this->current.pos.y += dVar15;
        a_this->old.pos.y += dVar15;
    }
    
    if (i_this->field_0x854 != 0) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, local_e6 + 0x8000, 8, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, local_e6 + 0x8000, 8, 0x400));
        i_this->field_0xb18 = 1;
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, -local_e6, 8, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, -local_e6, 8, 0x400));
    }
}

static void e_yh_attack_s(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    cXyz local_28;
    
    local_28 = player->eyePos - a_this->current.pos;
    
    s16 angleX = (s16)cM_atan2s(local_28.y, JMAFastSqrt(local_28.x * local_28.x + local_28.z * local_28.z));
    
    switch (i_this->field_0x670) {
    case 0:
        i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_ATTACK, -1);
        leaf_anm_init(i_this, 20, 5.0f, 2, 1.0f);
        anm_init(i_this, 7, 5.0f, 0, 1.0f);
        i_this->mAtSph.StartCAt(a_this->current.pos);
        i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 15.0f), 15);
        i_this->field_0x690 = 0.0f;
        i_this->field_0x670 = 1;
        // fallthrough
    case 1:
        i_this->field_0x678 = player->eyePos;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 0.8f, i_this->field_0x690 * 50.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 0.8f, i_this->field_0x690 * 50.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 0.8f, i_this->field_0x690 * 50.0f);
        actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.3f);
#else
        cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 0.8f, i_this->field_0x690 * 50.0f);
        cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 0.8f, i_this->field_0x690 * 50.0f);
        cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 0.8f, i_this->field_0x690 * 50.0f);
        cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.3f);
#endif
        
        local_28 = a_this->current.pos - a_this->home.pos;
        
        if (i_this->field_0x698[0] == 0 || local_28.abs() > 400.0f) {
            i_this->field_0x66e = 3;
            i_this->field_0x670 = 0;
        }
        break;
    }
    
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800);
    actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, angleX + 0x8000, 8, 0x400);
#else
    cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688 + 0x8000, 8, 0x800);
    cLib_addCalcAngleS2(&a_this->shape_angle.x, angleX + 0x8000, 8, 0x400);
#endif
    i_this->field_0xb18 = 1;
}

static void e_yh_chance(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_2c;
    f32 dVar8 = DUSK_IF_ELSE(60.0f * sizeMultiplier, 60.0f);
    
    switch (i_this->field_0x670) {
    case 0:
        if (i_this->field_0x854 != 0) {
            anm_init(i_this, 11, 5.0f, 2, 1.0f);
        } else {
            anm_init(i_this, 10, 5.0f, 2, 1.0f);
        }
        leaf_anm_init(i_this, 0x15, 5.0f, 2, 1.0f);
        i_this->field_0x670 = 1;
        i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_stun_timer(i_this, 60.0f), 0x3c);
        i_this->field_0x690 = 0.0f;
        // fallthrough
    case 1:
#if TARGET_PC  // enemy attribute integration
        local_2c.x = dVar8 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 900));
        local_2c.z = dVar8 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 1000));
#else
        local_2c.x = dVar8 * cM_ssin(i_this->field_0x66c * 900);
        local_2c.z = dVar8 * cM_scos(i_this->field_0x66c * 1000);
#endif
        
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, a_this->home.pos.x + local_2c.x,
#else
        cLib_addCalc2(&a_this->current.pos.x, a_this->home.pos.x + local_2c.x,
#endif
                      0.1f, i_this->field_0x690 * 20.0f);
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, a_this->home.pos.z + local_2c.z,
#else
        cLib_addCalc2(&a_this->current.pos.z, a_this->home.pos.z + local_2c.z,
#endif
                      0.1f, i_this->field_0x690 * 20.0f);
        
        if (i_this->field_0x854 != 0) {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, a_this->home.pos.y - 230.0f * sizeMultiplier,
#else
            cLib_addCalc2(&a_this->current.pos.y, a_this->home.pos.y - 230.0f,
#endif
                          0.1f, i_this->field_0x690 * 20.0f);
        } else {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, a_this->home.pos.y + 230.0f * sizeMultiplier,
#else
            cLib_addCalc2(&a_this->current.pos.y, a_this->home.pos.y + 230.0f,
#endif
                          0.1f, i_this->field_0x690 * 20.0f);
        }
        
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 1.0f, 1.0f, 0.1f), cLib_addCalc2(&i_this->field_0x690, 1.0f, 1.0f, 0.1f));
        
        if (DUSK_IF_ELSE(i_this->field_0x698[0] <= actor_attr::enemy_stun_countdown_milestone(i_this, 60.0f, 50.0f) && i_this->field_0x698[0] >= actor_attr::enemy_stun_countdown_milestone(i_this, 60.0f, 10.0f), i_this->field_0x698[0] <= 50 && i_this->field_0x698[0] >= 10)) {
            i_this->mSound.startCreatureSoundLevel(Z2SE_EN_DB_V_FAINT, 0, -1);
        }
        
        if (i_this->field_0x698[0] == 0) {
            i_this->field_0x670++;
            anm_init(i_this, 0x12, 3.0f, 0, 1.0f);
            leaf_anm_init(i_this, 0x14, 3.0f, 2, 1.0f);
            i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        }
        break;
    case 2:
        if (i_this->mpMorf->isStop()) {
            i_this->field_0x66e = 3;
            i_this->field_0x670 = 0;
            i_this->field_0x84c = 0.0f;
        }
        break;
    }
    
    if (i_this->field_0x854 != 0) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0x4000, 4, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, 0x4000, 4, 0x400));
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, -0x4000, 4, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, -0x4000, 4, 0x400));
    }
    DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x848, 0.5f, 1.0f, 0.05f), cLib_addCalc2(&i_this->field_0x848, 0.5f, 1.0f, 0.05f));
}

static void e_yh_s_damage(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_20;
    
    switch (i_this->field_0x670) {
    case 0:
        anm_init(i_this, 12, 3.0f, 0, 1.0f);
        leaf_anm_init(i_this, 20, 3.0f, 2, 0.0f);
        i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        
        if (i_this->field_0x854 != 0) {
            mDoMtx_YrotS(*calc_mtx, a_this->shape_angle.y + 0x8000);
        } else {
            mDoMtx_YrotS(*calc_mtx, a_this->shape_angle.y);
        }
        
        local_20.x = 0.0f;
#if TARGET_PC  // enemy attribute integration
        local_20.y = 15.0f * sizeMultiplier;
        local_20.z = -50.0f * sizeMultiplier;
#else
        local_20.y = 15.0f;
        local_20.z = -50.0f;
#endif
        MtxPosition(&local_20, &a_this->speed);
        IF_DUSK(a_this->speed *= actor_attr::enemy_action_speed_multiplier(i_this);)
        i_this->field_0x670 = 1;
        i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 10.0f), 10);
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_DAMAGE_S, -1);
        break;
        
    case 1:
        a_this->current.pos += a_this->speed;
        local_20.x = a_this->current.pos.x - a_this->home.pos.x;
        local_20.z = a_this->current.pos.z - a_this->home.pos.z;
        f32 distance = JMAFastSqrt(local_20.x * local_20.x + local_20.z * local_20.z);
        
        if (distance > DUSK_IF_ELSE(250.0f * sizeMultiplier, 250.0f)) {
            a_this->speed.set(0.0f, 0.0f, 0.0f);
        } else {
#if TARGET_PC  // enemy attribute integration
            a_this->speed *= actor_attr::enemy_action_damping(i_this, 0.92f);
#else
            a_this->speed.x *= 0.92f;
            a_this->speed.y *= 0.92f;
            a_this->speed.z *= 0.92f;
#endif
        }
        
        if (i_this->field_0x698[0] == 0) {
            i_this->field_0x66e = 6;
            i_this->field_0x670 = 0;
        }
        break;
    }
    
    if (i_this->field_0x854 != 0) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0x4000, 4, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, 0x4000, 4, 0x400));
    } else {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, -0x4000, 4, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.x, -0x4000, 4, 0x400));
    }
}

static void e_yh_damage(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz local_34;
    cXyz cStack_40;
    
    switch (i_this->field_0x670) {
    case 0:
        anm_init(i_this, 12, 3.0f, 0, 1.0f);
        leaf_anm_init(i_this, 20, 3.0f, 2, 0.0f);
        i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        
        mDoMtx_YrotS(*calc_mtx, i_this->field_0x684);
        local_34.x = 0.0f;
        if (i_this->field_0x5b4 != 1) {
            if (i_this->field_0x5b4 == 2) {
                local_34.y = 20.0f;
            } else {
                local_34.y = 30.0f;
            }
            local_34.z = -50.0f;
        } else {
            local_34.y = 0.0f;
            local_34.z = -20.0f;
        }
        MtxPosition(&local_34, &a_this->speed);
        IF_DUSK(a_this->speed *= actor_attr::enemy_action_speed_multiplier(i_this);)
        i_this->field_0x670 = 1;
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_DAMAGE, -1);
        break;
    case 1:
        local_34 = a_this->current.pos - a_this->home.pos;
        if (local_34.abs() > 450.0f) {
            a_this->current.pos = a_this->old.pos;
            a_this->speed.x *= -0.1f;
            a_this->speed.z *= -0.1f;
            i_this->field_0x670++;
        }
        // fallthrough
    case 2:
        s16 angleOffset;
        if (i_this->field_0x5b4 == 2) {
            angleOffset = 0;
        } else {
            angleOffset = -0x8000;
        }
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x684 + angleOffset, 2, 0x2000), cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x684 + angleOffset, 2, 0x2000));
        a_this->current.pos += a_this->speed;
        
        if (i_this->field_0x5b4 != 1) {
            if (i_this->field_0x5b4 == 2) {
                a_this->speed.y -= DUSK_IF_ELSE(actor_attr::enemy_gravity_step(i_this, 1.0f), 1.0f);
            } else {
                a_this->speed.y -= DUSK_IF_ELSE(actor_attr::enemy_gravity_step(i_this, 5.0f), 5.0f);
            }
        }

        f32 groundOffset = DUSK_IF_ELSE(40.0f * yh_effective_size(i_this), 40.0f * i_this->field_0x694 * l_HIO.mBasicSize);
        a_this->current.pos.y -= groundOffset;
        a_this->old.pos.y -= groundOffset;
        
        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        
        a_this->current.pos.y += groundOffset;
        a_this->old.pos.y += groundOffset;
        
        if (i_this->mpMorf->isStop()) {
            if (i_this->field_0x5b4 == 1) {
                i_this->field_0x66e = 10;
            } else {
                i_this->field_0x66e = 3;
            }
            i_this->field_0x670 = 0;
        }
        break;
    }
}

static void kuki_control1(e_yh_class* i_this) {
    static s16 pow_xa[11] = {
        0x0000,
        0x0000,
        0x1000,
        0x2000,
        0x3000,
        0x4000,
        0x3000,
        0x2000,
        0x1000,
        0x0000,
        0x0000,
    };
    
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz local_a0;
    cXyz cStack_ac;
    cXyz local_b8;
    int i;
    cXyz* pPos;
    cXyz local_c4;
    f32 deltaX;
    f32 deltaY;
    f32 dVar8;
    f32 dVar10;
    f32 dVar13;
    f32 deltaZ;
    s16 angleX;
    s16 angleY;
    f32 dVar11;

    pPos = i_this->field_0x6a0;
    *pPos = a_this->home.pos;

    local_a0.x = 0.0f;
    local_a0.y = 0.0f;
    local_a0.z = i_this->field_0x844;

    pPos++;

    dVar13 = DUSK_IF_ELSE((50.0f + TREG_F(2)) * sizeMultiplier, 50.0f + TREG_F(2));
    local_b8.x = 0.0f;
    local_b8.z = 0.0f;

    if (i_this->field_0x66e == 6) {
        local_b8.y = 0.0f;
#if TARGET_PC  // enemy attribute integration
        dVar13 = (70.0f + JREG_F(1)) * sizeMultiplier;
        local_c4.set(0.0f, (-30.0f + JREG_F(3)) * sizeMultiplier, 0.0f);
#else
        dVar13 = 70.0f + JREG_F(1);
        local_c4.set(0.0f, -30.0f + JREG_F(3), 0.0f);
#endif
    } else if (i_this->field_0x66e == 10) {
        local_b8.y = 0.0f;
#if TARGET_PC  // enemy attribute integration
        dVar13 = (70.0f + JREG_F(1)) * sizeMultiplier;
        local_c4.set(0.0f, (30.0f + JREG_F(3)) * sizeMultiplier, 0.0f);
#else
        dVar13 = 70.0f + JREG_F(1);
        local_c4.set(0.0f, 30.0f + JREG_F(3), 0.0f);
#endif
    } else {
        local_b8.y = DUSK_IF_ELSE(-(200.0f + TREG_F(0)) * sizeMultiplier, -(200.0f + TREG_F(0)));
    }

    dVar11 = i_this->field_0x84c;

    for (i = 1; i < 12; i++, pPos++) {

#if TARGET_PC  // enemy attribute integration
        if (i_this->field_0x66e == 6) {
            dVar10 = dVar13 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c, 1000 + TREG_S(0), i * (12000 + TREG_S(1))));
            dVar8 = dVar13 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c, 1300 + TREG_S(2), i * (10000 + TREG_S(3))));
        } else if (i_this->field_0x66e == 10) {
            dVar10 = dVar13 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c, 1000 + TREG_S(0), i * (12000 + TREG_S(1))));
            dVar8 = dVar13 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c, 1300 + TREG_S(2), i * (10000 + TREG_S(3))));
#else
        if (i_this->field_0x66e == 6 || i_this->field_0x66e == 10) {
            dVar10 = dVar13 * cM_ssin(i_this->field_0x66c * (1000 + TREG_S(0)) + i * (12000 + TREG_S(1)));
            dVar8 = dVar13 * cM_scos(i_this->field_0x66c * (1300 + TREG_S(2)) + i * (10000 + TREG_S(3)));
#endif
        } else {
#if TARGET_PC  // enemy attribute integration
            dVar10 = dVar13 * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 2000 + TREG_S(0), i * (12000 + TREG_S(1))));
            dVar8 = dVar13 * cM_scos(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 2300 + TREG_S(2), i * (10000 + TREG_S(3))));
#else
            dVar10 = dVar13 * cM_ssin(i_this->field_0x66c * (2000 + TREG_S(0)) + i * (12000 + TREG_S(1)));
            dVar8 = dVar13 * cM_scos(i_this->field_0x66c * (2300 + TREG_S(2)) + i * (10000 + TREG_S(3)));
#endif

            if (i_this->field_0x854 != 0) {
                cMtx_YrotS(*calc_mtx, (s16)a_this->shape_angle.y);
                cMtx_XrotM(*calc_mtx, pow_xa[i - 1] + 0x8000);
            } else {
                cMtx_YrotS(*calc_mtx, (s16)a_this->shape_angle.y);
                cMtx_XrotM(*calc_mtx, (s16)pow_xa[i - 1]);
            }
            MtxScale(dVar11, dVar11, dVar11, 1);
            MtxPosition(&local_b8, &local_c4);
        }

        deltaX =
            dVar10 + (local_c4.x + (pPos[0].x - pPos[-1].x));
        deltaY = (pPos[0].y - pPos[-1].y) - local_c4.y;
        deltaZ =
            dVar8 + (local_c4.z + (pPos[0].z - pPos[-1].z));

        angleY = (s16)cM_atan2s(deltaX, deltaZ);
        angleX = -cM_atan2s(deltaY, JMAFastSqrt(deltaX * deltaX + deltaZ * deltaZ));

        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        MtxPosition(&local_a0, &cStack_ac);

        pPos[0] = pPos[-1] + cStack_ac;
    }
}

static void kuki_control2(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz local_58;
    cXyz cStack_64;
    int i;
    s16 angleX;
    s16 angleY;
    cXyz* pPos;
    csXyz* angle_ptr;
    f32 deltaX;
    f32 deltaY;
    f32 deltaZ;

    pPos = &i_this->field_0x6a0[11];
    angle_ptr = &i_this->field_0x730[11];
    *pPos = a_this->current.pos;
    local_58.x = 0.0f;
    local_58.y = 0.0f;
    local_58.z = i_this->field_0x844;

    pPos--;
    angle_ptr--;
    
    for (i = 10; i >= 0; i--, pPos--, angle_ptr--) {
        deltaX = pPos[0].x - pPos[1].x;
        deltaY = pPos[0].y - pPos[1].y;
        deltaZ = pPos[0].z - pPos[1].z;
        
        angleY = (s16)cM_atan2s(deltaX, deltaZ);
        angleX = -cM_atan2s(deltaY, JMAFastSqrt(deltaX * deltaX + deltaZ * deltaZ));
        
        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        MtxPosition(&local_58, &cStack_64);
        
        pPos[0] = pPos[1] + cStack_64;
        
        angle_ptr->x = angleX;
        angle_ptr->y = angleY;
    }
}

static void kuki_control3(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz cStack_40;
    cXyz* pPos;
    int i;
    
    cStack_40 = i_this->field_0x6a0[0] - a_this->home.pos;

    pPos = &i_this->field_0x6a0[0];
    for (i = 0; i < 12; i++, pPos++) {
        *pPos -= cStack_40 * ((12 - i) / 11.0f);
    }
}

static void kuki_ha_set(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    cXyz* pPos = i_this->field_0x6a0;
    csXyz* pAngle = i_this->field_0x730;
    
    for (int i = 0; i < 12; i++, pPos++, pAngle++) {
        if (0 < i && i < 11 && i_this->mModels[i] != NULL) {
            mDoMtx_stack_c::transS(pPos->x, pPos->y, pPos->z);
            mDoMtx_stack_c::YrotM(pAngle->y);
            mDoMtx_stack_c::XrotM(pAngle->x);
            mDoMtx_stack_c::ZrotM(i << 13);
            
            if (!(a_this->scale.x == 1.0f && a_this->scale.y == 1.0f && a_this->scale.z == 1.0f)) {
#if TARGET_PC  // enemy attribute integration
                mDoMtx_stack_c::scaleM(a_this->scale.x * sizeMultiplier, a_this->scale.y * sizeMultiplier, a_this->scale.z * sizeMultiplier);
#else
                mDoMtx_stack_c::scaleM(a_this->scale.x, a_this->scale.y, a_this->scale.z);
#endif
            } else {
                mDoMtx_stack_c::scaleM(i_this->field_0x7d8[i], i_this->field_0x7d8[i], i_this->field_0x7d8[i]);
            }
            
            i_this->mModels[i]->setBaseTRMtx(mDoMtx_stack_c::get());
        }
    }
}

static void kuki_control1_e(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz local_c8;
    cXyz cStack_d4;
    dBgS_GndChk gndChk;
    int i;
    cXyz* pPos;
    csXyz* pAngle;
    f32* pfVar8;
    f32 dVar12;
    Vec groundPos;
    f32 deltaX;
    f32 deltaY;
    f32 deltaZ;
    f32 fVar1;
    s16 angleX;
    s16 angleY;
    
    i_this->field_0x6a0[0] = a_this->current.pos;
    pPos = i_this->field_0x6a0 + 1;
    pAngle = i_this->field_0x730 + 1;
    pfVar8 = i_this->field_0x7a8 + 1;
    
    local_c8.x = 0.0f;
    local_c8.y = 0.0f;
    local_c8.z = i_this->field_0x844;
    
    dVar12 = i_this->field_0x868;
    DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x868, -10.0f, 1.0f, 0.5f), cLib_addCalc2(&i_this->field_0x868, -10.0f, 1.0f, 0.5f));
    
    cXyz local_ec;
    cXyz local_f8;
#if TARGET_PC  // enemy attribute integration

    const f32 stemSpinStep = actor_attr::action_step(i_this->field_0x852, actor_attr::enemy_action_time_speed(i_this));
    i_this->field_0x850 += static_cast<s16>(stemSpinStep);
#else
    
    i_this->field_0x850 += i_this->field_0x852;
#endif
    
    for (i = 1; i < 12; i++, pPos++, pAngle++, pfVar8++) {
        cMtx_YrotS(*calc_mtx, a_this->shape_angle.y);
        local_f8.x = 10.0f * cM_ssin(i_this->field_0x850 + i * 10000);
        local_f8.y = 0.0f;
        local_f8.z = -5.0f;
        MtxPosition(&local_f8, &local_ec);
        
        deltaX = local_ec.x + (pPos[0].x - pPos[-1].x);
        fVar1 = pPos[0].y + dVar12;
        if (fVar1 <= *pfVar8) {
            fVar1 = *pfVar8;
        }
        deltaY = fVar1 - pPos[-1].y;
        deltaZ = local_ec.z + (pPos[0].z - pPos[-1].z);
        
        angleY = (s16)cM_atan2s(deltaX, deltaZ);
        angleX = -cM_atan2s(deltaY, JMAFastSqrt(deltaX * deltaX + deltaZ * deltaZ));
        
        cMtx_YrotS(*calc_mtx, angleY);
        cMtx_XrotM(*calc_mtx, angleX);
        MtxPosition(&local_c8, &cStack_d4);
        
        pPos[0] = pPos[-1] + cStack_d4;
        
        pAngle->x = angleX;
        pAngle->y = angleY;
        
        if (DUSK_IF_ELSE(actor_attr::action_event_count(i_this->field_0x66c , 4, actor_attr::enemy_action_time_speed(i_this), i) > 0, ((i_this->field_0x66c + i) & 3) == 0)) {
            groundPos = *pPos;
            groundPos.y += 100.0f;
            gndChk.SetPos(&groundPos);
            *pfVar8 = dComIfG_Bgsp().GroundCross(&gndChk) + 5.0f;
            if (*pfVar8 - pPos[0].y > 200.0f) {
                *pfVar8 = pPos[0].y;
            }
        } else {
            *pfVar8 = i_this->mAcch.GetGroundH() + 5.0f;
        }
    }
}

static s8 e_yh_escape(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    cXyz local_bc;
    cXyz local_c8;
    f32 groundY;
    f32 playerDistance;
    s8 local_115;
    s8 local_116;
    s8 bVar5;
    s16 local_114;
    
    i_this->mAtInfo.field_0x18 = 0;
    dBgS_ObjGndChk_Spl gndChk;
    
    local_bc = a_this->current.pos;
    local_bc.y += 100.0f;
    gndChk.SetPos(&local_bc);
    groundY = dComIfG_Bgsp().GroundCross(&gndChk);
    playerDistance = fopAcM_searchPlayerDistance(a_this);
    local_115 = 1;
    local_116 = 2;
    bVar5 = 1;
    stage_stag_info_class* info = dComIfGp_getStage()->getStagInfo();
#if TARGET_PC  // enemy attribute integration
    const s16 modeAtStart = i_this->field_0x670;
#endif

    
    switch (i_this->field_0x670) {
    case 0:
        local_116 = 1;
        i_this->field_0x844 = DUSK_IF_ELSE(25.0f * actor_attr::enemy_size_multiplier(i_this), 25.0f);
        if (i_this->mAcch.ChkGroundHit()) {
            a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 20.0f), 20.0f);
            i_this->field_0x670 = 1;
            anm_init(i_this, 16, 10.0f, 2, 1.0f);
            i_this->mAtSph.StartCAt(a_this->current.pos);
            i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, cM_rndF(100.0f) + 200.0f), (s16)(cM_rndF(100.0f) + 200.0f));
            fopAcM_effSmokeSet1(&i_this->field_0x125c, &i_this->field_0x1260,
                               &a_this->current.pos, &a_this->shape_angle,
                               DUSK_IF_ELSE(actor_attr::enemy_size_multiplier(i_this), 1.0f), &a_this->tevStr, 1);
        }
        break;
        
    case 1:
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x688, 8, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x688, 8, 0x400));
        if (dStage_stagInfo_GetSTType(info) == 0 && 
            i_this->field_0x698[0] == 0 && 
            i_this->field_0x5b4 == 0) {
            i_this->field_0x670 = 2;
        } else if (i_this->field_0x698[1] == 0) {
            s16 angleDiff = (s16)(i_this->field_0x688 - a_this->shape_angle.y);
            if (angleDiff < 0x1000 && angleDiff > -0x1000 && playerDistance < DUSK_IF_ELSE(actor_attr::enemy_size_value(i_this, 300.0f), 300.0f)) {
                daPy_py_c* pyPlayer = (daPy_py_c*)daPy_getPlayerActorClass();
                if (!pyPlayer->getDkCaught() && !pyPlayer->getDkCaught2()) {
                    i_this->field_0x670 = 15;
                    i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_BITE, -1);
                    if (cM_rndF(1.0f) < 0.5f) {
                        i_this->field_0x864 = 0x4000;
                    } else {
                        i_this->field_0x864 = -0x4000;
                    }
                    a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 30.0f), 30.0f);
                    a_this->speedF = 15.0f;
                    a_this->shape_angle.x = -0x2000;
                    i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_ATTACK, -1);
                }
            }
        }
        break;

    case 2: {
        local_bc = a_this->home.pos - a_this->current.pos;
        local_bc.y = 0.0f;
        s16 homeAngle = cM_atan2s(local_bc.x, local_bc.z);
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, homeAngle, 8, 0x800), cLib_addCalcAngleS2(&a_this->shape_angle.y, homeAngle, 8, 0x800));
        if (local_bc.abs() < 100.0f) {
            i_this->field_0x670 = 3;
            i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 25.0f), 25);
            leaf_anm_init(i_this, 20, 10.0f, 2, 1.0f);
            i_this->mSound.startCreatureSound(Z2SE_EN_DB_GRASS, 0, -1);
        }
        break;
    }

    case 3:
        local_116 = 0;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x844, 1.0f, 1.0f);
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, a_this->home.pos.x, 0.2f, fabsf(a_this->speed.x));
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, a_this->home.pos.z, 0.2f, fabsf(a_this->speed.z));
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, -0x4000, 4, 0x400);
#else
        cLib_addCalc0(&i_this->field_0x844, 1.0f, 1.0f);
        cLib_addCalc2(&a_this->current.pos.x, a_this->home.pos.x, 0.2f, fabsf(a_this->speed.x));
        cLib_addCalc2(&a_this->current.pos.z, a_this->home.pos.z, 0.2f, fabsf(a_this->speed.z));
        cLib_addCalcAngleS2(&a_this->shape_angle.x, -0x4000, 4, 0x400);
#endif
        if (i_this->field_0x698[0] == 0) {
            i_this->field_0x66e = 1;
            i_this->field_0x670 = 0;
            a_this->health = DUSK_IF_ELSE(actor_attr::enemy_health_value(i_this, 30.0f), 30);
            i_this->mAtInfo.field_0x18 = 45;
        }
        break;
        
    case 10:
        anm_init(i_this, 12, 3.0f, 0, 1.0f);
        i_this->field_0x670 = 11;
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_DAMAGE, -1);
        break;
        
    case 11:
        if (i_this->mpMorf->isStop()) {
            i_this->field_0x670 = 1;
            anm_init(i_this, 16, 5.0f, 2, 1.0f);
        }
        break;
        
    case 15:
        local_116 = 1;
        if (i_this->field_0x864 == 0) {
            if (cM_rndF(1.0f) < 0.5f) {
                i_this->field_0x864 = 0x4000;
            } else {
                i_this->field_0x864 = -0x4000;
            }
        }
        i_this->mCcSph.OffCoSetBit();
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0x2000, 2, 0x400);
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, i_this->field_0x864, 2, 0x800);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.x, 0x2000, 2, 0x400);
        cLib_addCalcAngleS2(&a_this->shape_angle.z, i_this->field_0x864, 2, 0x800);
#endif
        
        if (i_this->field_0x698[1] == 0) {
            i_this->field_0xb18 = 2;
            if (!daPy_getPlayerActorClass()->getDkCaught() && !daPy_getPlayerActorClass()->getDkCaught2() &&
                i_this->mAtSph.ChkAtHit() && !i_this->mAtSph.ChkAtShieldHit()) {
                OS_REPORT("E_YH//////////////AT  SET 1 %d !!\n", daPy_getPlayerActorClass()->getDkCaught2());
                i_this->field_0x670 = 20;
                i_this->field_0x690 = 15.0f;
                i_this->field_0x698[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 120.0f), 120);
                daPy_getPlayerActorClass()->onDkCaught2();
                OS_REPORT("E_YH//////////////AT  SET 2 %d !!\n", daPy_getPlayerActorClass()->getDkCaught2());
                i_this->field_0x862 = a_this->shape_angle.y - player->shape_angle.y;
                anm_init(i_this, 8, 2.0f, 2, 1.0f);
                break;
            }
        }
        
        if (i_this->mAcch.ChkGroundHit()) {
            i_this->field_0x670 = 1;
            i_this->field_0x698[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30.0f), 30);
            anm_init(i_this, 16, 10.0f, 2, 1.0f);
            a_this->speedF = 7.5f;
            fopAcM_effSmokeSet1(&i_this->field_0x125c, &i_this->field_0x1260,
                               &a_this->current.pos, &a_this->shape_angle,
                               DUSK_IF_ELSE(actor_attr::enemy_size_multiplier(i_this), 1.0f), &a_this->tevStr, 1);
        }
        break;
        
    case 20:
        i_this->field_0x855 = 1;
        i_this->mSound.startCreatureVoiceLevel(Z2SE_EN_DB_V_BITING, -1);
        local_115 = 0;
        daPy_py_c::setLookPos(&a_this->current.pos);
        local_116 = 0;
        i_this->mCcSph.OffCoSetBit();
        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        
        if (!i_this->mAcch.ChkWallHit()) {
            if (daPy_getPlayerActorClass()->getDkCaught2()) {
#if TARGET_PC  // enemy attribute integration
                if (i_this->field_0x698[0] == actor_attr::enemy_sync_timer(i_this, 30.0f)) {
                    dComIfGp_setItemLifeCount(-actor_attr::enemy_attack_damage(i_this, 1.0f), 0);
#else
                if (i_this->field_0x698[0] == 30) {
                    dComIfGp_setItemLifeCount(-1.0f, 0);
#endif
                }
                if (DUSK_IF_ELSE(actor_attr::action_event_count(i_this->field_0x66c , 16, actor_attr::enemy_action_time_speed(i_this) ) > 0, (i_this->field_0x66c & 0xf) == 0)) {
                    dComIfGp_getVibration().StartShock(3, 0x1f, cXyz(0.0f, 1.0f, 0.0f));
                }
                s16 angle = player->shape_angle.y + i_this->field_0x862;
                mDoMtx_YrotS(*calc_mtx, angle);
                local_bc.x = 0.0f;
                local_bc.y = 100.0f - 30.0f;
                local_bc.z = -50.0f - 20.0f;
                MtxPosition(&local_bc, &i_this->field_0x678);
                i_this->field_0x678 += player->current.pos;
#if TARGET_PC  // enemy attribute integration
                actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, angle, 2, 0x2000);
                actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.x, i_this->field_0x678.x, 1.0f, i_this->field_0x690);
                actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.z, i_this->field_0x678.z, 1.0f, i_this->field_0x690);
                actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, i_this->field_0x678.y, 1.0f, i_this->field_0x690);
                actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x690, 200.0f, 1.0f, 10.0f);
                actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, i_this->field_0x864 + 2000.0f * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 12000)), 2, 0x2000);
                actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0, 2, 0x800);
#else
                cLib_addCalcAngleS2(&a_this->shape_angle.y, angle, 2, 0x2000);
                cLib_addCalc2(&a_this->current.pos.x, i_this->field_0x678.x, 1.0f, i_this->field_0x690);
                cLib_addCalc2(&a_this->current.pos.z, i_this->field_0x678.z, 1.0f, i_this->field_0x690);
                cLib_addCalc2(&a_this->current.pos.y, i_this->field_0x678.y, 1.0f, i_this->field_0x690);
                cLib_addCalc2(&i_this->field_0x690, 200.0f, 1.0f, 10.0f);
                cLib_addCalcAngleS2(&a_this->shape_angle.z, i_this->field_0x864 + 2000.0f * cM_ssin(i_this->field_0x66c * 12000), 2, 0x2000);
                cLib_addCalcAngleS2(&a_this->shape_angle.x, 0, 2, 0x800);
#endif
                break;
            }
        }
        
        i_this->field_0x670 = 21;
        i_this->field_0x698[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 30.0f), 30);
        anm_init(i_this, 17, 2.0f, 0, 1.0f);
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_RELEASE, -1);
        a_this->speedF = -10.0f;
        a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 15.0f), 15.0f);
        daPy_getPlayerActorClass()->offDkCaught2();
        break;
        
    case 21:
        if (i_this->mpMorf->isStop()) {
            anm_init(i_this, 16, 10.0f, 2, 1.0f);
            i_this->field_0x670 = 1;
        }
        break;
        
    case 50:
        bVar5 = 0;
        local_116 = 0;
        local_115 = 0;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_calc2(i_this, &a_this->current.pos.y, groundY + 20.0f, 0.1f, 5.0f);
        actor_attr::enemy_add_action_calc0(i_this, &a_this->speed.y, 0.1f, 0.25f);
#else
        cLib_addCalc2(&a_this->current.pos.y, groundY + 20.0f, 0.1f, 5.0f);
        cLib_addCalc0(&a_this->speed.y, 0.1f, 0.25f);
#endif
        a_this->current.pos.y += a_this->speed.y;
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, i_this->field_0x864, 2, 0x800);
        actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x852, 0, 1, 0x96);
        f32 swing = 600.0f * cM_ssin(actor_attr::enemy_action_phase_angle(i_this, i_this->field_0x66c , 1000));
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, (s16)swing, 2, 0x800);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.z, i_this->field_0x864, 2, 0x800);
        cLib_addCalcAngleS2(&i_this->field_0x852, 0, 1, 0x96);
        f32 swing = 600.0f * cM_ssin(i_this->field_0x66c * 1000);
        cLib_addCalcAngleS2(&a_this->shape_angle.x, (s16)swing, 2, 0x800);
#endif
        if (i_this->field_0x698[0] == 0) {
            yh_disappear(i_this);
            fopAcM_onActor(a_this);
        }
        break;
    }
    
    if (local_116 != 0) {
        if (local_116 == 2) {
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0, 2, 0x800);
            actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, 0, 2, 0x800);
            if (i_this->field_0x698[1] < actor_attr::enemy_sync_timer(i_this, 10.0f)) {
                actor_attr::enemy_add_action_calc2(i_this, &a_this->speedF, 7.5f, 1.0f, 1.0f);
#else
            cLib_addCalcAngleS2(&a_this->shape_angle.x, 0, 2, 0x800);
            cLib_addCalcAngleS2(&a_this->shape_angle.z, 0, 2, 0x800);
            if (i_this->field_0x698[1] < 10) {
                cLib_addCalc2(&a_this->speedF, 7.5f, 1.0f, 1.0f);
#endif
            }
            local_114 = 5000.0f * cM_ssin((i_this->mpMorf->getFrame() * 65536.0f) / 11.0f);
        } else {
            local_114 = 0;
        }
        
        mDoMtx_YrotS(*calc_mtx, local_114 + a_this->shape_angle.y);
        local_bc.x = 0.0f;
        local_bc.y = 0.0f;
#if TARGET_PC  // enemy attribute integration
        const f32 actionSpeed = modeAtStart != 20 ? actor_attr::enemy_action_speed_multiplier(i_this) : 1.0f;
        local_bc.z = actor_attr::action_step(a_this->speedF, actionSpeed);
#else
        local_bc.z = a_this->speedF;
#endif
        MtxPosition(&local_bc, &local_c8);
        a_this->speed.x = local_c8.x;
        a_this->speed.z = local_c8.z;
        a_this->current.pos += a_this->speed;
#if TARGET_PC  // enemy attribute integration
        f32 gravityStep = 3.0f;
        if (modeAtStart != 20) {
            gravityStep = actor_attr::enemy_gravity_step(i_this, gravityStep);
        }
        a_this->speed.y -= gravityStep;

        const f32 maxFallSpeed = actor_attr::action_step(-80.0f, actionSpeed);
        if (a_this->speed.y < maxFallSpeed) {
            a_this->speed.y = maxFallSpeed;
        }
#else
        a_this->speed.y -= 3.0f;
#endif
        
        if (i_this->field_0x85c > 0.1f) {
            local_bc.x = 0.0f;
            local_bc.y = 0.0f;
            local_bc.z = DUSK_IF_ELSE(-actor_attr::action_step(i_this->field_0x85c, actionSpeed), -i_this->field_0x85c);
            mDoMtx_YrotS(*calc_mtx, i_this->field_0x860);
            MtxPosition(&local_bc, &local_c8);
            a_this->current.pos += local_c8;
        }
        
        cXyz* ccMove = i_this->mStts.GetCCMoveP();
        if (ccMove != NULL) {
            a_this->current.pos.x += ccMove->x;
            a_this->current.pos.y += ccMove->y;
            a_this->current.pos.z += ccMove->z;
        }
        
        f32 groundOffset = DUSK_IF_ELSE(40.0f * yh_effective_size(i_this), 40.0f * i_this->field_0x694 * l_HIO.mBasicSize);
        a_this->current.pos.y -= groundOffset;
        a_this->old.pos.y -= groundOffset;
        i_this->mAcch.CrrPos(dComIfG_Bgsp());
        a_this->current.pos.y += groundOffset;
        a_this->old.pos.y += groundOffset;
    }
    
    if (bVar5 && (a_this->current.pos.y - 35.0f) < groundY) {
        i_this->field_0x670 = 50;
        anm_init(i_this, 15, 20.0f, 0, 1.0f);
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_DEATH, -1);
        daPy_py_c* pyPlayer = (daPy_py_c*)daPy_getPlayerActorClass();
        pyPlayer->onEnemyDead();
#if TARGET_PC  // enemy attribute integration
        i_this->field_0x69e = actor_attr::enemy_sync_timer(i_this, 200.0f);
        i_this->field_0x698[0] = actor_attr::enemy_sync_timer(i_this, 80.0f);
#else
        i_this->field_0x69e = 200;
        i_this->field_0x698[0] = 80;
#endif
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x864 = 0x4000;
        } else {
            i_this->field_0x864 = -0x4000;
        }

#if TARGET_PC  // enemy attribute integration
        if (a_this->speed.y < actor_attr::enemy_move_step(i_this, -20.0f)) {
            a_this->speed.y = actor_attr::enemy_move_step(i_this, -7.0f);
#else
        if (a_this->speed.y < -20.0f) {
            a_this->speed.y = -7.0f;
#endif
        } else {
            a_this->speed.y = 0.0f;
        }
        fopKyM_createWpillar(&a_this->current.pos, DUSK_IF_ELSE(1.8f * actor_attr::enemy_size_multiplier(i_this), 1.8f), 0);
        i_this->mSound.startCreatureSound(Z2SE_CM_BODYFALL_WATER_S, 0, -1);
    }
    return local_115;
}

static void e_yh_e_dead(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz local_a0;
    cXyz cStack_ac;

    i_this->field_0x69e = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(i_this, 6.0f), 6);
    dBgS_ObjGndChk_Spl gndChk;
    f32 groundY;
    
    local_a0 = a_this->current.pos;
    local_a0.y += 100.0f;
    gndChk.SetPos(&local_a0);
    groundY = dComIfG_Bgsp().GroundCross(&gndChk);
    
    switch (i_this->field_0x670) {
    case 0:
        i_this->field_0x844 = DUSK_IF_ELSE((TREG_F(8) + 25.0f) * actor_attr::enemy_size_multiplier(i_this), TREG_F(8) + 25.0f);
        cMtx_YrotS(*calc_mtx, i_this->field_0x684);
        local_a0.x = 0.0f;
        local_a0.y = 30.0f;
        local_a0.z = -40.0f;
        MtxPosition(&local_a0, &a_this->speed);
        IF_DUSK(a_this->speed *= actor_attr::enemy_action_speed_multiplier(i_this);)
        anm_init(i_this, 13, 3.0f, 2, 1.0f);
        i_this->field_0x670 = 1;
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_DEATH, -1);
        
        daPy_getPlayerActorClass()->onEnemyDead();
        break;
        
    case 1:
        if (i_this->mAcch.ChkWallHit()) {
            a_this->speed.x *= -0.3f;
            a_this->speed.z *= -0.3f;
            i_this->field_0x670 = 2;
        }
        // fallthrough        
    case 2:
        if (i_this->mAcch.ChkGroundHit()) {
            a_this->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(i_this, 20.0f), 20.0f);
            i_this->field_0x670 = 3;
            i_this->field_0x698[0] = 0;
            a_this->speed.x *= 0.2f;
            a_this->speed.z *= 0.2f;
            
            if (cM_rndF(1.0f) < 0.5f) {
                i_this->field_0x864 = 0x4000;
            } else {
                i_this->field_0x864 = -0x4000;
            }
            
            fopAcM_effSmokeSet1(&i_this->field_0x125c, &i_this->field_0x1260,
                               &a_this->current.pos, &a_this->shape_angle,
                               DUSK_IF_ELSE((NREG_F(18) + 1.0f) * actor_attr::enemy_size_multiplier(i_this), NREG_F(18) + 1.0f), &a_this->tevStr, 1);
        }
        break;
        
    case 3:
    case 4:
        if (i_this->mAcch.ChkGroundHit()) {
            a_this->speed.x = a_this->speed.z = 0.0f;
            if (i_this->field_0x670 == 3) {
                anm_init(i_this, 15, 10.0f, 0, 1.0f);
                i_this->field_0x670 = 4;
            }
        }
#if TARGET_PC  // enemy attribute integration
        actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, i_this->field_0x864, 4, 0x400);
        actor_attr::enemy_add_action_angle(i_this, &i_this->field_0x852, 0, 1, 0x96);
#else
        cLib_addCalcAngleS2(&a_this->shape_angle.z, i_this->field_0x864, 4, 0x400);
        cLib_addCalcAngleS2(&i_this->field_0x852, 0, 1, 0x96);
#endif
        if (i_this->field_0x698[0] == 0) {
            yh_disappear(i_this);
            fopAcM_onActor(a_this);
        }
        break;
    }
    
#if TARGET_PC  // enemy attribute integration
    actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.y, i_this->field_0x684 + 0x8000, 2, 0x1000);
    actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.x, 0, 2, 0x800);
#else
    cLib_addCalcAngleS2(&a_this->shape_angle.y, i_this->field_0x684 + 0x8000, 2, 0x1000);
    cLib_addCalcAngleS2(&a_this->shape_angle.x, 0, 2, 0x800);
#endif
    a_this->current.pos += a_this->speed;
    a_this->speed.y -= DUSK_IF_ELSE(actor_attr::enemy_gravity_step(i_this, 3.0f), 3.0f);

    f32 fVar1 = cM_scos(a_this->shape_angle.z) * 15.0f;
    f32 groundOffset =
#if TARGET_PC  // enemy attribute integration
        (fVar1 + 25.0f + TREG_F(15)) * yh_effective_size(i_this);
#else
        (fVar1 + 25.0f + TREG_F(15)) * i_this->field_0x694 * l_HIO.mBasicSize;
#endif
    a_this->current.pos.y -= groundOffset;
    a_this->old.pos.y -= groundOffset;
    i_this->mAcch.CrrPos(dComIfG_Bgsp());
    a_this->current.pos.y += groundOffset;
    a_this->old.pos.y += groundOffset;
    
    if ((a_this->current.pos.y - 35.0f + TREG_F(8)) < groundY) {
        i_this->field_0x66e = 20;
        i_this->field_0x670 = 50;
        anm_init(i_this, 15, 20.0f, 0, 1.0f);
        i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_DEATH, -1);
        
        daPy_getPlayerActorClass()->onEnemyDead();
#if TARGET_PC  // enemy attribute integration
        i_this->field_0x69e = actor_attr::enemy_sync_timer(i_this, 200.0f);
        i_this->field_0x698[0] = actor_attr::enemy_sync_timer(i_this, 80.0f);
#else
        i_this->field_0x69e = 200;
        i_this->field_0x698[0] = 80;
#endif
        
        if (cM_rndF(1.0f) < 0.5f) {
            i_this->field_0x864 = 0x4000;
        } else {
            i_this->field_0x864 = -0x4000;
        }
        
#if TARGET_PC  // enemy attribute integration
        if (a_this->speed.y < actor_attr::enemy_move_step(i_this, -20.0f)) {
            a_this->speed.y = actor_attr::enemy_move_step(i_this, -7.0f);
#else
        if (a_this->speed.y < -20.0f) {
            a_this->speed.y = -7.0f;
#endif
        } else {
            a_this->speed.y = 0.0f;
        }
        
        fopKyM_createWpillar(&a_this->current.pos, DUSK_IF_ELSE(1.8f * actor_attr::enemy_size_multiplier(i_this), 1.8f), 0);
        i_this->mSound.startCreatureSound(Z2SE_CM_BODYFALL_WATER_S, 0, -1);
    }
}

static void action(e_yh_class* i_this) {
    fopEn_enemy_c* a_this = (fopEn_enemy_c*)i_this;
    fopAc_ac_c* player = (fopAc_ac_c*)dComIfGp_getPlayer(0);
    cXyz cStack_20;
    cXyz cStack_2c;
    s16 playerAngle = fopAcM_searchPlayerAngleY(a_this);
    i_this->field_0x688 = playerAngle;
    
    i_this->field_0x68c = (a_this->home.pos - player->current.pos).abs();
    
    a_this->field_0x566 = 0;
    i_this->field_0x123d = 1;
    damage_check(i_this);
    
    if (i_this->field_0x69e != 0 && i_this->field_0x855 != 0) {
        daPy_getPlayerActorClass()->offDkCaught();
        daPy_getPlayerActorClass()->offDkCaught2();
    }
    
    s8 local_85 = 0;
    s8 bVar2 = 0;
    s8 bVar3 = 0;
    s8 bVar1 = 0;
    
    i_this->mCcSph.OnCoSetBit();
    i_this->field_0x855 = 0;
    
    switch (i_this->field_0x66e) {
    case 0:
        e_yh_stay(i_this);
        i_this->field_0x123d = 0;
        break;
    case 1:
        e_yh_appear(i_this);
        local_85 = 1;
        bVar3 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        break;
    case 2:
        e_yh_appear_v(i_this);
        local_85 = 1;
        bVar3 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        break;
    case 3:
        e_yh_wait(i_this);
        local_85 = 1;
        bVar3 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        break;
    case 4:
        e_yh_attack(i_this);
        local_85 = 1;
        bVar3 = true;
        a_this->field_0x566 = 1;
        bVar1 = true;
        break;
    case 5:
        e_yh_attack_s(i_this);
        local_85 = 1;
        bVar3 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        break;
    case 6:
        e_yh_chance(i_this);
        local_85 = 1;
        bVar3 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        break;
    case 7:
        e_yh_s_damage(i_this);
        local_85 = 1;
        bVar3 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        break;
    case 8:
        e_yh_damage(i_this);
        local_85 = 1;
        break;
    case 10:
        e_yh_mk_roof(i_this);
        bVar3 = 1;
        bVar1 = 1;
        break;
    case 20:
        local_85 = e_yh_escape(i_this);
        bVar2 = 1;
        a_this->field_0x566 = 1;
        bVar1 = 1;
        fopAcM_OffStatus(a_this, 0x10000);
        break;
    case 21:
        e_yh_e_dead(i_this);
        bVar2 = 1;
    }
    
    if (bVar1) {
        i_this->mSound.setLinkSearch(true);
    } else {
        i_this->mSound.setLinkSearch(false);
    }
    
    if (local_85 != 0) {
        fopAcM_OnStatus(a_this, 0);
        a_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
    } else {
        fopAcM_OffStatus(a_this, 0);
        a_this->attention_info.flags = 0;
    }
    
    if (!bVar2) {
        cXyz diff = a_this->current.pos - a_this->home.pos;
        i_this->field_0x844 = i_this->field_0x848 * (0.2f * diff.abs());
#if TARGET_PC  // enemy attribute integration

        const f32 maximumStalkStep = 25.0f * actor_attr::enemy_size_multiplier(i_this);
        if (i_this->field_0x844 > maximumStalkStep) {
            i_this->field_0x844 = maximumStalkStep;
#else
        
        if (i_this->field_0x844 > 25.0f) {
            i_this->field_0x844 = 25.0f;
#endif
        }
        
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x848, 1.0f, 1.0f, 0.01f), cLib_addCalc2(&i_this->field_0x848, 1.0f, 1.0f, 0.01f));
        kuki_control1(i_this);
        kuki_control2(i_this);
        kuki_control3(i_this);
        kuki_ha_set(i_this);
        
        e_yd_leaf_class* leaf = (e_yd_leaf_class*)fopAcM_SearchByID(i_this->mChildID);
        if (leaf != NULL) {
#if TARGET_PC  // enemy attribute integration
            leaf->scale.y = actor_attr::enemy_size_multiplier(i_this);
            leaf->speedF = actor_attr::enemy_action_time_speed(i_this);
#endif
            leaf->mpMorf->play(NULL, 0, 0);
        }
        
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x84c, 1.0f, 1.0f, 0.05f), cLib_addCalc2(&i_this->field_0x84c, 1.0f, 1.0f, 0.05f));
    } else {
        kuki_control1_e(i_this);
        kuki_ha_set(i_this);
    }
    
    cXyz* kukiPos = i_this->field_0x6a0;
    cXyz* linePos = i_this->mLine.getPos(0);
    cXyz offset(-30000.0f, 300000.0f, 30000.0f);
    int sphereIdx = 0;
    
    for (int i = 0; i < 12; i++) {
        *linePos = *kukiPos;
        
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            if (bVar3) {
                i_this->mKukiSphs[sphereIdx].SetC(*kukiPos);
                i_this->mKukiSphs[sphereIdx].SetR(DUSK_IF_ELSE(30.0f * actor_attr::enemy_size_multiplier(i_this), 30.0f));
            } else {
                i_this->mKukiSphs[sphereIdx].SetC(*kukiPos + offset);
            }
            dComIfG_Ccsp()->Set(&i_this->mKukiSphs[sphereIdx]);
            sphereIdx++;
        }
        linePos++;
        kukiPos++;
    }
#if TARGET_PC  // enemy attribute integration

    actor_attr::enemy_add_action_calc0(i_this, &i_this->field_0x85c, 1.0f, 5.0f);

    const f32 maxFallSpeed = actor_attr::action_step(-100.0f, actor_attr::enemy_action_time_speed(i_this));
    if (a_this->speed.y < maxFallSpeed) {
        a_this->speed.y = maxFallSpeed;
#else
    
    cLib_addCalc0(&i_this->field_0x85c, 1.0f, 5.0f);
    
    if (a_this->speed.y < -100.0f) {
        a_this->speed.y = -100.0f;
#endif
    }
#if TARGET_PC  // enemy attribute integration

    const f32 effectScale = actor_attr::enemy_size_multiplier(i_this);
    cXyz scale(effectScale, effectScale, effectScale);
#else
    
    cXyz scale(1.0f, 1.0f, 1.0f);
#endif
    setMidnaBindEffect(a_this, &i_this->mSound, &a_this->eyePos, &scale);
}

static void eff_set(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    
    if (a_this->field_0x567 == 0) {
        J3DModel* model = i_this->mpMorf->getModel();
        
        if (i_this->field_0x123c != 0) {
            i_this->field_0x123c--;
            i_this->field_0x123d = 3;
        }
        
        f32 scale;
        if (i_this->field_0x123d != 1) {
            scale = 0.0f;
        } else {
            scale = 1.0f;
        }
        
        if (a_this->scale.x != 1.0f || a_this->scale.y != 1.0f || a_this->scale.z != 1.0f) {
            scale = 0.0f;
            i_this->field_0x1258 = 0.0f;
        }
        
        DUSK_IF_ELSE(actor_attr::enemy_add_action_calc2(i_this, &i_this->field_0x1258, scale, 1.0f, 0.05f), cLib_addCalc2(&i_this->field_0x1258, scale, 1.0f, 0.05f));
        
        for (int i = 0; i <= 2; i++) {
            static u16 p_name[3] = {
                dPa_RM(ID_ZI_S_YD_YODAREM1_A),
                dPa_RM(ID_ZI_S_YD_YODAREM1_B),
                dPa_RM(ID_ZI_S_YD_YODAREM2_A),
            };

            i_this->mParticleIds[i] = dComIfGp_particle_set(i_this->mParticleIds[i], p_name[i],
                                                            &a_this->current.pos, 0, NULL);

            JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->mParticleIds[i]);
            if (emitter != NULL) {
                static int p_idx[3] = {
                    2, 2, 6,
                };

                MTXCopy(model->getAnmMtx(p_idx[i]), *calc_mtx);
                MtxScale(i_this->field_0x1258, i_this->field_0x1258, i_this->field_0x1258, 1);
                emitter->setGlobalSRTMatrix(*calc_mtx);
            }
        }

        if (i_this->field_0x123d != 0) {
            if (i_this->field_0x123d == 1) {
                cXyz pos = a_this->current.pos;
                pos.y += 100.0f;
                if (fopAcM_gc_c::gndCheck(&pos)) {
                    pos.y = fopAcM_gc_c::getGroundY();
                }
                i_this->mParticleIds2[1] = dComIfGp_particle_set(
                    i_this->mParticleIds2[1], dPa_RM(ID_ZI_S_YD_YODAPOTA_A), &pos, 0, NULL);
            } else if (i_this->field_0x123d == 2) {
                i_this->mParticleIds2[0] =
                    dComIfGp_particle_set(i_this->mParticleIds2[0], dPa_RM(ID_ZI_S_YD_YODABURU_A),
                                          &a_this->current.pos, 0, NULL);
                {
                    JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->mParticleIds2[0]);
                    if (emitter != NULL) {
                        MTXCopy(model->getAnmMtx(1), *calc_mtx);
                        emitter->setGlobalRTMatrix(*calc_mtx);
                    }
                }
            } else if (i_this->field_0x123d == 3) {
                i_this->mParticleIds2[2] =
                    dComIfGp_particle_set(i_this->mParticleIds2[2], dPa_RM(ID_ZI_S_YD_YODAHIT_A),
                                          &a_this->current.pos, 0, NULL);
                {
                    JPABaseEmitter* emitter = dComIfGp_particle_getEmitter(i_this->mParticleIds2[2]);
                    if (emitter != NULL) {
                        MTXCopy(model->getAnmMtx(1), *calc_mtx);
                        emitter->setGlobalRTMatrix(*calc_mtx);
                    }
                }
            }
            i_this->field_0x123d = 0;
        }
    }
}

static void anm_se_set(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    
    if (i_this->field_0x664 == 6) {
        if (i_this->mpMorf->checkFrame(22.0f)) {
            i_this->mSound.startCreatureSound(Z2SE_EN_DB_BERON, 0, -1);
        }
    } else if (i_this->field_0x664 == 7) {
        if (i_this->mpMorf->checkFrame(21.0f)) {
            i_this->mSound.startCreatureSound(Z2SE_EN_DB_ATTACK, 0, -1);
        }
    } else if (i_this->field_0x664 == 0x12) {
        if (i_this->mpMorf->checkFrame(15.0f)) {
            i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_AWAKE, -1);
        }
    } else if (i_this->field_0x664 == 0x10) {
        if (i_this->mpMorf->checkFrame(3.0f)) {
            i_this->mSound.startCreatureVoice(Z2SE_EN_DB_V_CREEP, -1);
        }
    }
}

static int daE_YH_Execute(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    cXyz cStack_34;
    cXyz cStack_40;
    
    if (a_this->health == 1000) {
#if TARGET_PC  // enemy attribute integration
        const f32 effectMultiplier = actor_attr::enemy_size_multiplier(i_this);
        cXyz cStack_4c(effectMultiplier, effectMultiplier, effectMultiplier);
#else
        cXyz cStack_4c(1.0f, 1.0f, 1.0f);
#endif
        cXyz cStack_58(a_this->current.pos);
        dComIfGp_particle_set(0x109, &cStack_58, 0, &cStack_4c);
        fopAcM_delete(a_this);
        return 1;
    }
    i_this->field_0x66c++;
    for (int i = 0; i < 3; i++) {
        if (i_this->field_0x698[i] != 0) {
            i_this->field_0x698[i]--;
        }
    }
    if (i_this->field_0x69e != 0) {
        i_this->field_0x69e--;
    }
    
    action(i_this);
    anm_se_set(i_this);
    
    mDoMtx_stack_c::transS(a_this->current.pos.x, a_this->current.pos.y, a_this->current.pos.z);
    mDoMtx_stack_c::YrotM(a_this->shape_angle.y);
    mDoMtx_stack_c::XrotM(a_this->shape_angle.x);
    mDoMtx_stack_c::ZrotM(a_this->shape_angle.z);
    
    if (i_this->field_0x66e != 0x15) {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(i_this, &a_this->shape_angle.z, 0, 8, 0x400), cLib_addCalcAngleS2(&a_this->shape_angle.z, 0, 8, 0x400));
    }
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
    f32 scale = yh_effective_size(i_this);
#else
    
    f32 scale = l_HIO.mBasicSize * i_this->field_0x694;
#endif
    if (!(a_this->scale.x == 1.0f && a_this->scale.y == 1.0f && a_this->scale.z == 1.0f)) {
#if TARGET_PC  // enemy attribute integration
        mDoMtx_stack_c::scaleM(a_this->scale.x * sizeMultiplier, a_this->scale.y * sizeMultiplier, a_this->scale.z * sizeMultiplier);
#else
        mDoMtx_stack_c::scaleM(a_this->scale.x, a_this->scale.y, a_this->scale.z);
#endif
    } else {
        mDoMtx_stack_c::scaleM(scale, scale, scale);
    }
    
    J3DModel* model = i_this->mpMorf->getModel();
    model->setBaseTRMtx(mDoMtx_stack_c::get());
    
    i_this->mpMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(a_this)));
    i_this->mBrk->play();
    i_this->mpMorf->modelCalc();
    
    cXyz cStack_64(-30000.0f, 10000.0f, -20000.0f);
    MTXCopy(model->getAnmMtx(1), *calc_mtx);
    cStack_34.set(30.0f, 0.0f, 0.0f);
    
    if (i_this->field_0x854 != 0) {
        a_this->eyePos = i_this->field_0x6a0[6];
        a_this->attention_info.position = a_this->eyePos;
        i_this->mCcSph.SetTgType(0xd8fbfdff);
    } else {
        MtxPosition(&cStack_34, &a_this->eyePos);
        a_this->attention_info.position = a_this->eyePos;
        a_this->attention_info.position.y += DUSK_IF_ELSE(60.0f * sizeMultiplier, 60.0f);
        i_this->mCcSph.SetTgType(0xd8fbfdff);
    }
    
    MtxPosition(&cStack_34, &cStack_40);
    if (i_this->field_0x66e == 0 || i_this->field_0x69e != 0) {
        cStack_40 += cStack_64;
    }
    
    i_this->mCcSph.SetC(cStack_40);
    i_this->mCcSph.SetR(DUSK_IF_ELSE(40.0f * scale, l_HIO.mBasicSize * 40.0f * i_this->field_0x694));
    dComIfG_Ccsp()->Set(&i_this->mCcSph);
    
    if (i_this->field_0xb18 != 0) {
        i_this->mAtSph.MoveCAt(cStack_40);
        i_this->field_0xb18 = 0;
    } else {
        i_this->mAtSph.SetC(cStack_40 + cStack_64);
    }
    
    i_this->mAtSph.SetR(DUSK_IF_ELSE(30.0f * scale, l_HIO.mBasicSize * 30.0f * i_this->field_0x694));
    dComIfG_Ccsp()->Set(&i_this->mAtSph);
    
    eff_set(i_this);
    return 1;
}

static int daE_YH_IsDelete(e_yh_class* i_this) {
    return 1;
}

static int daE_YH_Delete(e_yh_class* i_this) {
    fopAc_ac_c* a_this = (fopAc_ac_c*)i_this;
    fopAcM_RegisterDeleteID(a_this, "E_YH");
    dComIfG_resDelete(&i_this->mPhase, "E_yd");
    if (i_this->mIsHIOOwner) {
        hio_set = false;
        mDoHIO_DELETE_CHILD(l_HIO.mId);
    }

    if (a_this->heap != NULL) {
        i_this->mpMorf->stopZelAnime();
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* a_this) {
    e_yh_class* i_this = (e_yh_class*)a_this;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
#endif
    i_this->mpMorf = JKR_NEW mDoExt_McaMorfSO((J3DModelData*)dComIfG_getObjectRes("E_yd", 0x18), NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("E_yd", 0x13), 2, 1.0, 0, -1, &i_this->mSound,
            0x80000, 0x11000084);

    if (i_this->mpMorf == NULL || i_this->mpMorf->getModel() == NULL) {
        return 0;
    }

    J3DModel* model = i_this->mpMorf->getModel();
    i_this->mBrk = JKR_NEW mDoExt_brkAnm();
    if (i_this->mBrk == NULL) {
        return 0;
    }

    if (i_this->mBrk->init(model->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_yd", 0x1d), 1, 2, 1.0f, 0, -1) == 0) {
        return 0;
    }

    if (i_this->mLine.init(1, 12, (ResTIMG*)dComIfG_getObjectRes("E_yd", 0x20), 1) == 0) {
        return 0;
    }

    f32* pSize = i_this->mLine.getSize(0);
    for (int i = 0; i < 12; i++, pSize++) {
        *pSize = DUSK_IF_ELSE((XREG_S(0) * 0.1f + 5.0f) * sizeMultiplier, XREG_S(0) * 0.1f + 5.0f);
    }

    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_yd", 0x1a);
    JUT_ASSERT(3719, modelData != NULL);

    for (int i = 1; i < 11; i++) {
        if ((i & 1) != 0) {
            i_this->mModels[i] = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
            if (i_this->mModels[i] == NULL) {
                return 0;
            }

            i_this->field_0x7d8[i] = DUSK_IF_ELSE((cM_rndF(1.0f) + 1.0f) * sizeMultiplier, cM_rndF(1.0f) + 1.0f);
        }
    }
    return 1;
}

static int daE_YH_Create(fopAc_ac_c* a_this) {
    e_yh_class* i_this = (e_yh_class*)a_this;
    fopAcM_ct(a_this, e_yh_class);
    int rv = dComIfG_resLoad(&i_this->mPhase, "E_yd");
    if (rv == cPhs_COMPLEATE_e) {
        OS_REPORT("E_YH PARAM %x\n", fopAcM_GetParam(a_this));
        u8 cVar9 = (fopAcM_GetParam(a_this) >> 24) & 0xff;
        if (cVar9 != 0xff) {
            if (dComIfGs_isSwitch(cVar9, fopAcM_GetRoomNo(a_this))) {
                // E_YH: Since it's after being hit, it won't reset.
                OS_REPORT("E_YH やられ後なので再セットしません\n");
                return cPhs_ERROR_e;
            }
        }

        i_this->field_0x5b4 = fopAcM_GetParam(a_this) & 0xff;
        i_this->field_0x5b5 = (fopAcM_GetParam(a_this) >> 8) & 0xff;
        i_this->field_0x5b6 = (fopAcM_GetParam(a_this) >> 16) & 0xf;
        i_this->field_0x5b7 = (fopAcM_GetParam(a_this) >> 20) & 0xf;
        if (i_this->field_0x5b4 == 0xff) {
            i_this->field_0x5b4 = 0;
        }

        if (i_this->field_0x5b5 == 0xff || i_this->field_0x5b5 == 0) {
            i_this->field_0x674 = 500.0f;
        } else {
            i_this->field_0x674 = i_this->field_0x5b5 * 100.0f;
        }

        OS_REPORT("E_YH//////////////E_YH SET 1 !!\n");

        if (!fopAcM_entrySolidHeap(a_this, useHeapInit, 0x4b000)) {
            OS_REPORT("//////////////E_YH SET NON !!\n");
            return cPhs_ERROR_e;
        }

        OS_REPORT("//////////////E_YH SET 2 !!\n");

        if (!hio_set) {
            i_this->mIsHIOOwner = true;
            hio_set = true;
            l_HIO.mId = mDoHIO_CREATE_CHILD("デグババ", &l_HIO);
        }

        a_this->attention_info.flags = fopAc_AttnFlag_BATTLE_e;
        fopAcM_SetMtx(a_this, i_this->mpMorf->getModel()->getBaseTRMtx());
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(i_this);
        fopAcM_SetMin(a_this, -300.0f * sizeMultiplier, -400.0f * sizeMultiplier, -300.0f * sizeMultiplier);
        fopAcM_SetMax(a_this, 300.0f * sizeMultiplier, 400.0f * sizeMultiplier, 300.0f * sizeMultiplier);
        a_this->health = actor_attr::enemy_health_value(i_this, 30.0f);
        a_this->field_0x560 = a_this->health;
#else
        fopAcM_SetMin(a_this, -300.0f, -400.0f, -300.0f);
        fopAcM_SetMax(a_this, 300.0f, 400.0f, 300.0f);
        a_this->health = 30;
        a_this->field_0x560 = 30;
#endif
        i_this->mStts.Init(100, 0, a_this);

        static dCcD_SrcSph cc_sph_src = {
            {
                {0x0, {{0x0, 0x0, 0x0}, {0xd8fbfdff, 0x3}, 0x25}},  // mObj
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                 // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},                 // mGObjTg
                {0x0},                                              // mGObjCo
            },                                                      // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 40.0f}  // mSph
            }  // mSphAttr
        };

        static dCcD_SrcSph at_sph_src = {
            {
                {0x0, {{AT_TYPE_CSTATUE_SWING, 0x1, 0xd}, {0x0, 0x0}, 0x0}},  // mObj
                {dCcD_SE_HARD_BODY, 0x0, 0x0, 0x0, 0x0},                      // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},                           // mGObjTg
                {0x0},                                                        // mGObjCo
            },                                                                // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 35.0f}  // mSph
            }  // mSphAttr
        };

        static dCcD_SrcSph kuki_sph_src = {
            {
                {0x0, {{0x0, 0x0, 0x0}, {0xd8fbfdff, 0x3}, 0x0}},  // mObj
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0},                // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2},                // mGObjTg
                {0x0},                                             // mGObjCo
            },                                                     // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 20.0f}  // mSph
            }  // mSphAttr
        };

        i_this->mCcSph.Set(cc_sph_src);
        i_this->mCcSph.SetStts(&i_this->mStts);
        i_this->mAtSph.Set(at_sph_src);
        i_this->mAtSph.SetStts(&i_this->mStts);
        IF_DUSK(i_this->mAtSph.SetAtAtp(actor_attr::byte_value(actor_attr::enemy_attack_damage(i_this, i_this->mAtSph.GetAtAtp())));)
        for (int i = 0; i < 4; i++) {
            i_this->mKukiSphs[i].Set(kuki_sph_src);
            i_this->mKukiSphs[i].SetStts(&i_this->mStts);
        }

        i_this->mAcch.Set(fopAcM_GetPosition_p(a_this), fopAcM_GetOldPosition_p(a_this),
                            a_this, 1, &i_this->mAcchCir, fopAcM_GetSpeed_p(a_this),
                            NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        i_this->mAcchCir.SetWall(50.0f * sizeMultiplier, 50.0f * sizeMultiplier);
#else
        i_this->mAcchCir.SetWall(50.0f, 50.0f);
#endif
        i_this->mSound.init(&a_this->current.pos, &a_this->eyePos, 3, 1);
        i_this->mSound.setEnemyName("E_yh");
        i_this->mAtInfo.mpSound = &i_this->mSound;
        i_this->mAtInfo.mPowerType = 1;
        i_this->mAtInfo.field_0x18 = 45;
        i_this->field_0x66c = cM_rndF(65535.0f);
        i_this->field_0x694 = l_HIO.mFlowerSeasonSize;
        if (i_this->field_0x5b4 == 1) {
            i_this->field_0x66e = 10;
            i_this->field_0x670 = 0;
            a_this->current.angle.x = -0x8000;
            a_this->shape_angle.x = -0x8000;
            i_this->field_0x854 = 1;
        } else if (i_this->field_0x5b4 == 2) {
            a_this->current.angle.x = -0x8000;
            a_this->shape_angle.x = -0x8000;
            i_this->field_0x854 = 1;
        }

        i_this->mChildID =
            fopAcM_createChild(fpcNm_E_YD_LEAF_e, fopAcM_GetID(a_this), 0, &a_this->current.pos, fopAcM_GetRoomNo(a_this),
                                                &a_this->shape_angle, 0, -1, 0);
        daE_YH_Execute(i_this);
    }

    return rv;
}

static DUSK_CONST actor_method_class l_daE_YH_Method = {
    (process_method_func)daE_YH_Create,
    (process_method_func)daE_YH_Delete,
    (process_method_func)daE_YH_Execute,
    (process_method_func)daE_YH_IsDelete,
    (process_method_func)daE_YH_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_YH = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_YH_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(e_yh_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_YH_e,
    /* Actor SubMtd */ &l_daE_YH_Method,
    /* Status       */ fopAcStts_UNK_0x10000000_e | fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x10000_e | fopAcStts_CULL_e | fopAcStts_UNK_0x20_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

/**
 * @file d_a_e_ai.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep
#if TARGET_PC  // additional actor attribute integration

#include "dusk/mods/svc/actor_attribute_helpers.hpp"
#include "dusk/mods/svc/actor_attribute_swept_acch.hpp"

namespace actor_attr = dusk::mods::svc::actor_attr;
#endif

#include "d/actor/d_a_e_ai.h"

#include "f_op/f_op_actor_enemy.h"

class daE_AI_HIO_c : public JORReflexible {
public:
    daE_AI_HIO_c();
    virtual ~daE_AI_HIO_c() {}

    void genMessage(JORMContext*);

    /* 0x04 */ s8 id;
    /* 0x08 */ f32 model_size;
    /* 0x0C */ f32 movement_speed;
    /* 0x10 */ f32 pl_recognition_distance;
    /* 0x14 */ f32 attack_range_1;
    /* 0x18 */ f32 attack_range_2;
    /* 0x1C */ f32 home_distance;
};

daE_AI_HIO_c::daE_AI_HIO_c() {
    id = -1;
    model_size = 1.0f;
    movement_speed = 20.0f;
    pl_recognition_distance = 700.0f;
    attack_range_1 = 350.0f;
    attack_range_2 = 190.0f;
    home_distance = 0.0f;
}

void e_ai_class::initCcCylinder() {
    m_ccAtStts.Init(250, 0, this);
    m_ccShieldStts.Init(250, 0, this);

    const static dCcD_SrcSph at_sph_src = {
        {
            {0x0, {{AT_TYPE_CSTATUE_SWING, 0x2, 0xd}, {0x0, 0x0}, 0x75}}, // mObj
            {dCcD_SE_STONE, 0x0, 0x0, 0x0, 0x0}, // mGObjAt
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
            {0x0}, // mGObjCo
        }, // mObjInf
        {
            {{0.0f, 0.0f, 0.0f}, 30.0f} // mSph
        } // mSphAttr
    };

    m_ccAtSph.Set(at_sph_src);
    m_ccAtSph.SetStts(&m_ccAtStts);
    IF_DUSK(m_ccAtSph.SetAtAtp(actor_attr::enemy_attack_power_byte(this, 2.0f));)
    m_ccAtSph.OnCoSetBit();

    const static dCcD_SrcSph shield_sph_src = {
        {
            {0x0, {{0x0, 0x0, 0x0}, {0x0, 0x0}, 0x75}}, // mObj
            {dCcD_SE_STONE, 0x0, 0x0, 0x0, 0x0}, // mGObjAt
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
            {0x0}, // mGObjCo
        }, // mObjInf
        {
            {{0.0f, 0.0f, 0.0f}, 30.0f} // mSph
        } // mSphAttr
    };

    m_ccShieldSph.Set(shield_sph_src);
    m_ccShieldSph.SetStts(&m_ccShieldStts);
    m_ccShieldSph.OnCoSetBit();

    static DUSK_CONSTEXPR dCcD_SrcCyl cc_cyl_src = {
        {
            {0x0, {{0x0, 0x0, 0x0}, {0xdafbf9ff, 0x43}, 0x75}}, // mObj
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x0}, // mGObjAt
            {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
            {0x0}, // mGObjCo
        }, // mObjInf
        {
            {
                {0.0f, 0.0f, 0.0f}, // mCenter
                80.0f, // mRadius
                250.0f // mHeight
            } // mCyl
        }
    };

    m_ccCyl.Set(cc_cyl_src);
    m_ccCyl.SetStts(&m_ccAtStts);
    m_ccCyl.OnTgNoHitMark();
    m_ccCyl.OnTgIronBallRebound();
}

void e_ai_class::setCcCylinder() {
#if TARGET_PC  // enemy attribute integration

    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif
    m_ccCyl.SetC(current.pos);
#if TARGET_PC  // enemy attribute integration
    m_ccCyl.SetR(80.0f * sizeMultiplier);
    m_ccCyl.SetH(250.0f * sizeMultiplier);
#endif
    dComIfG_Ccsp()->Set(&m_ccCyl);

    cXyz local_pos;
    cXyz pos;
    pos.set(0.0f, 0.0f, 0.0f);
#if TARGET_PC  // enemy attribute integration
    local_pos.set((100.0f + TREG_F(5)) * sizeMultiplier, (100.0f + TREG_F(6)) * sizeMultiplier, TREG_F(7) * sizeMultiplier);
#else
    local_pos.set(100.0f + TREG_F(5), 100.0f + TREG_F(6), TREG_F(7));
#endif

    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::multVec(&local_pos, &pos);

    m_ccShieldSph.SetR(DUSK_IF_ELSE(50.0f * sizeMultiplier, 50.0f));
    m_ccShieldSph.SetC(pos);
    dComIfG_Ccsp()->Set(&m_ccShieldSph);
}

void e_ai_class::anm_init(int i_anm, f32 i_morf, u8 i_mode, f32 i_speed) {
#if TARGET_PC  // enemy attribute integration

    const actor_attr::ActionAnimationParams params = actor_attr::enemy_animation_params(this, i_morf, i_speed);
    m_modelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_AI", i_anm), i_mode, params.morph, params.playbackSpeed, 0.0f, -1.0f);
#else
    m_modelMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("E_AI", i_anm), i_mode, i_morf, i_speed, 0.0f, -1.0f);
#endif
    m_anm = i_anm;
}

int e_ai_class::Draw() {
    J3DModel* model_p = m_modelMorf->getModel();
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
#endif

    g_env_light.settingTevStruct(8, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(model_p, &tevStr);

    J3DModelData* modelData_p = model_p->getModelData();
    m_brk->entry(model_p->getModelData());
    m_modelMorf->entryDL();
    m_brk->remove(modelData_p);

    cXyz sp8;
#if TARGET_PC  // enemy attribute integration
    sp8.set(current.pos.x, current.pos.y + (50.0f + BREG_F(18)) * sizeMultiplier, current.pos.z);
    m_shadowKey = dComIfGd_setShadow(m_shadowKey, 1, model_p, &sp8, (1000.0f + BREG_F(19)) * sizeMultiplier, 100.0f, current.pos.y, m_acch.GetGroundH(), m_acch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
#else
    sp8.set(current.pos.x, 50.0f + current.pos.y + BREG_F(18), current.pos.z);
    m_shadowKey = dComIfGd_setShadow(m_shadowKey, 1, model_p, &sp8, 1000.0f + BREG_F(19), 100.0f, current.pos.y, m_acch.GetGroundH(), m_acch.m_gnd, &tevStr, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
#endif
    return 1;
}

BOOL e_ai_class::player_way_check() {
    s16 angle_diff = shape_angle.y - dComIfGp_getPlayer(0)->shape_angle.y;
    if (angle_diff < 0) {
        angle_diff = -angle_diff;
    }

    if ((u16)angle_diff < 0x4000) {
        return FALSE;
    }

    return TRUE;
}

BOOL e_ai_class::other_bg_check(fopAc_ac_c* i_other) {
    dBgS_LinChk linchk;
    cXyz sp24;
    cXyz start;
    cXyz end;

    end = i_other->current.pos;
    end.y += 100.0f;

    start = current.pos;
    start.y = eyePos.y;

    linchk.Set(&start, &end, this);
    if (dComIfG_Bgsp().LineCross(&linchk)) {
        return TRUE;
    }

    return FALSE;
}

BOOL e_ai_class::pl_check(f32 i_range, s16 i_angle) {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    
    if (m_playerDist < i_range) {
        s16 angle_diff = shape_angle.y - m_angleToPlayer;
        if (angle_diff < i_angle && angle_diff > (s16)-i_angle && !other_bg_check(player)) {
            return TRUE;
        }
    }

    return FALSE;
}

void e_ai_class::damage_check() {
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz sp44;
#if TARGET_PC  // enemy attribute integration
    const s16 maxHitCount = actor_attr::enemy_health_value(this, 3.0f);
    cXyz breakEffectScale = actor_attr::enemy_size_multiplier(this, 1.0f);
#endif

    if (m_invulnerabilityTimer != 0) {
        m_ccCyl.ClrTgHit();
        return;
    }

    m_ccAtStts.Move();
    health = DUSK_IF_ELSE(actor_attr::enemy_health_value(this, 1000.0f), 1000);

    u8 hitmark;
    int hitstun_time = 0;
    u8 set_hitmark = TRUE;

    if (m_ccCyl.ChkTgHit()) {
        m_invulnerabilityTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5), 5);
        
        cCcD_Obj* tg_hit_obj = m_ccCyl.GetTgHitObj();
        OS_REPORT("type = %x\n", tg_hit_obj->GetAtType());

        if (tg_hit_obj->ChkAtType(AT_TYPE_2000000)) {
            m_atInfo.field_0x18 = 35;
            hitmark = 1;
            cXyz sp38(0.0f, 80.0f, -60.0f);
            cLib_offsetPos(&sp44, &current.pos, shape_angle.y, &sp38);
        } else if (tg_hit_obj->ChkAtType(AT_TYPE_SHIELD_ATTACK)) {
            set_hitmark = FALSE;
        } else {
            if (tg_hit_obj->ChkAtType(AT_TYPE_BOOMERANG)) {
                return;
            }

            if (field_0xd28 == 0) {
                m_atInfo.field_0x18 = 35;
                hitmark = 1;
                cXyz sp2C(0.0f, 80.0f, -60.0f);
                cLib_offsetPos(&sp44, &current.pos, shape_angle.y, &sp2C);
                m_invulnerabilityTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5), 5);
            } else {
                m_atInfo.field_0x18 = 2;
                hitmark = 2;
                sp44 = *m_ccCyl.GetTgHitPosP();
            }
        }

        m_atInfo.mpCollider = m_ccCyl.GetTgHitObj();

        if (m_atInfo.mpSound != NULL) {
            dCcD_GObjInf* hitobj = (dCcD_GObjInf*)m_atInfo.mpCollider;
            u8 at_se = hitobj->GetAtSe();
            m_atInfo.mpSound->startCollisionSE(dCcD_GObjInf::getHitSeID(at_se, 0), m_atInfo.field_0x18);
        }

        if (set_hitmark) {
            u32 cut_type = daPy_getPlayerActorClass()->getCutType();
            if ((cut_type == daPy_py_c::CUT_TYPE_LARGE_TURN_LEFT ||
                 cut_type == daPy_py_c::CUT_TYPE_LARGE_TURN_RIGHT ||
                 cut_type == daPy_py_c::CUT_TYPE_LARGE_JUMP_INIT ||
                 cut_type == daPy_py_c::CUT_TYPE_LARGE_JUMP ||
                 cut_type == daPy_py_c::CUT_TYPE_LARGE_JUMP_FINISH ||
                 cut_type == daPy_py_c::CUT_TYPE_MORTAL_DRAW_B) && field_0xd28 == 0)
            {
                m_timers[2] = 0;
                m_hitCount = DUSK_IF_ELSE(maxHitCount, 3);
            }

            if (tg_hit_obj->ChkAtType(AT_TYPE_IRON_BALL) && field_0xd28 == 0) {
                m_hitCount = DUSK_IF_ELSE(maxHitCount, 3);
                m_timers[2] = 0;
            } else if (tg_hit_obj->ChkAtType(AT_TYPE_BOMB)) {
                s16 spA = cLib_targetAngleY(m_ccCyl.GetTgHitPosP(), &current.pos) - shape_angle.y;
                if (spA > -0x4000 && spA < 0x4000) {
                    m_timers[2] = 0;
                    m_hitCount = DUSK_IF_ELSE(maxHitCount, 3);
                    field_0xd28 = 0;
                }
            }

            if (tg_hit_obj->ChkAtType(AT_TYPE_2000000)) {
                m_sound.startCreatureVoice(Z2SE_EN_AI_V_DAMAGE, -1);
                hitmark = 3;
                m_sound.startCreatureSound(Z2SE_EN_AI_BREAK_LAST, 0, -1);
                m_action = ACTION_DAMAGE;

                field_0x692 = 0;
                m_mode = 0;
#if TARGET_PC  // enemy attribute integration
                dComIfGp_particle_set(0x81D7, &current.pos, &shape_angle, &breakEffectScale);
                field_0x6ba = actor_attr::enemy_sync_timer(this, 200);
#else
                dComIfGp_particle_set(0x81D7, &current.pos, &shape_angle, NULL);
                field_0x6ba = 200;
#endif
                m_atInfo.mHitStatus = 2;
                m_hitCount = DUSK_IF_ELSE(maxHitCount, 3);

                dScnPly_c::setPauseTimer(5);
                m_timers[2] = 0;
            } else if (field_0xd28 == 0) {
                m_invulnerabilityTimer = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5), 5);
                if (m_atInfo.mHitType != 0x10) {
                    m_sound.startCreatureVoice(Z2SE_EN_AI_V_DAMAGE, -1);
                    if (m_hitCount >= DUSK_IF_ELSE(maxHitCount - 1, 2)) {
                        hitmark = 3;
                        m_sound.startCreatureSound(Z2SE_EN_AI_BREAK_LAST, 0, -1);
                        m_action = ACTION_DAMAGE;

                        field_0x692 = 0;
                        m_mode = 0;
                        hitstun_time = 5;
#if TARGET_PC  // enemy attribute integration
                        dComIfGp_particle_set(0x81D7, &current.pos, &shape_angle, &breakEffectScale);
                        field_0x6ba = actor_attr::enemy_sync_timer(this, 200);
#else
                        dComIfGp_particle_set(0x81D7, &current.pos, &shape_angle, NULL);
                        field_0x6ba = 200;
#endif
                        m_timers[2] = 0;
                        m_atInfo.mHitStatus = 2;
                        m_hitCount++;
                    } else {
                        hitstun_time = 3;
                        m_hitCount++;
                        dComIfGp_particle_set(0x81D6, &current.pos, &shape_angle, DUSK_IF_ELSE(&breakEffectScale, NULL));

                        if (m_hitCount >= DUSK_IF_ELSE(maxHitCount - 1, 2)) {
                            m_ccCyl.SetTgHitMark(CcG_Tg_UNK_MARK_3);
                        }

                        if (m_action == ACTION_WAIT && m_mode == 1) {
                            m_mode = 3;
                            m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x12), 1, 0, 1.0f, 0, -1);
                            field_0x5cc = 0x12;
                        } else if (field_0x5cc == 0x11) {
                            m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x10), 1, 0, 1.0f, 0, -1);
                            field_0x5cc = 0x10;
                        }

#if TARGET_PC  // enemy attribute integration
                        field_0x6ba = actor_attr::enemy_sync_timer(this, 15);
                        field_0x6bc = actor_attr::enemy_sync_timer(this, 0x14);
#else
                        field_0x6ba = 15;
                        field_0x6bc = 0x14;
#endif
                    }
                }

                dScnPly_c::setPauseTimer(hitstun_time);
            } else if (tg_hit_obj->ChkAtType(AT_TYPE_BOMB) || tg_hit_obj->ChkAtType(AT_TYPE_IRON_BALL) || (tg_hit_obj->ChkAtType(AT_TYPE_NORMAL_SWORD) && field_0xd28 == 1)) {
                if (m_action == ACTION_WAIT && m_mode == 1) {
                    m_mode = 3;
                    m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x12), 1, 0, 1.0f, 0, -1);
                    field_0x5cc = 0x12;
                    m_sound.startCreatureSound(Z2SE_EN_AI_AWAKE, 0, -1);
                } else {
                    anm_init(10, 5.0f, 0, 1.0f);
                    field_0x692 = 1;
                    m_action = ACTION_DAMAGE;
                    speedF = 0.0f;

                    if (tg_hit_obj->ChkAtType(AT_TYPE_NORMAL_SWORD)) {
                        m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
                    } else {
                        m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 100), 100);
                    }
                }
            }

            dComIfGp_setHitMark(hitmark, this, &sp44, NULL, NULL, 0);
        }
    }
}

static u8 hio_set;

static daE_AI_HIO_c l_HIO;

void e_ai_class::e_ai_damage() {
    switch (field_0x692) {
    case 0:
        speedF = 0.0f;

        switch (field_0x694) {
        case 0:
            if (m_acch.ChkGroundHit()) {
                anm_init(8, 5.0f, 0, 1.0f + yREG_F(0));
                speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(this, 40.0f + TREG_F(10)), 40.0f + TREG_F(10));
                field_0x694 = 1;
            }
            break;
        case 1:
            field_0x67c = 1;

            if (m_modelMorf->checkFrame(4.0f)) {
                m_sound.startCreatureVoice(Z2SE_EN_AI_V_HAMMER, -1);
                m_sound.startCreatureSound(Z2SE_EN_AI_HAMMER, 0, -1);
#if TARGET_PC  // enemy attribute integration
            } if (m_modelMorf->checkFrame(7.0f)) {
#else
            } else if (m_modelMorf->checkFrame(7.0f)) {
#endif
                m_sound.startCreatureSound(Z2SE_EN_AI_HAMMER_HIT, 0, -1);
                dComIfGp_getVibration().StartShock(YREG_S(2) + 5, 0xF, cXyz(0.0f, 1.0f, 0.0f));
                field_0x67c = 2;
            }

            if (m_acch.ChkGroundHit()) {
                field_0x694 = 0;
                m_sound.startCreatureSound(Z2SE_EN_AI_DOSU, 0, -1);
                fopAcM_effSmokeSet1(&field_0xd2c, &field_0xd34, &current.pos, &shape_angle, DUSK_IF_ELSE((1.0f + TREG_F(9)) * actor_attr::enemy_size_multiplier(this), 1.0f + TREG_F(9)), &tevStr, 1);
            }
            break;
        }

        switch (m_mode) {
        case 0:
            m_mode = 1;
#if TARGET_PC  // enemy attribute integration
            m_timers[1] = actor_attr::enemy_sync_timer(this, 70);
            gravity = actor_attr::enemy_signed_gravity_step(this, -20.0f + TREG_F(11));
#else
            m_timers[1] = 70;
            gravity = -20.0f + TREG_F(11);
#endif
            break;
        case 1:
            if (m_timers[0] == 0) {
                m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 5.0f + cM_rndF(10.0f)), 5.0f + cM_rndF(10.0f));

                if (cM_rndF(1.0f) < 0.5f) {
                    field_0x6a8 = 0x800;
                } else {
                    field_0x6a8 = -0x800;
                }

                m_sound.startCreatureVoice(Z2SE_EN_AI_V_MOGAKU, -1);
            }

            speedF = 0.5f * l_HIO.movement_speed;
#if TARGET_PC  // enemy attribute integration
            actor_attr::enemy_angle_add(this, current.angle.y , field_0x6a8);
            shape_angle.z = (2000.0f + TREG_F(16)) * cM_ssin(actor_attr::enemy_action_phase_angle(this, m_lifetime, TREG_S(7) + 6000));
#else
            current.angle.y += field_0x6a8;
            shape_angle.z = (2000.0f + TREG_F(16)) * cM_ssin(m_lifetime * (TREG_S(7) + 6000));
#endif

            if (m_timers[1] == 0) {
                m_sound.startCreatureSound(Z2SE_EN_AI_FLASH, 0, -1);
                mpEmitter = dComIfGp_particle_set(0x81ED, &current.pos, &tevStr, &shape_angle, NULL);
                if (mpEmitter != NULL) {
#if TARGET_PC  // enemy attribute integration
                    cXyz effectScale = actor_attr::enemy_size_multiplier(this, 1.0f);
                    mpEmitter->setGlobalScale(effectScale);
#endif
                    mpEmitter->becomeImmortalEmitter();
                }
#if TARGET_PC  // enemy attribute integration
                m_timers[1] = actor_attr::enemy_sync_timer(this, 1000);
                m_timers[2] = actor_attr::enemy_sync_timer(this, 56);
#else
                m_timers[1] = 1000;
                m_timers[2] = 56;
#endif
            }

            if (m_timers[2] > 0 && mpEmitter != NULL) {
                mpEmitter->setGlobalTranslation(current.pos);
            }

            if (m_timers[2] == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 1), 1)) {
                cXyz pos(current.pos);
                pos.y += 80.0f;

                fopAcM_createDisappear(this, &pos, DUSK_IF_ELSE(static_cast<u8>(12.0f * actor_attr::enemy_size_multiplier(this)), 12), 0, 0x1E);

                if (m_swbit != 0xFF && !dComIfGs_isSwitch(m_swbit, fopAcM_GetRoomNo(this))) {
                    dComIfGs_onSwitch(m_swbit, fopAcM_GetRoomNo(this));
                }

                fopAcM_delete(this);
            }
        }
        break;
    case 1:
        if (m_modelMorf->isStop() && m_timers[0] == 0) {
            m_action = ACTION_MOVE;
            anm_init(7, 1.0f, 0, 1.0f);
            m_modelMorf->setFrame(25.0f);
            m_mode = 1;
            field_0x694 = 0;
            field_0x696 = 0;
        }
        break;
    }
}

void e_ai_class::e_ai_attack() {
    speedF = 0.0f;
    DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, field_0x6a8 + TREG_S(4) + 4000, 2, 0x400), cLib_addCalcAngleS2(&current.angle.y, field_0x6a8 + TREG_S(4) + 4000, 2, 0x400));

    switch (m_mode) {
    case 0:
        if (m_timers[0] == 0) {
            anm_init(6, 0.5f, 0, 1.0f);
            m_sound.startCreatureVoice(Z2SE_EN_AI_V_HAMMER, -1);
            m_sound.startCreatureSound(Z2SE_EN_AI_HAMMER, 0, -1);
            m_mode = 1;
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, field_0x6a8 + TREG_S(4) + 4000, 1, 0x800), cLib_addCalcAngleS2(&current.angle.y, field_0x6a8 + TREG_S(4) + 4000, 1, 0x800));
        }
        break;
    case 1:
        field_0x67c = 1;
        if (DUSK_IF_ELSE(m_modelMorf->checkFrame(4.0f), (int)m_modelMorf->getFrame() == 4)) {
            m_sound.startCreatureSound(Z2SE_EN_AI_HAMMER_HIT, 0, -1);
            dComIfGp_getVibration().StartShock(YREG_S(2) + 5, 0xF, cXyz(0.0f, 1.0f, 0.0f));
            field_0x67c = 2;
        }

        if (m_modelMorf->isStop()) {
            m_mode = 2;

            if (m_hitCount == 0) {
                m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
            } else if (m_hitCount == 1) {
                m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 25), 25);
            } else {
                m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            }
        }
        return;
    case 2:
        m_ccAtSph.OffAtSetBit();
        field_0x67c = 1;

        if (m_timers[0] == 0) {
            m_ccAtSph.OnAtSetBit();
            anm_init(9, 15.0f, 2, 0.0f);
            m_action = ACTION_MOVE;
            m_mode = 0;
            field_0x694 = 0;
            field_0x696 = 0;
        }
        break;
    }
}

void e_ai_class::e_ai_move() {
    if ((field_0x5cc == 0x10 || field_0x5cc == 0x12) && m_brk->isStop()) {
        m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x11), 1, 2, 1.0f, 0, -1);
        field_0x5cc = 0x11;
    }

    if (m_timers[2] != 0) {
        shape_angle.z = (500.0f + TREG_F(18)) * cM_ssin(DUSK_IF_ELSE(actor_attr::enemy_action_phase_angle(this, m_lifetime, TREG_S(5) + 10000), m_lifetime * (TREG_S(5) + 10000)));

        if (m_timers[2] == DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 1), 1)) {
            m_sound.startCreatureVoice(Z2SE_EN_AI_V_AWAKE, -1);
        } else {
            m_sound.startCreatureSoundLevel(Z2SE_EN_AI_GURAGURA, 0, -1);
        }
        return;
    }

    switch (field_0x694) {
    case 0:
        if (m_acch.ChkGroundHit()) {
            if (m_timers[1] == 0) {
                speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(this, 25.0f + TREG_F(0)), 25.0f + TREG_F(0));
                field_0x694 = 1;
                shape_angle.z = 0;
            }
            speedF = 0.0f;
        }
        break;
    case 1:
        if (m_acch.ChkGroundHit()) {
            speedF = 0.0f;
            field_0x694 = 0;

            if (m_hitCount == 0) {
                m_timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
            } else if (m_hitCount == 1) {
                m_timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            } else {
                m_timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
            }

            m_sound.startCreatureSound(Z2SE_EN_AI_DOSU, 0, -1);
            fopAcM_effSmokeSet1(&field_0xd2c, &field_0xd34, &current.pos, &shape_angle, DUSK_IF_ELSE((1.0f + TREG_F(9)) * actor_attr::enemy_size_multiplier(this), 1.0f + TREG_F(9)), &tevStr, 1);

            if (m_playerDist > DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, l_HIO.pl_recognition_distance), l_HIO.pl_recognition_distance) && current.pos.absXZ(home.pos) > 100.0f * field_0x5ba) {
                field_0x694 = 0;
                m_mode = 0;
                m_action = ACTION_RETURN;
                return;
            }
        }
        break;
    }

    switch (m_mode) {
    case 0:
        if (m_anm != 7) {
            anm_init(7, 0.5f, 0, 1.0f);
        }
        m_mode = 1;
        break;
    case 1:
        if (field_0x694 == 1) {
            field_0x6a8 = m_angleToPlayer;
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, field_0x6a8, ((yREG_S(0) + 16) / (m_hitCount + 1)), 0x800), cLib_addCalcAngleS2(&current.angle.y, field_0x6a8, ((yREG_S(0) + 16) / (m_hitCount + 1)), 0x800));

            if (m_playerDist > DUSK_IF_ELSE(actor_attr::enemy_size_value(this, l_HIO.attack_range_2), l_HIO.attack_range_2)) {
                speedF = l_HIO.movement_speed;
            }
        }

        if (m_modelMorf->isStop() && pl_check(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, l_HIO.attack_range_2), l_HIO.attack_range_2), 0x7FFF)) {
            m_action = ACTION_ATTACK;
            m_mode = 0;
            m_timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 4), 4);
        }
        break;
    }
}

void e_ai_class::e_ai_wait() {
    switch (m_mode) {
    case 0:
        anm_init(9, 5.0f, 2, 0.0f);
        m_mode = 1;
        break;
    case 1:
        if (pl_check(DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, l_HIO.pl_recognition_distance), l_HIO.pl_recognition_distance), 0x5000)) {
            m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x12), 1, 0, 1.0f, 0, -1);
            field_0x5cc = 0x12;
            m_mode = 3;
        }
        break;
    case 2:
        if (m_modelMorf->isStop()) {
            m_mode = 0;
        }
        break;
    case 3:
        if (m_brk->isStop()) {
            m_action = ACTION_MOVE;
            m_mode = 0;
            m_timers[2] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 50), 50);
            m_sound.startCreatureSound(Z2SE_EN_AI_AWAKE, 0, -1);
            m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x11), 1, 2, 1.0f, 0, -1);
            field_0x5cc = 0x11;
        }
        break;
    case 10:
        if (m_brk->isStop()) {
            m_mode = 1;
        }
        break;
    }
}

void e_ai_class::e_ai_return() {
    switch (m_mode) {
    case 0:
        if (m_acch.ChkGroundHit()) {
            if (m_timers[1] == 0) {
                speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(this, 25.0f + TREG_F(0)), 25.0f + TREG_F(0));
                m_mode = 1;
            }
            speedF = 0.0f;
        }
        break;
    case 1:
        if (current.pos.absXZ(home.pos) > 50.0f) {
            speedF = l_HIO.movement_speed;
            field_0x6a8 = cLib_targetAngleY(&current.pos, &home.pos);
            DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &current.angle.y, field_0x6a8, 2, 0x500), cLib_addCalcAngleS2(&current.angle.y, field_0x6a8, 2, 0x500));
        } else {
            speedF = 0.0f;
            field_0x6a8 = home.angle.y;
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_angle(this, &current.angle.y, field_0x6a8, 0x300), cLib_chaseAngleS(&current.angle.y, field_0x6a8, 0x300));
        }

        if (home.angle.y == current.angle.y && m_acch.ChkGroundHit()) {
            m_mode = 10;
            m_action = ACTION_WAIT;
            anm_init(9, 5.0f, 2, 0.0f);
            m_brk->init(m_modelMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x13), 1, 0, 1.0f, 0, -1);
            field_0x5cc = 0x13;
        }

        if (m_acch.ChkGroundHit()) {
            if ((m_playerDist < DUSK_IF_ELSE(actor_attr::enemy_notice_distance(this, l_HIO.pl_recognition_distance), l_HIO.pl_recognition_distance) || m_ccCyl.ChkTgHit()) && field_0x5cc != 0x13) {
                m_action = ACTION_MOVE;
            }

            speedF = 0.0f;
            m_mode = 0;

            if (m_hitCount == 0) {
                m_timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 30), 30);
            } else if (m_hitCount == 1) {
                m_timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 20), 20);
            } else {
                m_timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, 15), 15);
            }

            m_sound.startCreatureSound(Z2SE_EN_AI_DOSU, 0, -1);
            fopAcM_effSmokeSet1(&field_0xd2c, &field_0xd34, &current.pos, &shape_angle, DUSK_IF_ELSE((1.0f + TREG_F(9)) * actor_attr::enemy_size_multiplier(this), 1.0f + TREG_F(9)), &tevStr, 1);
        }
        break;
    }
}

void e_ai_class::action() {
    cXyz sp14;
    cXyz sp8;

    m_angleToPlayer = fopAcM_searchPlayerAngleY(this);
    m_playerDist = fopAcM_searchPlayerDistance(this);

    if (m_hitCount < DUSK_IF_ELSE(actor_attr::enemy_health_value(this, 3.0f), 3)) {
        damage_check();
    }

    switch (m_action) {
    case ACTION_WAIT:
        e_ai_wait();
        break;
    case ACTION_MOVE:
        e_ai_move();
        break;
    case ACTION_ATTACK:
        e_ai_attack();
        break;
    case ACTION_DAMAGE:
        e_ai_damage();
        break;
    case ACTION_RETURN:
        e_ai_return();
        break;
    }

    DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, current.angle.y, 1, 0x2000), cLib_addCalcAngleS2(&shape_angle.y, current.angle.y, 1, 0x2000));
    cMtx_YrotS(*calc_mtx, current.angle.y);

    sp14.x = 0.0f;
    sp14.y = 0.0f;
    m_brk->play();

    if (field_0x6bc != 0) {
        shape_angle.z = (200.0f + TREG_F(18)) * cM_ssin(DUSK_IF_ELSE(actor_attr::enemy_action_phase_angle(this, m_lifetime, TREG_S(5) + 15000), m_lifetime * (TREG_S(5) + 15000)));
        sp14.z = 0.0f;
    } else {
        sp14.z = DUSK_IF_ELSE(actor_attr::enemy_move_step(this, speedF), speedF);
        m_modelMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
    }

    MtxPosition(&sp14, &sp8);
    speed.x = sp8.x;
    speed.z = sp8.z;
#if TARGET_PC  // enemy attribute integration
    const cXyz movementStart = current.pos;
#endif
    current.pos += speed;
    speed.y += gravity;

    if (field_0x6ba != 0) {
        field_0x6ba--;
        field_0x6c0 = 0.5f + (0.5f * cM_ssin(DUSK_IF_ELSE(actor_attr::enemy_action_phase_angle(this, field_0x6ba, BREG_S(5) + 8000), field_0x6ba * (BREG_S(5) + 8000))));
    }

    if (m_action != ACTION_WAIT) {
        cXyz* cc_move_p = m_ccAtStts.GetCCMoveP();
        if (cc_move_p != NULL) {
            current.pos.x += cc_move_p->x;
            current.pos.y += cc_move_p->y;
            current.pos.z += cc_move_p->z;
        }

        cc_move_p = m_ccShieldStts.GetCCMoveP();
        if (cc_move_p != NULL) {
            current.pos.x += cc_move_p->x;
            current.pos.y += cc_move_p->y;
            current.pos.z += cc_move_p->z;
        }
    }

    IF_DUSK(actor_attr::enemy_swept_ground_correct(this, m_acch, movementStart);)
    m_acch.CrrPos(dComIfG_Bgsp());
}

void e_ai_class::PlayerWaySet() {
    if (player_way_check()) {
        m_ccCyl.OnTgShield();
        m_ccCyl.SetTgHitMark(CcG_Tg_UNK_MARK_2);
        field_0xd28 = 1;
    } else {
        m_ccCyl.OffTgShield();
        m_ccCyl.SetTgHitMark(CcG_Tg_UNK_MARK_0);
        field_0xd28 = 0;
    }
}

void e_ai_class::AttentionSet() {
    eyePos = current.pos;
    eyePos.y += DUSK_IF_ELSE((100.0f + TREG_F(2)) * actor_attr::enemy_size_multiplier(this), 100.0f + TREG_F(2));

    attention_info.position = eyePos;
    attention_info.position.y += DUSK_IF_ELSE(130.0f * actor_attr::enemy_size_multiplier(this), 130.0f);
}

int e_ai_class::Execute() {
    cXyz sp38;
    cXyz sp2C;

    m_lifetime++;

    setCcCylinder();

    for (int i = 0; i < 4; i++) {
        if (m_timers[i] != 0) {
            m_timers[i]--;
        }
    }

    if (m_invulnerabilityTimer != 0) {
        m_invulnerabilityTimer--;
    }

    if (field_0x6bc != 0) {
        field_0x6bc--;
    }

    action();

#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
    cXyz eff_size(1.5f * sizeMultiplier, 1.5f * sizeMultiplier, 1.5f * sizeMultiplier);
#else
    cXyz eff_size(1.5f, 1.5f, 1.5f);
#endif
    setMidnaBindEffect(this, &m_sound, &current.pos, &eff_size);

    AttentionSet();

    if (m_action != ACTION_WAIT) {
        PlayerWaySet();
    } else {
        m_ccCyl.OnTgShield();
        m_ccCyl.SetTgHitMark(CcG_Tg_UNK_MARK_2);
        field_0xd28 = 1;
    }

    setBaseMtx();

    if (field_0x67c != 0) {
        MTXCopy(m_modelMorf->getModel()->getAnmMtx(2), *calc_mtx);
        sp38.set(50.0f + TREG_F(5), -130.0f + TREG_F(6), TREG_F(7));
        MtxPosition(&sp38, &sp2C);

        m_ccAtSph.SetR(DUSK_IF_ELSE((50.0f) * actor_attr::enemy_size_multiplier(this), 50.0f));
        m_ccAtSph.SetC(sp2C);

        if (field_0x67c == 2) {
            sp38.set(50.0f + TREG_F(5), -150.0f + TREG_F(6), TREG_F(7));
            MtxPosition(&sp38, &sp2C);
            fopAcM_effSmokeSet1(&field_0xd30, &field_0xd38, &sp2C, &shape_angle, DUSK_IF_ELSE((1.0f + TREG_F(8)) * actor_attr::enemy_size_multiplier(this), 1.0f + TREG_F(8)), &tevStr, 1);
        }
        field_0x67c = 0;
    } else {
        cXyz sp14(-30000.0f, 20000.0f, 15000.0f);
        m_ccAtSph.SetC(current.pos + sp14);
    }

    dComIfG_Ccsp()->Set(&m_ccAtSph);
    return 1;
}

void e_ai_class::setBaseMtx() {
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::YrotM(shape_angle.y);
    mDoMtx_stack_c::ZrotM(shape_angle.z);
#if TARGET_PC  // enemy attribute integration
    const f32 modelScale = l_HIO.model_size * actor_attr::enemy_size_multiplier(this);
    mDoMtx_stack_c::scaleM(modelScale, modelScale, modelScale);
#else
    mDoMtx_stack_c::scaleM(l_HIO.model_size, l_HIO.model_size, l_HIO.model_size);
#endif

    J3DModel* model_p = m_modelMorf->getModel();
    model_p->setBaseTRMtx(mDoMtx_stack_c::get());
    m_modelMorf->modelCalc();
}

int e_ai_class::Delete() {
    dComIfG_resDelete(&m_phase, "E_AI");

    if (m_HIOInit) {
        hio_set = FALSE;
        mDoHIO_DELETE_CHILD(l_HIO.id);
    }

    if (heap != NULL) {
        m_modelMorf->stopZelAnime();
    }

    return 1;
}

static int daE_AI_Draw(e_ai_class* i_this) {
    return i_this->Draw();
}

static int daE_AI_Execute(e_ai_class* i_this) {
    return i_this->Execute();
}

static int daE_AI_IsDelete(e_ai_class* i_this) {
    return 1;
}

static int daE_AI_Delete(e_ai_class* i_this) {
    return i_this->Delete();
}

int e_ai_class::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_AI", 0xD);
    JUT_ASSERT(1703, modelData != NULL);

    m_modelMorf = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("E_AI", 9), 2, 1.0f, 0, -1, &m_sound, 0, 0x11000084);
    if (m_modelMorf == NULL || m_modelMorf->getModel() == NULL) {
        return 0;
    }

    m_brk = JKR_NEW mDoExt_brkAnm();
    if (m_brk == NULL) {
        return 0;
    }

    J3DAnmTevRegKey* brk_p = (J3DAnmTevRegKey*)dComIfG_getObjectRes("E_AI", 0x12);
    if (!m_brk->init(m_modelMorf->getModel()->getModelData(), brk_p, 1, 0, 0.0f, 0, -1)) {
        return 0;
    }

    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    return ((e_ai_class*)i_this)->CreateHeap();
}

int e_ai_class::Create() {
    fopAcM_ct(this, e_ai_class);

    int phase_state = dComIfG_resLoad(&m_phase, "E_AI");
    if (phase_state == cPhs_COMPLEATE_e) {
        OS_REPORT("E_AI PARAM %x\n", fopAcM_GetParam(this));
        field_0x5ba = fopAcM_GetParam(this);

        m_swbit = fopAcM_GetParam(this) >> 0x10;
        if (m_swbit != 0xFF && dComIfGs_isSwitch(m_swbit, fopAcM_GetRoomNo(this))) {
            OS_REPORT("E_AI やられ後なので再セットしません\n");
            return cPhs_ERROR_e;
        }

        OS_REPORT("E_AI//////////////E_AI SET 1 !!\n");
        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x1080)) {
            OS_REPORT("//////////////E_AI SET NON !!\n");
            return cPhs_ERROR_e;
        }

        OS_REPORT("//////////////E_AI SET 2 !!\n");
        if (!hio_set) {
            hio_set = TRUE;
            m_HIOInit = TRUE;
            l_HIO.id = mDoHIO_CREATE_CHILD("アモス", &l_HIO);
        }

        attention_info.flags = fopAc_AttnFlag_BATTLE_e;

        fopAcM_SetMtx(this, m_modelMorf->getModel()->getBaseTRMtx());
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(this);
        fopAcM_SetMin(this, -200.0f * sizeMultiplier, -300.0f * sizeMultiplier, -200.0f * sizeMultiplier);
        fopAcM_SetMax(this, 200.0f * sizeMultiplier, 300.0f * sizeMultiplier, 200.0f * sizeMultiplier);
#else
        fopAcM_SetMin(this, -200.0f, -300.0f, -200.0f);
        fopAcM_SetMax(this, 200.0f, 300.0f, 200.0f);
#endif

#if TARGET_PC  // enemy attribute integration
        health = actor_attr::enemy_health_value(this, 1000.0f);
        field_0x560 = health;
#else
        health = 1000;
        field_0x560 = 1000;
#endif

        initCcCylinder();

        m_acch.Set(fopAcM_GetPosition_p(this), fopAcM_GetOldPosition_p(this), this, 1, &m_acchcir, fopAcM_GetSpeed_p(this), NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        m_acchcir.SetWall(100.0f * sizeMultiplier, 150.0f * sizeMultiplier);
#else
        m_acchcir.SetWall(100.0f, 150.0f);
#endif

        field_0x5b9 = 0;

        m_sound.init(&current.pos, &eyePos, 3, 1);
        m_atInfo.mpSound = &m_sound;

        l_HIO.home_distance = 100.0f * field_0x5ba;
        gravity = DUSK_IF_ELSE(actor_attr::enemy_signed_gravity_step(this, -10.0f), -10.0f);

        daE_AI_Execute(this);
    }

    return phase_state;
}

static int daE_AI_Create(fopAc_ac_c* i_this) {
    return ((e_ai_class*)i_this)->Create();
}

static DUSK_CONST actor_method_class l_daE_AI_Method = {
    (process_method_func)daE_AI_Create,
    (process_method_func)daE_AI_Delete,
    (process_method_func)daE_AI_Execute,
    (process_method_func)daE_AI_IsDelete,
    (process_method_func)daE_AI_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_AI = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_AI_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(e_ai_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_AI_e,
    /* Actor SubMtd */ &l_daE_AI_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_CULL_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

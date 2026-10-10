/**
 * @file d_a_e_cr_egg.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_e_cr_egg.h"
#include "d/d_cc_d.h"
#include "d/d_com_inf_game.h"
#include "d/d_s_play.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"

namespace actor_attr = dusk::mods::svc::actor_attr;

namespace dusk::mods::svc::actor_attr {

template <>
struct EnemyActorAccessor<e_cr_egg_class> {
    static fopAc_ac_c* get(e_cr_egg_class* i_this) {

        return &i_this->enemy;

}
};

template <>
struct EnemyAttributeOwner<e_cr_egg_class> {
    static fopAc_ac_c* get(e_cr_egg_class* i_this) {

        fopAc_ac_c* actor = EnemyActorAccessor<e_cr_egg_class>::get(i_this);
        fopAc_ac_c* parent = fopAcM_SearchByID(fopAcM_GetLinkId(actor));

        if (parent != NULL && fopAcM_GetName(parent) == fpcNm_E_CR_e) {
            return parent;
        }

        return actor;

}
};

}  // namespace dusk::mods::svc::actor_attr
#endif

static int daE_CR_EGG_Draw(e_cr_egg_class* a_this) {
    fopAc_ac_c* actor = &a_this->enemy;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(a_this);
#endif

    g_env_light.settingTevStruct(0, &actor->current.pos, &actor->tevStr);
    g_env_light.setLightTevColorType_MAJI(a_this->model, &actor->tevStr);
    mDoExt_modelUpdateDL(a_this->model);

    dComIfGd_setSimpleShadow(&actor->current.pos, a_this->acch.GetGroundH(), DUSK_IF_ELSE((30.0f + TREG_F(10)) * sizeMultiplier, 30.0f + TREG_F(10)), a_this->acch.m_gnd, 0, 1.0f, dDlst_shadowControl_c::getSimpleTex());
    return 1;
}

static void e_cr_egg_move(e_cr_egg_class* a_this) {
    fopAc_ac_c* actor = &a_this->enemy;

    switch (a_this->mode) {
    case 0:
        a_this->mode = 1;
        a_this->timers[0] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(a_this, 150), 150);
        actor->speedF = 5.0f + cM_rndF(3.0f);
        actor->current.angle.y += (s16)cM_rndFX(10000.0f);
    case 1:
    case 2:
    case 3:
    case 4:
        if (a_this->acch.ChkWallHit() && a_this->timers[1] == 0) {
            a_this->timers[1] = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(a_this, 10), 10);
            actor->speedF *= -0.5f;
        }

        if (a_this->acch.ChkGroundHit()) {
            if (a_this->mode < 4) {
                static f32 spy[] = {17.0f, 8.0f, 5.0f};
                actor->speed.y = DUSK_IF_ELSE(actor_attr::enemy_move_step(a_this, spy[a_this->mode - 1]), spy[a_this->mode - 1]);
                actor->current.angle.y += (s16)cM_rndFX(8000.0f);

                int sp28[3] = {40, 20, 10};
                Z2GetAudioMgr()->seStart(Z2SE_EN_CR_EGG_BOUND, &actor->current.pos, sp28[a_this->mode], 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                a_this->mode++;
            }

            DUSK_IF_ELSE(actor_attr::enemy_add_action_calc0(a_this, &actor->speedF, 1.0f, 0.5f + TREG_F(4)), cLib_addCalc0(&actor->speedF, 1.0f, 0.5f + TREG_F(4)));
        }

        if (a_this->timers[0] == 0 || a_this->ccSph.ChkTgHit() || a_this->ccSph.ChkAtHit()) {
            fopAcM_delete(actor);

#if TARGET_PC  // enemy attribute integration
            cXyz effscale = actor_attr::enemy_size_multiplier(a_this, 0.5f);
#else
            cXyz effscale(0.5f, 0.5f, 0.5f);
#endif
            u16 eff_id = 0x109;
            if (a_this->timers[0] == 0) {
                eff_id = 0x108;
            }
            cXyz effpos(actor->current.pos);
            dComIfGp_particle_set(eff_id, &effpos, NULL, &effscale);
            Z2GetAudioMgr()->seStart(Z2SE_EN_CR_EGG_BOMB, &actor->current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    }
}

static void action(e_cr_egg_class* a_this) {
    fopAc_ac_c* actor = &a_this->enemy;
    cXyz mae;
    cXyz ato;

    switch (a_this->action) {
    case 0:
        e_cr_egg_move(a_this);
        break;
    }

    DUSK_IF_ELSE(actor_attr::enemy_angle_add(a_this, actor->current.angle.x , (s16)(actor->speedF * (700.0f + TREG_F(9)))), actor->current.angle.x += (s16)(actor->speedF * (700.0f + TREG_F(9))));

    cMtx_YrotS(*calc_mtx, actor->current.angle.y);
    mae.x = 0.0f;
    mae.y = 0.0f;
    mae.z = DUSK_IF_ELSE(actor_attr::enemy_move_step(a_this, actor->speedF), actor->speedF);
    MtxPosition(&mae, &ato);
    actor->speed.x = ato.x;
    actor->speed.z = ato.z;
    actor->current.pos += actor->speed;
    actor->speed.y -= DUSK_IF_ELSE(actor_attr::enemy_gravity_step(a_this, 3.0f), 3.0f);

#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(a_this);
    actor->current.pos.y -= 20.0f * sizeMultiplier;
    actor->old.pos.y -= 20.0f * sizeMultiplier;
#else
    actor->current.pos.y -= 20.0f;
    actor->old.pos.y -= 20.0f;
#endif
    a_this->acch.CrrPos(dComIfG_Bgsp());
#if TARGET_PC  // enemy attribute integration
    actor->current.pos.y += 20.0f * sizeMultiplier;
    actor->old.pos.y += 20.0f * sizeMultiplier;
#else
    actor->current.pos.y += 20.0f;
    actor->old.pos.y += 20.0f;
#endif
}

static int daE_CR_EGG_Execute(e_cr_egg_class* a_this) {
    fopAc_ac_c* actor = &a_this->enemy;
#if TARGET_PC  // enemy attribute integration
    const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(a_this);
#endif
    cXyz sp2C;
    cXyz sp20;

    a_this->lifetime++;

    for (int i = 0; i < 2; i++) {
        if (a_this->timers[i] != 0) {
            a_this->timers[i]--;
        }
    }

    if (a_this->field_0x656 != 0) {
        a_this->field_0x656--;
    }

    action(a_this);

    mDoMtx_stack_c::transS(actor->current.pos.x, actor->current.pos.y, actor->current.pos.z);
    mDoMtx_stack_c::YrotM(actor->current.angle.y);
    mDoMtx_stack_c::XrotM(actor->current.angle.x);
    mDoMtx_stack_c::transM(0.0f, DUSK_IF_ELSE((TREG_F(12) - 20.0f) * sizeMultiplier, TREG_F(12) - 20.0f), 0.0f);
    
    f32 size = 1.0f + TREG_F(17);
#if TARGET_PC  // enemy attribute integration
    const f32 modelSize = size * sizeMultiplier;
    mDoMtx_stack_c::scaleM(modelSize, modelSize, modelSize);
#else
    mDoMtx_stack_c::scaleM(size, size, size);
#endif
    a_this->model->setBaseTRMtx(mDoMtx_stack_c::get());

    cXyz c_offset(0.0f, DUSK_IF_ELSE(20.0f * sizeMultiplier, 20.0f), 0.0f);
    a_this->ccSph.SetC(actor->current.pos + c_offset);
    a_this->ccSph.SetR(DUSK_IF_ELSE(size * (20.0f + BREG_F(0)) * sizeMultiplier, size * (20.0f + BREG_F(0))));
    dComIfG_Ccsp()->Set(&a_this->ccSph);
    return 1;
}

static int daE_CR_EGG_IsDelete(e_cr_egg_class* a_this) {
    return 1;
}

static int daE_CR_EGG_Delete(e_cr_egg_class* a_this) {
    fopAc_ac_c* actor = &a_this->enemy;

    fopAcM_RegisterDeleteID(a_this, "E_CR_EGG");
    dComIfG_resDelete(&a_this->phase, "E_CR");
    a_this->sound.stopAnime();
    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    e_cr_egg_class* a_this = (e_cr_egg_class*)i_this;

    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("E_CR", 0xC);
    JUT_ASSERT(374, modelData != NULL);

    a_this->model = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
    if (a_this->model == NULL) {
        return 0;
    }

    return 1;
}

static int daE_CR_EGG_Create(fopAc_ac_c* i_this) {
    e_cr_egg_class* a_this = (e_cr_egg_class*)i_this;
    fopAcM_ct(&a_this->enemy, e_cr_egg_class);

    int phase_state = dComIfG_resLoad(&a_this->phase, "E_CR");
    if (phase_state == cPhs_COMPLEATE_e) {
        OS_REPORT("E_CR_EGG PARAM %x\n", fopAcM_GetParam(i_this));
    
        a_this->field_0x5b4 = fopAcM_GetParam(i_this);
        if (a_this->field_0x5b4 == 0xFF) {
            a_this->field_0x5b4 = 0;
        }
    
        a_this->field_0x5b5 = (fopAcM_GetParam(i_this) & 0xFF00) >> 8;
        if (a_this->field_0x5b5 == 0xFF) {
            a_this->field_0x5b5 = 0;
        }
    
        OS_REPORT("E_CR_EGG//////////////E_CR_EGG SET 1 !!\n");
        if (!fopAcM_entrySolidHeap(i_this, useHeapInit, 0x820)) {
            OS_REPORT("//////////////E_CR_EGG SET NON !!\n");
            return cPhs_ERROR_e;
        }

        OS_REPORT("//////////////E_CR_EGG SET 2 !!\n");

        fopAcM_SetMtx(i_this, a_this->model->getBaseTRMtx());

        static dCcD_SrcSph cc_sph_src = {
            {
                {0x0, {{AT_TYPE_CSTATUE_SWING, 0x1, 0xd}, {0xd8fbfdff, 0x3}, 0x75}}, // mObj
                {dCcD_SE_NONE, 0x1, 0x0, 0x0, 0x0}, // mGObjAt
                {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x2}, // mGObjTg
                {0x0}, // mGObjCo
            }, // mObjInf
            {
                {{0.0f, 0.0f, 0.0f}, 20.0f} // mSph
            } // mSphAttr
        };

        a_this->ccStts.Init(50, 0, i_this);
        a_this->ccSph.Set(cc_sph_src);
        a_this->ccSph.SetStts(&a_this->ccStts);
        IF_DUSK(a_this->ccSph.SetAtAtp(actor_attr::enemy_attack_power_byte(a_this, 1.0f));)
        a_this->sound.init(&i_this->current.pos, NULL, 3, 1);

        a_this->acch.Set(fopAcM_GetPosition_p(i_this), fopAcM_GetOldPosition_p(i_this), i_this, 1, &a_this->acchcir, fopAcM_GetSpeed_p(i_this), NULL, NULL);
#if TARGET_PC  // enemy attribute integration
        const f32 sizeMultiplier = actor_attr::enemy_size_multiplier(a_this);
        a_this->acchcir.SetWall(20.0f * sizeMultiplier, 20.0f * sizeMultiplier);
#else
        a_this->acchcir.SetWall(20.0f, 20.0f);
#endif
    
        daE_CR_EGG_Execute(a_this);
    }

    return phase_state;
}

static DUSK_CONST actor_method_class l_daE_CR_EGG_Method = {
    (process_method_func)daE_CR_EGG_Create,
    (process_method_func)daE_CR_EGG_Delete,
    (process_method_func)daE_CR_EGG_Execute,
    (process_method_func)daE_CR_EGG_IsDelete,
    (process_method_func)daE_CR_EGG_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_E_CR_EGG = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 7,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_E_CR_EGG_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(e_cr_egg_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_E_CR_EGG_e,
    /* Actor SubMtd */ &l_daE_CR_EGG_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_CULL_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_0_e,
};

/**
 * d_a_obj_lv4floor.cpp
 * Moving sand-floor in Stallord Arena
 */

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_obj_lv4floor.h"
#if TARGET_PC  // additional actor attribute integration
#include "d/actor/d_a_b_ds.h"
#endif
#include "d/d_com_inf_game.h"
#include "f_pc/f_pc_name.h"
#if TARGET_PC  // additional actor attribute integration
#include "d/actor/d_a_alink.h"
#include "dusk/mods/svc/actor_attribute_helpers.hpp"

static f32 spinner_boss_room_speed_multiplier();
#endif

void daObjLv4Floor_c::initBaseMtx() {
    mpModel->setBaseScale(scale);
    setBaseMtx();
}

void daObjLv4Floor_c::setBaseMtx() {
    mDoMtx_stack_c::transS(current.pos.x, current.pos.y + mMoveYPos, current.pos.z);
    mDoMtx_stack_c::YrotM(shape_angle.y);

    mpModel->setBaseTRMtx(mDoMtx_stack_c::get());
    MTXCopy(mDoMtx_stack_c::get(), mBgMtx);
}

int daObjLv4Floor_c::Create() {
    initBaseMtx();
    return 1;
}

static DUSK_CONSTEXPR char DUSK_CONST* l_arcName = "P_L4Floor";

int daObjLv4Floor_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes(l_arcName, 4);
    mpModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000084);
    if (mpModel == NULL) {
        return 0;
    }

    return 1;
}

int daObjLv4Floor_c::create1st() {
    int phase = dComIfG_resLoad(&mPhase, l_arcName);
    if (phase != cPhs_COMPLEATE_e) {
        return phase;
    }

    if (fopAcM_isSwitch(this, getSwbit())) {
        return cPhs_ERROR_e;
    }

    phase = MoveBGCreate(l_arcName, 7, dBgS_MoveBGProc_TypicalRotY, 0x4000, NULL);
    if (phase == cPhs_ERROR_e) {
        return phase;
    }

    return phase;
}

int daObjLv4Floor_c::Execute(Mtx** param_0) {
    action();
    *param_0 = &mBgMtx;
    setBaseMtx();

    return 1;
}

void daObjLv4Floor_c::action() {
    typedef void (daObjLv4Floor_c::*actionFunc)();
    static actionFunc l_func[] = {&daObjLv4Floor_c::mode_wait, &daObjLv4Floor_c::mode_move,
                                  &daObjLv4Floor_c::mode_dead};

    (this->*l_func[mAction])();
}

void daObjLv4Floor_c::mode_wait() {
    if (fopAcM_isSwitch(this, getSwbit())) {
        mode_init_move();
    }
}

void daObjLv4Floor_c::mode_init_move() {
    speed.y = 0.0f;
    mAction = MODE_MOVE_e;
}

#if TARGET_PC  // enemy attribute integration
static void* spinner_boss_search(void* i_actor, void* i_data) {

    if (!fopAcM_IsActor(i_actor) || fopAcM_GetName(i_actor) != fpcNm_B_DS_e) {
        return NULL;
    }

    fopAc_ac_c* actor = static_cast<fopAc_ac_c*>(i_actor);
    u8 actorType = fopAcM_GetParamBit(actor, 0, 8);
    if (actorType == 0xFF) {
        actorType = daB_DS_c::TYPE_BATTLE_1;
    }

    return actorType == *static_cast<const u8*>(i_data) ? i_actor : NULL;
}

static fopAc_ac_c* spinner_find_active_boss() {

    // During the phase transition both Stallord instances briefly exist, so
    // prefer the newly created phase-two actor. Filtering by type also keeps
    // B_DS projectile actors out of the attribute lookup.
    u8 bossType = daB_DS_c::TYPE_BATTLE_2;
    fopAc_ac_c* boss = static_cast<fopAc_ac_c*>(fpcM_Search(spinner_boss_search, &bossType));

    if (boss == NULL) {
        bossType = daB_DS_c::TYPE_BATTLE_1;
        boss = static_cast<fopAc_ac_c*>(fpcM_Search(spinner_boss_search, &bossType));
    }

    return boss;
}

static f32 spinner_boss_speed_multiplier() {

    fopAc_ac_c* boss = spinner_find_active_boss();
    if (boss == NULL) {
        return 1.0f;
    }

    return dusk::mods::svc::actor_attr::resolve_multiplier(boss, ACTOR_ATTRIBUTE_MOVEMENT_SPEED);
}

static f32 spinner_boss_room_speed_multiplier() {

    return daAlink_c::checkStageName("D_MN10A") ? spinner_boss_speed_multiplier() : 1.0f;
}
#endif




void daObjLv4Floor_c::mode_move() {
#if TARGET_PC  // enemy attribute integration

    const f32 speedMultiplier = spinner_boss_room_speed_multiplier();

    const f32 targetSpeed = 3.8f * speedMultiplier;
    const f32 acceleration = 0.08f * speedMultiplier * speedMultiplier;

    cLib_chaseF(&speed.y, targetSpeed, acceleration);
#else
    cLib_chaseF(&speed.y, 3.8f, 0.08f);
#endif

    if (cLib_chaseF(&mMoveYPos, -1500.0f, speed.y)) {
        mode_init_dead();
    }
}

void daObjLv4Floor_c::mode_init_dead() {
    mAction = MODE_DEAD_e;
}

void daObjLv4Floor_c::mode_dead() {
    fopAcM_delete(this);
}

int daObjLv4Floor_c::Draw() {
    g_env_light.settingTevStruct(0x10, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(mpModel, &tevStr);

    dComIfGd_setListBG();
    mDoExt_modelUpdateDL(mpModel);
    dComIfGd_setList();
    return 1;
}

int daObjLv4Floor_c::Delete() {
    dComIfG_resDelete(&mPhase, l_arcName);
    return 1;
}

static int daObjLv4Floor_create1st(daObjLv4Floor_c* i_this) {
    fopAcM_ct(i_this, daObjLv4Floor_c);
    return i_this->create1st();
}

static int daObjLv4Floor_MoveBGDelete(daObjLv4Floor_c* i_this) {
    return i_this->MoveBGDelete();
}

static int daObjLv4Floor_MoveBGExecute(daObjLv4Floor_c* i_this) {
    return i_this->MoveBGExecute();
}

static int daObjLv4Floor_MoveBGDraw(daObjLv4Floor_c* i_this) {
    return i_this->MoveBGDraw();
}

static DUSK_CONST actor_method_class daObjLv4Floor_METHODS = {
    (process_method_func)daObjLv4Floor_create1st,
    (process_method_func)daObjLv4Floor_MoveBGDelete,
    (process_method_func)daObjLv4Floor_MoveBGExecute,
    (process_method_func)NULL,
    (process_method_func)daObjLv4Floor_MoveBGDraw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_Obj_Lv4Floor = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 3,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_Obj_Lv4Floor_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daObjLv4Floor_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_Obj_Lv4Floor_e,
    /* Actor SubMtd */ &daObjLv4Floor_METHODS,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

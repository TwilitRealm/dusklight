/**
 * @file d_a_obj_gb.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#include "d/actor/d_a_obj_gb.h"
#include "SSystem/SComponent/c_lib.h"
#include "SSystem/SComponent/c_math.h"
#include "d/d_bg_w.h"
#include "d/d_com_inf_game.h"
#if TARGET_PC  // additional actor attribute integration
#include "JSystem/J3DGraphBase/J3DShape.h"
#include "d/actor/d_a_b_gnd.h"
#include "d/d_bg_s_gnd_chk.h"
#include "dusk/mods/svc/actor_attribute_helpers.hpp"
#include "f_op/f_op_actor_mng.h"
#include <algorithm>
#endif
#include <cstring>
#if TARGET_PC  // additional actor attribute integration

namespace actor_attr = dusk::mods::svc::actor_attr;

static void adjustFinalBattleBarrierGround(obj_gb_class* i_this) {
    if (fopAcM_GetParam(i_this) != 0xF0069600 || i_this->scale.x == 1.5f || i_this->scale.x <= 0.0f) {
        return;
    }

    J3DModelData* modelData = i_this->mModel->getModelData();
    if (modelData->getShapeNum() == 0 || i_this->scale.y <= 0.0f) {
        return;
    }
    f32 model_min_y = modelData->getShapeNodePointer(0)->getMin()->y;
    f32 model_max_y = modelData->getShapeNodePointer(0)->getMax()->y;
    for (u16 i = 1; i < modelData->getShapeNum(); i++) {
        model_min_y = std::min(model_min_y, modelData->getShapeNodePointer(i)->getMin()->y);
        model_max_y = std::max(model_max_y, modelData->getShapeNodePointer(i)->getMax()->y);
    }
    if (model_max_y <= model_min_y) {
        return;
    }

    const f32 barrier_bottom_y = i_this->current.pos.y + model_min_y * i_this->scale.y;
    const f32 barrier_top_y = i_this->current.pos.y + model_max_y * i_this->scale.y;
    f32 lowest_ground_y = barrier_bottom_y;
    dBgS_ObjGndChk ground_chk;
    ground_chk.SetActorPid(fopAcM_GetID(i_this));
    // Obj_gb's ring and its sound positions use a local radius of 1000 units.
    // Check both sides of the wall so its resized base reaches the hillside.
    const f32 barrier_radius = i_this->scale.x * 1000.0f;
    const int ground_samples = 128;
    for (int i = 0; i < ground_samples; i++) {
        const s16 angle = i_this->current.angle.y + i * (0x10000 / ground_samples);
        for (int j = 0; j < 3; j++) {
            const f32 radius = barrier_radius * (0.99f + j * 0.01f);
            cXyz ground_pos(i_this->current.pos.x + cM_ssin(angle) * radius, std::max(i_this->current.pos.y, barrier_top_y) + 1000.0f, i_this->current.pos.z + cM_scos(angle) * radius);
            ground_chk.SetPos(&ground_pos);
            const f32 ground_y = dComIfG_Bgsp().GroundCross(&ground_chk);
            if (dComIfG_Bgsp().ChkPolySafe(ground_chk) && ground_y < lowest_ground_y) {
                lowest_ground_y = ground_y;
            }
        }
    }
    if (lowest_ground_y >= barrier_bottom_y) {
        return;
    }

    // Keep the authored top in place and extend the bottom slightly into the
    // lowest sampled ground. Execute uses this same transform for the model
    // and background collision, so Link cannot walk under a floating wall.
    i_this->scale.y = (barrier_top_y - (lowest_ground_y - 50.0f)) / (model_max_y - model_min_y);
    i_this->current.pos.y = barrier_top_y - model_max_y * i_this->scale.y;
    i_this->old.pos.y = i_this->current.pos.y;
}

static void adjustFinalBattleGanondorfPlacement(obj_gb_class* i_this) {
    // field_0x684 marks a pending one-time boss placement. The ring itself
    // is positioned in Create, before its background collision is registered.
    if (fopAcM_GetParam(i_this) != 0xF0069600 || i_this->field_0x684 == 0.0f) {
        return;
    }

    fopAc_ac_c* actor = NULL;
    if (!fopAcM_SearchByName(fpcNm_B_GND_e, &actor) || actor == NULL) {
        return;
    }
    b_gnd_class* ganondorf = static_cast<b_gnd_class*>(actor);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    const s16 demo_mode = ganondorf->mDemoCamMode;
    // The full intro has finished walking at 45/46; retry setup is fixed at
    // 96. Skip mode 92 still resets both actors before handing back control.
    if (player == NULL || ganondorf->checkRide() || ganondorf->mNoDrawTimer != 0 || (demo_mode != 45 && demo_mode != 46 && demo_mode != 96 && (demo_mode != 0 || dComIfGp_event_runCheck()))) {
        return;
    }

    const f32 size = i_this->scale.x / 1.5f;
    const f32 start_distance = 1200.0f * size;
    ganondorf->current.pos.x = player->current.pos.x + cM_ssin(player->shape_angle.y) * start_distance;
    ganondorf->current.pos.z = player->current.pos.z + cM_scos(player->shape_angle.y) * start_distance;
    ganondorf->current.angle.y = ganondorf->shape_angle.y = static_cast<s16>(player->shape_angle.y + 0x8000);

    dBgS_ObjGndChk ground_chk;
    ground_chk.SetActorPid(fopAcM_GetID(ganondorf));
    cXyz ground_pos = ganondorf->current.pos;
    ground_pos.y += 1000.0f;
    ground_chk.SetPos(&ground_pos);
    const f32 ground_y = dComIfG_Bgsp().GroundCross(&ground_chk);
    if (dComIfG_Bgsp().ChkPolySafe(ground_chk)) {
        ganondorf->current.pos.y = ground_y;
    }
    ganondorf->old.pos = ganondorf->current.pos;
    i_this->field_0x684 = 0.0f;
}

static void adjustFinalBattleBarrierPlacement(obj_gb_class* i_this) {
    if (fopAcM_GetParam(i_this) != 0xF0069600 || i_this->field_0x684 == 0.0f) {
        return;
    }

    fopAc_ac_c* actor = NULL;
    if (!fopAcM_SearchByName(fpcNm_B_GND_e, &actor) || actor == NULL) {
        return;
    }
    b_gnd_class* ganondorf = static_cast<b_gnd_class*>(actor);
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    if (player == NULL) {
        return;
    }

    cXyz link_start = player->current.pos;
    s16 link_angle = player->shape_angle.y;
    if (ganondorf->mDemoCamMode == 92) {
        // This skip path creates the barrier before it installs these authored
        // fight positions. Use that upcoming Link placement for the new ring.
        link_start.set(600.0f, 1100.0f, 0.0f);
        link_angle = -0x4802;
    }

    const f32 size = i_this->scale.x / 1.5f;
    const f32 center_distance = 600.0f * size;
    i_this->current.pos.x = link_start.x + cM_ssin(link_angle) * center_distance;
    i_this->current.pos.z = link_start.z + cM_scos(link_angle) * center_distance;
    i_this->old.pos = i_this->current.pos;
    adjustFinalBattleGanondorfPlacement(i_this);
}
#endif

static int daObj_Gb_Draw(obj_gb_class* i_this) {
    g_env_light.settingTevStruct(0x10, &i_this->current.pos, &i_this->tevStr);
    g_env_light.setLightTevColorType_MAJI(i_this->mModel, &i_this->tevStr);
    J3DModelData* modelData = (J3DModelData*)i_this->mModel->getModelData();
    for (u16 i = 0; i <= 1; i++) {
        J3DMaterial* material = (J3DMaterial*)modelData->getMaterialNodePointer(i);
        material->getTevKColor(1)->a = i_this->mColorAlpha;
    }

    dKy_bg_MAxx_proc(i_this->mModel);
    i_this->mBtk->entry(modelData);
    i_this->mBrk->entry(modelData);
    mDoExt_modelUpdateDL(i_this->mModel);
    return 1;
}

static int daObj_Gb_Execute(obj_gb_class* i_this) {
    IF_DUSK(adjustFinalBattleGanondorfPlacement(i_this);)
    fopAc_ac_c* player = dComIfGp_getPlayer(0);
    cXyz acStack_30;
    cXyz cStack_3c;
    i_this->field_0x58e++;
    for (int i = 0; i < 2; i++) {
        if (i_this->field_0x58a[i] != 0) {
            i_this->field_0x58a[i]--;
        }
    }
    switch(i_this->mIsFinalBattle) {
    case 0:
        if (i_this->field_0x57c == 0) {
            for (int i = 0; i < 20; i++) {
                if (i_this->mBrkFrame < 0.5f) {
                    Z2GetAudioMgr()->seStart(Z2SE_OBJ_GANON_BARRIER_APPR, &i_this->field_0x594[i],
                                             0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
                }
                Z2GetAudioMgr()->seStartLevel(Z2SE_OBJ_GANON_BARRIER, &i_this->field_0x594[i], 0, 0,
                                              1.0f, 1.0f, -1.0f, -1.0f, 0);
            }
        } else {
            if (i_this->mBrkFrame < 0.5f) {
                Z2GetAudioMgr()->seStart(Z2SE_OBJ_GANON_BARRIER_APPR, &i_this->current.pos, 0, 0,
                                         1.0f, 1.0f, -1.0f, -1.0f, 0);
            }
            Z2GetAudioMgr()->seStartLevel(Z2SE_OBJ_GANON_BARRIER, &i_this->current.pos, 0, 0, 1.0f,
                                          1.0f, -1.0f, -1.0f, 0);
        }
        cLib_addCalc2(&i_this->mBrkFrame, 29.0f, 1.0f, 1.0f);
        if (strcmp(dComIfGp_getStartStageName(), "D_MN09B") != 0) {
            if (dComIfGs_isSwitch(i_this->mSw2, fopAcM_GetRoomNo(i_this)) ||
                !dComIfGs_isSwitch(i_this->mSw1, fopAcM_GetRoomNo(i_this)))
            {
                i_this->mIsFinalBattle = 1;
            }
        }
        break;
    case 1:
        cLib_addCalc2(&i_this->mBrkFrame, 0, 1.0f, 1.0f);
        if (!dComIfGs_isSwitch(i_this->mSw2, fopAcM_GetRoomNo(i_this)) &&
            dComIfGs_isSwitch(i_this->mSw1, fopAcM_GetRoomNo(i_this)))
        {
            i_this->mIsFinalBattle = 0;
        }
        break;
    }
    mDoMtx_stack_c::transS(i_this->current.pos.x, i_this->current.pos.y, i_this->current.pos.z);
    mDoMtx_stack_c::YrotM(i_this->current.angle.y);
    if (i_this->field_0x57c == 0) {
        mDoMtx_stack_c::scaleM(i_this->scale.x, i_this->scale.y, i_this->scale.x);
    } else {
        mDoMtx_stack_c::scaleM(i_this->scale.x, i_this->scale.y, 1.0f);
    }
    i_this->mModel->setBaseTRMtx(mDoMtx_stack_c::get());
    i_this->mBtk->play();
    i_this->mBrk->setFrame(i_this->mBrkFrame);
    if (i_this->mBrkFrame < 1.0f) {
        mDoMtx_stack_c::transM(0.0f, -200.0f, 0.0f);
        mDoMtx_stack_c::scaleM(1.0f, 0.0f, 1.0f);
    }
    MTXCopy(mDoMtx_stack_c::get(), i_this->mBgMtx);
    i_this->mpBgW->Move();
    return 1;
}

static int daObj_Gb_IsDelete(obj_gb_class* param_0) {
    return 1;
}

static int daObj_Gb_Delete(obj_gb_class* i_this) {
    fopAcM_GetID(i_this);
    dComIfG_resDelete(&i_this->mPhase, "Obj_gb");
    if (i_this->mpBgW != NULL) {
        dComIfG_Bgsp().Release(i_this->mpBgW);
    }
    return 1;
}

static int bmd[2] = {
    6, 7,
};

#ifdef TARGET_PC
static int brk_res[2] = {
#else
static int brk[2] = {
#endif
    10, 11,
};

static int btk[2] = {
    14, 15,
};

static int dzb[2] = {
    18, 19,
};

static int useHeapInit(fopAc_ac_c* actor) {
    obj_gb_class* i_this = (obj_gb_class*)actor;
    J3DModelData* modelData = (J3DModelData*) dComIfG_getObjectRes("Obj_gb", bmd[i_this->field_0x57c]);
    JUT_ASSERT(324, modelData != NULL);
    i_this->mModel = mDoExt_J3DModel__create(modelData, 0x80000, 0x11000284);
    if (i_this->mModel == NULL) {
        return 0;
    }
    i_this->mBtk = JKR_NEW mDoExt_btkAnm();
    if (i_this->mBtk == NULL) {
        return 0;
    }
    J3DAnmTextureSRTKey* anmTexture = (J3DAnmTextureSRTKey*) dComIfG_getObjectRes(
        "Obj_gb", btk[i_this->field_0x57c]);
    if (i_this->mBtk->init(i_this->mModel->getModelData(), anmTexture, 1, 2, 1.0f, 0, -1) == 0) {
        return 0;
    }
    i_this->mBrk = JKR_NEW mDoExt_brkAnm();
    if (i_this->mBrk== NULL) {
        return 0;
    }
    J3DAnmTevRegKey* anmTevKey = (J3DAnmTevRegKey*)dComIfG_getObjectRes(
#ifdef TARGET_PC
        "Obj_gb", brk_res[i_this->field_0x57c]);
#else
        "Obj_gb", brk[i_this->field_0x57c]);
#endif
    if (i_this->mBrk->init(i_this->mModel->getModelData(), anmTevKey, 1, 2, 0.0f, 0, -1) == 0) {
        return 0;
    }
    i_this->mpBgW = JKR_NEW dBgW();
    if (i_this->mpBgW == NULL) {
        return 0;
    }
    cBgD_t* pbGd = (cBgD_t*)dComIfG_getObjectRes(
        "Obj_gb", dzb[i_this->field_0x57c]);
    return i_this->mpBgW->Set(pbGd, 1, &i_this->mBgMtx) == 1 ? 0 : 1;
}

static int daObj_Gb_Create(fopAc_ac_c* actor) {
    fopAcM_ct(actor, obj_gb_class);
    obj_gb_class* i_this = (obj_gb_class*)actor;
    int rv = dComIfG_resLoad(&i_this->mPhase, "Obj_gb");
    
    if (rv == cPhs_COMPLEATE_e) {
        OS_REPORT("OBJ_GB PARAM %x\n", fopAcM_GetParam(i_this));
        i_this->field_0x57c = fopAcM_GetParam(i_this);
        if (i_this->field_0x57c == 0xff) {
            i_this->field_0x57c = 0;
        }
        u8 local_47 = (fopAcM_GetParam(i_this) >> 8) & 0xff;
        if (local_47 == 0xff) {
            if (i_this->field_0x57c == 0) {
                local_47 = 100;
            } else {
                local_47 = 10;
            }
        }
        if (i_this->field_0x57c == 0) {
            i_this->scale.x = local_47 * 0.01f;
#if TARGET_PC  // enemy attribute integration

            // The final Ganondorf barrier is spawned as a separate Obj_gb actor,
            // so inherit Ganondorf's randomized size explicitly.  Keep the
            // barrier's encoded vanilla 1.5x ring scale as the baseline and
            // scale its horizontal radius (X/Z). Place the ring about Link's
            // start position before registering collision, and put Ganondorf
            // ahead of Link once the script has finished placing the actors.
            // Ground fitting keeps the authored top and reaches the terrain.
            if (fopAcM_GetParam(i_this) == 0xF0069600) {
                i_this->field_0x684 = 0.0f;
                fopAc_ac_c* ganondorf = NULL;
                if (fopAcM_SearchByName(fpcNm_B_GND_e, &ganondorf) && ganondorf != NULL) {
                    const f32 size = actor_attr::enemy_size_multiplier(ganondorf);
                    i_this->scale.x *= size;
                    if (size != 1.0f) {
                        i_this->field_0x684 = 1.0f;
                    }
                }
            }
#endif
        } else {
            i_this->scale.x = local_47 * 0.5f;
        }
        i_this->scale.y = ((fopAcM_GetParam(i_this) >> 16) & 0xff) * 0.333333f * 0.5f;
        i_this->mColorAlpha = fopAcM_GetParam(i_this) >> 24;
        i_this->mSw1 = i_this->current.angle.x & 0xff;
        i_this->mSw2 = (i_this->current.angle.x & 0xff00) >> 8;
        i_this->shape_angle.x = 0;
        i_this->current.angle.x = 0;
        OS_REPORT("OBJ_GB//////////////OBJ_GB SET 1 !!\n");
        int heapSize;
        if (i_this->field_0x57c == 0) {
            heapSize = 0x1c40;
        } else {
            heapSize = 0x1320;
        }
        if (fopAcM_entrySolidHeap(i_this, useHeapInit, heapSize) == 0) {
            OS_REPORT("//////////////OBJ_GB SET NON !!\n");
            return cPhs_ERROR_e;
        }
        OS_REPORT("//////////////OBJ_GB SET 2 !!\n");
#if TARGET_PC  // enemy attribute integration
        adjustFinalBattleBarrierPlacement(i_this);
        adjustFinalBattleBarrierGround(i_this);
#endif
        if (i_this->mpBgW != NULL) {
            if (dComIfG_Bgsp().Regist(i_this->mpBgW, i_this) != 0) {
                return cPhs_ERROR_e;
            }
        }
        i_this->field_0x58e = cM_rndF(65536.0f);
        if (strcmp(dComIfGp_getStartStageName(), "D_MN09B") != 0) {
            i_this->mIsFinalBattle = 1;
        }
        s16 local_44 = 0;
        cXyz cStack_30(0.0f, 0.0f, i_this->scale.x * 1000.0f);
        for (int i = 0; i < 20; i++, local_44 += 0xccc) {
            cMtx_YrotS(*calc_mtx, local_44);
            MtxPosition(&cStack_30, &i_this->field_0x594[i]);
            i_this->field_0x594[i] += i_this->current.pos;
        }
        daObj_Gb_Execute(i_this);
    }
    return rv;
}

static DUSK_CONST actor_method_class l_daObj_Gb_Method = {
    (process_method_func)daObj_Gb_Create,
    (process_method_func)daObj_Gb_Delete,
    (process_method_func)daObj_Gb_Execute,
    (process_method_func)daObj_Gb_IsDelete,
    (process_method_func)daObj_Gb_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_OBJ_GB = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 3,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_OBJ_GB_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(obj_gb_class),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_OBJ_GB_e,
    /* Actor SubMtd */ &l_daObj_Gb_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e | fopAcStts_UNK_0x4000_e,
    /* Group        */ fopAc_ACTOR_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

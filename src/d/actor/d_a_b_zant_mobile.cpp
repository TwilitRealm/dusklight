/**
 * @file d_a_b_zant_mobile.cpp
 * 
*/

#include "d/dolzel_rel.h" // IWYU pragma: keep

#if TARGET_PC  // additional actor attribute integration
#include <algorithm>
#include <cmath>
#include "SSystem/SComponent/c_math.h"
#include "d/actor/d_a_b_zant.h"
#endif
#include "d/actor/d_a_b_zant_mobile.h"
#if TARGET_PC  // additional actor attribute integration
#include "d/actor/d_a_player.h"
#include "d/d_cc_uty.h"
#else
#include "d/actor/d_a_b_zant.h"
#endif
#include "d/d_com_inf_game.h"
#if TARGET_PC  // additional actor attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"

namespace actor_attr = dusk::mods::svc::actor_attr;

namespace dusk::mods::svc::actor_attr {
template <>
struct EnemyAttributeOwner<daB_ZANTZ_c> {
    static fopAc_ac_c* get(daB_ZANTZ_c* i_this) {

        fopAc_ac_c* parent = fopAcM_SearchByID(i_this->parentActorID);
        return parent != NULL && fopAcM_GetName(parent) == fpcNm_B_ZANT_e ? parent : i_this;

}
};
}  // namespace dusk::mods::svc::actor_attr
#else
#include "d/d_cc_uty.h"
#include "d/actor/d_a_player.h"
#include "SSystem/SComponent/c_math.h"
#include <cmath>
#endif

namespace {
dCcD_SrcSph cc_zant_src = {
    {
        {0x0, {{0x0, 0x1, 0x0}, {0xD8FBFDFF, 0x3}, 0x55}}, // mObj
        {dCcD_SE_METAL, 0x0, 0x1, 0x0, 0x0}, // mGObjAt
        {dCcD_SE_NONE, 0x0, 0x0, 0x0, 0x303}, // mGObjTg
        {0x0}, // mGObjCo
    }, // mObjInf
    {
        {{0.0f, 0.0f, 0.0f}, 40.0f} // mSph
    } // mSphAttr
};

static dCcD_SrcCyl cc_zant_snort_src = {
    {
        {0, {{0x400, 0, 0}, {0xD8FBFDFF, 0}, 0x55}},
        {dCcD_SE_METAL, 0, 1, 0, {0}},
        {dCcD_SE_NONE, 0, 0, 0, {2}},
        {0},
    },
    {
        {
            {0.0f, 0.0f, 0.0f},
            40.0f,
            40.0f,
        },
    }
};
}

daB_ZANTZ_HIO_c::daB_ZANTZ_HIO_c() {
    field_0x4 = -1;
    mModelSize = 1.0f;
}

int daB_ZANTZ_c::draw() {
    J3DModel* model = mpMorf->getModel();
    g_env_light.settingTevStruct(0, &current.pos, &tevStr);
    g_env_light.setLightTevColorType_MAJI(model, &tevStr);
    mpBrk->entry(model->getModelData());
    mpMorf->entryDL();
    return 1;
}

static int daB_ZANTZ_Draw(daB_ZANTZ_c* i_this) {
    return i_this->draw();
}

void daB_ZANTZ_c::setSnortEffect(int param_0) {
    if (field_0x668 == 0) {
        Z2GetAudioMgr()->seStart(Z2SE_EN_ZZ_HANAIKI, &current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
    }

    field_0x668 = DUSK_IF_ELSE(actor_attr::enemy_sync_timer(this, param_0), param_0);
}

void daB_ZANTZ_c::calcSnortEffect() {
    cXyz sp28;
    cXyz sp34;
#if !TARGET_PC  // enemy attribute integration
    cXyz sp40(5.0f, 1.5f, 5.0f);
#endif
    csXyz sp48;

#if TARGET_PC  // enemy attribute integration
    const f32 size_mul = actor_attr::enemy_size_multiplier(this);
    cXyz particle_scale(size_mul, size_mul, size_mul);

    // The snort collider dimensions were already size-scaled, but its center
    // offsets were still vanilla. Scale the entire mouth-relative volume so it
    // stays attached to the same place on an enlarged/shrunken statue head.
    sp34.set(150.0f * size_mul, 800.0f * size_mul, 1000.0f * size_mul);
#else
    sp34.set(150.0f, 800.0f, 1000.0f);
#endif
    cLib_offsetPos(&sp28, &current.pos, shape_angle.y, &sp34);
    sp48.set(0, 0, 0x7C00);

#if TARGET_PC  // enemy attribute integration
    // Scale the visible breath emitter too. Previously NULL left the particle
    // effect at its vanilla visual size even when the damaging volume grew.
    field_0xd04 = dComIfGp_particle_set(field_0xd04, 0x8908, &current.pos, &tevStr, &shape_angle, &particle_scale, 0xFF, NULL, -1, NULL, NULL, NULL);
    sp28.y -= 900.0f * size_mul;
#else
    field_0xd04 = dComIfGp_particle_set(field_0xd04, 0x8908, &current.pos, &tevStr, &shape_angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
    sp28.y -= 900.0f;
#endif

    mSnortColliders[0].SetC(sp28);
#if TARGET_PC  // enemy attribute integration
    mSnortColliders[0].SetR(300.0f * size_mul);
    mSnortColliders[0].SetH(900.0f * size_mul);
#else
    mSnortColliders[0].SetR(300.0f);
    mSnortColliders[0].SetH(900.0f);
#endif
    dComIfG_Ccsp()->Set(&mSnortColliders[0]);

#if TARGET_PC  // enemy attribute integration
    sp34.set(-150.0f * size_mul, 800.0f * size_mul, 1000.0f * size_mul);
#else
    sp34.set(-150.0f, 800.0f, 1000.0f);
#endif
    cLib_offsetPos(&sp28, &current.pos, shape_angle.y, &sp34);
    sp48.set(0, 0, -0x7C00);

#if TARGET_PC  // enemy attribute integration
    field_0xd08 = dComIfGp_particle_set(field_0xd08, 0x8909, &current.pos, &tevStr, &shape_angle, &particle_scale, 0xFF, NULL, -1, NULL, NULL, NULL);
    sp28.y -= 900.0f * size_mul;
#else
    field_0xd08 = dComIfGp_particle_set(field_0xd08, 0x8909, &current.pos, &tevStr, &shape_angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
    sp28.y -= 900.0f;
#endif

    mSnortColliders[1].SetC(sp28);
#if TARGET_PC  // enemy attribute integration
    mSnortColliders[1].SetR(300.0f * size_mul);
    mSnortColliders[1].SetH(900.0f * size_mul);
#else
    mSnortColliders[1].SetR(300.0f);
    mSnortColliders[1].SetH(900.0f);
#endif
    dComIfG_Ccsp()->Set(&mSnortColliders[1]);

    cCcD_Obj* var_r28 = NULL;
    if (mSnortColliders[0].ChkCoHit()) {
        var_r28 = mSnortColliders[0].GetCoHitObj();
    }

    if (mSnortColliders[1].ChkCoHit()) {
        var_r28 = mSnortColliders[1].GetCoHitObj();
    }

    if (var_r28 != NULL) {
        if (fopAcM_GetName(dCc_GetAc(var_r28->GetAc())) == fpcNm_ALINK_e) {
            daPy_py_c* player = daPy_getPlayerActorClass();
            s16 var_r26 = cLib_targetAngleY(&current.pos, &player->current.pos);
            player->setThrowDamage(var_r26, 30.0f, 10.0f, 0, 0, 2);
        }
    }
}

void daB_ZANTZ_c::setMouthMode(u8 param_0) {
    if (param_0 == 2) {
        if (mMouthMode == 0 || mMouthMode == 1) {
            mMouthMode = 2;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_CLOSE), J3DFrameCtrl::EMode_NONE, 0.0f, 1.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_CLOSE), J3DFrameCtrl::EMode_NONE, 0.0f, 1.0f, 0.0f, -1.0f));
            Z2GetAudioMgr()->seStart(Z2SE_EN_ZZ_CLOSE, &current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    } else if (param_0 == 0) {
        if (mMouthMode == 2 || mMouthMode == 3) {
            mMouthMode = 0;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_NONE, 0.0f, 1.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_NONE, 0.0f, 1.0f, 0.0f, -1.0f));
            Z2GetAudioMgr()->seStart(Z2SE_EN_ZZ_OPEN, &current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
    }
}

void daB_ZANTZ_c::setAppearMode(u8 param_0) {
    cXyz sp34(0.0f, 0.0f, 2000.0f);

    if (param_0 == 2) {
        if (mAppearMode == 0 || mAppearMode == 1) {
            mAppearMode = 2;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f));
            field_0x660 = 1.0f;
            dComIfGp_getVibration().StartQuake(1, 31, cXyz(0.0f, 1.0f, 0.0f));
        }
    } else if (param_0 == 0) {
        if (mAppearMode == 2 || mAppearMode == 3) {
            mAppearMode = 0;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f));
            field_0x660 = 1.0f;
            dComIfGp_getVibration().StartQuake(1, 31, cXyz(0.0f, 1.0f, 0.0f));

            if (field_0x66f == 0) {
                field_0x66f = 1;
                current.pos.set(sp34.x, current.pos.y, sp34.z);
                current.angle.y = shape_angle.y = fopAcM_searchPlayerAngleY(this);
            } else {
                s16 sp68 = cLib_targetAngleY(&daPy_getPlayerActorClass()->current.pos, &sp34);
                s16 var_r28 = sp68 + 0x2000 + (field_0x670 * 0x4000);

#if TARGET_PC  // enemy attribute integration
                // Scale the four-head ring by only half of the randomized size delta.
                // 100% size stays at the vanilla 3000 radius, 400% size uses 250%
                // radius, and 25% size uses 62.5% radius.
                const f32 size_mul = actor_attr::enemy_size_multiplier(this);
                const f32 formation_radius = 3000.0f * (1.0f + (size_mul - 1.0f) * 0.25f);
                current.pos.set(sp34.x + formation_radius * cM_ssin(var_r28), current.pos.y, sp34.z + formation_radius * cM_scos(var_r28));
#else
                current.pos.set(sp34.x + 3000.0f * cM_ssin(var_r28), current.pos.y, sp34.z + 3000.0f * cM_scos(var_r28));
#endif
                current.angle.y = shape_angle.y = cLib_targetAngleY(&current.pos, &sp34);
            }

            field_0x664 = getGroundPos();
        }
    } else if (param_0 == 4) {
        if (mAppearMode == 1) {
            if (abs((s16)(fopAcM_searchPlayerAngleY(this) - shape_angle.y)) > 0x1800) {
                mAppearMode = 4;
                DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f));
                field_0x660 = 1.0f;
                dComIfGp_getVibration().StartQuake(1, 31, cXyz(0.0f, 1.0f, 0.0f));
            }
        }
    } else if (param_0 == 5) {
        if (mAppearMode == 1) {
            mAppearMode = 5;
            mSearchContinue = true;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_APPEARANCE), J3DFrameCtrl::EMode_LOOP, 0.0f, 1.0f, 0.0f, -1.0f));
            field_0x660 = 1.0f;
            dComIfGp_getVibration().StartQuake(1, 31, cXyz(0.0f, 1.0f, 0.0f));
        }
    }
}

f32 daB_ZANTZ_c::getGroundPos() {
    cXyz sp88(current.pos);
    dBgS_ObjGndChk gndchk;
    sp88.y += 4000.0f;

    while (1) {
        gndchk.SetPos(&sp88);
        sp88.y = dComIfG_Bgsp().GroundCross(&gndchk);

        if (sp88.y == -G_CM3D_F_INF) {
            break;
        }

        if (dComIfG_Bgsp().GetActorPointer(gndchk) != this) {
            break;
        }

        sp88.y -= 10.0f;
    }

    return sp88.y;
}

void daB_ZANTZ_c::action() {
    if (mMouthMode == 0) {
        if (mpMorf->checkFrame(4.0f) || mpMorf->checkFrame(10.0f) || mpMorf->checkFrame(15.0f)) {
            dComIfGp_getVibration().StartShock(1, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (mpMorf->checkFrame(40.0f)) {
            dComIfGp_getVibration().StartShock(3, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (mpMorf->isStop()) {
            mMouthMode++;
        }
    } else if (mMouthMode == 2) {
        if (mpMorf->checkFrame(6.0f) || mpMorf->checkFrame(9.0f) || mpMorf->checkFrame(13.0f) || mpMorf->checkFrame(16.0f)) {
            dComIfGp_getVibration().StartShock(1, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (mpMorf->checkFrame(26.0f)) {
            dComIfGp_getVibration().StartShock(3, 31, cXyz(0.0f, 1.0f, 0.0f));
        }

        if (mpMorf->isStop()) {
            mMouthMode++;
        }
    }

    cXyz particle_pos(current.pos.x, field_0x664, current.pos.z);
    
    switch (mAppearMode) {
    case 0: {
#if TARGET_PC  // enemy attribute integration
        // The hidden-to-emerged travel distance scales with model size. Match the
        // positional step to size as well so a 400% head does not take 4x as long
        // to rise back into the arena. Movement/action speed is still applied by
        // enemy_add_action_calc().
        const f32 size_mul = actor_attr::enemy_size_multiplier(this);
        f32 var_f31 = actor_attr::enemy_add_action_calc(this, &current.pos.y, home.pos.y + 3500.0f, 0.1f, 15.0f * size_mul, 1.0f * size_mul);
#else
        f32 var_f31 = cLib_addCalc(&current.pos.y, home.pos.y + 3500.0f, 0.1f, 15.0f, 1.0f);
#endif
        if (std::abs(var_f31) < 15.0f) {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x660, 0.0f, 0.2f), cLib_chaseF(&field_0x660, 0.0f, 0.2f));
            mpMorf->setFrame(field_0x660);
        }

        if (current.pos.y > home.pos.y + 300.0f) {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x664, 0.0f, 1.7f), cLib_chaseF(&field_0x664, 0.0f, 1.7f));

            field_0xd0c = dComIfGp_particle_set(field_0xd0c, 0x8906, &particle_pos, &tevStr, &shape_angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
            Z2GetAudioMgr()->seStartLevel(Z2SE_EN_ZZ_MV, &current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }

        if (!var_f31) {
            dComIfGp_getVibration().StopQuake(0x1F);
            mAppearMode++;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_LOOP, 0.0f, 0.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_LOOP, 0.0f, 0.0f, 0.0f, -1.0f));
        }
        break;
    }
    case 2: {
#if TARGET_PC  // enemy attribute integration
        // home.pos.y is the vanilla buried position.  The visible/emerged target
        // remains vanilla, but the buried depth must grow with model size or a
        // large statue head remains exposed above the floor.
        const f32 size_mul = actor_attr::enemy_size_multiplier(this);
        const f32 buried_y = (home.pos.y + 3500.0f) - (3500.0f * size_mul);
        f32 var_f31 = actor_attr::enemy_add_action_calc(this, &current.pos.y, buried_y, 0.1f, 15.0f * size_mul, 1.0f * size_mul);
#else
        f32 var_f31 = cLib_addCalc(&current.pos.y, home.pos.y, 0.1f, 15.0f, 1.0f);
#endif
        if (std::abs(var_f31) < 15.0f) {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x660, 0.0f, 0.2f), cLib_chaseF(&field_0x660, 0.0f, 0.2f));
            mpMorf->setFrame(field_0x660);
        }

        if (current.pos.y > home.pos.y + 300.0f) {
            if (current.pos.y < home.pos.y + 500.0f) {
                DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x664, getGroundPos(), 1.7f), cLib_chaseF(&field_0x664, getGroundPos(), 1.7f));
            }

            field_0xd0c = dComIfGp_particle_set(field_0xd0c, 0x8907, &particle_pos, &tevStr, &shape_angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
            Z2GetAudioMgr()->seStartLevel(Z2SE_EN_ZZ_MV, &current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }

        if (!var_f31) {
            dComIfGp_getVibration().StopQuake(0x1F);
            mAppearMode++;
            DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_LOOP, 0.0f, 0.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_LOOP, 0.0f, 0.0f, 0.0f, -1.0f));
        }
        break;
    }
    case 3: {
        current.angle.y = shape_angle.y = fopAcM_searchPlayerAngleY(this);
        break;
    }
    case 4:
    case 5: {
        DUSK_IF_ELSE(actor_attr::enemy_add_action_angle(this, &shape_angle.y, fopAcM_searchPlayerAngleY(this), 0x10, 0x80), cLib_addCalcAngleS2(&shape_angle.y, fopAcM_searchPlayerAngleY(this), 0x10, 0x80));
        current.angle.y = shape_angle.y;

        field_0xd0c = dComIfGp_particle_set(field_0xd0c, 0x890A, &particle_pos, &tevStr, &shape_angle, NULL, 0xFF, NULL, -1, NULL, NULL, NULL);
        Z2GetAudioMgr()->seStartLevel(Z2SE_EN_ZZ_MV, &current.pos, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);

        BOOL sp6C = false;
        if (mAppearMode == 4) {
            if (abs((s16)(shape_angle.y - fopAcM_searchPlayerAngleY(this))) < 0x1000) {
                sp6C = true;
            }
        } else if (!mSearchContinue) {
            sp6C = true;
        }

        if (sp6C) {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x660, 0.0f, 0.1f), cLib_chaseF(&field_0x660, 0.0f, 0.1f));
            mpMorf->setFrame(field_0x660);

            if (!field_0x660) {
                dComIfGp_getVibration().StopQuake(0x1F);
                mAppearMode = 1;
                DUSK_IF_ELSE(actor_attr::enemy_set_animation(this, mpMorf, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_LOOP, 0.0f, 0.0f, 0.0f, -1.0f), mpMorf->setAnm((J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), J3DFrameCtrl::EMode_LOOP, 0.0f, 0.0f, 0.0f, -1.0f));
            }
        } else {
            DUSK_IF_ELSE(actor_attr::enemy_chase_action_float(this, &field_0x660, 1.0f, 0.2f), cLib_chaseF(&field_0x660, 1.0f, 0.2f));
            mpMorf->setFrame(field_0x660);
        }
        break;
    }
    }

    DUSK_IF_ELSE(actor_attr::enemy_action_pos_move_f(this, NULL), fopAcM_posMoveF(this, NULL));
    mpMorf->play(0, dComIfGp_getReverb(fopAcM_GetRoomNo(this)));
}

void daB_ZANTZ_c::cc_set() {
    cXyz cc_center;
    eyePos = current.pos;
    eyePos.y += DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 100.0f), 100.0f);

    for (int i = 0; i < 3; i++) {
        mDoMtx_stack_c::copy(mpMorf->getModel()->getAnmMtx(i + 2));
        mDoMtx_stack_c::transM(0.0f, 0.0f, 130.0f);
        mDoMtx_stack_c::multVecZero(&cc_center);

        field_0x6e4[i].SetC(cc_center);
        field_0x6e4[i].SetR(DUSK_IF_ELSE(actor_attr::enemy_size_value(this, 230.0f), 230.0f));
        dComIfG_Ccsp()->Set(&field_0x6e4[i]);
    }
}

static u8 hio_set;

static daB_ZANTZ_HIO_c l_HIO;

int daB_ZANTZ_c::execute() {
    if (field_0x668 != 0) {
        field_0x668--;
    }

    action();

    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
#if TARGET_PC  // enemy attribute integration
    mDoMtx_stack_c::scaleM((l_HIO.mModelSize) * actor_attr::enemy_size_multiplier(this), (l_HIO.mModelSize) * actor_attr::enemy_size_multiplier(this), (l_HIO.mModelSize) * actor_attr::enemy_size_multiplier(this));
#else
    mDoMtx_stack_c::scaleM(l_HIO.mModelSize, l_HIO.mModelSize, l_HIO.mModelSize);
#endif
    mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
    mpMorf->modelCalc();

    mpBrk->play();

#if TARGET_PC  // enemy attribute integration
    // Keep the statue head's MoveBG collision matched to its visible model.
    // Zant's mouth position is moved forward for oversized heads in d_a_b_zant.cpp
    // instead of shrinking this collision and making parts of the head walk-through.
    const f32 bg_size_mul = actor_attr::enemy_size_multiplier(this);
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    mDoMtx_stack_c::scaleM((l_HIO.mModelSize) * bg_size_mul, (l_HIO.mModelSize) * bg_size_mul, (l_HIO.mModelSize) * bg_size_mul);
#endif
    cMtx_copy(mDoMtx_stack_c::get(), mBgMtx);
    
    if (mpBgW != NULL) {
        mpBgW->Move();
    }

    if (field_0x668 != 0) {
        calcSnortEffect();
    }

    cc_set();
    return 1;
}

static int daB_ZANTZ_Execute(daB_ZANTZ_c* i_this) {
    return i_this->execute();
}

static int daB_ZANTZ_IsDelete(daB_ZANTZ_c* i_this) {
    return 1;
}

int daB_ZANTZ_c::_delete() {
    if (mInitHIO) {
        hio_set = false;
    }

    dComIfG_resDelete(&mPhase, "B_oh");

    if (mpBgW != NULL) {
        dComIfG_Bgsp().Release(mpBgW);
    }

    mSound.deleteObject();
    return 1;
}

static int daB_ZANTZ_Delete(daB_ZANTZ_c* i_this) {
    return i_this->_delete();
}

int daB_ZANTZ_c::CreateHeap() {
    J3DModelData* modelData = (J3DModelData*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BMDR_ZZ);
    JUT_ASSERT(0, modelData != NULL);

    mpMorf = JKR_NEW mDoExt_McaMorfSO(modelData, NULL, NULL, (J3DAnmTransform*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BCK_ZZ_OPEN), 0, 0.0f, 0, -1, &mSound, 0, 0x11000084);
    if (mpMorf == NULL) {
        return 0;
    }

    mpBrk = JKR_NEW mDoExt_brkAnm();
    if (mpBrk == NULL) {
        return 0;
    }

    if (!mpBrk->init(mpMorf->getModel()->getModelData(), (J3DAnmTevRegKey*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::BRK_ZZ), TRUE, J3DFrameCtrl::EMode_LOOP, DUSK_IF_ELSE(actor_attr::enemy_action_step(this, 1.0f), 1.0f), 0, -1)) {
        return 0;
    }

    mpBgW = JKR_NEW dBgW();
    if (mpBgW == NULL) {
        return 0;
    }

#if TARGET_PC  // enemy attribute integration
    // dBgW::Set transforms its vertices immediately. Initialize the matrix
    // with the same size as the visible head before handing it to MoveBG.
    mDoMtx_stack_c::transS(current.pos);
    mDoMtx_stack_c::ZXYrotM(shape_angle);
    const f32 initial_size = l_HIO.mModelSize * actor_attr::enemy_size_multiplier(this);
    mDoMtx_stack_c::scaleM(initial_size, initial_size, initial_size);
    cMtx_copy(mDoMtx_stack_c::get(), mBgMtx);
#endif
    if (mpBgW->Set((cBgD_t*)dComIfG_getObjectRes("B_zan", daB_ZANT_c::DZB_ZZ), 1, &mBgMtx) == 1) {
        return 0;
    }

    mpBgW->SetCrrFunc(dBgS_MoveBGProc_Typical);
    return 1;
}

static int useHeapInit(fopAc_ac_c* i_this) {
    return ((daB_ZANTZ_c*)i_this)->CreateHeap();
}

int daB_ZANTZ_c::create() {
    fopAcM_ct(this, daB_ZANTZ_c);

    int phase_state = dComIfG_resLoad(&mPhase, "B_oh");
    if (phase_state == cPhs_COMPLEATE_e) {
        if (!fopAcM_entrySolidHeap(this, useHeapInit, 0x3220)) {
            return cPhs_ERROR_e;
        }

        if (!hio_set) {
            hio_set = true;
            mInitHIO = true;
            l_HIO.field_0x4 = -1;
        }

        OS_REPORT("B_ZANTZ PARAM %x\n", fopAcM_GetParam(this));
        field_0x670 = fopAcM_GetParam(this);
        if (field_0x670 != 0) {
            field_0x66f = 1;
        }

#if TARGET_PC  // enemy attribute integration
        // MoveBG was registered before the buried spawn position was applied.
        // Set its scaled matrix and vertices first, so the rise starts with
        // collision at the same position and size as the visible head.
        {
            const f32 size_mul = actor_attr::enemy_size_multiplier(this);
            current.pos.y = (home.pos.y + 3500.0f) - (3500.0f * size_mul);
            old.pos.y = current.pos.y;
            mDoMtx_stack_c::transS(current.pos);
            mDoMtx_stack_c::ZXYrotM(shape_angle);
            const f32 model_size = l_HIO.mModelSize * size_mul;
            mDoMtx_stack_c::scaleM(model_size, model_size, model_size);
            cMtx_copy(mDoMtx_stack_c::get(), mBgMtx);
            mpBgW->Move();
        }

#endif
        if (mpBgW != NULL) {
            if (dComIfG_Bgsp().Regist(mpBgW, this)) {
                return cPhs_ERROR_e;
            }
        }

        attention_info.flags = 0;
        
        fopAcM_SetMtx(this, mpMorf->getModel()->getBaseTRMtx());
        fopAcM_SetMin(this, -400.0f, -400.0f,-400.0f);
        fopAcM_SetMax(this, 400.0f, 400.0f, 400.0f);

#if TARGET_PC  // enemy attribute integration
        health = actor_attr::enemy_health_value(this, 80);
        field_0x560 = health;
#else
        health = 80;
        field_0x560 = 80;
#endif

        mCcStts.Init(0xFE, 0, this);

        for (int i = 0; i < 3; i++) {
            field_0x6e4[i].Set(cc_zant_src);
#if TARGET_PC  // enemy attribute integration
            const f32 cc_size_mul = std::min(actor_attr::enemy_size_multiplier(this), 1.0f);
            field_0x6e4[i].SetR(field_0x6e4[i].GetR() * cc_size_mul);
            field_0x6e4[i].SetAtAtp(actor_attr::enemy_attack_power_byte(this, field_0x6e4[i].GetAtAtp()));
#endif
            field_0x6e4[i].SetStts(&mCcStts);
        }

        for (int i = 0; i < 2; i++) {
            mSnortColliders[i].Set(cc_zant_snort_src);
#if TARGET_PC  // enemy attribute integration
            mSnortColliders[i].SetR(actor_attr::enemy_size_value(this, mSnortColliders[i].GetR()));
            mSnortColliders[i].SetH(actor_attr::enemy_size_value(this, mSnortColliders[i].GetH()));
            mSnortColliders[i].SetAtAtp(actor_attr::enemy_attack_power_byte(this, mSnortColliders[i].GetAtAtp()));
#endif
            mSnortColliders[i].SetStts(&mCcStts);
        }

        mMouthMode = 2;
        mAppearMode = 2;
        gravity = 0.0f;

        mDoMtx_stack_c::transS(current.pos);
        mDoMtx_stack_c::ZXYrotM(shape_angle);
#if TARGET_PC  // enemy attribute integration
        mDoMtx_stack_c::scaleM((l_HIO.mModelSize) * actor_attr::enemy_size_multiplier(this), (l_HIO.mModelSize) * actor_attr::enemy_size_multiplier(this), (l_HIO.mModelSize) * actor_attr::enemy_size_multiplier(this));
#else
        mDoMtx_stack_c::scaleM(l_HIO.mModelSize, l_HIO.mModelSize, l_HIO.mModelSize);
#endif
        mpMorf->getModel()->setBaseTRMtx(mDoMtx_stack_c::get());
        mpMorf->modelCalc();
    }

    return phase_state;
}

static int daB_ZANTZ_Create(daB_ZANTZ_c* i_this) {
    return i_this->create();
}

static DUSK_CONST actor_method_class l_daB_ZANTZ_Method = {
    (process_method_func)daB_ZANTZ_Create,
    (process_method_func)daB_ZANTZ_Delete,
    (process_method_func)daB_ZANTZ_Execute,
    (process_method_func)daB_ZANTZ_IsDelete,
    (process_method_func)daB_ZANTZ_Draw,
};

DUSK_PROFILE actor_process_profile_definition DUSK_CONST g_profile_B_ZANTZ = {
    /* Layer ID     */ fpcLy_CURRENT_e,
    /* List ID      */ 4,
    /* List Prio    */ fpcPi_CURRENT_e,
    /* Proc Name    */ fpcNm_B_ZANTZ_e,
    /* Proc SubMtd  */ &g_fpcLf_Method.base,
    /* Size         */ sizeof(daB_ZANTZ_c),
    /* Size Other   */ 0,
    /* Parameters   */ 0,
    /* Leaf SubMtd  */ &g_fopAc_Method.base,
    /* Draw Prio    */ fpcDwPi_B_ZANTZ_e,
    /* Actor SubMtd */ &l_daB_ZANTZ_Method,
    /* Status       */ fopAcStts_UNK_0x40000_e,
    /* Group        */ fopAc_ENEMY_e,
    /* Cull Type    */ fopAc_CULLBOX_CUSTOM_e,
};

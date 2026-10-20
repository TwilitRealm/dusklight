/**
 * d_a_alink_spinner.inc
 * Player Spinner action handling
 */

#include "d/actor/d_a_alink.h"
#if TARGET_PC  // enemy attribute integration
#include "d/actor/d_a_b_ds.h"
#endif
#include "d/actor/d_a_spinner.h"
#if TARGET_PC  // enemy attribute integration
#include "dusk/mods/svc/actor_attribute_helpers.hpp"

static f32 spinner_boss_room_speed_multiplier();
#endif

f32 daAlink_c::getSpinnerGravity() const {
#if TARGET_PC  // enemy attribute integration
    const f32 speed_multiplier = spinner_boss_room_speed_multiplier();
    return mpHIO->mItem.mSpinner.m.mGravity * speed_multiplier * speed_multiplier;
#else
    return mpHIO->mItem.mSpinner.m.mGravity;
#endif
}

f32 daAlink_c::getSpinnerMaxFallSpeed() const {
#if TARGET_PC  // enemy attribute integration
    return mpHIO->mItem.mSpinner.m.mMaxFallSpeed * spinner_boss_room_speed_multiplier();
#else
    return mpHIO->mItem.mSpinner.m.mMaxFallSpeed;
#endif
}

f32 daAlink_c::getSpinnerJumpRate() const {
    return mpHIO->mItem.mSpinner.m.mJumpRate;
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

    u8 bossType = fopAcM_GetParamBit(boss, 0, 8);
    if (bossType == 0xFF) {
        bossType = daB_DS_c::TYPE_BATTLE_1;
    }

    // The spinner only follows Stallord's randomized movement speed during
    // phase 2.  Phase 1 always keeps completely vanilla spinner timing/speed.
    if (bossType != daB_DS_c::TYPE_BATTLE_2) {
        return 1.0f;
    }

    return dusk::mods::svc::actor_attr::resolve_multiplier(boss, ACTOR_ATTRIBUTE_MOVEMENT_SPEED);
}

static f32 spinner_boss_room_speed_multiplier() {
    return daAlink_c::checkStageName("D_MN10A") ? spinner_boss_speed_multiplier() : 1.0f;
}

#endif
s16 daAlink_c::getSpinnerRideMoveTime() {
    if (checkLv4BossRoom()) {
#if TARGET_PC  // enemy attribute integration
        return dusk::mods::svc::actor_attr::sync_timer(mpHIO->mItem.mSpinner.m.mBossRideMoveTime, spinner_boss_speed_multiplier());
#else
        return mpHIO->mItem.mSpinner.m.mBossRideMoveTime;
#endif
    }

    return mpHIO->mItem.mSpinner.m.mRideMoveTime;
}

f32 daAlink_c::getSpinnerRideSpeedF() {
    if (checkLv4BossRoom()) {
#if TARGET_PC  // enemy attribute integration
        return mpHIO->mItem.mSpinner.m.mBossRideSpeed * spinner_boss_speed_multiplier();
#else
        return mpHIO->mItem.mSpinner.m.mBossRideSpeed;
#endif
    }

    return mpHIO->mItem.mSpinner.m.mRideSpeed;
}

f32 daAlink_c::getSpinnerRideDecSpeedMax() const {
#if TARGET_PC  // enemy attribute integration
    const f32 speed_multiplier = spinner_boss_room_speed_multiplier();
    return mpHIO->mItem.mSpinner.m.mDecelSpeedMax * speed_multiplier * speed_multiplier;
#else
    return mpHIO->mItem.mSpinner.m.mDecelSpeedMax;
#endif
}

f32 daAlink_c::getSpinnerRideDecSpeedMin() const {
#if TARGET_PC  // enemy attribute integration
    const f32 speed_multiplier = spinner_boss_room_speed_multiplier();
    return mpHIO->mItem.mSpinner.m.mDecelSpeedMin * speed_multiplier * speed_multiplier;
#else
    return mpHIO->mItem.mSpinner.m.mDecelSpeedMin;
#endif
}

f32 daAlink_c::getSpinnerRideDecSpeedRate() const {
#if TARGET_PC  // enemy attribute integration
    return dusk::mods::svc::actor_attr::action_fraction(mpHIO->mItem.mSpinner.m.mDecelRate, spinner_boss_room_speed_multiplier());
#else
    return mpHIO->mItem.mSpinner.m.mDecelRate;
#endif
}

s16 daAlink_c::getSpinnerRideRotAngleMax() const {
#if TARGET_PC  // enemy attribute integration
    const f32 speedMultiplier = spinner_boss_room_speed_multiplier();

    return static_cast<s16>(mpHIO->mItem.mSpinner.m.mRideRotAngleMax * speedMultiplier);
#else
    return mpHIO->mItem.mSpinner.m.mRideRotAngleMax;
#endif
}

s16 daAlink_c::getSpinnerRideRotAngleMin() const {
#if TARGET_PC  // enemy attribute integration
    const f32 speedMultiplier = spinner_boss_room_speed_multiplier();

    return static_cast<s16>(mpHIO->mItem.mSpinner.m.mRideRotAngleMin * speedMultiplier);
#else
    return mpHIO->mItem.mSpinner.m.mRideRotAngleMin;
#endif
}

void daAlink_c::onSpinnerPathForceRemove() {
    if (checkSpinnerRide() && mRideAcKeep.getActor() != NULL) {
        ((daSpinner_c*)mRideAcKeep.getActor())->onPathForceRemove();
    }
}

bool daAlink_c::checkSpinnerPathMove() {
    if (checkSpinnerRide() && mRideAcKeep.getActor() != NULL) {
        if (((daSpinner_c*)mRideAcKeep.getActor())->checkPathMoveNow() != NULL) {
            return true;
        }
    }

    return false;
}

bool daAlink_c::checkLv4BossRoom() {
    return checkStageName("D_MN10A");
}

bool daAlink_c::checkSpinnerTriggerAttack() {
    if (checkSpinnerRide() && mRideAcKeep.getActor() != NULL) {
        if (((daSpinner_c*)mRideAcKeep.getActor())->reflectAccept()) {
            return true;
        }
    }

    return false;
}

void daAlink_c::setSpinnerSyncPos() {
    daSpinner_c* spinner = (daSpinner_c*)mRideAcKeep.getActor();

    mDoMtx_multVecZero(spinner->getModelMtx(), &current.pos);
    mDoMtx_stack_c::ZXYrotS(spinner->shape_angle);
    mDoMtx_stack_c::YrotM(shape_angle.y - spinner->shape_angle.y);
    mDoMtx_MtxToRot(mDoMtx_stack_c::get(), &shape_angle);
    field_0x37a4 = spinner->current.pos;
}

int daAlink_c::procSpinnerReadyInit() {
    fopAc_ac_c* spinner = (fopAc_ac_c*)fopAcM_fastCreate(
        fpcNm_SPINNER_e, daSpinner_c::getWaitArg(), &current.pos, fopAcM_GetRoomNo(this),
        &shape_angle, NULL, -1, NULL, NULL);

    if (spinner == NULL) {
        return 0;
    }

    commonProcInit(PROC_SPINNER_READY);
    mRideAcKeep.setData(spinner);
    setSingleAnime(ANM_VJUMP_START, mpHIO->mWallHang.mSmallJump.m.mSmallJumpAnm.mSpeed, 6.0f,
                   mpHIO->mWallHang.mSmallJump.m.mSmallJumpAnm.mEndFrame,
                   mpHIO->mWallHang.mSmallJump.m.mSmallJumpAnm.mInterpolation);
    mNormalSpeed = 0.0f;
    setHeavyBoots(0);

    speed.y = 26.0f;
    voiceStart(Z2SE_AL_V_JUMP_S);
    seStartOnlyReverb(Z2SE_AL_SPINNER_START);

    field_0x2f99 = 15;
    deleteEquipItem(TRUE, TRUE);

    current.angle.y = shape_angle.y;
    field_0x3588 = l_waitBaseAnime;
    field_0x33b0 = field_0x3588.y;
    field_0x34d4 = l_waitBaseAnime;
    mProcVar2.field_0x300c = 8;

    return 1;
}

int daAlink_c::procSpinnerReady() {
    daSpinner_c* spinner = (daSpinner_c*)mRideAcKeep.getActor();

    if (spinner == NULL) {
        return checkNextAction(0);
    }

    current.pos.x = spinner->current.pos.x;
    current.pos.z = spinner->current.pos.z;

    mProcVar2.field_0x300c--;
    field_0x2f99 = 15;

    if ((mProcVar2.field_0x300c < 0 && current.pos.y <= spinner->getModelMtx()[1][3]) ||
        mProcVar2.field_0x300c < -30) {
        current.pos.y = spinner->getModelMtx()[1][3];
        procSpinnerWaitInit();
    }

    return 1;
}

void daAlink_c::setSpinnerStatus(u8 i_status, u8 i_flag) {
    dComIfGp_setDoStatus(i_status, i_flag);
}

int daAlink_c::procSpinnerWaitInit() {
    if (!commonProcInitNotSameProc(PROC_SPINNER_WAIT)) {
        return 0;
    }

    mRideStatus = RIDETYPE_SPINNER;
    setBgCheckParam();
    setSingleAnimeBaseSpeed(ANM_RIDE_CROUCH, mpHIO->mItem.mBoard.m.mSitAnmSpeed, 4.0f);
    speed.y = 0.0f;
    setSpecialGravity(0.0f, maxFallSpeed, 0);
    mNormalSpeed = 0.0f;
    mProcVar2.field_0x300c = 0;

    mProcVar3.field_0x300e = shape_angle.y;
    mProcVar4.field_0x3010 = 0;
    field_0x3198 = ANM_RIDE_CROUCH;
    field_0x33b0 = 70.0f;
    mProcVar5.field_0x3012 = 0;
    field_0x37a4 = mRideAcKeep.getActor()->current.pos;

    return 1;
}

int daAlink_c::procSpinnerWait() {
    daSpinner_c* spinner = (daSpinner_c*)mRideAcKeep.getActor();

    if (spinner == NULL || spinner->getDeleteFlg() || checkWindSpeedOnXZ() ||
        checkEndResetFlg1(ERFLG1_NS_SCREAM) || checkEndResetFlg0(ERFLG0_FORCE_WOLF_CHANGE))
    {
        mNormalSpeed = 0.0f;
        setJumpMode();
        return procFallInit(1, mpHIO->mAutoJump.m.mFallInterpolation);
    }

    if (spinner->checkSpinnerTagEnd()) {
        return procSmallJumpInit(2);
    }

    int itemSetBtn = checkItemSetButton(dItemNo_SPINNER_e);
    if (mProcVar2.field_0x300c == 0) {
        if (!itemButton()) {
            mProcVar2.field_0x300c = 1;
            mNormalSpeed = getSpinnerRideSpeedF();
            spinner->setMove(mNormalSpeed, getSpinnerRideMoveTime());
        } else if (checkInputOnR()) {
            int direction = getDirectionFromAngle(mStickAngle);
            s16 angle = mMoveValue * 384.0f * mMoveValue;

            if (direction == DIR_LEFT) {
                shape_angle.y += angle;
            } else if (direction == DIR_RIGHT) {
                shape_angle.y -= angle;
            }

            current.angle.y = shape_angle.y;
            mProcVar3.field_0x300e = shape_angle.y;
        }
    } else {
        if (checkSetItemTrigger(dItemNo_SPINNER_e) || swordSwingTrigger() || itemSetBtn == 2) {
            if (swordSwingTrigger()) {
                swordEquip(0);
            }
            setJumpMode();
            mNormalSpeed = 0.0f;
            return procFallInit(1, mpHIO->mAutoJump.m.mFallInterpolation);
        }

        daPy_frameCtrl_c* frameCtrl = mUnderFrameCtrl;
        dMeter2Info_onDirectUseItem(itemSetBtn);

        if (spinner->checkPathMoveNow() == NULL) {
            seStartOnlyReverbLevel(Z2SE_AL_SPINNER_RIDE);
            setSpinnerStatus(BUTTON_STATUS_ATTACK, BUTTON_STATUS_FLAG_NONE);
        } else {
            seStartOnlyReverbLevel(Z2SE_AL_SPINNER_RAIL);
            setSpinnerStatus(BUTTON_STATUS_UNK_101, BUTTON_STATUS_FLAG_NONE);
        }

        mNormalSpeed = spinner->speedF;
        current.angle.y = spinner->current.angle.y;

        if (spinner->checkSpinnerTagIntoIncRot()) {
            mProcVar5.field_0x3012 = 1;
            setSpinnerStatus(BUTTON_STATUS_SPIN, BUTTON_STATUS_FLAG_CONTINUATION);

            if (field_0x3198 != ANM_RIDE_KICK && field_0x2060->getOldFrameRate() < 0.01f) {
                setSingleAnimeBaseSpeed(ANM_RIDE_CROUCH, mpHIO->mItem.mBoard.m.mSitAnmSpeed,
                                        mpHIO->mItem.mBoard.m.mFastPushInterpolation);
                field_0x3198 = ANM_RIDE_KICK;
                fopAcM_seStartCurrent(spinner, Z2SE_OBJ_SPNR_SW_PUSH, 0);
            }
        } else if (spinner->checkSpinnerTagInto()) {
            if (mProcVar5.field_0x3012 != 0) {
                setSpinnerStatus(BUTTON_STATUS_SPIN, BUTTON_STATUS_FLAG_CONTINUATION);
            } else {
                setSpinnerStatus(BUTTON_STATUS_SPIN, BUTTON_STATUS_FLAG_NONE);
            }

            if ((field_0x3198 == ANM_RIDE_KICK && field_0x2060->getOldFrameRate() < 0.01f) ||
                (field_0x3198 != ANM_RIDE_KICK && field_0x3198 != ANM_RIDE_WAIT)) {
                setSingleAnimeBaseSpeed(ANM_RIDE_WAIT, mpHIO->mItem.mBoard.m.mStandAnmSpeed,
                                        mpHIO->mItem.mBoard.m.mStandInterpolation);
                field_0x3198 = ANM_RIDE_WAIT;
            }
        } else if (spinner->getButtonJump() && field_0x3198 != ANM_RIDE_JUMP) {
            field_0x3198 = ANM_RIDE_JUMP;
            setSingleAnime(ANM_RIDE_JUMP, 0.0f, mpHIO->mItem.mBoard.m.mAirborneAnm.mStartFrame,
                           mpHIO->mItem.mBoard.m.mAirborneAnm.mEndFrame,
                           mpHIO->mItem.mBoard.m.mAirborneAnm.mInterpolation);
            mProcVar4.field_0x3010 = 0;
        } else if (field_0x3198 == ANM_RIDE_JUMP) {
            if (!spinner->getJumpFlg()) {
                setSingleAnime(ANM_RIDE_JUMP_LAND, mpHIO->mItem.mBoard.m.mLandAnm.mSpeed,
                               mpHIO->mItem.mBoard.m.mLandAnm.mStartFrame, 16,
                               mpHIO->mItem.mBoard.m.mLandAnm.mInterpolation);
                field_0x3198 = ANM_RIDE_JUMP_LAND;
            } else if (mProcVar4.field_0x3010 == 0 && field_0x2060->getOldFrameRate() < 0.01f) {
                setSingleAnime(ANM_RIDE_JUMP, mpHIO->mItem.mBoard.m.mAirborneAnm.mSpeed,
                               mpHIO->mItem.mBoard.m.mAirborneAnm.mStartFrame,
                               mpHIO->mItem.mBoard.m.mAirborneAnm.mEndFrame, -1.0f);
                mProcVar4.field_0x3010 = 1;
            }
        } else if ((field_0x3198 == ANM_RIDE_JUMP_LAND && checkAnmEnd(frameCtrl)) ||
                   (field_0x3198 != ANM_RIDE_JUMP_LAND && field_0x3198 != ANM_RIDE_CROUCH)) {
            setSingleAnimeBaseSpeed(ANM_RIDE_CROUCH, mpHIO->mItem.mBoard.m.mSitAnmSpeed,
                                    mpHIO->mItem.mBoard.m.mSitInterpolation);
            field_0x3198 = ANM_RIDE_CROUCH;
        }
    }

    setSpinnerSyncPos();
    shape_angle.y = mProcVar3.field_0x300e;

    return 1;
}

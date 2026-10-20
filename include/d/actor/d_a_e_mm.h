#ifndef D_A_E_MM_H
#define D_A_E_MM_H

#include "f_op/f_op_actor_mng.h"
#include "d/d_bg_s_acch.h"
#include "d/d_cc_d.h"
#include "d/d_cc_uty.h"
#if TARGET_PC

class mDoExt_McaMorfSO;
#endif

/**
 * @ingroup actors-enemies
 * @class e_mm_class
 * @brief Helmasaur
 * 
 * @details 
 * 
 */
class e_mm_class {
public:
    /* 0x000 */ fopEn_enemy_c enemy;
    /* 0x5AC */ request_of_phase_process_class phase;
    /* 0x5B4 */ u8 field_0x5b4;
    /* 0x5B5 */ u8 field_0x5b5;
    /* 0x5B6 */ u8 bitSw;
    /* 0x5B8 */ mDoExt_McaMorfSO* modelMorf;
    /* 0x5BC */ Z2CreatureEnemy sound;
    /* 0x660 */ int anm;
    /* 0x664 */ u32 shadowId;
    /* 0x668 */ fpc_ProcID armorChildId;
    /* 0x66C */ s16 lifetime;
    /* 0x66E */ s16 action;
    /* 0x670 */ s16 mode;
    /* 0x672 */ s8 field_0x672;
    /* 0x674 */ cXyz field_0x674;
    /* 0x680 */ u8 field_0x680[0x68C - 0x680];
    /* 0x68C */ s16 target_ya;
    /* 0x68E */ u8 field_0x68E[0x690 - 0x68E];
    /* 0x690 */ s16 angle_to_pl;
    /* 0x694 */ f32 dist_to_pl;
    /* 0x698 */ f32 field_0x698;
    /* 0x69C */ s16 timers[4];
    /* 0x6A4 */ s16 field_0x6a4;
    /* 0x6A8 */ f32 field_0x6a8;
    /* 0x6AC */ s16 field_0x6ac;
    /* 0x6B0 */ dBgS_AcchCir acchcir;
    /* 0x6F0 */ dBgS_ObjAcch acch;
    /* 0x8C8 */ dCcD_Stts ccStts;
    /* 0x904 */ dCcD_Sph ccSph;
    /* 0xA3C */ dCcD_Sph ccWindSph;
    /* 0xB74 */ dCcU_AtInfo atInfo;
    /* 0xB98 */ s8 field_0xb98;
    /* 0xB99 */ s8 field_0xb99;
    /* 0xB9C */ u32 field_0xb9c[6];
    /* 0xBB4 */ u32 waterEffEmtIds[4];
    /* 0xBC4 */ cXyz field_0xbc4;
    /* 0xBD0 */ cXyz field_0xbd0;
    /* 0xBDC */ cXyz field_0xbdc;
    /* 0xBE8 */ cXyz field_0xbe8;
    /* 0xBF4 */ cXyz field_0xbf4;
    /* 0xC00 */ u8 field_0xc00;
    /* 0xC01 */ u8 field_0xc01;
    /* 0xC04 */ u32 field_0xc04;
    /* 0xC08 */ u32 field_0xc08;
    /* 0xC0C */ u8 field_0xC0C[0xC1C - 0xC0C];
};

STATIC_ASSERT(sizeof(e_mm_class) == 0xc1c);

#endif /* D_A_E_MM_H */

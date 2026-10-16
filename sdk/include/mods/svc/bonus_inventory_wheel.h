#pragma once

#include <mods/api.h>

#ifdef __cplusplus
#include <mods/service.hpp>
#endif

#define BONUS_WHEEL_SERVICE_ID DUSKLIGHT_SERVICE_ID_PREFIX "bonus_inventory_wheel"
#define BONUS_WHEEL_SERVICE_MAJOR 1u
#define BONUS_WHEEL_SERVICE_MINOR 0u

#define BONUS_WHEEL_ICON_SIZE 64u
#define BONUS_WHEEL_ICON_BYTE_SIZE (BONUS_WHEEL_ICON_SIZE * BONUS_WHEEL_ICON_SIZE * 4u)

/* Registration copies one 64x64 RGBA icon. */
typedef uint64_t BonusWheelItemHandle;

/* Invoked on the game thread when the registered item is activated. */
typedef void (*BonusWheelItemUseFn)(ModContext* ctx, void* user_data);

typedef struct BonusWheelItemDesc {
    uint32_t struct_size;
    const char* id;
    const uint8_t* icon_rgba;
    size_t icon_size;
    BonusWheelItemUseFn on_use;
    void* user_data;
} BonusWheelItemDesc;

#define BONUS_WHEEL_ITEM_DESC_INIT {sizeof(BonusWheelItemDesc), NULL, NULL, 0u, NULL, NULL}

typedef struct BonusWheelService {
    ServiceHeader header;

    /* IDs must be unique per registering mod; the icon is copied before this returns. */
    ModResult (*register_item)(ModContext* ctx, const BonusWheelItemDesc* desc,
        BonusWheelItemHandle* out_handle);

    /* Remove an item previously registered by the calling mod. */
    ModResult (*unregister_item)(ModContext* ctx, BonusWheelItemHandle handle);
} BonusWheelService;

MOD_DECLARE_SERVICE(BonusWheelService, svc_bonus_wheel, BONUS_WHEEL_SERVICE_ID,
    BONUS_WHEEL_SERVICE_MAJOR, BONUS_WHEEL_SERVICE_MINOR);

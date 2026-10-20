#pragma once

#include <mods/api.h>

#if !defined(DUSK_BUILDING_GAME) && !defined(DUSK_MOD_FEATURE_IMGUI)
#error "mods/svc/imgui.h requires add_mod(... FEATURES imgui)"
#endif

#define IMGUI_SERVICE_ID DUSKLIGHT_SERVICE_ID_PREFIX "imgui"
#define IMGUI_SERVICE_MAJOR 1u
#define IMGUI_SERVICE_MINOR 0u

/*
 * Service for making use of Dear ImGui.
 *
 * Note that Dear ImGui is not ABI stable. Therefore, this service (and the relevant symbol exports)
 * are only available when building against a debug Dusklight.
 */

typedef void (*ImGuiCallback)(ModContext* context, void* user_data);

typedef enum ImguiCallbackPoint {
    /**
     * Called every frame after all built-in game ImGuis.
     */
    IMGUI_CALLBACK_FRAME = 0,

    /**
     * Called from inside the ImGui menu bar.
     */
    IMGUI_CALLBACK_MENU_BAR = 1,

    IMGUI_CALLBACK_MAX,
} ImguiCallbackPoint;

typedef struct ImguiService {
    ServiceHeader header;

    /**
     * Set an imgui-related callback for a certain point in the frame, suitable for drawing your
     * own custom ImGui windows or otherwise.
     *
     * @remarks Fundamentally, nothing stops you from using a hook and calling ImGui without
     * touching this service. It's just convenient.
     */
    ModResult (*set_callback)(ModContext* context, ImguiCallbackPoint point, ImGuiCallback callback, void* user_data);
} ImguiService;

MOD_DECLARE_SERVICE(ImguiService, svc_imgui, IMGUI_SERVICE_ID, IMGUI_SERVICE_MAJOR, IMGUI_SERVICE_MINOR);

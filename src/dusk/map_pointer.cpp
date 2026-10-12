#include "dusk/logging.h"
#include "dusk/map_pointer.h"
#include "dusk/menu_pointer.h"
#include "dusk/settings.h"

#include "d/d_menu_dmap.h"
#include "d/d_menu_dmap_map.h"
#include "d/d_menu_fmap2D.h"

namespace dusk::map_pointer {

MapPointerBounds get_fmap_pointer_bounds(dMenu_Fmap2DBack_c* map) {
    if (map == nullptr) {
        return {};
    }

    const f32 left = map->getMapScissorAreaLX();
    const f32 top = map->getMapScissorAreaLY();
    const f32 right = left + map->getMapScissorAreaSizeRealX();
    const f32 bottom = top + map->getMapScissorAreaSizeRealY();

    return {
        .left = left,
        .top = top,
        .right = right,
        .bottom = bottom,
    };
}

MapPointerBounds get_dmap_pointer_bounds(dMenu_DmapBg_c* background) {
    if (background == nullptr) {
        return {};
    }

    CPaneMgr pane;
    Mtx matrix;
    const Vec topLeft = pane.getGlobalVtx(background->getMapPane(), &matrix, 0, false, 0);
    const Vec bottomRight = pane.getGlobalVtx(background->getMapPane(), &matrix, 3, false, 0);

    return {
        .left = topLeft.x,
        .top = topLeft.y,
        .right = bottomRight.x,
        .bottom = bottomRight.y,
    };
}

MapPointerInput fmap_pointer_drag(dMenu_Fmap2DBack_c* map) {
    static MapDragState state;
    MapPointerInput mapPointerInput;
    const bool isMirrorMode = dusk::getSettings().game.enableMirrorMode;

    // Check if the map is valid and the menu pointer setting is enabled
    if (map == nullptr || !dusk::menu_pointer::enabled()) {
        state.dragging = false;
        return {};
    }

    dusk::menu_pointer::begin_context(dusk::menu_pointer::Context::Map);
    const auto& pointer = dusk::menu_pointer::state();
    if (!pointer.valid) {
        state.dragging = false;
        return {};
    }

    // Calculate map bounds
    const auto bounds = get_fmap_pointer_bounds(map);
    mapPointerInput.hover = bounds.contains(pointer.x, pointer.y);

    // If the pointer is pressed inside the map bounds, turn on the dragging flag
    if (pointer.pressed && mapPointerInput.hover) {
        state.dragging = true;
        state.previousX = pointer.x;
        state.previousY = pointer.y;
    }

    // Compute the current pointer's world position
    f32 current_x = pointer.x;
    if (isMirrorMode) {
        current_x = map->getMirrorPosX(current_x, 0.0f);
    }

    f32 current_world_x = 0.0f;
    f32 current_world_z = 0.0f;
    const bool needsCurrentWorldPos = (state.dragging && pointer.down) || mapPointerInput.hover;
    if (needsCurrentWorldPos) {
        map->calcAllMapPosWorld(current_x, pointer.y, &current_world_x, &current_world_z);
    }

    // Drag the map (works even if the pointer is outside the map bounds)
    if (state.dragging && pointer.down) {
        mapPointerInput.dragging = true;

        f32 previous_x = state.previousX;
        if (isMirrorMode) {
            previous_x = map->getMirrorPosX(previous_x, 0.0f);
        }

        f32 previous_world_x;
        f32 previous_world_z;
        map->calcAllMapPosWorld(previous_x, state.previousY, &previous_world_x, &previous_world_z);

        mapPointerInput.deltaX = current_world_x - previous_world_x;
        mapPointerInput.deltaZ = current_world_z - previous_world_z;

        state.previousX = pointer.x;
        state.previousY = pointer.y;
    }

    // Update the cursor position on the Fmap if the pointer is hovering over it
    if (mapPointerInput.hover) {
        map->setArrowPosAxis(current_world_x, current_world_z);

        // Consume clicks inside the map bounds
        dusk::menu_pointer::set_hover_target(0);
        mapPointerInput.clicked = pointer.clicked;
    }

    if (pointer.released) {
        state.dragging = false;
    }

    return mapPointerInput;
}

MapPointerInput dmap_pointer_drag(dMenu_DmapBg_c* background, dMenu_DmapMapCtrl_c* map) {
    static MapDragState state;
    MapPointerInput mapPointerInput;

    // Check if the map is valid and the menu pointer setting is enabled
    if (background == nullptr || map == nullptr || !dusk::menu_pointer::enabled()) {
        state.dragging = false;
        return {};
    }

    dusk::menu_pointer::begin_context(dusk::menu_pointer::Context::Map);
    const auto& pointer = dusk::menu_pointer::state();
    if (!pointer.valid) {
        state.dragging = false;
        return {};
    }

    // Calculate map bounds
    const auto bounds = get_dmap_pointer_bounds(background);
    mapPointerInput.hover = bounds.contains(pointer.x, pointer.y);

    // If the pointer is pressed inside the map bounds, turn on the dragging flag
    if (pointer.pressed && mapPointerInput.hover) {
        state.dragging = true;
        state.previousX = pointer.x;
        state.previousY = pointer.y;
    }

    // Drag the map (works even if the pointer is outside the map bounds)
    if (state.dragging && pointer.down) {
        mapPointerInput.dragging = true;
        // This value tries to match 1:1 the cursor movement to the dmap dragging speed
        constexpr f32 kDragMultiplier = 4000.0f;
        const f32 kPixelPerCm = map->getPixelPerCm() * kDragMultiplier;

        mapPointerInput.deltaX = (pointer.x - state.previousX) * kPixelPerCm;
        mapPointerInput.deltaZ = -(pointer.y - state.previousY) * kPixelPerCm;

        state.previousX = pointer.x;
        state.previousY = pointer.y;
    }

    // Consume clicks inside the map bounds
    if (mapPointerInput.hover) {
        dusk::menu_pointer::set_hover_target(0);
        mapPointerInput.clicked = pointer.clicked;
    }

    if (pointer.released) {
        state.dragging = false;
    }

    return mapPointerInput;
}

} // namespace dusk::map_pointer

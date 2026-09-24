#include "dusk/map_pointer.h"
#include "dusk/logging.h"
#include "dusk/menu_pointer.h"
#include "dusk/settings.h"

#include "d/d_menu_dmap.h"
#include "d/d_menu_dmap_map.h"
#include "d/d_menu_fmap2D.h"

namespace dusk::map_pointer {

static dusk::map_pointer::MapPointerBounds fMapBounds;
static dusk::map_pointer::MapPointerBounds dMapBounds;
// This value tries to match 1:1 the cursor movement to the dmap dragging speed
constexpr f32 kDMapDragMultiplier = 4000.0f;

MapPointerInput fmap_pointer_drag(dMenu_Fmap2DBack_c* map) {
    static bool mapMouseDragging = false;
    static f32 mapMousePreviousX = 0.0f;
    static f32 mapMousePreviousY = 0.0f;
    MapPointerInput input;

    // Check if the map is valid and the menu pointer setting is enabled
    if (map == nullptr || !dusk::menu_pointer::enabled()) {
        mapMouseDragging = false;
        return {};
    }

    dusk::menu_pointer::begin_context(dusk::menu_pointer::Context::Map);
    const auto& pointer = dusk::menu_pointer::state();
    if (!pointer.valid) {
        mapMouseDragging = false;
        return {};
    }

    // Calculate map bounds
    dusk::map_pointer::get_fmap_pointer_bounds(map);
    input.hovered = pointer.x >= fMapBounds.left && pointer.x <= fMapBounds.right &&
                    pointer.y >= fMapBounds.top && pointer.y <= fMapBounds.bottom;

    // If the pointer is pressed inside the map bounds, turn on the dragging flag
    if (pointer.pressed && input.hovered) {
        mapMouseDragging = true;
        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    // Drag the map (works even if the pointer is outside the map bounds)
    if (mapMouseDragging && pointer.down) {
        input.dragging = true;

        f32 previous_x = mapMousePreviousX;
        f32 current_x = pointer.x;
        if (dusk::getSettings().game.enableMirrorMode) {
            previous_x = map->getMirrorPosX(previous_x, 0.0f);
            current_x = map->getMirrorPosX(current_x, 0.0f);
        }

        f32 previous_world_x;
        f32 previous_world_z;
        f32 current_world_x;
        f32 current_world_z;
        map->calcAllMapPosWorld(previous_x, mapMousePreviousY, &previous_world_x, &previous_world_z);
        map->calcAllMapPosWorld(current_x, pointer.y, &current_world_x, &current_world_z);

        input.deltaX = current_world_x - previous_world_x;
        input.deltaZ = current_world_z - previous_world_z;

        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    if (input.hovered && !input.dragging) {
        f32 pointer_x = pointer.x;

        if (dusk::getSettings().game.enableMirrorMode) {
            pointer_x = map->getMirrorPosX(pointer_x, 0.0f);
        }

        f32 pos_x;
        f32 pos_z;
        map->calcAllMapPosWorld(pointer_x, pointer.y, &pos_x, &pos_z);
        map->setArrowPosAxis(pos_x, pos_z);
    }

    if (input.hovered) {
        dusk::menu_pointer::set_hover_target(0);
        input.clicked = pointer.clicked || dusk::menu_pointer::consume_click();
    }

    if (pointer.released) {
        mapMouseDragging = false;
    }

    return input;
}

void get_fmap_pointer_bounds(dMenu_Fmap2DBack_c* map) {
    if (map == nullptr) {
        fMapBounds = {};
        return;
    }

    const f32 _left = map->getMapScissorAreaLX();
    const f32 _top = map->getMapScissorAreaLY();
    const f32 _right = _left + map->getMapScissorAreaSizeRealX();
    const f32 _bottom = _top + map->getMapScissorAreaSizeRealY();

    fMapBounds = {
        .left = _left,
        .top = _top,
        .right = _right,
        .bottom = _bottom,
    };
}

void get_dmap_pointer_bounds(dMenu_DmapBg_c* background) {
    if (background == nullptr) {
        dMapBounds = {};
        return;
    }

    CPaneMgr pane;
    Mtx matrix;
    const Vec top_left = pane.getGlobalVtx(background->getMapPane(), &matrix, 0, false, 0);
    const Vec bottom_right = pane.getGlobalVtx(background->getMapPane(), &matrix, 3, false, 0);
    dMapBounds = {
        .left = top_left.x,
        .top = top_left.y,
        .right = bottom_right.x,
        .bottom = bottom_right.y,
    };
}

MapPointerInput dmap_pointer_drag(dMenu_DmapBg_c* background, dMenu_DmapMapCtrl_c* map) {
    static bool mapMouseDragging = false;
    static f32 mapMousePreviousX = 0.0f;
    static f32 mapMousePreviousY = 0.0f;
    MapPointerInput input;

    // Check if the map is valid and the menu pointer setting is enabled
    if (background == nullptr || map == nullptr || !dusk::menu_pointer::enabled()) {
        mapMouseDragging = false;
        return {};
    }

    dusk::menu_pointer::begin_context(dusk::menu_pointer::Context::Map);
    const auto& pointer = dusk::menu_pointer::state();
    if (!pointer.valid) {
        mapMouseDragging = false;
        return {};
    }

    // Calculate map bounds
    dusk::map_pointer::get_dmap_pointer_bounds(background);
    input.hovered = pointer.x >= dMapBounds.left && pointer.x <= dMapBounds.right &&
                    pointer.y >= dMapBounds.top && pointer.y <= dMapBounds.bottom;

    // If the pointer is pressed inside the map bounds, turn on the dragging flag
    if (pointer.pressed && input.hovered) {
        mapMouseDragging = true;
        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    // Drag the map (works even if the pointer is outside the map bounds)
    if (pointer.down) {
        input.dragging = true;

        const f32 pixelPerCm = map->getPixelPerCm() * kDMapDragMultiplier;

        input.deltaX = (pointer.x - mapMousePreviousX) * pixelPerCm;
        input.deltaZ = -(pointer.y - mapMousePreviousY) * pixelPerCm;

        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    if (input.hovered) {
        dusk::menu_pointer::set_hover_target(0);
        input.clicked = pointer.clicked || dusk::menu_pointer::consume_click();
    }

    if (pointer.released) {
        mapMouseDragging = false;
    }

    return input;
}

} // namespace dusk::map_pointer

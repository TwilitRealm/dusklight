
#include "d/d_menu_dmap.h"
#include "d/d_menu_dmap_map.h"
#include "d/d_menu_fmap2D.h"
#include "d/d_menu_item_explain.h"
#include "d/d_meter2_info.h"
#include "d/d_meter_haihai.h"

#include "dusk/map_pointer.h"
#include "dusk/menu_pointer.h"
#include "dusk/settings.h"
#include "dusk/logging.h"

namespace dusk::map_pointer {

static dusk::map_pointer::FMapPointerBounds fMapPointerBounds;
static dusk::map_pointer::DMapPointerBounds dMapPointerBounds;

MapPointerInput pointer_drag_fmap(dMenu_Fmap2DBack_c* map) {
    static bool mapMouseDragging = false;
    static f32 mapMousePreviousX = 0.0f;
    static f32 mapMousePreviousY = 0.0f;

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
    dusk::map_pointer::get_field_map_pointer_bounds(map);
    const bool inside = pointer.x >= fMapPointerBounds.left &&
                        pointer.x <= fMapPointerBounds.right &&
                        pointer.y >= fMapPointerBounds.top &&
                        pointer.y <= fMapPointerBounds.bottom;

    const bool isMouse = !pointer.touch;
    if (isMouse && pointer.pressed && inside) {
        mapMouseDragging = true;
        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    MapPointerInput input;
    input.hovered = inside;

    // Drag the map
    if (isMouse && mapMouseDragging && pointer.down) {
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

    if (inside && !input.dragging) {
        f32 pointer_x = pointer.x;

        if (dusk::getSettings().game.enableMirrorMode) {
            pointer_x = map->getMirrorPosX(pointer_x, 0.0f);
        }

        f32 pos_x;
        f32 pos_z;
        map->calcAllMapPosWorld(pointer_x, pointer.y, &pos_x, &pos_z);
        map->setArrowPosAxis(pos_x, pos_z);
    }

    if (inside) {

        dusk::menu_pointer::set_hover_target(0);
        input.clicked = dusk::menu_pointer::consume_click();
    }

    if (isMouse && pointer.released) {
        mapMouseDragging = false;
    }

    return input;
}

void get_field_map_pointer_bounds(dMenu_Fmap2DBack_c* map) {
    if (map == nullptr) {
        fMapPointerBounds = {};
        return;
    }

    const f32 _left = map->getMapScissorAreaLX();
    const f32 _top = map->getMapScissorAreaLY();
    const f32 _right = _left + map->getMapScissorAreaSizeRealX();
    const f32 _bottom = _top + map->getMapScissorAreaSizeRealY();
    fMapPointerBounds = {
        .left = _left,
        .top = _top,
        .right = _right,
        .bottom = _bottom,
    };
}

void get_dungeon_map_pointer_bounds(dMenu_DmapBg_c* background) {
    if (background == nullptr) {
        dMapPointerBounds = {};
        return;
    }

    CPaneMgr pane;
    Mtx matrix;
    const Vec top_left = pane.getGlobalVtx(background->getMapPane(), &matrix, 0, false, 0);
    const Vec bottom_right = pane.getGlobalVtx(background->getMapPane(), &matrix, 3, false, 0);
    dMapPointerBounds = {
        .left = top_left.x,
        .top = top_left.y,
        .right = bottom_right.x,
        .bottom = bottom_right.y,
        .background = background,
    };
}

MapPointerInput pointer_drag_dmap(dMenu_DmapBg_c* background, dMenu_DmapMapCtrl_c* map) {
    static bool mapMouseDragging = false;
    static f32 mapMousePreviousX = 0.0f;
    static f32 mapMousePreviousY = 0.0f;

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
    dusk::map_pointer::get_dungeon_map_pointer_bounds(background);
    const bool inside = pointer.x >= dMapPointerBounds.left &&
                        pointer.x <= dMapPointerBounds.right &&
                        pointer.y >= dMapPointerBounds.top &&
                        pointer.y <= dMapPointerBounds.bottom;

    const bool isMouse = !pointer.touch;
    if (isMouse && pointer.pressed && inside) {
        mapMouseDragging = true;
        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    MapPointerInput input;
    input.hovered = inside;

    // Drag the map
    if (isMouse && mapMouseDragging && pointer.down) {
        input.dragging = true;

        // This value tries to match 1:1 the cursor movement to the map dragging speed
        constexpr f32 dragMultiplier = 3900.0f;
        const f32 pixelPerCm = map->getPixelPerCm() * dragMultiplier;

        input.deltaX = (pointer.x - mapMousePreviousX) * pixelPerCm;
        input.deltaZ = -(pointer.y - mapMousePreviousY) * pixelPerCm;

        mapMousePreviousX = pointer.x;
        mapMousePreviousY = pointer.y;
    }

    if (inside) {
        dusk::menu_pointer::set_hover_target(0);
        input.clicked = pointer.clicked || dusk::menu_pointer::consume_click();
    }

    if (isMouse && pointer.released) {
        mapMouseDragging = false;
    }

    return input;
}

} // namespace dusk::map_pointer

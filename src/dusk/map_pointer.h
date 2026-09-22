#pragma once

#include "d/d_menu_dmap.h"
#include "d/d_menu_fmap.h"

namespace dusk::map_pointer {

struct DMapPointerBounds {
    f32 left = 0.0f;
    f32 top = 0.0f;
    f32 right = 0.0f;
    f32 bottom = 0.0f;
    dMenu_DmapBg_c* background = nullptr;
    bool valid = false;
};

struct FMapPointerBounds {
    f32 left = 0.0f;
    f32 top = 0.0f;
    f32 right = 0.0f;
    f32 bottom = 0.0f;
};

struct MapPointerInput {
    f32 deltaX = 0.0f;
    f32 deltaZ = 0.0f;
    bool hovered = false;
    bool dragging = false;
    bool clicked = false;
};

void get_field_map_pointer_bounds(dMenu_Fmap2DBack_c* map);
void get_dungeon_map_pointer_bounds(dMenu_DmapBg_c* background);

MapPointerInput pointer_drag_fmap(dMenu_Fmap2DBack_c* map);
MapPointerInput pointer_drag_dmap(dMenu_DmapBg_c* background, dMenu_DmapMapCtrl_c* map);

}  // namespace dusk::map_pointer

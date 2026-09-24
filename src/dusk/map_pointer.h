#pragma once

#include "d/d_menu_dmap.h"
#include "d/d_menu_fmap.h"

namespace dusk::map_pointer {

struct MapPointerBounds {
    f32 left = 0.0f;
    f32 top = 0.0f;
    f32 right = 0.0f;
    f32 bottom = 0.0f;
    inline bool contains(f32 x, f32 y) const {
        return x >= left && x <= right && y >= top && y <= bottom;
    }
};

struct MapDragState {
    bool dragging = false;
    f32 previousX = 0.0f;
    f32 previousY = 0.0f;
};

struct MapPointerInput {
    f32 deltaX = 0.0f;
    f32 deltaZ = 0.0f;
    bool hover = false;
    bool dragging = false;
    bool clicked = false;
};

MapPointerBounds get_fmap_pointer_bounds(dMenu_Fmap2DBack_c* map);
MapPointerBounds get_dmap_pointer_bounds(dMenu_DmapBg_c* background);

MapPointerInput fmap_pointer_drag(dMenu_Fmap2DBack_c* map);
MapPointerInput dmap_pointer_drag(dMenu_DmapBg_c* background, dMenu_DmapMapCtrl_c* map);

}  // namespace dusk::map_pointer

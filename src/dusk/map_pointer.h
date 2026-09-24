#pragma once

#include "d/d_menu_dmap.h"
#include "d/d_menu_fmap.h"

namespace dusk::map_pointer {

struct MapPointerBounds {
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

void get_fmap_pointer_bounds(dMenu_Fmap2DBack_c* map);
void get_dmap_pointer_bounds(dMenu_DmapBg_c* background);

MapPointerInput fmap_pointer_drag(dMenu_Fmap2DBack_c* map);
MapPointerInput dmap_pointer_drag(dMenu_DmapBg_c* background, dMenu_DmapMapCtrl_c* map);

}  // namespace dusk::map_pointer

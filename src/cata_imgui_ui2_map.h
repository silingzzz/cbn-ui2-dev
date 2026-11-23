#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_MAP_H
#define CATA_SRC_CATA_IMGUI_UI2_MAP_H

#if defined( TILES )

#include "coordinates.h"

namespace ui2
{
namespace mapview
{

/// Feed the tile currently under the game cursor.  Called from handle_mouseview
/// on MOUSE_MOVE, so the tile is already resolved by the game's own input code.
void set_hover( const tripoint_bub_ms &pos );
void clear_hover();
void draw();

} // namespace mapview
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_MAP_H

#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_WORLD_H
#define CATA_SRC_CATA_IMGUI_UI2_WORLD_H

#if defined( TILES )

#include <string>

#include "coordinates.h"

namespace ui2
{
namespace world
{

/// The first world-interaction slice: select a map tile, inspect it, then run
/// one of a deliberately small set of actions through the existing game code.
enum class action_kind {
    none,
    examine,
    pickup,
    open,
    close,
    walk_to,
};

struct request {
    action_kind kind = action_kind::none;
    tripoint_bub_ms target;
};

/// Whether the world inspector owns this frame. While open, the shell keeps
/// the platform console but the map remains mouse-interactive outside the
/// panel rectangle.
auto is_open() -> bool;
void set_open( bool open );

/// Whether the given screen-space point lies over the world panel rectangle.
/// Used by the event router while the panel is non-modal over the map.
auto mouse_over_panel( float x, float y ) -> bool;

/// Map-side hooks called from game::handle_mouseview().
void set_hover( const tripoint_bub_ms &pos );
void clear_hover();
void on_left_click( const tripoint_bub_ms &pos );
void on_right_click( const tripoint_bub_ms &pos );

/// Handles Escape before the shell closes the panel. Returns true when the
/// key only cleared the selection/result.
auto handle_escape() -> bool;

/// Draw the inspection panel.  Called from ui2::draw().
void draw();

/// Draw the current hover/selection highlight before the map is rendered.
void highlight_tile();

/// Deferred action consumed by game::handle_action().
auto has_request() -> bool;
auto take_request( request &out ) -> bool;
void set_result( const std::string &text, bool error );

} // namespace world
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_WORLD_H

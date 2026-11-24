#ifndef CATA_SRC_CATA_IMGUI_UI2_GEAR_H
#define CATA_SRC_CATA_IMGUI_UI2_GEAR_H

#if defined( TILES )

namespace ui2
{
namespace gear
{

/// Read-only worn/wielded equipment inspector, including nested containers.
void draw();
void toggle();
bool is_open();
void set_open( bool open );

/// True when a click queued an item-use request that the game loop still owes.
bool has_deferred_action();

/// Run the queued item-use request from `game::handle_action`, after the
/// ImGui frame has been presented and input returned to the game loop.
/// Returns true when a request was consumed (even if it proved stale).
bool take_deferred_action();

} // namespace gear
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_GEAR_H

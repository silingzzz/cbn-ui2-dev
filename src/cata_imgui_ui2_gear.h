#ifndef CATA_SRC_CATA_IMGUI_UI2_GEAR_H
#define CATA_SRC_CATA_IMGUI_UI2_GEAR_H

#if defined( TILES )

class Character;
class item;

namespace ui2
{
namespace gear
{

/// True when the character's inventory can accept the detached item without
/// falling back to the map. Mirrors the checks used by i_add_or_drop().
bool can_add_to_inventory( const Character &you, const item &it );

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

/// Clear raw item pointers and session-scoped notices across saves/new games.
void reset_session();

} // namespace gear
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_GEAR_H

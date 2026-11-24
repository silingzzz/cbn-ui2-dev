#ifndef CATA_SRC_CATA_IMGUI_UI2_CRAFT_H
#define CATA_SRC_CATA_IMGUI_UI2_CRAFT_H

#if defined( TILES )

namespace ui2
{
namespace craft
{

/// Recipe browser with a direct "start craft" path into the existing game flow.
void draw();
void toggle();
bool is_open();
void set_open( bool open );

/// True when a start/cancel/queue step is waiting for the safe drain point.
bool has_deferred_action();

/// Run one queued craft action from game::handle_action().
bool take_deferred_action();

} // namespace craft
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_CRAFT_H

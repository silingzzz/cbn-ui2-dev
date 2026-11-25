#ifndef CATA_IMGUI_UI2_MESSAGES_H
#define CATA_IMGUI_UI2_MESSAGES_H

#if defined( TILES )

namespace ui2
{
namespace messages
{

bool is_open();
void toggle();
void set_open( bool open );
void draw();

/// True when the panel queued a game-state action that must run outside ImGui.
bool has_deferred_action();

/// Run one queued action from game::handle_action(); returns true if consumed.
bool take_deferred_action();

/// Clear selection, pending state and notices across saves/new games.
void reset_session();

} // namespace messages
} // namespace ui2

#endif // TILES

#endif // CATA_IMGUI_UI2_MESSAGES_H

#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_MISSIONS_H
#define CATA_SRC_CATA_IMGUI_UI2_MISSIONS_H

#if defined( TILES )

class mission;

namespace ui2
{
namespace missions
{

enum class tab_kind : int {
    active = 0,
    completed = 1,
    failed = 2,
};

bool is_open();
void toggle();
void set_open( bool open );
void reset_session();
void draw();

// Helper used by UI and Catch2 tests to set the player's tracked mission
// immediately without closing the panel.
bool track_mission( mission *target );
void set_tab( tab_kind tab );
tab_kind current_tab();

} // namespace missions
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_MISSIONS_H

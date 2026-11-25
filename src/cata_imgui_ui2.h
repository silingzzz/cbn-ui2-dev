#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_H
#define CATA_SRC_CATA_IMGUI_UI2_H

#if defined( TILES )

namespace ui2
{

/// Draw the UI 2.0 component demo (PR-1.5). Must be called inside an ImGui
/// frame (between NewFrame and Render). Uses demo data only -- no game state.
void draw();

/// Reset the demo window/tab state, e.g. when the overlay is (re)opened so a
/// closed demo window never becomes unreachable.
void reset();

/// Reset game-session state across UI 2.0 modules when starting or loading
/// a world so raw item pointers and crafting/notice state do not leak across saves.
void reset_session();

} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_H

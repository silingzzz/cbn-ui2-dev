#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_AIM_H
#define CATA_SRC_CATA_IMGUI_UI2_AIM_H

#if defined( TILES )

namespace ui2
{
namespace aim
{

/// PR-2A AIM-2: real backpack view plus a one-item move command.
/// The command only builds ACT_MOVE_ITEMS; all placement rules stay in the game.
void draw();

/// Development-preview entry point (the demo window button toggles this).
void toggle();
auto is_open() -> bool;

} // namespace aim
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_AIM_H

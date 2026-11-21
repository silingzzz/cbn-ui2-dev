#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_AIM_H
#define CATA_SRC_CATA_IMGUI_UI2_AIM_H

#if defined( TILES )

namespace ui2
{
namespace aim
{

/// PR-2A AIM-1: read-only view of the player's real backpack.
void draw();

/// PoC mode: draw the same read-only inventory as a fixed, borderless panel
/// occupying the given pixel rect (used to prove that a real tiles map and a
/// real ImGui panel can coexist side by side).
void draw_panel( float x, float y, float width, float height );

/// Development-preview entry point (the demo window button toggles this).
void toggle();
auto is_open() -> bool;

} // namespace aim
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_AIM_H

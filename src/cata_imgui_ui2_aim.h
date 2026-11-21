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

/// Development-preview entry point (the demo window button toggles this).
void toggle();
auto is_open() -> bool;

} // namespace aim
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_AIM_H

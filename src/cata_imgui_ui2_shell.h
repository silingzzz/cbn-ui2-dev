#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_SHELL_H
#define CATA_SRC_CATA_IMGUI_UI2_SHELL_H

#if defined( TILES )

namespace ui2
{
namespace shell
{

/// One primary business panel at a time. The shell owns navigation so a closed
/// window never strands the player in a second "home page".
enum class panel {
    none,
    aim,
    gear,
    craft,
    messages,
    zone,
    world,
    missions,
    vehicle,
};

/// Draw the always-available console while the ImGui overlay is open.
void draw();

/// Open one panel and close every other panel.
void open( panel target );

/// Close every business panel, leaving the overlay itself open.
void close_all();

/// The panel currently visible, or none when all are closed.
auto active() -> panel;

auto any_open() -> bool;

} // namespace shell
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_SHELL_H

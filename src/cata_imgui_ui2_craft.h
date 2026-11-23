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

} // namespace craft
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_CRAFT_H

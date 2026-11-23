#ifndef CATA_SRC_CATA_IMGUI_UI2_GEAR_H
#define CATA_SRC_CATA_IMGUI_UI2_GEAR_H

#if defined( TILES )

namespace ui2
{
namespace gear
{

/// Read-only worn/wielded equipment inspector, including nested containers.
void draw();
void toggle();
bool is_open();

} // namespace gear
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_GEAR_H

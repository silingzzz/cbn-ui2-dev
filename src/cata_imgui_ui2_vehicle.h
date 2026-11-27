#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_VEHICLE_H
#define CATA_SRC_CATA_IMGUI_UI2_VEHICLE_H

#if defined( TILES )

#include <vector>

class vehicle;

namespace ui2
{
namespace veh
{

bool is_open();
void toggle();
void set_open( bool open );
void reset_session();
void draw();

// Read-only query helpers exposed for Catch2 tests.
auto collect_nearby_vehicles() -> std::vector<vehicle *>;
auto count_damaged_parts( const ::vehicle &v ) -> int;
void set_damaged_only( bool damaged_only );
bool damaged_only();

} // namespace veh
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_VEHICLE_H

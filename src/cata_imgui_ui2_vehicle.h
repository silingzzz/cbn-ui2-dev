#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_VEHICLE_H
#define CATA_SRC_CATA_IMGUI_UI2_VEHICLE_H

#if defined( TILES )

#include <string>
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

// Read-only query & interactive control helpers exposed for UI and Catch2 tests.
auto collect_nearby_vehicles() -> std::vector<vehicle *>;
auto count_damaged_parts( const ::vehicle &v ) -> int;
bool has_feature( const ::vehicle &v, const std::string &feature );
bool is_feature_enabled( const ::vehicle &v, const std::string &feature );
bool toggle_feature( ::vehicle &v, const std::string &feature );
void set_damaged_only( bool damaged_only );
bool damaged_only();

} // namespace veh
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_VEHICLE_H

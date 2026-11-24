#pragma once

#if defined( TILES )

namespace ui2
{
namespace zone
{

// PR-5A: read-only zone inspector plus a deferred enable/disable toggle.
bool is_open();
void toggle();
void set_open( bool open );
void draw();

// PR-5C: map rectangle selection and zone creation.
auto take_map_select_request() -> bool;
void run_map_select_flow();

} // namespace zone
} // namespace ui2

#endif // TILES

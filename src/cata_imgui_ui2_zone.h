#pragma once

#if defined( TILES )

namespace ui2
{
namespace zone
{

// PR-5A: read-only zone inspector plus a deferred enable/disable toggle.
bool is_open();
void toggle();
void draw();

} // namespace zone
} // namespace ui2

#endif // TILES

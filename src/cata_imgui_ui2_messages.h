#ifndef CATA_IMGUI_UI2_MESSAGES_H
#define CATA_IMGUI_UI2_MESSAGES_H

#if defined( TILES )

namespace ui2
{
namespace messages
{

bool is_open();
void toggle();
void set_open( bool open );
void draw();

} // namespace messages
} // namespace ui2

#endif // TILES

#endif // CATA_IMGUI_UI2_MESSAGES_H

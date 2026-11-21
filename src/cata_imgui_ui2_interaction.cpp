#include "cata_imgui_ui2_interaction.h"

#if defined( TILES )

#include <imgui.h>

namespace
{

int g_modal_depth = 0;

auto to_imgui_cursor( ui2::interaction::cursor c ) -> ImGuiMouseCursor
{
    switch( c ) {
        case ui2::interaction::cursor::hand:
            return ImGuiMouseCursor_Hand;
        case ui2::interaction::cursor::ibeam:
            return ImGuiMouseCursor_TextInput;
        case ui2::interaction::cursor::resize_ew:
            return ImGuiMouseCursor_ResizeEW;
        case ui2::interaction::cursor::resize_ns:
            return ImGuiMouseCursor_ResizeNS;
        case ui2::interaction::cursor::drag:
            return ImGuiMouseCursor_ResizeAll;
        case ui2::interaction::cursor::arrow:
        default:
            return ImGuiMouseCursor_Arrow;
    }
}

} // namespace

namespace ui2
{
namespace interaction
{

void hover_cursor( cursor c )
{
    if( ImGui::IsItemHovered() ) {
        ImGui::SetMouseCursor( to_imgui_cursor( c ) );
    }
}

void set_cursor( cursor c )
{
    ImGui::SetMouseCursor( to_imgui_cursor( c ) );
}

void begin_modal_scope()
{
    ++g_modal_depth;
    // A modal must swallow clicks meant for the windows underneath it.
    ImGui::SetNextFrameWantCaptureMouse( true );
}

void end_modal_scope()
{
    if( g_modal_depth > 0 ) {
        --g_modal_depth;
    }
}

auto modal_open() -> bool
{
    return g_modal_depth > 0;
}

auto begin_drag_source( const char *type ) -> bool
{
    if( !ImGui::BeginDragDropSource( ImGuiDragDropFlags_SourceAllowNullID ) ) {
        return false;
    }
    ImGui::SetMouseCursor( ImGuiMouseCursor_ResizeAll );
    return true;
}

auto accept_drag_target( const char *type ) -> bool
{
    ( void )type;
    return ImGui::BeginDragDropTarget();
}

auto mouse_over_ui() -> bool
{
    return ImGui::GetIO().WantCaptureMouse;
}

} // namespace interaction
} // namespace ui2

#endif // TILES

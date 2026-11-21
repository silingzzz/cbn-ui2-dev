#include "cata_imgui_ui2_style.h"

#if defined( TILES )

#include "cata_imgui_ui2_interaction.h"

namespace ui2
{
namespace style
{

void apply()
{
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.ChildRounding = 4.0f;
    s.FrameRounding = 4.0f;
    s.PopupRounding = 4.0f;
    s.ScrollbarRounding = 4.0f;
    s.GrabRounding = 4.0f;
    s.TabRounding = 4.0f;
    s.WindowBorderSize = 1.0f;
    s.FrameBorderSize = 1.0f;
    s.WindowPadding = ImVec2( 12.0f, 10.0f );
    s.FramePadding = ImVec2( 8.0f, 4.0f );
    s.ItemSpacing = ImVec2( 8.0f, 6.0f );
    s.ScrollbarSize = 12.0f;

    ImVec4 *c = s.Colors;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = text_dim;
    c[ImGuiCol_WindowBg] = panel_bg;
    c[ImGuiCol_ChildBg] = ImVec4( 0.06f, 0.075f, 0.09f, 0.90f );
    c[ImGuiCol_PopupBg] = panel_bg;
    c[ImGuiCol_Border] = border;
    c[ImGuiCol_FrameBg] = ImVec4( 0.043f, 0.055f, 0.066f, 1.00f );
    c[ImGuiCol_FrameBgHovered] = hover_bg;
    c[ImGuiCol_FrameBgActive] = active_bg;
    c[ImGuiCol_TitleBg] = ImVec4( 0.05f, 0.063f, 0.075f, 1.00f );
    c[ImGuiCol_TitleBgActive] = ImVec4( 0.07f, 0.086f, 0.10f, 1.00f );
    c[ImGuiCol_Button] = ImVec4( 0.1098f, 0.1412f, 0.1608f, 1.00f );
    c[ImGuiCol_ButtonHovered] = ImVec4( 0.16f, 0.20f, 0.22f, 1.00f );
    c[ImGuiCol_ButtonActive] = active_bg;
    c[ImGuiCol_Header] = active_bg;
    c[ImGuiCol_HeaderHovered] = hover_bg;
    c[ImGuiCol_HeaderActive] = active_bg;
    c[ImGuiCol_Separator] = border;
    c[ImGuiCol_ScrollbarBg] = ImVec4( 0.03f, 0.04f, 0.05f, 1.00f );
    c[ImGuiCol_ScrollbarGrab] = ImVec4( 0.22f, 0.26f, 0.28f, 1.00f );
    c[ImGuiCol_CheckMark] = accent;
    c[ImGuiCol_SliderGrab] = accent;
    c[ImGuiCol_SliderGrabActive] = ImVec4( 0.60f, 0.78f, 0.52f, 1.00f );
    c[ImGuiCol_Tab] = ImVec4( 0.09f, 0.11f, 0.13f, 1.00f );
    c[ImGuiCol_TabHovered] = hover_bg;
    c[ImGuiCol_TabSelected] = active_bg;
}
} // namespace style
} // namespace ui2

#endif // TILES

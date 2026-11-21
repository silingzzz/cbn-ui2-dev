#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_STYLE_H
#define CATA_SRC_CATA_IMGUI_UI2_STYLE_H

#if defined( TILES )

#include <imgui.h>

namespace ui2
{
namespace style
{

/// Apply the UI 2.0 visual language to the global ImGui style.
/// Call once after ImGui::CreateContext().
void apply();

// Palette (kept in one place so every window looks the same).
inline constexpr ImVec4 panel_bg{ 0.0706f, 0.0863f, 0.1020f, 0.94f };
inline constexpr ImVec4 border{ 0.1647f, 0.1961f, 0.2196f, 1.00f };
inline constexpr ImVec4 text{ 0.9020f, 0.8863f, 0.8471f, 1.00f };
inline constexpr ImVec4 text_dim{ 0.6039f, 0.6392f, 0.6588f, 1.00f };
inline constexpr ImVec4 accent{ 0.4980f, 0.6902f, 0.4118f, 1.00f };
inline constexpr ImVec4 warning{ 0.8510f, 0.6275f, 0.2314f, 1.00f };
inline constexpr ImVec4 danger{ 0.7608f, 0.3333f, 0.2902f, 1.00f };
inline constexpr ImVec4 hover_bg{ 0.1098f, 0.1412f, 0.1608f, 1.00f };
inline constexpr ImVec4 active_bg{ 0.2275f, 0.2902f, 0.2471f, 1.00f };

} // namespace style
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_STYLE_H

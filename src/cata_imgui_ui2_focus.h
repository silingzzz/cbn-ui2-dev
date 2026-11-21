#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_FOCUS_H
#define CATA_SRC_CATA_IMGUI_UI2_FOCUS_H

#if defined( TILES )

namespace ui2
{
namespace focus
{

/// PR-1.5 D batch: widget focus, Tab/Enter/Esc handling, text input and IME.
/// Demo data only; input routing still comes from the PR-1 layer.
void draw();

} // namespace focus
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_FOCUS_H

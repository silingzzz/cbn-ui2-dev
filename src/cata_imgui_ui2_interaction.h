#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_INTERACTION_H
#define CATA_SRC_CATA_IMGUI_UI2_INTERACTION_H

#if defined( TILES )

namespace ui2
{
namespace interaction
{

/// Unified mouse interaction policy for every UI 2.0 window.
///
/// ImGui already implements the hard parts (hit testing, drag, scroll, focus,
/// z-order, menus). This layer exists so windows never re-implement them and so
/// behaviour stays identical across windows:
///   * cursor feedback
///   * modal exclusivity
///   * drag and drop plumbing
///   * hover / press helpers with one definition
///   * a single place that answers "is this region UI?" for input routing.

enum class cursor {
    arrow,        ///< default
    hand,         ///< clickable row / button
    ibeam,        ///< text field
    resize_ew,    ///< horizontal splitter
    resize_ns,    ///< vertical splitter
    drag,         ///< drag in progress
};

/// Apply a cursor while the last item is hovered (no-op otherwise).
void hover_cursor( cursor c );

/// Force a cursor for this frame.
void set_cursor( cursor c );

/// Modal scope: while open, lower windows must not receive clicks.
void begin_modal_scope();
void end_modal_scope();
auto modal_open() -> bool;

/// Drag and drop plumbing (payload lifetimes handled by ImGui).
auto begin_drag_source( const char *type ) -> bool;
auto accept_drag_target( const char *type ) -> bool;

/// True while the pointer is over any UI 2.0 window.
auto mouse_over_ui() -> bool;

} // namespace interaction
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_INTERACTION_H

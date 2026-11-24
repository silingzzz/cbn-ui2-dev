#pragma once
#ifndef CATA_SRC_CATA_IMGUI_H
#define CATA_SRC_CATA_IMGUI_H

#if defined( TILES )

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace cata_imgui
{

/// True when the ImGui context has been created and the feature flag is on.
auto initialized() -> bool;

/// Draw the prototype window on the next frame.
auto render( SDL_Renderer *renderer ) -> void;

/// Feed one SDL event to ImGui.  Returns true if the game should ignore it.
auto process_event( const SDL_Event &event ) -> bool;

/// Create the ImGui context and backends.  No-op when the feature flag is off.
void init( SDL_Window *window, SDL_Renderer *renderer );

void shutdown();

auto is_open() -> bool;
void toggle();

/// True while the overlay window is shown; the game uses this to know whether
/// it should keep pumping frames for the overlay.
auto should_pump_frames() -> bool;

/// One-shot repaint request, set when the overlay stops showing. The host must
/// repaint once so the last presented frame does not keep the window on screen.
auto take_redraw_request() -> bool;

/// True when ImGui is consuming mouse input (hover / drag / popup). Safe to
/// call before init and after shutdown.
auto want_capture_mouse() -> bool;

/// World inspect is the one intentional non-modal mode: the map keeps mouse
/// input while the platform shell stays visible. Outside that mode the overlay
/// remains fully modal. Set by ui2::world::set_open().
void set_mouse_passthrough( bool enabled );

/// True when ImGui holds keyboard focus. Query only: the game's own keyboard
/// handling is deliberately not changed by this prototype.
auto want_capture_keyboard() -> bool;

/// Drop all latched keyboard ownership (focus loss). Without this a key taken
/// by the overlay before losing focus could stay "owned" and swallow its release.
void forget_key_ownership();

/// Drop the focused ImGui widget so the overlay releases keyboard focus
/// (Esc / Enter-commit semantics for forms).
void clear_widget_focus();

/// Temporarily yield the mouse/keyboard to a legacy ncurses popup.
/// The overlay is hidden while the popup is active and restored afterwards.
void begin_legacy_popup();
void end_legacy_popup();

/// RAII helper for legacy modal loops (direction queries, look-around).
struct legacy_popup_scope {
    legacy_popup_scope();
    ~legacy_popup_scope();
    legacy_popup_scope( const legacy_popup_scope & ) = delete;
    legacy_popup_scope &operator=( const legacy_popup_scope & ) = delete;
};

/// Diagnostic counter (only active with CBN_IMGUI_TRACE set): count presented frames.
void trace_present();

/// Diagnostic counter: record a mouse input that was actually handed to the
/// game (0 = move, 1 = click/release, 2 = wheel). Used to prove routing.
void trace_game_mouse( int kind );

} // namespace cata_imgui

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_H

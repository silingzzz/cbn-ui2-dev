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

/// True when ImGui holds keyboard focus. Query only: the game's own keyboard
/// handling is deliberately not changed by this prototype.
auto want_capture_keyboard() -> bool;

/// Drop all latched keyboard ownership (focus loss). Without this a key taken
/// by the overlay before losing focus could stay "owned" and swallow its release.
void forget_key_ownership();

/// Diagnostic counter (only active with CBN_IMGUI_TRACE set): count presented frames.
void trace_present();

/// Diagnostic counter: record a mouse input that was actually handed to the
/// game (0 = move, 1 = click/release, 2 = wheel). Used to prove routing.
void trace_game_mouse( int kind );

} // namespace cata_imgui

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_H

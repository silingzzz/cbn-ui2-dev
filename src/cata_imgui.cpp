#include "cata_imgui.h"
#include "cata_imgui_ui2.h"

#if defined( TILES )

#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#include <SDL3/SDL.h>
#include <imgui.h>
// ClearActiveID() is an internal-API call; there is no public equivalent for
// dropping widget focus, and losing focus is exactly what a closing overlay
// needs (see g_clear_text_focus).
#include <imgui_internal.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include "cata_imgui_ui2_aim.h"
#include "cata_imgui_ui2_interaction.h"
#include "cata_imgui_ui2_style.h"
#include "debug.h"
#include "filesystem.h"
#include "options.h"
#include "output.h"
#include "path_info.h"
#include "translations.h"

namespace
{

bool g_initialized = false;
bool g_open = false;
// Set when the overlay stops showing so the host does one last repaint --
// otherwise the last presented frame keeps the window on screen forever.
bool g_redraw_pending = false;
// Set when the overlay closes while an ImGui widget still owns the keyboard.
// Cleared at the next frame, because ClearActiveID() must run inside a frame.
bool g_clear_text_focus = false;
SDL_Window *g_window = nullptr;
SDL_Renderer *g_renderer = nullptr;

// Legacy ncurses popups (query_popup) need the mouse and keyboard while they
// are running. The overlay yields for their lifetime and is restored after.
int g_legacy_popup_depth = 0;
bool g_legacy_restore_overlay = false;
// After a legacy modal loop returns, the same key/mouse event that dismissed it
// is still in the queue. Skip one overlay input frame so Esc/Enter cannot fall
// through and close the shell that just came back.
int g_resume_guard_frames = 0;

// Prototype widgets only; real UI work starts in PR-2.
char g_text_buffer[128] = "你好, Cataclysm";
int g_slider_value = 50;
bool g_checkbox_value = true;
unsigned long long g_btn_clicks = 0;

// Diagnostic counters, enabled with CBN_IMGUI_TRACE (value = output file path,
// or "1" for ./imgui_trace.log). Purpose: separate "the event never arrived"
// from "toggle ran but no frame was ever drawn" without guessing.
struct trace_counters {
    unsigned long long events = 0;
    unsigned long long motion = 0;
    unsigned long long mouse_down = 0;
    unsigned long long mouse_up = 0;
    unsigned long long wheel = 0;
    unsigned long long f10_keydown = 0;
    unsigned long long toggles = 0;
    unsigned long long process_calls = 0;
    unsigned long long frames = 0;
    unsigned long long renders = 0;
    unsigned long long presents = 0;
    unsigned long long btn_clicks = 0;
    unsigned long long btn_hover = 0;
    // Mouse inputs actually delivered to the game, by kind: 0 move, 1 click, 2 wheel, 3 key down.
    unsigned long long game_mouse[4] = {};
    // KEYDOWN events captured by the overlay in this window.
    unsigned long long key_ui = 0;
    Uint64 last_frame_tick = 0;
    Uint64 frame_gap_max_ms = 0;
    unsigned long long frame_gap_n = 0;
    Uint64 lag_sum_ms = 0;
    unsigned long long lag_n = 0;
    Uint64 pending_event_tick = 0;
    bool has_pending_event = false;
    Uint64 last_log = 0;
};

trace_counters g_trace;
// Last observed window pixel size, for DPI diagnostics.
static int g_dpi_win_w = 0;
static int g_dpi_win_h = 0;

auto trace_on() -> bool
{
    static const bool enabled = SDL_getenv( "CBN_IMGUI_TRACE" ) != nullptr;
    return enabled;
}

void trace_event( const SDL_Event &ev )
{
    if( !trace_on() ) {
        return;
    }
    ++g_trace.events;
    switch( ev.type ) {
        case SDL_EVENT_MOUSE_MOTION:
            ++g_trace.motion;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            ++g_trace.mouse_down;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            ++g_trace.mouse_up;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            ++g_trace.wheel;
            break;
        case SDL_EVENT_KEY_DOWN:
            if( ev.key.key == SDLK_F10 && !ev.key.repeat ) {
                ++g_trace.f10_keydown;
            }
            break;
        default:
            break;
    }
    g_trace.pending_event_tick = SDL_GetTicks();
    g_trace.has_pending_event = true;
}

void trace_frame()
{
    if( !trace_on() ) {
        return;
    }
    ++g_trace.frames;
    const Uint64 now = SDL_GetTicks();
    if( g_trace.last_frame_tick != 0 ) {
        const Uint64 gap = now - g_trace.last_frame_tick;
        if( gap > g_trace.frame_gap_max_ms ) {
            g_trace.frame_gap_max_ms = gap;
        }
        ++g_trace.frame_gap_n;
    }
    g_trace.last_frame_tick = now;
}

void trace_maybe_log()
{
    if( !trace_on() ) {
        return;
    }
    const Uint64 now = SDL_GetTicks();
    if( g_trace.has_pending_event ) {
        g_trace.lag_sum_ms += now - g_trace.pending_event_tick;
        ++g_trace.lag_n;
        g_trace.has_pending_event = false;
    }
    if( g_trace.last_log == 0 ) {
        g_trace.last_log = now;
        return;
    }
    if( now - g_trace.last_log < 1000 ) {
        return;
    }
    g_trace.last_log = now;
    const double lag = g_trace.lag_n > 0 ?
                       static_cast<double>( g_trace.lag_sum_ms ) / static_cast<double>( g_trace.lag_n ) : 0.0;
    float mx = -1.0f;
    float my = -1.0f;
    int want_mouse = -1;
    int want_kb = -1;
    if( ImGui::GetCurrentContext() != nullptr ) {
        const ImGuiIO &io = ImGui::GetIO();
        mx = io.MousePos.x;
        my = io.MousePos.y;
        want_mouse = io.WantCaptureMouse ? 1 : 0;
        want_kb = io.WantCaptureKeyboard ? 1 : 0;
    }
    // DebugLog() is buffered until shutdown, so trace goes to its own flushed file.
    const char *const trace_path = SDL_getenv( "CBN_IMGUI_TRACE" );
    const char *const path = ( trace_path != nullptr && trace_path[0] != '\0' &&
                               std::string( trace_path ) != "1" ) ? trace_path : "imgui_trace.log";
    if( FILE *const f = std::fopen( path, "a" ) ) {
        std::fprintf( f,
                      "IMGUI_TRACE t=%llu ev=%llu mot=%llu down=%llu up=%llu wheel=%llu f10=%llu tog=%llu proc=%llu "
                      "frames=%llu rend=%llu pres=%llu btn=%llu hover=%llu gapMax=%llu gapN=%llu lagMs=%.1f "
                      "open=%d mouse=(%.0f,%.0f) wantMouse=%d wantKb=%d pxW=%d pxH=%d gmove=%llu gclick=%llu gwheel=%llu gkey=%llu kui=%llu\n",
                      static_cast<unsigned long long>( now ), g_trace.events, g_trace.motion, g_trace.mouse_down,
                      g_trace.mouse_up, g_trace.wheel, g_trace.f10_keydown, g_trace.toggles, g_trace.process_calls,
                      g_trace.frames, g_trace.renders, g_trace.presents, g_trace.btn_clicks, g_trace.btn_hover,
                      static_cast<unsigned long long>( g_trace.frame_gap_max_ms ), g_trace.frame_gap_n, lag,
                      g_open ? 1 : 0, mx, my, want_mouse, want_kb, g_dpi_win_w, g_dpi_win_h,
                      g_trace.game_mouse[0], g_trace.game_mouse[1], g_trace.game_mouse[2],
                      g_trace.game_mouse[3], g_trace.key_ui );
        std::fclose( f );
    }
    g_trace.game_mouse[0] = g_trace.game_mouse[1] = g_trace.game_mouse[2] = 0;
    g_trace.game_mouse[3] = 0;
    g_trace.key_ui = 0;
    g_trace.events = g_trace.motion = g_trace.mouse_down = g_trace.mouse_up = 0;
    g_trace.wheel = g_trace.f10_keydown = g_trace.toggles = g_trace.process_calls = 0;
    g_trace.frames = g_trace.renders = g_trace.presents = 0;
    g_trace.btn_clicks = g_trace.btn_hover = 0;
    g_trace.frame_gap_max_ms = 0;
    g_trace.frame_gap_n = 0;
    g_trace.lag_sum_ms = 0;
    g_trace.lag_n = 0;
}

// Keyboard ownership, indexed by SDL scancode. Latching the owner at KEYDOWN is
// what keeps DOWN/UP consistent: re-querying WantCaptureKeyboard on release
// would misroute a key when focus or the overlay state changed while it was
// held.
enum key_owner : int8_t {
    KEY_OWNER_NONE = 0,
    KEY_OWNER_IMGUI = 1,
    KEY_OWNER_GAME = 2,
};
static int8_t key_owner_table[SDL_SCANCODE_COUNT] = {};

auto key_slot( SDL_Scancode sc ) -> size_t
{
    const int idx = static_cast<int>( sc );
    return ( idx >= 0 && idx < SDL_SCANCODE_COUNT ) ? static_cast<size_t>( idx ) : 0;
}

auto load_cjk_font() -> void
{
    // Prototype font source order: explicit override, game fonts, CJK fallback
    // shipped in data/font (unifont covers CJK).
    std::vector<std::string> candidates;
    if( const char *override_font = SDL_getenv( "CBN_IMGUI_FONT" ) ) {
        candidates.emplace_back( override_font );
    }
    candidates.emplace_back( PATH_INFO::user_fontdir() + "ZhenKai_GBK-Regular.ttf" );
    candidates.emplace_back( PATH_INFO::fontdir() + "ZhenKai_GBK-Regular.ttf" );
    candidates.emplace_back( PATH_INFO::fontdir() + "unifont.ttf" );
    candidates.emplace_back( PATH_INFO::fontdir() + "Terminus.ttf" );

    ImGuiIO &io = ImGui::GetIO();
    for( const std::string &path : candidates ) {
        if( !file_exist( path ) ) {
            continue;
        }
        ImFontConfig cfg;
        cfg.MergeMode = false;
        if( io.Fonts->AddFontFromFileTTF( path.c_str(), 18.0f, &cfg,
                                          io.Fonts->GetGlyphRangesChineseSimplifiedCommon() ) != nullptr ) {
            DebugLog( DL::Info, DC::Main ) << "ImGui prototype: font loaded from " << path;
            return;
        }
    }
    DebugLog( DL::Warn, DC::Main ) << "ImGui prototype: no CJK font found, using built-in font";
}

auto draw_prototype_window() -> void
{
    ImGui::SetNextWindowSize( ImVec2( 420.0f, 260.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "CBN UI 2.0 prototype", &g_open ) ) {
        ImGui::End();
        return;
    }

    ImGui::Text( "帧率: %.1f FPS", ImGui::GetIO().Framerate );
    ImGui::Separator();
    // Oversized target button: PR-1 fault isolation needs a hit test that cannot
    // fail for reasons of tiny geometry. The press counter is the evidence.
    if( ImGui::Button( "BIG TEST BUTTON", ImVec2( 380.0f, 120.0f ) ) ) {
        ++g_btn_clicks;
        if( trace_on() ) {
            ++g_trace.btn_clicks;
        }
    }
    if( trace_on() && ImGui::IsItemHovered() ) {
        ++g_trace.btn_hover;
    }
    ImGui::Text( "按钮点击: %llu 次", g_btn_clicks );
    ImGui::InputText( "文本输入 (IME)", g_text_buffer, IM_ARRAYSIZE( g_text_buffer ) );
    ImGui::Checkbox( "复选框", &g_checkbox_value );
    ImGui::SliderInt( "滑块", &g_slider_value, 0, 100 );
    if( ImGui::Button( "关闭" ) ) {
        g_open = false;
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "F10 切换显示" );
    ImGui::End();
}

} // namespace

namespace cata_imgui
{

auto initialized() -> bool
{
    return g_initialized;
}

auto is_open() -> bool
{
    return g_open;
}

void toggle()
{
    g_open = !g_open;
    if( trace_on() ) {
        ++g_trace.toggles;
    }
    if( g_open ) {
        SDL_StartTextInput( g_window );
        // Reopening the overlay must never leave the demo windows unreachable.
        ui2::reset();
    } else {
        SDL_StopTextInput( g_window );
        g_redraw_pending = true;
        // A focused text widget would otherwise keep eating keystrokes after the
        // overlay is gone; the input manager has no other way to drop that focus.
        g_clear_text_focus = true;
        // Keys pressed while the overlay was open must not keep it as their owner.
        forget_key_ownership();
    }
}

void init( SDL_Window *window, SDL_Renderer *renderer )
{
    if( g_initialized || window == nullptr || renderer == nullptr ) {
        return;
    }
    // Feature flag lives in the options menu; the environment variable is a
    // developer hook so the prototype can be exercised without a saved world.
    const bool enabled_by_env = SDL_getenv( "CBN_IMGUI" ) != nullptr;
    if( !enabled_by_env && !get_option<bool>( "IMGUI_PROTOTYPE" ) ) {
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    // The project palette must win over the framework default, so apply it last.
    ImGui::StyleColorsDark();
    ::ui2::style::apply();

    // Font atlas must exist before the renderer backend builds its texture.
    load_cjk_font();

    if( !ImGui_ImplSDL3_InitForSDLRenderer( window, renderer ) ) {
        DebugLog( DL::Error, DC::Main ) << "ImGui prototype: SDL3 backend init failed";
        ImGui::DestroyContext();
        return;
    }
    if( !ImGui_ImplSDLRenderer3_Init( renderer ) ) {
        DebugLog( DL::Error, DC::Main ) << "ImGui prototype: SDLRenderer3 backend init failed";
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return;
    }

    g_window = window;
    g_renderer = renderer;
    g_initialized = true;
    g_open = enabled_by_env;
    DebugLog( DL::Info, DC::Main ) << "ImGui prototype: initialized";
}

void shutdown()
{
    if( !g_initialized ) {
        return;
    }
    if( trace_on() ) {
        // Proof that the teardown actually ran on the normal quit path; written
        // and flushed immediately because the process is about to exit.
        const char *const trace_path = SDL_getenv( "CBN_IMGUI_TRACE" );
        const char *const path = ( trace_path != nullptr && trace_path[0] != '\0' &&
                                   std::string( trace_path ) != "1" ) ? trace_path : "imgui_trace.log";
        if( FILE *const f = std::fopen( path, "a" ) ) {
            std::fprintf( f, "IMGUI_SHUTDOWN renderer_backend=%d platform_backend=%d\n", 1, 1 );
            std::fclose( f );
        }
    }
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    g_initialized = false;
    g_open = false;
    g_legacy_popup_depth = 0;
    g_legacy_restore_overlay = false;
}

auto process_event( const SDL_Event &event ) -> bool
{
    if( !g_initialized ) {
        return false;
    }

    trace_event( event );
    if( trace_on() ) {
        ++g_trace.process_calls;
    }

    // Mouse coordinates arrive in *window* space, while ImGui draws in renderer
    // *logical* space. When SCALING_FACTOR > 1 the two differ (logical
    // presentation), so translate before handing the event over -- otherwise
    // every hit test is off by the scale factor. When logical presentation is
    // disabled the coordinates already match, so the event is forwarded as-is.
    SDL_Event converted{};
    const SDL_Event *to_feed = &event;
    const bool is_mouse = event.type == SDL_EVENT_MOUSE_MOTION ||
                          event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                          event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                          event.type == SDL_EVENT_MOUSE_WHEEL;
    // Always translate: SDL's window coordinate space is not the renderer's
    // coordinate space. They differ under SCALING_FACTOR logical presentation
    // *and* under a non-100% display scale, where SDL reports window coords
    // scaled relative to the render output (observed 1.25x at 125% DPI).
    // SDL_RenderCoordinatesFromWindow is the supported conversion for both.
    if( is_mouse && g_renderer != nullptr ) {
        {
            float wx = 0.0f;
            float wy = 0.0f;
            if( event.type == SDL_EVENT_MOUSE_WHEEL ) {
                wx = event.wheel.mouse_x;
                wy = event.wheel.mouse_y;
            } else if( event.type == SDL_EVENT_MOUSE_MOTION ) {
                wx = event.motion.x;
                wy = event.motion.y;
            } else {
                wx = event.button.x;
                wy = event.button.y;
            }
            float rx = 0.0f;
            float ry = 0.0f;
            if( SDL_RenderCoordinatesFromWindow( g_renderer, wx, wy, &rx, &ry ) ) {

                // Record the pixel size for diagnostics. NOTE: under DPI
                // virtualisation SDL reports the same virtualised size for both
                // window and pixels (e.g. 2560x1344 for a ~2048px client), so the
                // true scale CANNOT be derived here -- the process has to declare
                // DPI awareness before SDL_Init for coordinates to line up.
                int win_w = 0;
                int win_h = 0;
                SDL_GetWindowSizeInPixels( g_window, &win_w, &win_h );
                g_dpi_win_w = win_w;
                g_dpi_win_h = win_h;
                converted = event;
                if( event.type == SDL_EVENT_MOUSE_WHEEL ) {
                    converted.wheel.mouse_x = rx;
                    converted.wheel.mouse_y = ry;
                } else if( event.type == SDL_EVENT_MOUSE_MOTION ) {
                    converted.motion.x = rx;
                    converted.motion.y = ry;
                } else {
                    converted.button.x = rx;
                    converted.button.y = ry;
                }
                to_feed = &converted;
            }
        }
    }

    ImGui_ImplSDL3_ProcessEvent( to_feed );
    if( trace_on() ) {
        trace_maybe_log();
    }

    if( event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F10 ) {
        // While a legacy popup owns input, F10 must not pull the overlay back
        // over it.
        if( g_legacy_popup_depth > 0 ) {
            return true;
        }
        // F10 toggles the overlay and belongs to it for this press (including
        // auto-repeat), so the game never sees the key up or the repeats.
        if( !event.key.repeat ) {
            toggle();
        }
        key_owner_table[key_slot( event.key.scancode )] = KEY_OWNER_IMGUI;
        return true;
    }

    // F10 above is the only key handled unconditionally. Everything else is
    // routed by ownership latched at KEYDOWN, so a key never changes owner while
    // it is held.
    const ImGuiIO &io = ImGui::GetIO();
    switch( event.type ) {
        case SDL_EVENT_KEY_DOWN: {
            const size_t slot = key_slot( event.key.scancode );
            // The UI 2.0 overlay is modal: while it is open, the game must not
            // act on keys that ImGui did not route to a text field. This also
            // keeps Space/Enter from leaking to the paused game when a
            // checkbox or button has focus.
            const bool captured = g_open;
            key_owner_table[slot] = captured ? KEY_OWNER_IMGUI : KEY_OWNER_GAME;
            if( trace_on() && captured ) {
                ++g_trace.key_ui;
            }
            return captured;
        }
        case SDL_EVENT_KEY_UP: {
            const size_t slot = key_slot( event.key.scancode );
            const int8_t owner = key_owner_table[slot];
            key_owner_table[slot] = KEY_OWNER_NONE;
            // A release whose press was lost (focus change, overlay toggled
            // while held) falls through to the game: that is the pre-overlay
            // behaviour and avoids both stuck keys and orphan releases.
            return owner == KEY_OWNER_IMGUI;
        }
        case SDL_EVENT_TEXT_INPUT:
        case SDL_EVENT_TEXT_EDITING:
            // Text entry is governed by WantTextInput, which is narrower than
            // WantCaptureKeyboard (a focused button wants keys, not characters).
            return g_open && io.WantTextInput;
        default:
            break;
    }

    if( !g_open ) {
        return false;
    }

    switch( event.type ) {
        case SDL_EVENT_MOUSE_MOTION:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_WHEEL:
            return io.WantCaptureMouse;
        default:
            return false;
    }
}

void clear_widget_focus()
{
    if( g_initialized && ImGui::GetCurrentContext() != nullptr ) {
        ImGui::ClearActiveID();
    }
}

void forget_key_ownership()
{
    for( int8_t &owner : key_owner_table ) {
        owner = KEY_OWNER_NONE;
    }
}

void begin_legacy_popup()
{
    ++g_legacy_popup_depth;
    if( g_legacy_popup_depth != 1 ) {
        return;
    }
    g_legacy_restore_overlay = g_open;
    if( g_open ) {
        // A legacy query popup is about to read input. Hide the overlay so its
        // mouse clicks and keys reach the popup, not the ImGui shell.
        g_open = false;
        g_redraw_pending = true;
        g_clear_text_focus = true;
        forget_key_ownership();
    }
}

void end_legacy_popup()
{
    if( g_legacy_popup_depth == 0 ) {
        return;
    }
    --g_legacy_popup_depth;
    if( g_legacy_popup_depth != 0 ) {
        return;
    }
    if( g_legacy_restore_overlay ) {
        g_legacy_restore_overlay = false;
        g_open = true;
        g_redraw_pending = true;
        g_resume_guard_frames = 1;
    }
}

legacy_popup_scope::legacy_popup_scope()
{
    begin_legacy_popup();
}

legacy_popup_scope::~legacy_popup_scope()
{
    end_legacy_popup();
}

auto render( SDL_Renderer *renderer ) -> void
{
    if( !g_initialized || renderer != g_renderer ) {
        return;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    // ImGui draws into the renderer's *logical* coordinate space, which differs
    // from the window's pixel size whenever SCALING_FACTOR > 1 sets up logical
    // presentation (see SDL_SetRenderLogicalPresentation in sdltiles.cpp).
    // Never trust the backend default here: it reports output (pixel) size.
    {
        ImGuiIO &io = ImGui::GetIO();
        int lw = 0;
        int lh = 0;
        SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
        if( SDL_GetRenderLogicalPresentation( renderer, &lw, &lh, &mode ) && lw > 0 && lh > 0 ) {
            io.DisplaySize = ImVec2( static_cast<float>( lw ), static_cast<float>( lh ) );
        } else if( SDL_GetRenderOutputSize( renderer, &lw, &lh ) && lw > 0 && lh > 0 ) {
            io.DisplaySize = ImVec2( static_cast<float>( lw ), static_cast<float>( lh ) );
        }
        io.DisplayFramebufferScale = ImVec2( 1.0f, 1.0f );
    }

    ImGui::NewFrame();
    trace_frame();
    if( g_clear_text_focus ) {
        // Drop any focused widget so a closed overlay stops consuming keys.
        ImGui::ClearActiveID();
        g_clear_text_focus = false;
    }
    const bool drawn_open = g_open;
    if( g_resume_guard_frames > 0 ) {
        --g_resume_guard_frames;
        g_redraw_pending = true;
    } else if( g_open ) {
        // Diagnostic prototype widgets are opt-in; the default overlay is the
        // UI 2.0 shell plus its business windows.
        const char *const prototype = SDL_getenv( "CBN_IMGUI_PROTOTYPE" );
        if( prototype != nullptr && std::strcmp( prototype, "0" ) != 0 ) {
            draw_prototype_window();
        }
        // PR-1.5 component demo, kept out of the PR-1 diagnostic window.
        ui2::draw();
    }
    // The window can also be closed from inside the frame (title bar X or the
    // 关闭 button). Schedule one repaint so the pixels actually go away, and the
    // focus drop as well -- that repaint is the last frame the overlay runs.
    if( drawn_open && !g_open ) {
        g_redraw_pending = true;
        g_clear_text_focus = true;
    }
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData( ImGui::GetDrawData(), renderer );
    if( trace_on() ) {
        ++g_trace.renders;
        trace_maybe_log();
    }
}

auto should_pump_frames() -> bool
{
    return g_initialized && g_open;
}

auto take_redraw_request() -> bool
{
    const bool pending = g_redraw_pending;
    g_redraw_pending = false;
    return pending;
}

auto want_capture_mouse() -> bool
{
    return g_open && g_initialized && ImGui::GetCurrentContext() != nullptr &&
           ImGui::GetIO().WantCaptureMouse;
}

auto want_capture_keyboard() -> bool
{
    // The shell is a modal layer while it is visible. ImGui reports
    // WantCaptureKeyboard=false when only buttons exist (no text field or
    // active widget), which let gameplay keys such as Space leak through and
    // trigger actions behind the overlay. While open, the platform owns the
    // keyboard; legacy popup scopes hide the overlay first.
    return g_open && g_initialized;
}

void trace_game_mouse( int kind )
{
    if( trace_on() && kind >= 0 && kind < 4 ) {
        ++g_trace.game_mouse[kind];
    }
}

void trace_present()
{
    if( trace_on() ) {
        ++g_trace.presents;
        trace_maybe_log();
    }
}

} // namespace cata_imgui

#endif // TILES

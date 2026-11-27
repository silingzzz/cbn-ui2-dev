#include "cata_imgui_ui2.h"
#include "cata_imgui_ui2_lists.h"
#include "cata_imgui_ui2_menu.h"
#include "cata_imgui_ui2_focus.h"
#include "cata_imgui_ui2_aim.h"
#include "cata_imgui_ui2_craft.h"
#include "cata_imgui_ui2_gear.h"
#include "cata_imgui_ui2_messages.h"
#include "cata_imgui_ui2_vehicle.h"
#include "cata_imgui_ui2_zone.h"
#include "cata_imgui_ui2_shell.h"

#if defined( TILES )

#include <cstdlib>
#include <cstring>

#include <imgui.h>

#include "cata_imgui_ui2_map.h"
#include "cata_imgui_ui2_world.h"

namespace
{

// ---------------------------------------------------------------------------
// PR-1.5 A batch: window & tab components from ImGui's native toolkit.
// All state here is DEMO-ONLY: nothing reads or writes game data.
// ---------------------------------------------------------------------------

bool g_main_open = true;
bool g_second_open = true;
bool g_controls_open = true;

int g_tab_index = 0;
int g_tab_switch_count = 0;

bool g_demo_check = false;
float g_demo_slider = 0.5f;
bool g_slider_moved = false;
int g_slider_moves = 0;

const char *const k_tab_names[] = { "窗口", "标签页", "说明" };
constexpr int k_tab_count = 3;

void track_tab( int index )
{
    if( g_tab_index != index ) {
        g_tab_index = index;
        ++g_tab_switch_count;
    }
}

void draw_windows_tab()
{
    ImGui::TextWrapped( "拖动标题栏移动窗口；拖动窗口边缘或右下角调整大小（ImGui 原生行为，无需自研）。" );
    ImGui::Separator();

    ImGui::Checkbox( "显示示例子窗口", &g_second_open );
    ImGui::Checkbox( "例选项（演示数据）", &g_demo_check );

    const bool changed = ImGui::SliderFloat( "例滑块（演示数据）", &g_demo_slider, 0.0f, 1.0f, "%.2f" );
    if( changed ) {
        g_slider_moved = true;
        ++g_slider_moves;
    }

    ImGui::Separator();
    ImGui::Text( "多窗口状态：主窗口=%s 子窗口=%s 控制条=%s",
                 g_main_open ? "开" : "关",
                 g_second_open ? "开" : "关",
                 g_controls_open ? "开" : "关" );
    ImGui::TextDisabled( "键盘路径：Tab / Shift+Tab 切换焦点，Enter 激活，Esc 关闭弹出层。" );
}

void draw_tabs_tab()
{
    ImGui::TextWrapped( "鼠标点击上方标签切换；Ctrl+Tab / Ctrl+Shift+Tab 为 ImGui 原生键盘路径。" );
    ImGui::Separator();
    ImGui::Text( "当前标签：%s（索引 %d / %d）", k_tab_names[g_tab_index], g_tab_index, k_tab_count );
    ImGui::Text( "累计切换次数：%d", g_tab_switch_count );
    if( ImGui::Button( "下一个标签" ) ) {
        track_tab( ( g_tab_index + 1 ) % k_tab_count );
    }
    ImGui::SameLine();
    if( ImGui::Button( "上一个标签" ) ) {
        track_tab( ( g_tab_index + k_tab_count - 1 ) % k_tab_count );
    }
}

void draw_about_tab()
{
    ImGui::TextWrapped( "PR-1.5 A 批次：窗口与标签页组件。" );
    ImGui::BulletText( "全部为演示数据，不接入游戏业务。" );
    ImGui::BulletText( "组件位于 cata_imgui_ui2.{h,cpp}，与 PR-1 原型窗口分离。" );
    ImGui::BulletText( "输入复用 PR-1 的分流层，未新建输入系统。" );
    ImGui::Separator();
    ImGui::Text( "滑动状态：%s（累计 %d 次）", g_slider_moved ? "已操作" : "未操作", g_slider_moves );
}

void draw_main_window()
{
    if( !g_main_open ) {
        return;
    }
    ImGui::SetNextWindowPos( ImVec2( 60.0f, 300.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 520.0f, 330.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "UI 2.0 组件演示 (PR-1.5)", &g_main_open ) ) {
        ImGui::End();
        return;
    }
    if( ImGui::BeginTabBar( "ui2_tabs" ) ) {
        // BeginTabItem must be called for every tab in order each frame; the one
        // returning true is the selected tab.
        bool selected[k_tab_count] = {};
        selected[0] = ImGui::BeginTabItem( k_tab_names[0] );
        if( selected[0] ) {
            draw_windows_tab();
            ImGui::EndTabItem();
        }
        selected[1] = ImGui::BeginTabItem( k_tab_names[1] );
        if( selected[1] ) {
            draw_tabs_tab();
            ImGui::EndTabItem();
        }
        selected[2] = ImGui::BeginTabItem( k_tab_names[2] );
        if( selected[2] ) {
            draw_about_tab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();

        int now = 0;
        for( int i = 0; i < k_tab_count; ++i ) {
            if( selected[i] ) {
                now = i;
                break;
            }
        }
        if( now != g_tab_index ) {
            g_tab_index = now;
            ++g_tab_switch_count;
        }
    }
    ImGui::End();
}

void draw_second_window()
{
    if( !g_second_open ) {
        return;
    }
    ImGui::SetNextWindowPos( ImVec2( 620.0f, 300.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 320.0f, 240.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "示例子窗口", &g_second_open ) ) {
        ImGui::End();
        return;
    }
    ImGui::TextWrapped( "独立窗口：验证多窗口并存与焦点隔离。" );
    ImGui::Separator();
    ImGui::TextDisabled( "仍是演示数据。" );
    if( ImGui::Button( "关闭子窗口" ) ) {
        g_second_open = false;
    }
    ImGui::SameLine();
    if( ImGui::Button( "关闭主窗口" ) ) {
        g_main_open = false;
    }
    ImGui::End();
}

// The PR-1.5 demo windows are scaffolding: they all open at once, stack above
// the real business windows and swallow clicks aimed at them. When testing the
// business UI set CBN_IMGUI_DEMO=0 to draw only the real windows.
auto demo_windows_enabled() -> bool
{
    static const bool enabled = []() {
        const char *const value = std::getenv( "CBN_IMGUI_DEMO" );
        return value != nullptr && std::strcmp( value, "1" ) == 0;
    }();
    return enabled;
}

void draw_controls_window()
{
    if( !g_controls_open ) {
        return;
    }
    ImGui::SetNextWindowPos( ImVec2( 60.0f, 180.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "组件开关 (PR-1.5)", &g_controls_open, ImGuiWindowFlags_AlwaysAutoResize ) ) {
        ImGui::End();
        return;
    }
    ImGui::Checkbox( "主窗口", &g_main_open );
    ImGui::SameLine();
    ImGui::Checkbox( "示例子窗口", &g_second_open );
    ImGui::Text( "标签：%s（切换 %d 次）", k_tab_names[g_tab_index], g_tab_switch_count );
    // PR-2A AIM-2 development preview: real backpack plus inventory move.
    if( ImGui::Button( "打开新版 AIM（开发预览）" ) ) {
        ui2::shell::open( ui2::shell::panel::aim );
    }
    ImGui::TextDisabled( "关掉组件后可用这里恢复。" );
    ImGui::End();
}

} // namespace

namespace ui2
{

void reset()
{
    g_main_open = true;
    g_second_open = true;
    g_controls_open = true;
    // Business windows keep their open state across F10 hide/show. Hiding the
    // overlay changes input ownership and pixels only; it must not silently
    // reset the panel the player was working in.
}

void reset_session()
{
    ui2::gear::reset_session();
    ui2::craft::reset_session();
    ui2::messages::reset_session();
    ui2::veh::reset_session();
    ui2::world::reset_session();
}

void draw()
{
    // Platform layer first: it owns navigation and consumes Escape before the
    // business windows draw their own frame.
    shell::draw();
    if( demo_windows_enabled() ) {
        draw_controls_window();
        draw_main_window();
        draw_second_window();
        // PR-1.5 B batch: list and table components.
        lists::draw();
        // PR-1.5 C batch: context menu, tooltip and toast feedback.
        menu::draw();
        // PR-1.5 D batch: focus, keyboard and text input.
        focus::draw();
    }
    // PR-2A AIM-1: read-only real backpack window.
    ui2::aim::draw();
    // PR-3A: read-only worn/wielded equipment and nested containers.
    ui2::gear::draw();
    // PR-4A: recipe browser with a direct path into the existing craft flow.
    ui2::craft::draw();
    // PR-5A: read-only zone inspector with a deferred enable toggle.
    ui2::zone::draw();
    // Message history viewer with a mouse-driven list and detail pane.
    ui2::messages::draw();
    // World inspect: non-modal map selection plus a small action panel.
    ui2::world::draw();
    // P3 Vehicle: read-only nearby vehicle overview.
    ui2::veh::draw();
    // PR-6A: hover card for the tile under the game cursor.
    ui2::mapview::draw();
}

} // namespace ui2

#endif // TILES

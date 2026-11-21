#include "cata_imgui_ui2_focus.h"

#if defined( TILES )

#include <cstdio>
#include <string>

#include <imgui.h>

#include "cata_imgui.h"

namespace
{

// ---------------------------------------------------------------------------
// PR-1.5 D batch: focus / keyboard / text-input components.
// Demo data only. Input routing is NOT reimplemented here -- PR-1 already
// routes SDL events into ImGui; this file only exercises focus behaviour.
// ---------------------------------------------------------------------------

bool g_window_open = true;
char g_name[64] = "演示文本";
int g_amount = 3;
bool g_enabled = true;
int g_choice = 0;
int g_focus_events = 0;
std::string g_last_action = "（无）";
std::string g_focused_item = "（无）";

const char *const k_choices[] = { "演示选项 A", "演示选项 B", "演示选项 C" };

void note( const char *action )
{
    g_last_action = action;
    ++g_focus_events;
}

void track_focus( const char *what )
{
    if( ImGui::IsItemFocused() ) {
        g_focused_item = what;
    }
}

void draw_form()
{
    ImGui::TextUnformatted( "Tab / Shift+Tab 切换焦点；Enter 确认；Esc 取消（释放焦点）。" );
    ImGui::Separator();

    if( ImGui::Button( "聚焦到「名称」" ) ) {
        ImGui::SetKeyboardFocusHere( 2 );   // jump to the input below
        note( "请求聚焦名称框" );
    }
    ImGui::SetNextItemWidth( 240.0f );
    const bool name_changed = ImGui::InputText( "名称", g_name, IM_ARRAYSIZE( g_name ) );
    track_focus( "名称" );
    if( name_changed ) {
        note( "名称已修改" );
    }

    ImGui::SetNextItemWidth( 120.0f );
    if( ImGui::InputInt( "数量", &g_amount ) ) {
        note( "数量已修改" );
    }
    track_focus( "数量" );

    if( ImGui::Checkbox( "启用（演示）", &g_enabled ) ) {
        note( "开关已切换" );
    }
    track_focus( "启用开关" );

    ImGui::TextUnformatted( "选项列表：" );
    for( int i = 0; i < 3; ++i ) {
        if( ImGui::Selectable( k_choices[i], g_choice == i ) ) {
            g_choice = i;
            note( "已选择列表项" );
        }
        track_focus( k_choices[i] );
    }

    ImGui::Separator();
    if( ImGui::Button( "确定" ) ) {
        note( "确定" );
    }
    ImGui::SameLine();
    if( ImGui::Button( "取消" ) ) {
        note( "取消" );
    }
}

void handle_keys( ImGuiIO &io )
{
    // Esc: drop focus so the overlay stops owning the keyboard.
    if( ImGui::IsKeyPressed( ImGuiKey_Escape, false ) ) {
        cata_imgui::clear_widget_focus();
        note( "Esc：释放焦点" );
    }
    // Enter while a text field is active: commit and move focus away, which is
    // the behaviour a form is expected to have.
    if( io.WantTextInput && ImGui::IsKeyPressed( ImGuiKey_Enter, false ) ) {
        cata_imgui::clear_widget_focus();
        note( "Enter：提交文本并释放焦点" );
    }
}

} // namespace

namespace ui2
{
namespace focus
{

void draw()
{
    ImGui::SetNextWindowPos( ImVec2( 1300.0f, 300.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 420.0f, 420.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "焦点与输入 (PR-1.5 D)", &g_window_open ) ) {
        ImGui::End();
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    handle_keys( io );

    ImGui::Text( "ImGui 想要键盘：%s　文本输入：%s",
                 io.WantCaptureKeyboard ? "是" : "否",
                 io.WantTextInput ? "是" : "否" );
    ImGui::TextDisabled( "焦点在：%s", g_focused_item.c_str() );
    ImGui::Separator();

    draw_form();

    ImGui::Separator();
    ImGui::TextDisabled( "最近动作：%s（累计 %d 次）", g_last_action.c_str(), g_focus_events );
    ImGui::TextDisabled( "全部为演示数据；输入分流沿用 PR-1。" );

    ImGui::End();
}

} // namespace focus
} // namespace ui2

#endif // TILES

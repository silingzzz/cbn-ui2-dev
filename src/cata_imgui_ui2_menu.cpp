#include "cata_imgui_ui2_menu.h"

#if defined( TILES )

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

namespace
{

// ---------------------------------------------------------------------------
// PR-1.5 C batch: context menu, tooltip and toast feedback components.
// Every row and message below is DEMO DATA. Nothing here touches game state.
// ---------------------------------------------------------------------------

enum class toast_level : int {
    success = 0,
    info = 1,
    warning = 2,
    error = 3,
};

struct toast_msg {
    std::string text;
    toast_level level = toast_level::info;
    Uint64 expire_tick = 0;
};

struct demo_entry {
    std::string name;
    std::string category;
    int count = 0;
    bool locked = false;
};

bool g_window_open = true;
int g_selected = -1;
int g_pending_delete = -1;
int g_action_count = 0;
std::vector<demo_entry> g_entries;
std::vector<toast_msg> g_toasts;

constexpr size_t k_max_toasts = 4;
constexpr Uint64 k_toast_ms = 4000;

void ensure_entries()
{
    if( !g_entries.empty() ) {
        return;
    }
    static const char *const names[] = { "演示条目 · 药品箱", "演示条目 · 工具箱", "演示条目 · 水壶",
                                         "演示条目 · 手电筒", "演示条目 · 电池组", "演示条目 · 急救包"
                                       };
    static const char *const cats[] = { "药品", "工具", "生活", "电子", "能源", "医疗" };
    for( int i = 0; i < 6; ++i ) {
        demo_entry e;
        e.name = names[i];
        e.category = cats[i];
        e.count = ( i + 1 ) * 3;
        // One locked row demonstrates a clearly disabled menu entry.
        e.locked = ( i == 2 );
        g_entries.push_back( e );
    }
}

void push_toast( toast_level level, const std::string &text )
{
    toast_msg t;
    t.text = text;
    t.level = level;
    t.expire_tick = SDL_GetTicks() + k_toast_ms;
    g_toasts.push_back( t );
    // Bounded queue: never let notifications pile up and cover the UI.
    while( g_toasts.size() > k_max_toasts ) {
        g_toasts.erase( g_toasts.begin() );
    }
}

auto level_color( toast_level level ) -> ImVec4
{
    switch( level ) {
        case toast_level::success:
            return ImVec4( 0.45f, 0.85f, 0.45f, 1.0f );
        case toast_level::info:
            return ImVec4( 0.55f, 0.75f, 1.00f, 1.0f );
        case toast_level::warning:
            return ImVec4( 1.00f, 0.80f, 0.35f, 1.0f );
        case toast_level::error:
        default:
            return ImVec4( 1.00f, 0.45f, 0.45f, 1.0f );
    }
}

auto level_name( toast_level level ) -> const char *
{
    switch( level ) {
        case toast_level::success:
            return "成功";
        case toast_level::info:
            return "信息";
        case toast_level::warning:
            return "警告";
        case toast_level::error:
        default:
            return "失败";
    }
}

void draw_toasts()
{
    const Uint64 now = SDL_GetTicks();
    g_toasts.erase( std::remove_if( g_toasts.begin(), g_toasts.end(),
    [now]( const toast_msg & t ) {
        return now >= t.expire_tick;
    } ), g_toasts.end() );
    if( g_toasts.empty() ) {
        return;
    }

    // NOTE: io.DisplaySize is reported ~1.25x larger than the visible window
    // under 125% DPI (same mismatch that affects mouse injection), so anchoring
    // to the viewport's bottom-right would place this window off-screen. Use a
    // fixed, conservatively visible anchor in the top-right instead.
    ImGui::SetNextWindowPos( ImVec2( 1480.0f, 60.0f ), ImGuiCond_Always, ImVec2( 1.0f, 0.0f ) );
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
    if( ImGui::Begin( "##ui2_toasts", nullptr, flags ) ) {
        for( size_t i = 0; i < g_toasts.size(); ++i ) {
            const toast_msg &t = g_toasts[i];
            ImGui::PushStyleColor( ImGuiCol_Text, level_color( t.level ) );
            ImGui::Text( "[%s] %s", level_name( t.level ), t.text.c_str() );
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

void open_context_menu_actions( int index )
{
    const demo_entry &e = g_entries[index];
    ImGui::TextDisabled( "%s", e.name.c_str() );
    ImGui::Separator();

    if( ImGui::MenuItem( "查看详情" ) ) {
        char buf[160];
        std::snprintf( buf, sizeof( buf ), "%s ｜ %s ｜ 数量 %d", e.name.c_str(), e.category.c_str(), e.count );
        push_toast( toast_level::info, buf );
        ++g_action_count;
    }
    if( ImGui::MenuItem( "复制名称（演示）" ) ) {
        push_toast( toast_level::success, "已复制：" + e.name );
        ++g_action_count;
    }
    if( ImGui::MenuItem( "标记为警告（演示）" ) ) {
        push_toast( toast_level::warning, "该条目待复核：" + e.name );
        ++g_action_count;
    }
    ImGui::Separator();
    // Disabled entries stay visible but clearly greyed out and inert.
    if( ImGui::MenuItem( "锁定条目操作", nullptr, false, !e.locked ) ) {
        push_toast( toast_level::success, "锁定条目操作已执行：" + e.name );
        ++g_action_count;
    }
    if( e.locked ) {
        ImGui::TextDisabled( "（此条目已锁定，相关操作不可用）" );
    }
    ImGui::Separator();
    // Irreversible action: never executes on a single click.
    if( ImGui::MenuItem( "删除…（需要确认）" ) ) {
        g_pending_delete = index;
        ImGui::OpenPopup( "确认删除" );
    }
    if( ImGui::MenuItem( "取消" ) ) {
        // closing the popup is implicit; keep the entry for clarity
    }
}

void draw_confirm_modal()
{
    ImGui::SetNextWindowSize( ImVec2( 360.0f, 0.0f ), ImGuiCond_Appearing );
    if( !ImGui::BeginPopupModal( "确认删除", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
        return;
    }
    if( g_pending_delete >= 0 && g_pending_delete < static_cast<int>( g_entries.size() ) ) {
        ImGui::TextWrapped( "即将删除「%s」。此操作不可撤销（演示）。",
                            g_entries[g_pending_delete].name.c_str() );
    }
    ImGui::Separator();
    if( ImGui::Button( "确认删除", ImVec2( 120.0f, 0.0f ) ) ) {
        if( g_pending_delete >= 0 && g_pending_delete < static_cast<int>( g_entries.size() ) ) {
            push_toast( toast_level::error, "已删除（演示）：" + g_entries[g_pending_delete].name );
            g_entries.erase( g_entries.begin() + g_pending_delete );
            g_selected = -1;
            ++g_action_count;
        }
        g_pending_delete = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if( ImGui::Button( "取消", ImVec2( 120.0f, 0.0f ) ) ) {
        g_pending_delete = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void draw_entries()
{
    ImGui::TextUnformatted( "左键选择；右键打开该条目的上下文菜单；悬停查看提示。" );
    ImGui::Separator();
    if( g_entries.empty() ) {
        ImGui::TextDisabled( "（演示列表已清空）" );
        return;
    }
    for( int i = 0; i < static_cast<int>( g_entries.size() ); ++i ) {
        const demo_entry &e = g_entries[i];
        ImGui::PushID( i );
        char label[192];
        std::snprintf( label, sizeof( label ), "%s ｜ %s ｜ x%d%s", e.name.c_str(), e.category.c_str(),
                       e.count, e.locked ? " ｜ 已锁定" : "" );
        if( ImGui::Selectable( label, g_selected == i ) ) {
            g_selected = i;
        }
        // Context menu is bound to *this* row's ID, so the menu always acts on
        // the row that was right-clicked.
        if( ImGui::BeginPopupContextItem( "##row_ctx" ) ) {
            open_context_menu_actions( i );
            ImGui::EndPopup();
        }
        if( ImGui::IsItemHovered() ) {
            ImGui::BeginTooltip();
            ImGui::Text( "%s", e.name.c_str() );
            ImGui::TextDisabled( "分类：%s　数量：%d", e.category.c_str(), e.count );
            ImGui::TextDisabled( e.locked ? "状态：已锁定（不可执行锁定类操作）" : "状态：可用" );
            ImGui::TextDisabled( "右键打开菜单" );
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
}

} // namespace

namespace ui2
{
namespace menu
{

void draw()
{
    ensure_entries();
    if( g_window_open ) {
        ImGui::SetNextWindowPos( ImVec2( 60.0f, 760.0f ), ImGuiCond_FirstUseEver );
        ImGui::SetNextWindowSize( ImVec2( 520.0f, 330.0f ), ImGuiCond_FirstUseEver );
        if( ImGui::Begin( "上下文菜单与反馈 (PR-1.5 C)", &g_window_open ) ) {
            draw_entries();
            ImGui::SeparatorText( "操作反馈（演示）" );
            if( ImGui::Button( "成功示例" ) ) {
                push_toast( toast_level::success, "演示操作已成功完成" );
                ++g_action_count;
            }
            ImGui::SameLine();
            if( ImGui::Button( "信息示例" ) ) {
                push_toast( toast_level::info, "这是一条信息提示" );
                ++g_action_count;
            }
            ImGui::SameLine();
            if( ImGui::Button( "警告示例" ) ) {
                push_toast( toast_level::warning, "这是一条警告提示" );
                ++g_action_count;
            }
            ImGui::SameLine();
            if( ImGui::Button( "失败示例" ) ) {
                push_toast( toast_level::error, "这是一条失败提示" );
                ++g_action_count;
            }
            ImGui::TextDisabled( "提示 4 秒后自动消失，最多同时显示 4 条。已执行动作：%d 次，队列：%d 条", g_action_count, static_cast<int>( g_toasts.size() ) );
            draw_confirm_modal();
        }
        ImGui::End();
    }
    draw_toasts();
}

} // namespace menu
} // namespace ui2

#endif // TILES

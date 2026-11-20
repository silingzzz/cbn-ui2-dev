#include "cata_imgui_ui2_lists.h"

#if defined( TILES )

#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

namespace
{

// ---------------------------------------------------------------------------
// PR-1.5 B batch: list & table components built on ImGui natives only.
// Every string below is DEMO DATA -- it does not come from the game and must
// not be mistaken for a real item model.
// ---------------------------------------------------------------------------

struct demo_row {
    std::string name;
    std::string category;
    int count = 0;
    float weight = 0.0f;
    std::string status;
};

constexpr int k_row_count = 1000;

std::vector<demo_row> g_rows;
std::vector<unsigned char> g_multi_selected;

bool g_window_open = true;
bool g_empty = false;
bool g_multi_mode = false;
int g_single_selected = -1;
int g_table_selected = -1;
int g_tab_index = 0;
int g_last_scroll_row = 0;

void ensure_rows()
{
    if( !g_rows.empty() ) {
        return;
    }
    static const char *const categories[] = { "工具", "食物", "装备", "材料", "药品" };
    static const char *const statuses[] = { "完好", "磨损", "损坏", "待检" };
    g_rows.reserve( k_row_count );
    for( int i = 0; i < k_row_count; ++i ) {
        char buf[64];
        std::snprintf( buf, sizeof( buf ), "演示物品 #%04d", i + 1 );
        demo_row row;
        row.name = buf;
        row.category = categories[i % 5];
        row.count = ( i * 7 ) % 50 + 1;
        row.weight = 0.1f * static_cast<float>( i % 40 + 1 );
        row.status = statuses[i % 4];
        g_rows.push_back( row );
    }
    g_multi_selected.assign( k_row_count, 0 );
}

auto selected_count() -> int
{
    if( !g_multi_mode ) {
        return g_single_selected >= 0 ? 1 : 0;
    }
    int n = 0;
    for( const unsigned char s : g_multi_selected ) {
        if( s != 0 ) {
            ++n;
        }
    }
    return n;
}

void draw_list_tab()
{
    ImGui::Checkbox( "空列表（演示空状态）", &g_empty );
    ImGui::SameLine();
    ImGui::Checkbox( "多项选择", &g_multi_mode );
    ImGui::SameLine();
    if( ImGui::Button( "清除选择" ) ) {
        g_single_selected = -1;
        for( unsigned char &s : g_multi_selected ) {
            s = 0;
        }
        g_table_selected = -1;
    }

    ImGui::Text( "行数：%d　已选：%d　最近滚动到行：%d",
                 g_empty ? 0 : k_row_count, selected_count(), g_last_scroll_row );
    ImGui::Separator();

    const float list_height = 170.0f;
    if( ImGui::BeginChild( "b_list", ImVec2( 0.0f, list_height ), true ) ) {
        if( g_empty ) {
            ImGui::Dummy( ImVec2( 0.0f, 60.0f ) );
            ImGui::TextDisabled( "（空列表：当前没有任何条目）" );
        } else {
            ImGuiListClipper clipper;
            clipper.Begin( k_row_count );
            while( clipper.Step() ) {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i ) {
                    if( i >= static_cast<int>( g_rows.size() ) ) {
                        continue;
                    }
                    const demo_row &row = g_rows[i];
                    char label[160];
                    const bool marked = g_multi_mode ? g_multi_selected[i] != 0 : g_single_selected == i;
                    std::snprintf( label, sizeof( label ), "%s%s ｜ %s ｜ %d ｜ %.1f kg ｜ %s",
                                   marked ? "[x] " : "[ ] ", row.name.c_str(), row.category.c_str(),
                                   row.count, static_cast<double>( row.weight ), row.status.c_str() );
                    if( ImGui::Selectable( label, marked ) ) {
                        if( g_multi_mode ) {
                            g_multi_selected[i] = g_multi_selected[i] != 0 ? 0 : 1;
                        } else {
                            g_single_selected = g_single_selected == i ? -1 : i;
                        }
                    }
                }
                // Record how far the user scrolled, as visible scroll-state evidence.
                if( clipper.DisplayEnd > g_last_scroll_row ) {
                    g_last_scroll_row = clipper.DisplayEnd - 1;
                }
            }
        }
    }
    ImGui::EndChild();
}

void draw_table_tab()
{
    ImGui::Text( "表格：点击行选择；滚轮滚动（1,000 行，使用 ImGuiListClipper 裁剪）。" );
    ImGui::Text( "当前行选择：%s", g_table_selected >= 0 ? g_rows[g_table_selected].name.c_str() : "（无）" );
    ImGui::Separator();

    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    if( ImGui::BeginTable( "b_table", 5, flags, ImVec2( 0.0f, 240.0f ) ) ) {
        ImGui::TableSetupScrollFreeze( 0, 1 );
        ImGui::TableSetupColumn( "名称" );
        ImGui::TableSetupColumn( "分类" );
        ImGui::TableSetupColumn( "数量", ImGuiTableColumnFlags_WidthFixed, 60.0f );
        ImGui::TableSetupColumn( "重量", ImGuiTableColumnFlags_WidthFixed, 70.0f );
        ImGui::TableSetupColumn( "状态", ImGuiTableColumnFlags_WidthFixed, 70.0f );
        ImGui::TableHeadersRow();

        if( g_empty ) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled( "（空表格：没有条目）" );
        } else {
            ImGuiListClipper clipper;
            clipper.Begin( k_row_count );
            while( clipper.Step() ) {
                for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i ) {
                    if( i >= static_cast<int>( g_rows.size() ) ) {
                        continue;
                    }
                    const demo_row &row = g_rows[i];
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::PushID( i );
                    const bool selected = g_table_selected == i;
                    if( ImGui::Selectable( row.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns ) ) {
                        g_table_selected = selected ? -1 : i;
                    }
                    ImGui::PopID();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( row.category.c_str() );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%d", row.count );
                    ImGui::TableNextColumn();
                    ImGui::Text( "%.1f", static_cast<double>( row.weight ) );
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( row.status.c_str() );
                }
            }
        }
        ImGui::EndTable();
    }
}

void draw_about_tab()
{
    ImGui::TextWrapped( "PR-1.5 B 批次：可滚动列表与多列表格。" );
    ImGui::BulletText( "全部为演示数据（物品名/分类/数量/重量/状态），未连接游戏物品数据。" );
    ImGui::BulletText( "列表与表格各 1,000 行，使用 ImGuiListClipper 裁剪，只绘制可见行。" );
    ImGui::BulletText( "选择状态跨标签页保持（单选 / 多选 / 行选择三套独立状态）。" );
    ImGui::Separator();
    ImGui::Text( "单选：%d　多选：%d　表格：%d", g_single_selected, selected_count(), g_table_selected );
}

} // namespace

namespace ui2
{
namespace lists
{

void draw()
{
    ensure_rows();
    if( !g_window_open ) {
        return;
    }
    ImGui::SetNextWindowPos( ImVec2( 700.0f, 300.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 660.0f, 760.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "列表与表格 (PR-1.5 B)", &g_window_open ) ) {
        ImGui::End();
        return;
    }
    // Both sections are stacked rather than tabbed: a list and a table are
    // meant to be compared side by side, and it keeps the demo usable without
    // hunting for a tab.
    ImGui::SeparatorText( "可滚动列表（单选 / 多选 / 空状态）" );
    draw_list_tab();
    ImGui::SeparatorText( "多列表格（行选择）" );
    draw_table_tab();
    ImGui::SeparatorText( "说明" );
    draw_about_tab();
    ImGui::End();
}

} // namespace lists
} // namespace ui2

#endif // TILES

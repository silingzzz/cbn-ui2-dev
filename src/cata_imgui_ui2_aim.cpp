#include "cata_imgui_ui2_aim.h"

#if defined( TILES )

#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "aim_snapshot.h"
#include "avatar.h"
#include "inventory.h"
#include "item.h"
#include "units.h"

namespace
{

// ---------------------------------------------------------------------------
// PR-2A AIM-1: READ-ONLY real backpack view.
// Nothing in this file modifies game state: no item is moved, used or dropped.
// The snapshot is rebuilt every frame and never stores item pointers.
// ---------------------------------------------------------------------------

bool g_open = false;
char g_search[64] = "";
std::string g_category;   // empty = all categories
int g_selected = -1;      // index into the snapshot rows of the current frame
std::string g_selected_key;   // "category|type|stack" identity for re-validation
std::string g_status = "每帧重建快照 · 只读";

auto to_kg( units::mass m ) -> double
{
    return static_cast<double>( units::to_gram( m ) ) / 1000.0;
}

auto to_l( units::volume v ) -> double
{
    return static_cast<double>( units::to_milliliter( v ) ) / 1000.0;
}

auto row_key( const aim_row &row ) -> std::string
{
    char buf[32];
    std::snprintf( buf, sizeof( buf ), "|%d", row.stack_index );
    return row.category_id + "|" + row.type_id.str() + buf;
}

void draw_detail( const aim_row &row )
{
    // Details are generated here, inside the same frame that validated the row,
    // so the reference cannot outlive the inventory state it came from.
    const location_inventory &inv = get_avatar().get_inv();
    const int stack = row.stack_index;
    if( stack < 0 || stack >= static_cast<int>( inv.size() ) ) {
        ImGui::TextDisabled( "（背包已变化，请重新选择）" );
        return;
    }
    const std::vector<item *> &items = inv.const_stack( stack );
    if( items.empty() || items.front() == nullptr ) {
        ImGui::TextDisabled( "（背包已变化，请重新选择）" );
        return;
    }
    ImGui::TextUnformatted( row.name.c_str() );
    ImGui::TextDisabled( "分类：%s　数量：%d　重量：%.2f kg　体积：%.2f L",
                         row.category_name.c_str(), row.count, to_kg( row.weight ), to_l( row.volume ) );
    ImGui::Separator();
    if( ImGui::BeginChild( "aim_detail", ImVec2( 0.0f, 0.0f ), false ) ) {
        for( const iteminfo &ii : items.front()->info() ) {
            if( ii.sValue.empty() ) {
                if( !ii.sName.empty() ) {
                    ImGui::TextUnformatted( ii.sName.c_str() );
                }
                continue;
            }
            if( ii.bDrawName && !ii.sName.empty() ) {
                ImGui::Text( "%s %s", ii.sName.c_str(), ii.sValue.c_str() );
            } else {
                ImGui::TextUnformatted( ii.sValue.c_str() );
            }
        }
    }
    ImGui::EndChild();
}

} // namespace

namespace ui2
{
namespace aim
{

auto is_open() -> bool
{
    return g_open;
}

void toggle()
{
    g_open = !g_open;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    const aim_snapshot snap = build_aim_snapshot( get_avatar() );

    // Re-validate the selection against the freshly built snapshot: item has no
    // stable per-instance id, so identity is (category, type, stack slot).
    if( g_selected >= 0 ) {
        bool still_valid = false;
        for( std::size_t i = 0; i < snap.rows.size(); ++i ) {
            if( row_key( snap.rows[i] ) == g_selected_key ) {
                g_selected = static_cast<int>( i );
                still_valid = true;
                break;
            }
        }
        if( !still_valid ) {
            g_selected = -1;
            g_selected_key.clear();
            g_status = "背包已变化：选择已清除";
        }
    }

    ImGui::SetNextWindowPos( ImVec2( 60.0f, 60.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 780.0f, 520.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "新版 AIM（开发预览）· 只读", &g_open ) ) {
        ImGui::End();
        return;
    }

    // ---- toolbar
    ImGui::SetNextItemWidth( 200.0f );
    ImGui::InputTextWithHint( "##search", "搜索名称…", g_search, IM_ARRAYSIZE( g_search ) );

    ImGui::SameLine();
    ImGui::SetNextItemWidth( 160.0f );
    const char *preview = g_category.empty() ? "全部分类" : g_category.c_str();
    if( ImGui::BeginCombo( "##cat", preview ) ) {
        if( ImGui::Selectable( "全部分类", g_category.empty() ) ) {
            g_category.clear();
        }
        for( const std::string &cat : aim_categories( snap ) ) {
            if( ImGui::Selectable( cat.c_str(), g_category == cat ) ) {
                g_category = cat;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextDisabled( "共 %d 项 ｜ 总重 %.2f kg ｜ 总体积 %.2f L ｜ 刷新 #%llu",
                         static_cast<int>( snap.rows.size() ), to_kg( snap.total_weight ),
                         to_l( snap.total_volume ), static_cast<unsigned long long>( snap.build_id ) );

    ImGui::Separator();

    const std::vector<std::size_t> visible = aim_visible_rows( snap, g_category, g_search );

    const float list_width = 440.0f;
    if( ImGui::BeginChild( "aim_list", ImVec2( list_width, 0.0f ), true ) ) {
        if( visible.empty() ) {
            ImGui::Dummy( ImVec2( 0.0f, 40.0f ) );
            ImGui::TextDisabled( snap.rows.empty() ? "（背包是空的）" : "（没有匹配的物品）" );
        } else {
            std::string last_category;
            for( const std::size_t idx : visible ) {
                const aim_row &row = snap.rows[idx];
                if( row.category_name != last_category ) {
                    last_category = row.category_name;
                    ImGui::Spacing();
                    ImGui::TextDisabled( "▸ %s", last_category.c_str() );
                }
                char label[256];
                std::snprintf( label, sizeof( label ), "%s##%zu", row.name.c_str(), idx );
                if( ImGui::Selectable( label, g_selected == static_cast<int>( idx ) ) ) {
                    g_selected = static_cast<int>( idx );
                    g_selected_key = row_key( row );
                    g_status = "已选中";
                }
                ImGui::SameLine( 260.0f );
                ImGui::TextDisabled( "%s ｜ x%d ｜ %.2f kg ｜ %.2f L", row.category_name.c_str(),
                                     row.count, to_kg( row.weight ), to_l( row.volume ) );
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if( ImGui::BeginChild( "aim_detail_panel", ImVec2( 0.0f, 0.0f ), true ) ) {
        if( g_selected >= 0 && g_selected < static_cast<int>( snap.rows.size() ) ) {
            draw_detail( snap.rows[static_cast<std::size_t>( g_selected )] );
        } else {
            ImGui::TextDisabled( "选择左侧物品查看真实详情" );
            ImGui::Separator();
            ImGui::TextDisabled( "本窗口只读：不会移动、使用或丢弃任何物品。" );
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled( "%s", g_status.c_str() );
    ImGui::SameLine();
    ImGui::TextDisabled( "｜数据源：Character::inv（穿戴/手持/地面/载具不在本阶段范围）" );

    ImGui::End();
}

} // namespace aim
} // namespace ui2

#endif // TILES

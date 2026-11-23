#include "cata_imgui_ui2_aim.h"

#if defined( TILES )

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <imgui.h>

#include "aim_action.h"
#include "aim_snapshot.h"
#include "avatar.h"
#include "cata_imgui_ui2_craft.h"
#include "cata_imgui_ui2_gear.h"
#include "cata_imgui_ui2_messages.h"
#include "cata_imgui_ui2_zone.h"
#include "inventory.h"
#include "item.h"
#include "map/map.h"
#include "messages.h"
#include "output.h"
#include "units.h"

namespace
{

// ---------------------------------------------------------------------------
// PR-2A AIM-2: real backpack view with a one-to-N item move command.
// The UI only builds an activity request; ACT_MOVE_ITEMS still owns capacity,
// weight, ownership and placement checks. Nothing here moves an item directly.
// ---------------------------------------------------------------------------

bool g_open = false;
char g_search[64] = "";
std::string g_category;   // empty = all categories
int g_selected = -1;      // index into the snapshot rows of the current frame
std::string g_selected_key;   // "category|type|stack" identity for re-validation
int g_dest_index = 0;
int g_move_amount = 1;
bool g_move_pending = false;
bool g_ground_open = false;
int g_ground_target = 0;   // index into aim_move_targets()
int g_selected_ground = -1;
bool g_pickup_pending = false;
std::string g_status = "每帧重建快照 · 可移动物品";
std::vector<std::string> g_checked_keys;
std::string g_last_filter;

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
    for( const iteminfo &ii : items.front()->info() ) {
        // Mirrors format_item_info(): <num> is replaced by sValue only when
        // the entry has a real numeric value; -999 means "no value".
        std::string line = ii.bDrawName ? ii.sName : "";
        if( ii.sType != "DESCRIPTION" ) {
            const std::string fmt = ii.sFmt;
            const std::size_t num_pos = fmt.find( "<num>" );
            if( num_pos != std::string::npos ) {
                line += fmt.substr( 0, num_pos );
                if( ii.sValue != "-999" ) {
                    line += ii.sValue;
                }
                line += fmt.substr( num_pos + 5 );
            } else {
                line += fmt;
            }
        }
        line = remove_color_tags( line );
        if( !line.empty() ) {
            ImGui::TextWrapped( "%s", line.c_str() );
        }
    }
}

auto find_row_index( const aim_snapshot &snap, const std::string &key ) -> int
{
    for( std::size_t i = 0; i < snap.rows.size(); ++i ) {
        if( row_key( snap.rows[i] ) == key ) {
            return static_cast<int>( i );
        }
    }
    return -1;
}

auto is_checked( const std::string &key ) -> bool
{
    return std::find( g_checked_keys.begin(), g_checked_keys.end(), key ) != g_checked_keys.end();
}

void toggle_checked( const std::string &key )
{
    const auto it = std::find( g_checked_keys.begin(), g_checked_keys.end(), key );
    if( it == g_checked_keys.end() ) {
        g_checked_keys.push_back( key );
    } else {
        g_checked_keys.erase( it );
    }
}

auto build_checked_requests( const aim_snapshot &snap, int amount ) ->
std::vector<aim_move_request>
{
    std::vector<aim_move_request> requests;
    requests.reserve( g_checked_keys.size() );
    for( const std::string &key : g_checked_keys ) {
        const int idx = find_row_index( snap, key );
        if( idx < 0 ) {
            continue;
        }
        const aim_row &row = snap.rows[static_cast<std::size_t>( idx )];
        aim_move_request request;
        request.stack_index = row.stack_index;
        request.type = row.type_id;
        request.amount = std::max( 1, std::min( amount, row.count ) );
        requests.push_back( request );
    }
    return requests;
}

auto submit_move( const aim_row &row, int amount, const aim_move_target &target ) -> aim_move_result
{
    const aim_move_result result = aim_move_items(
                                      get_avatar(), row.stack_index, row.type_id, amount, target.offset );
    if( result == aim_move_result::started ) {
        g_move_pending = true;
        char msg[512];
        std::snprintf( msg, sizeof( msg ),
                       "已提交：%s → %s，移动 %d 件。按 F10 关闭覆盖层，游戏才会推进并执行。",
                       row.name.c_str(), target.label, amount );
        g_status = msg;
        add_msg( m_info, "已提交移动：%s → %s（%d 件）", row.name.c_str(), target.label, amount );
    } else {
        g_selected = -1;
        g_selected_key.clear();
        g_status = "背包已变化：选择已清除，请重新选择";
    }
    return result;
}

auto submit_batch( const std::vector<aim_move_request> &requests, int total,
                   const aim_move_target &target ) -> aim_move_result
{
    const aim_move_result result = aim_move_items( get_avatar(), requests, target.offset );
    if( result == aim_move_result::started ) {
        g_move_pending = true;
        char msg[512];
        std::snprintf( msg, sizeof( msg ),
                       "已提交：%d 行 / 共 %d 件 → %s。按 F10 关闭覆盖层，游戏才会推进并执行。",
                       static_cast<int>( requests.size() ), total, target.label );
        g_status = msg;
        g_checked_keys.clear();
        add_msg( m_info, "已提交批量移动：%d 行 / %d 件 → %s",
                 static_cast<int>( requests.size() ), total, target.label );
    } else {
        g_selected = -1;
        g_selected_key.clear();
        g_checked_keys.clear();
        g_status = "背包已变化：批量选择已清除，请重新选择";
    }
    return result;
}

auto current_ground_tile() -> tripoint_bub_ms
{
    const auto &targets = aim_move_targets();
    return get_avatar().bub_pos() + targets[g_ground_target].offset;
}

auto ground_pickup_amount( const item &it ) -> int
{
    const int available = it.count_by_charges() ? it.charges : 1;
    if( ImGui::GetIO().KeyCtrl ) {
        return 1;
    }
    if( ImGui::GetIO().KeyShift ) {
        return std::max( 1, available / 2 );
    }
    return available;
}

auto parse_ground_payload( const char *data, std::size_t size, aim_ground_request &request ) -> bool
{
    if( data == nullptr || size < 3 ) {
        return false;
    }
    const std::string text( data, size );
    const std::size_t first = text.find( '|' );
    const std::size_t second = first == std::string::npos ? std::string::npos :
                               text.find( '|', first + 1 );
    if( first == std::string::npos || second == std::string::npos ) {
        return false;
    }
    request.stack_index = std::atoi( text.substr( 0, first ).c_str() );
    request.type = itype_id( text.substr( first + 1, second - first - 1 ) );
    request.amount = std::atoi( text.substr( second + 1 ).c_str() );
    return request.stack_index >= 0 && !request.type.is_empty() && request.amount > 0;
}

void submit_pickup( const aim_ground_request &request )
{
    const aim_pickup_result result = aim_pickup_items( get_avatar(), current_ground_tile(),
                                   std::vector<aim_ground_request>{ request } );
    if( result == aim_pickup_result::started ) {
        g_pickup_pending = true;
        g_status = "已提交拾取；按 F10 关闭覆盖层，游戏才会推进并执行。";
        add_msg( m_info, "已提交拾取：%s（%d 件）", request.type.str().c_str(), request.amount );
    } else {
        g_status = "地面物品已变化，拾取未提交";
    }
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

    // The activity owns the move from here on; once the game has consumed it the
    // button becomes available again on the next open.
    if( g_move_pending && !get_avatar().activity ) {
        g_move_pending = false;
    }
    if( g_pickup_pending && !get_avatar().activity ) {
        g_pickup_pending = false;
    }

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

    // Batch identity is also (category, type, stack slot). Drop keys whose stack
    // disappeared, and clear the batch when the filter changes so hidden rows
    // cannot be moved by accident.
    for( auto it = g_checked_keys.begin(); it != g_checked_keys.end(); ) {
        if( find_row_index( snap, *it ) < 0 ) {
            it = g_checked_keys.erase( it );
        } else {
            ++it;
        }
    }
    const std::string filter_now = g_category + "|" + g_search;
    if( filter_now != g_last_filter ) {
        g_checked_keys.clear();
        g_last_filter = filter_now;
    }

    const std::vector<std::size_t> visible = aim_visible_rows( snap, g_category, g_search );
    const auto &targets = aim_move_targets();

    ImGui::SetNextWindowPos( ImVec2( 60.0f, 60.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 900.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "新版 AIM（开发预览）· 背包移动", &g_open ) ) {
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
    if( ImGui::Button( "装备/容器", ImVec2( 90.0f, 0.0f ) ) ) {
        ui2::gear::toggle();
    }
    ImGui::SameLine();
    if( ImGui::Button( "制作", ImVec2( 60.0f, 0.0f ) ) ) {
        ui2::craft::toggle();
    }
    ImGui::SameLine();
    if( ImGui::Button( "区域", ImVec2( 60.0f, 0.0f ) ) ) {
        ui2::zone::toggle();
    }
    ImGui::SameLine();
    if( ImGui::Button( ui2::messages::is_open() ? "收起消息" : "消息", ImVec2( 80.0f, 0.0f ) ) ) {
        ui2::messages::toggle();
    }
    ImGui::SameLine();
    if( ImGui::Button( g_ground_open ? "收起地面" : "地面物品", ImVec2( 80.0f, 0.0f ) ) ) {
        g_ground_open = !g_ground_open;
    }

    ImGui::SameLine();
    ImGui::TextDisabled( "共 %d 项 ｜ 总重 %.2f kg ｜ 总体积 %.2f L ｜ 刷新 #%llu",
                         static_cast<int>( snap.rows.size() ), to_kg( snap.total_weight ),
                         to_l( snap.total_volume ), static_cast<unsigned long long>( snap.build_id ) );

    ImGui::TextDisabled( "批量：勾选 %d 行", static_cast<int>( g_checked_keys.size() ) );
    ImGui::SameLine();
    if( ImGui::SmallButton( "全选当前筛选" ) ) {
        for( const std::size_t idx : visible ) {
            const std::string key = row_key( snap.rows[idx] );
            if( !is_checked( key ) ) {
                g_checked_keys.push_back( key );
            }
        }
        g_status = "已全选当前筛选";
    }
    ImGui::SameLine();
    if( ImGui::SmallButton( "清空勾选" ) ) {
        g_checked_keys.clear();
        g_status = "已清空批量勾选";
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "Ctrl+A 全选 ｜ Ctrl+Enter 移动" );

    ImGui::Separator();

    auto select_row = [&snap]( std::size_t idx ) {
        g_selected = static_cast<int>( idx );
        g_selected_key = row_key( snap.rows[idx] );
        g_move_amount = 1;
        g_status = "已选中";
    };

    auto select_all_visible = [&snap, &visible]() {
        for( const std::size_t idx : visible ) {
            const std::string key = row_key( snap.rows[idx] );
            if( !is_checked( key ) ) {
                g_checked_keys.push_back( key );
            }
        }
    };

    auto commit_current = [&snap, &targets]() -> aim_move_result {
        if( !g_checked_keys.empty() ) {
            const std::vector<aim_move_request> requests =
                build_checked_requests( snap, g_move_amount );
            int total = 0;
            for( const aim_move_request &request : requests ) {
                total += request.amount;
            }
            if( !requests.empty() ) {
                return submit_batch( requests, total, targets[g_dest_index] );
            }
        }
        if( g_selected >= 0 && g_selected < static_cast<int>( snap.rows.size() ) ) {
            const aim_row &row = snap.rows[static_cast<std::size_t>( g_selected )];
            return submit_move( row, std::max( 1, std::min( g_move_amount, row.count ) ),
                                targets[g_dest_index] );
        }
        return aim_move_result::stale_selection;
    };

    if( !ImGui::GetIO().WantTextInput &&
        ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) ) {
        if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_A ) ) {
            select_all_visible();
            g_status = "已全选当前筛选";
        }
        if( ImGui::IsKeyChordPressed( ImGuiMod_Ctrl | ImGuiKey_Enter ) && !g_move_pending ) {
            commit_current();
        }
    }

    const float list_width = 520.0f;
    if( ImGui::BeginChild( "aim_list", ImVec2( list_width, 0.0f ), true ) ) {
        if( ImGui::Button( "把地面物品拖到这里放入背包（或点击拾取选中）",
                           ImVec2( -1.0f, 30.0f ) ) ) {
            map &here = get_map();
            map_stack stack = here.i_at( current_ground_tile() );
            int idx = 0;
            bool picked = false;
            for( item *it : stack ) {
                if( idx == g_selected_ground && it != nullptr ) {
                    aim_ground_request request{ idx, it->typeId(), ground_pickup_amount( *it ) };
                    submit_pickup( request );
                    picked = true;
                    break;
                }
                ++idx;
            }
            if( !picked ) {
                g_status = "先在右侧地面列表选中物品";
            }
        }
        if( ImGui::BeginDragDropTarget() ) {
            if( const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( "GROUND_ITEM" ) ) {
                aim_ground_request request;
                if( parse_ground_payload( static_cast<const char *>( payload->Data ),
                                          static_cast<std::size_t>( payload->DataSize ), request ) ) {
                    submit_pickup( request );
                } else {
                    g_status = "拖放来源已失效，请重新选择地面物品";
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::Separator();
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
                const std::string key = row_key( row );
                bool checked = is_checked( key );
                char chk_id[64];
                std::snprintf( chk_id, sizeof( chk_id ), "##aim_chk%zu", idx );
                if( ImGui::Checkbox( chk_id, &checked ) ) {
                    toggle_checked( key );
                    if( checked ) {
                        select_row( idx );
                    }
                }
                ImGui::SameLine( 0.0f, 6.0f );

                char label[256];
                std::snprintf( label, sizeof( label ), "%s##%zu", row.name.c_str(), idx );
                if( ImGui::Selectable( label, g_selected == static_cast<int>( idx ) ) ) {
                    select_row( idx );
                }
                const bool row_right_clicked = ImGui::IsItemClicked( ImGuiMouseButton_Right );
                if( row_right_clicked ) {
                    select_row( idx );
                }
                if( ImGui::BeginDragDropSource() ) {
                    ImGui::SetDragDropPayload( "AIM_ROW", key.c_str(), key.size() + 1 );
                    ImGui::Text( "移动：%s（%d 件）", row.name.c_str(), g_move_amount );
                    ImGui::EndDragDropSource();
                }

                char ctx_id[64];
                std::snprintf( ctx_id, sizeof( ctx_id ), "aim_row_context##%zu", idx );
                if( row_right_clicked ) {
                    ImGui::OpenPopup( ctx_id );
                }
                if( ImGui::BeginPopup( ctx_id ) ) {
                    char entry[160];
                    std::snprintf( entry, sizeof( entry ), "移动 %d 件到%s", g_move_amount,
                                   targets[g_dest_index].label );
                    ImGui::BeginDisabled( g_move_pending );
                    if( ImGui::MenuItem( entry ) ) {
                        submit_move( row, g_move_amount, targets[g_dest_index] );
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                    ImGui::MenuItem( "拖到右侧九宫格也可移动", nullptr, false, false );
                    ImGui::EndPopup();
                }

                ImGui::SameLine( 310.0f );
                ImGui::TextDisabled( "x%d ｜ %.2f kg ｜ %.2f L", row.count, to_kg( row.weight ),
                                     to_l( row.volume ) );
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if( ImGui::BeginChild( "aim_right_panel", ImVec2( 0.0f, 0.0f ), true ) ) {
        const float inner_height = ImGui::GetContentRegionAvail().y;
        if( ImGui::BeginChild( "aim_detail_scroll", ImVec2( 0.0f, std::max( 120.0f, inner_height - 292.0f ) ),
                               false ) ) {
            if( g_selected >= 0 && g_selected < static_cast<int>( snap.rows.size() ) ) {
                draw_detail( snap.rows[static_cast<std::size_t>( g_selected )] );
            } else {
                ImGui::TextDisabled( "选择左侧物品查看真实详情" );
                ImGui::Separator();
                ImGui::TextDisabled( "右侧下方选择落点并移动。" );
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::TextDisabled( "移动 1–N 件到地图" );
        const bool has_active = g_selected >= 0 && g_selected < static_cast<int>( snap.rows.size() );
        const aim_row *active_row = has_active ?
                                    &snap.rows[static_cast<std::size_t>( g_selected )] : nullptr;

        int batch_max = 0;
        for( const std::string &key : g_checked_keys ) {
            const int idx = find_row_index( snap, key );
            if( idx >= 0 ) {
                batch_max = std::max( batch_max,
                                      snap.rows[static_cast<std::size_t>( idx )].count );
            }
        }
        const bool has_batch = !g_checked_keys.empty() && batch_max > 0;
        const int amount_max = has_batch ? batch_max : ( active_row ? active_row->count : 0 );

        if( !has_active && !has_batch ) {
            ImGui::TextDisabled( "先选中左侧物品，或勾选若干行。" );
        } else {
            if( active_row != nullptr ) {
                ImGui::Text( "当前物品：%s（最多 %d 件）", active_row->name.c_str(), active_row->count );
            }
            if( has_batch ) {
                ImGui::Text( "批量勾选：%d 行（按行各自上限，最多到 %d）",
                             static_cast<int>( g_checked_keys.size() ), batch_max );
            }

            // 3x3 ground grid around the player. Clicking a cell only selects the
            // destination; the explicit button below still owns the commit.
            static const int grid[3][3] = { { 8, 1, 2 }, { 7, 0, 3 }, { 6, 5, 4 } };
            ImGui::BeginDisabled( g_move_pending );
            for( int gy = 0; gy < 3; ++gy ) {
                for( int gx = 0; gx < 3; ++gx ) {
                    const int ti = grid[gy][gx];
                    if( gx > 0 ) {
                        ImGui::SameLine();
                    }
                    if( g_dest_index == ti ) {
                        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.24f, 0.52f, 0.86f, 1.0f ) );
                    }
                    char dest_label[64];
                    std::snprintf( dest_label, sizeof( dest_label ), "%s##dest%d", targets[ti].label, ti );
                    if( ImGui::Button( dest_label, ImVec2( 72.0f, 26.0f ) ) ) {
                        g_dest_index = ti;
                        g_status = "已选择落点";
                    }
                    if( g_dest_index == ti ) {
                        ImGui::PopStyleColor();
                    }

                    if( ImGui::BeginDragDropTarget() ) {
                        if( const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( "AIM_ROW" ) ) {
                            const std::string key( static_cast<const char *>( payload->Data ),
                                                   static_cast<std::size_t>( payload->DataSize ) - 1 );
                            const int idx = find_row_index( snap, key );
                            if( idx >= 0 ) {
                                const aim_row &dropped = snap.rows[static_cast<std::size_t>( idx )];
                                const int amount = g_selected == idx ?
                                                   std::max( 1, std::min( g_move_amount, dropped.count ) ) : 1;
                                if( g_selected != idx ) {
                                    select_row( static_cast<std::size_t>( idx ) );
                                }
                                submit_move( dropped, amount, targets[ti] );
                            } else {
                                g_status = "拖放来源已失效，请重新选择";
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }
            ImGui::EndDisabled();

            ImGui::SetNextItemWidth( 110.0f );
            ImGui::InputInt( "##move_amount", &g_move_amount, 1, 10 );
            ImGui::SameLine();
            ImGui::TextDisabled( has_batch ? "每行数量（按各自上限截断）" : "数量（1–%d）", batch_max );
            g_move_amount = std::max( 1, std::min( g_move_amount, amount_max ) );

            if( has_batch ) {
                const std::vector<aim_move_request> requests =
                    build_checked_requests( snap, g_move_amount );
                int total = 0;
                for( const aim_move_request &request : requests ) {
                    total += request.amount;
                }
                ImGui::BeginDisabled( g_move_pending || requests.empty() );
                char batch_label[128];
                std::snprintf( batch_label, sizeof( batch_label ),
                               "移动勾选 %d 行（合计 %d 件）→ %s",
                               static_cast<int>( requests.size() ), total, targets[g_dest_index].label );
                if( ImGui::Button( g_move_pending ? "已提交，等待执行" : batch_label,
                                   ImVec2( -1.0f, 0.0f ) ) ) {
                    submit_batch( requests, total, targets[g_dest_index] );
                }
                ImGui::EndDisabled();
            } else if( active_row != nullptr ) {
                ImGui::BeginDisabled( g_move_pending );
                char move_label[96];
                std::snprintf( move_label, sizeof( move_label ), "移动 %d 件 → %s", g_move_amount,
                               targets[g_dest_index].label );
                if( ImGui::Button( g_move_pending ? "已提交，等待执行" : move_label, ImVec2( -1.0f, 0.0f ) ) ) {
                    submit_move( *active_row, g_move_amount, targets[g_dest_index] );
                }
                ImGui::EndDisabled();
            }
            ImGui::TextDisabled( "容量、重量、归属与落点由 ACT_MOVE_ITEMS 判定。" );
            if( g_move_pending ) {
                ImGui::TextDisabled( "已提交：按 F10 关闭覆盖层推进时间。" );
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    ImGui::TextDisabled( "%s", g_status.c_str() );
    ImGui::SameLine();
    ImGui::TextDisabled( "｜数据源：Character::inv（其余操作仍走旧 AIM）" );

    ImGui::End();

    if( !g_ground_open ) {
        return;
    }

    ImGui::SetNextWindowPos( ImVec2( 965.0f, 60.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 430.0f, 560.0f ), ImGuiCond_FirstUseEver );
    bool ground_open = true;
    if( ImGui::Begin( "地面物品##aim_ground", &ground_open,
                      ImGuiWindowFlags_NoSavedSettings ) ) {
        const auto &targets = aim_move_targets();
        const tripoint_bub_ms ground = current_ground_tile();
        const tripoint_abs_ms ground_abs = bub_to_abs( ground );
        ImGui::Text( "脚下与相邻地面格（%d,%d,%d）", ground_abs.x(), ground_abs.y(), ground_abs.z() );
        ImGui::TextDisabled( "先选地面格，再拖动或双击物品。" );
        ImGui::TextDisabled( "默认整堆；Ctrl 拖 1 件；Shift 拖一半。" );

        static const int ground_grid[3][3] = { { 8, 1, 2 }, { 7, 0, 3 }, { 6, 5, 4 } };
        for( int gy = 0; gy < 3; ++gy ) {
            for( int gx = 0; gx < 3; ++gx ) {
                const int ti = ground_grid[gy][gx];
                if( gx > 0 ) {
                    ImGui::SameLine();
                }
                if( g_ground_target == ti ) {
                    ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.24f, 0.52f, 0.86f, 1.0f ) );
                }
                char tile_label[64];
                std::snprintf( tile_label, sizeof( tile_label ), "%s##gt%d", targets[ti].label, ti );
                if( ImGui::Button( tile_label, ImVec2( 72.0f, 26.0f ) ) ) {
                    g_ground_target = ti;
                    g_selected_ground = -1;
                }
                if( g_ground_target == ti ) {
                    ImGui::PopStyleColor();
                }
            }
        }
        ImGui::Separator();

        map &here = get_map();
        map_stack stack = here.i_at( ground );
        if( ImGui::BeginChild( "aim_ground_list", ImVec2( 0.0f, 220.0f ), true ) ) {
            if( stack.empty() ) {
                ImGui::TextDisabled( "这一格没有物品。" );
            }
            int idx = 0;
            for( item *it : stack ) {
                if( it == nullptr ) {
                    ++idx;
                    continue;
                }
                const int amount = ground_pickup_amount( *it );
                char row_label[160];
                std::snprintf( row_label, sizeof( row_label ), "%s ×%d##ground%d",
                               it->tname().c_str(), it->count(), idx );
                if( ImGui::Selectable( row_label, g_selected_ground == idx,
                                       ImGuiSelectableFlags_AllowDoubleClick ) ) {
                    g_selected_ground = idx;
                    if( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                        aim_ground_request request{ idx, it->typeId(), amount };
                        submit_pickup( request );
                    }
                }
                if( ImGui::BeginDragDropSource() ) {
                    char payload[160];
                    std::snprintf( payload, sizeof( payload ), "%d|%s|%d", idx,
                                   it->typeId().str().c_str(), amount );
                    ImGui::SetDragDropPayload( "GROUND_ITEM", payload, std::strlen( payload ) + 1 );
                    ImGui::Text( "拾取：%s（%d 件）", it->tname().c_str(), amount );
                    ImGui::EndDragDropSource();
                }

                char ctx_id[64];
                std::snprintf( ctx_id, sizeof( ctx_id ), "aim_ground_context##%d", idx );
                if( ImGui::IsItemClicked( ImGuiMouseButton_Right ) ) {
                    ImGui::OpenPopup( ctx_id );
                }
                if( ImGui::BeginPopup( ctx_id ) ) {
                    ImGui::BeginDisabled( g_pickup_pending );
                    if( ImGui::MenuItem( "拾取到背包" ) ) {
                        aim_ground_request request{ idx, it->typeId(), amount };
                        submit_pickup( request );
                    }
                    ImGui::EndDisabled();
                    ImGui::EndPopup();
                }
                ImGui::SameLine( 260.0f );
                ImGui::TextDisabled( "拖到左侧背包" );
                ++idx;
            }
        }
        ImGui::EndChild();

        ImGui::Separator();
        ImGui::BeginDisabled( g_pickup_pending || g_selected_ground < 0 );
        if( ImGui::Button( g_pickup_pending ? "拾取已提交" : "拾取选中：地面 → 背包",
                           ImVec2( -1.0f, 0.0f ) ) ) {
            int idx = 0;
            for( item *it : stack ) {
                if( idx == g_selected_ground && it != nullptr ) {
                    const int amount = ground_pickup_amount( *it );
                    aim_ground_request request{ idx, it->typeId(), amount };
                    submit_pickup( request );
                    break;
                }
                ++idx;
            }
        }
        ImGui::EndDisabled();

        if( ImGui::Button( "把背包物品拖到这里丢下（或点击丢下选中）",
                           ImVec2( -1.0f, 30.0f ) ) ) {
            const bool has_row = g_selected >= 0 &&
                                 g_selected < static_cast<int>( snap.rows.size() );
            if( has_row ) {
                const aim_row &dropped = snap.rows[static_cast<std::size_t>( g_selected )];
                const int amount = std::max( 1, std::min( g_move_amount, dropped.count ) );
                submit_move( dropped, amount, targets[g_ground_target] );
            } else {
                g_status = "先在左侧选中一件背包物品";
            }
        }
        if( ImGui::BeginDragDropTarget() ) {
            if( const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( "AIM_ROW" ) ) {
                const std::string key( static_cast<const char *>( payload->Data ),
                                       static_cast<std::size_t>( payload->DataSize ) - 1 );
                const int idx = find_row_index( snap, key );
                if( idx >= 0 ) {
                    const aim_row &dropped = snap.rows[static_cast<std::size_t>( idx )];
                    const int amount = g_selected == idx ?
                                       std::max( 1, std::min( g_move_amount, dropped.count ) ) : 1;
                    submit_move( dropped, amount, targets[g_ground_target] );
                } else {
                    g_status = "拖放来源已失效，请重新选择";
                }
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::TextDisabled( "容量、重量与耗时由 ACT_PICKUP / ACT_MOVE_ITEMS 判定。" );
        if( g_pickup_pending || g_move_pending ) {
            ImGui::TextDisabled( "已提交：按 F10 关闭覆盖层推进时间。" );
        }
    }
    ImGui::End();
    if( !ground_open ) {
        g_ground_open = false;
    }
}

} // namespace aim
} // namespace ui2

#endif // TILES

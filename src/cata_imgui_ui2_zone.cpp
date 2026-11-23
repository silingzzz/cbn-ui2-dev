#include "cata_imgui_ui2_zone.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "cata_imgui_ui2_aim.h"
#include "clzones.h"
#include "faction.h"
#include "game.h"
#include "messages.h"
#include "popup.h"

namespace
{

bool g_open = false;
zone_data *g_selected = nullptr;
zone_data *g_toggle_request = nullptr;
zone_data *g_delete_candidate = nullptr;
zone_data *g_delete_request = nullptr;
bool g_open_delete_popup = false;
char g_filter[64] = "";

bool g_map_select_request = false;
bool g_map_select_for_edit = false;
zone_data *g_edit_target = nullptr;
bool g_has_pending_rect = false;
tripoint_abs_ms g_pending_start;
tripoint_abs_ms g_pending_end;
char g_new_name[96] = "新区域";
zone_type_id g_new_type;
char g_type_filter[64] = "";
zone_data *g_rename_for = nullptr;
char g_rename[96] = "";

bool contains_ci( const std::string &haystack, const std::string &needle )
{
    if( needle.empty() ) {
        return true;
    }
    const auto it = std::search( haystack.begin(), haystack.end(),
                                 needle.begin(), needle.end(),
    []( char a, char b ) {
        return std::tolower( static_cast<unsigned char>( a ) ) ==
               std::tolower( static_cast<unsigned char>( b ) );
    } );
    return it != haystack.end();
}

} // namespace

namespace ui2
{
namespace zone
{

bool is_open()
{
    return g_open;
}

void toggle()
{
    g_open = !g_open;
}

auto take_map_select_request() -> bool
{
    if( !g_map_select_request ) {
        return false;
    }
    g_map_select_request = false;
    return true;
}

void run_map_select_flow()
{
    if( g == nullptr ) {
        return;
    }
    static_popup popup;
    popup.on_top( true );
    popup.message( "%s", _( "框选区域：请选择起点。" ) );

    auto center = g->u.bub_pos() + g->u.view_offset;
    const look_around_result first = g->look_around( false, center, center, false, true, false );
    if( !first.position ) {
        add_msg( m_info, _( "已取消区域框选。" ) );
        return;
    }

    popup.message( "%s", _( "框选区域：请选择终点。" ) );
    const look_around_result second = g->look_around( false, center, *first.position,
            true, true, false );
    if( !second.position ) {
        add_msg( m_info, _( "已取消区域框选。" ) );
        return;
    }

    const tripoint_bub_ms low( std::min( first.position->x(), second.position->x() ),
                               std::min( first.position->y(), second.position->y() ),
                               std::min( first.position->z(), second.position->z() ) );
    const tripoint_bub_ms high( std::max( first.position->x(), second.position->x() ),
                                std::max( first.position->y(), second.position->y() ),
                                std::max( first.position->z(), second.position->z() ) );
    const tripoint_abs_ms start = bub_to_abs( low );
    const tripoint_abs_ms end = bub_to_abs( high );

    if( g_map_select_for_edit && g_edit_target != nullptr ) {
        zone_manager &mgr = zone_manager::get_manager();
        bool alive = false;
        for( zone_manager::ref_zone_data ref : mgr.get_zones() ) {
            if( &ref.get() == g_edit_target ) {
                alive = true;
                break;
            }
        }
        if( alive ) {
            g_edit_target->set_position( { start, end } );
            mgr.zone_edited( *g_edit_target );
            mgr.save_zones();
            g_selected = g_edit_target;
            add_msg( m_info, _( "区域“%s”的范围已更新。" ), g_edit_target->get_name() );
        }
        g_map_select_for_edit = false;
        g_edit_target = nullptr;
        g_open = true;
        return;
    }

    g_pending_start = start;
    g_pending_end = end;
    g_has_pending_rect = true;
    g_open = true;
    g_selected = nullptr;
    g_type_filter[0] = '\0';
    if( g_new_type.is_empty() ) {
        g_new_type = zone_type_id( "AUTO_DRINK" );
    }
    add_msg( m_info, _( "已框选区域范围，请在“新建区域”中确认名称与类型。" ) );
}

void draw()
{
    if( !g_open ) {
        return;
    }

    zone_manager &mgr = zone_manager::get_manager();
    auto zones = mgr.get_zones();

    if( g_selected != nullptr ) {
        bool still_exists = false;
        for( zone_manager::ref_zone_data ref : zones ) {
            if( &ref.get() == g_selected ) {
                still_exists = true;
                break;
            }
        }
        if( !still_exists ) {
            g_selected = nullptr;
        }
    }

    ImGui::SetNextWindowPos( ImVec2( 60.0f, 620.0f ), ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( 1180.0f, 370.0f ), ImGuiCond_Always );

    if( !ImGui::Begin( "区域与营地（PR-5 预览）", &g_open ) ) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth( 260.0f );
    ImGui::InputTextWithHint( "##zone_filter", "按名称/类型过滤…", g_filter, IM_ARRAYSIZE( g_filter ) );
    ImGui::SameLine();
    ImGui::TextDisabled( "共 %u 个区域", mgr.size() );
    ImGui::SameLine();
    if( ImGui::Button( "框选新建区域…" ) ) {
        g_map_select_request = true;
        g_map_select_for_edit = false;
        g_edit_target = nullptr;
        g_open = false;
        if( ui2::aim::is_open() ) {
            ui2::aim::toggle();
        }
        add_msg( m_info, _( "在地图上点击两次选择区域范围，Esc 取消。" ) );
    }

    if( g_has_pending_rect ) {
        ImGui::Separator();
        ImGui::Text( "框选范围：(%d,%d,%d) → (%d,%d,%d)",
                     g_pending_start.x(), g_pending_start.y(), g_pending_start.z(),
                     g_pending_end.x(), g_pending_end.y(), g_pending_end.z() );
        ImGui::Text( "名称" );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 200.0f );
        ImGui::InputText( "##new_name", g_new_name, IM_ARRAYSIZE( g_new_name ) );
        ImGui::SameLine();
        ImGui::Text( "类型" );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 220.0f );

        std::string current_type_name = g_new_type.is_empty() ? std::string( "未选择" )
                                        : mgr.get_name_from_type( g_new_type );
        if( ImGui::BeginCombo( "##zone_type", current_type_name.c_str() ) ) {
            std::vector<std::pair<std::string, zone_type_id>> types;
            for( const auto &entry : mgr.get_types() ) {
                const std::string haystack = entry.second.name() + " " + entry.first.c_str();
                if( contains_ci( haystack, g_type_filter ) ) {
                    types.emplace_back( entry.second.name(), entry.first );
                }
            }
            std::sort( types.begin(), types.end() );
            for( const auto &entry : types ) {
                const bool selected = g_new_type == entry.second;
                if( ImGui::Selectable( entry.first.c_str(), selected ) ) {
                    g_new_type = entry.second;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 140.0f );
        ImGui::InputTextWithHint( "##type_filter", "过滤类型…", g_type_filter,
                                  IM_ARRAYSIZE( g_type_filter ) );
        ImGui::SameLine();
        if( ImGui::Button( "创建区域", ImVec2( 160.0f, 0.0f ) ) ) {
            if( g_new_name[0] == '\0' || g_new_type.is_empty() ) {
                add_msg( m_warning, _( "区域名称与类型不能为空。" ) );
            } else {
                mgr.add( g_new_name, g_new_type, g->u.get_faction()->id, false, true,
                         g_pending_start, g_pending_end, nullptr );
                mgr.cache_data();
                mgr.save_zones();
                add_msg( m_info, _( "区域“%s”已创建。" ), g_new_name );
                g_has_pending_rect = false;
            }
        }
        ImGui::SameLine();
        if( ImGui::Button( "取消框选", ImVec2( 160.0f, 0.0f ) ) ) {
            g_has_pending_rect = false;
        }
        ImGui::Separator();
    }

    ImGui::Separator();

    ImGui::BeginChild( "##zone_list", ImVec2( 560.0f, 0.0f ), true );
    unsigned shown = 0;
    for( zone_manager::ref_zone_data ref : zones ) {
        zone_data &z = ref.get();
        const std::string type_name = mgr.get_name_from_type( z.get_type() );
        const std::string haystack = z.get_name() + " " + type_name + " " + z.get_type().c_str();
        if( !contains_ci( haystack, g_filter ) ) {
            continue;
        }
        ++shown;
        char label[320];
        std::snprintf( label, sizeof( label ), "%s ｜ %s ｜ %s",
                       z.get_name().c_str(), type_name.c_str(), z.get_enabled() ? "启用" : "停用" );
        const bool selected = g_selected == &z;
        if( ImGui::Selectable( label, selected ) ) {
            g_selected = &z;
        }
    }
    if( shown == 0 ) {
        ImGui::TextDisabled( "没有匹配的区域。" );
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild( "##zone_detail", ImVec2( 0.0f, 0.0f ), true );
    if( g_selected == nullptr ) {
        g_rename_for = nullptr;
        ImGui::TextDisabled( "选择左侧区域查看详情。" );
    } else {
        zone_data &z = *g_selected;
        ImGui::Text( "名称：%s", z.get_name().c_str() );
        ImGui::Text( "类型：%s（%s）", mgr.get_name_from_type( z.get_type() ).c_str(),
                     z.get_type().c_str() );
        ImGui::Text( "阵营：%s", z.get_faction().c_str() );
        ImGui::Text( "维度：%s", z.get_dimension().c_str() );
        ImGui::Text( "状态：%s", z.get_enabled() ? "启用" : "停用" );
        if( g_rename_for != &z ) {
            g_rename_for = &z;
            std::snprintf( g_rename, sizeof( g_rename ), "%s", z.get_name().c_str() );
        }
        ImGui::SetNextItemWidth( 240.0f );
        ImGui::InputText( "##zone_rename", g_rename, IM_ARRAYSIZE( g_rename ) );
        ImGui::SameLine();
        if( ImGui::Button( "重命名" ) ) {
            if( g_rename[0] == '\0' ) {
                add_msg( m_warning, _( "区域名称不能为空。" ) );
            } else {
                z.set_name( g_rename );
                mgr.zone_edited( z );
                mgr.save_zones();
                add_msg( m_info, _( "区域已重命名为“%s”。" ), g_rename );
            }
        }
        ImGui::Text( "范围：(%d,%d,%d) → (%d,%d,%d)", z.get_start_point().x(),
                     z.get_start_point().y(), z.get_start_point().z(),
                     z.get_end_point().x(), z.get_end_point().y(), z.get_end_point().z() );
        const std::vector<tripoint_abs_ms> covered = get_zone_covered_points( z );
        ImGui::Text( "覆盖格数：%d", static_cast<int>( covered.size() ) );
        ImGui::Text( "反转：%s ｜ 载具区域：%s", z.get_invert() ? "是" : "否",
                     z.get_is_vehicle() ? "是" : "否" );
        ImGui::Separator();
        if( ImGui::Button( z.get_enabled() ? "停用该区域" : "启用该区域", ImVec2( 160.0f, 0.0f ) ) ) {
            g_toggle_request = &z;
        }
        ImGui::SameLine();
        if( ImGui::Button( "重选范围…", ImVec2( 130.0f, 0.0f ) ) ) {
            g_map_select_request = true;
            g_map_select_for_edit = true;
            g_edit_target = &z;
            g_open = false;
            if( ui2::aim::is_open() ) {
                ui2::aim::toggle();
            }
            add_msg( m_info, _( "在地图上重新框选“%s”的范围。" ), z.get_name() );
        }
        ImGui::SameLine();
        if( ImGui::Button( "删除该区域…", ImVec2( 130.0f, 0.0f ) ) ) {
            g_delete_candidate = &z;
            g_open_delete_popup = true;
        }
        ImGui::TextDisabled( "切换后由 zone_manager 保存。" );
    }
    ImGui::EndChild();

    if( g_open_delete_popup ) {
        g_open_delete_popup = false;
        ImGui::OpenPopup( "确认删除区域" );
    }

    if( ImGui::BeginPopupModal( "确认删除区域", nullptr, ImGuiWindowFlags_AlwaysAutoResize ) ) {
        if( g_delete_candidate != nullptr ) {
            ImGui::Text( "将删除区域“%s”（类型 %s），此操作不可撤销。",
                         g_delete_candidate->get_name().c_str(),
                         g_delete_candidate->get_type().c_str() );
        }
        ImGui::Separator();
        if( ImGui::Button( "确认删除", ImVec2( 120.0f, 0.0f ) ) ) {
            g_delete_request = g_delete_candidate;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if( ImGui::Button( "取消", ImVec2( 120.0f, 0.0f ) ) ) {
            g_delete_candidate = nullptr;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();

    if( g_toggle_request != nullptr ) {
        zone_data &z = *g_toggle_request;
        g_toggle_request = nullptr;
        z.set_enabled( !z.get_enabled() );
        mgr.zone_edited( z );
        mgr.save_zones();
        add_msg( m_info, "区域“%s”已%s。", z.get_name(), z.get_enabled() ? "启用" : "停用" );
    }

    if( g_delete_request != nullptr ) {
        zone_data *target = g_delete_request;
        g_delete_request = nullptr;
        g_delete_candidate = nullptr;
        bool found = false;
        for( zone_manager::ref_zone_data ref : mgr.get_zones() ) {
            if( &ref.get() == target ) {
                found = true;
                break;
            }
        }
        if( found ) {
            const std::string name = target->get_name();
            if( mgr.remove( *target ) ) {
                mgr.save_zones();
                if( g_selected == target ) {
                    g_selected = nullptr;
                }
                add_msg( m_info, "区域“%s”已删除。", name );
            } else {
                add_msg( m_warning, "区域“%s”删除失败。", name );
            }
        }
    }
}

} // namespace zone
} // namespace ui2

#endif // TILES

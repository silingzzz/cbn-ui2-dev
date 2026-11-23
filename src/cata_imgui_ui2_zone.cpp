#include "cata_imgui_ui2_zone.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "clzones.h"
#include "messages.h"

namespace
{

bool g_open = false;
zone_data *g_selected = nullptr;
zone_data *g_toggle_request = nullptr;
zone_data *g_delete_candidate = nullptr;
zone_data *g_delete_request = nullptr;
bool g_open_delete_popup = false;
char g_filter[64] = "";

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

    ImGui::SetNextWindowPos( ImVec2( 60.0f, 520.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 1180.0f, 470.0f ), ImGuiCond_FirstUseEver );

    if( !ImGui::Begin( "区域与营地（PR-5 预览）", &g_open ) ) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth( 260.0f );
    ImGui::InputTextWithHint( "##zone_filter", "按名称/类型过滤…", g_filter, IM_ARRAYSIZE( g_filter ) );
    ImGui::SameLine();
    ImGui::TextDisabled( "共 %u 个区域", mgr.size() );

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
        ImGui::TextDisabled( "选择左侧区域查看详情。" );
    } else {
        zone_data &z = *g_selected;
        ImGui::Text( "名称：%s", z.get_name().c_str() );
        ImGui::Text( "类型：%s（%s）", mgr.get_name_from_type( z.get_type() ).c_str(),
                     z.get_type().c_str() );
        ImGui::Text( "阵营：%s", z.get_faction().c_str() );
        ImGui::Text( "维度：%s", z.get_dimension().c_str() );
        ImGui::Text( "状态：%s", z.get_enabled() ? "启用" : "停用" );
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
        if( ImGui::Button( "删除该区域…", ImVec2( 130.0f, 0.0f ) ) ) {
            g_delete_candidate = &z;
            g_open_delete_popup = true;
        }
        ImGui::SameLine();
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

#include "cata_imgui_ui2_vehicle.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "game.h"
#include "itype.h"
#include "map/map.h"
#include "units_mass.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"

namespace
{

bool g_open = false;
vehicle *g_selected = nullptr;
bool g_damaged_only = false;
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
namespace veh
{

bool is_open()
{
    return g_open;
}

void toggle()
{
    g_open = !g_open;
}

void set_open( bool open )
{
    g_open = open;
}

void reset_session()
{
    g_open = false;
    g_selected = nullptr;
    g_damaged_only = false;
    g_filter[0] = '\0';
}

void set_damaged_only( bool damaged_only )
{
    g_damaged_only = damaged_only;
}

bool damaged_only()
{
    return g_damaged_only;
}

auto collect_nearby_vehicles() -> std::vector<vehicle *>
{
    std::vector<vehicle *> result;
    if( g == nullptr ) {
        return result;
    }
    const avatar &you = get_avatar();
    const map &here = get_map();
    const tripoint_bub_ms center = you.bub_pos();
    for( int dx = -1; dx <= 1; ++dx ) {
        for( int dy = -1; dy <= 1; ++dy ) {
            const tripoint_bub_ms p = center + point( dx, dy );
            vehicle *v = veh_pointer_or_null( here.veh_at( p ) );
            if( v != nullptr &&
                std::find( result.begin(), result.end(), v ) == result.end() ) {
                result.push_back( v );
            }
        }
    }
    return result;
}

auto count_damaged_parts( const ::vehicle &v ) -> int
{
    int damaged = 0;
    for( const vpart_reference &vpr : v.get_all_parts() ) {
        const vehicle_part &pt = vpr.part();
        const int max_hp = vpr.info().durability;
        if( pt.is_broken() || ( max_hp > 0 && pt.hp() < max_hp ) ) {
            ++damaged;
        }
    }
    return damaged;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 132.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 920.0f, 540.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "载具总览 (只读)###ui2_vehicle_window", &g_open ) ) {
        ImGui::End();
        return;
    }

    if( g == nullptr ) {
        ImGui::TextDisabled( "世界尚未载入。" );
        ImGui::End();
        return;
    }

    const std::vector<vehicle *> nearby = collect_nearby_vehicles();
    if( std::find( nearby.begin(), nearby.end(), g_selected ) == nearby.end() ) {
        g_selected = nearby.empty() ? nullptr : nearby.front();
    }

    ImGui::Checkbox( "仅看受损/损坏部件", &g_damaged_only );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 220.0f );
    ImGui::InputTextWithHint( "##veh_part_filter", "过滤部件名称…",
                              g_filter, IM_ARRAYSIZE( g_filter ) );
    if( g_filter[0] != '\0' ) {
        ImGui::SameLine();
        if( ImGui::Button( "清空" ) ) {
            g_filter[0] = '\0';
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "│ 周边载具：%d 辆（3x3 范围）", static_cast<int>( nearby.size() ) );

    ImGui::Separator();

    ImGui::BeginChild( "##veh_list", ImVec2( 260.0f, 0.0f ), true );
    if( nearby.empty() ) {
        ImGui::TextDisabled( "身边 3x3 范围内没有载具。" );
    } else {
        for( size_t i = 0; i < nearby.size(); ++i ) {
            vehicle *v = nearby[i];
            const int damaged = count_damaged_parts( *v );
            char label[256];
            std::snprintf( label, sizeof( label ), "%s (受损 %d)##veh_%zu",
                           v->name.c_str(), damaged, i );
            if( ImGui::Selectable( label, g_selected == v ) ) {
                g_selected = v;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild( "##veh_detail", ImVec2( 0.0f, 0.0f ), true );
    if( g_selected == nullptr ) {
        ImGui::TextDisabled( "选择左侧载具查看总览。" );
    } else {
        const vehicle &v = *g_selected;
        const int damaged = count_damaged_parts( v );
        const auto mass_kg = units::to_kilogram( v.total_mass() );
        ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "%s", v.name.c_str() );
        ImGui::Text( "速度：%d / 安全速度 %d  │  总质量：%lld kg  │  受损部件：%d",
                     v.velocity, v.safe_velocity(),
                     static_cast<long long>( mass_kg ), damaged );

        const std::map<itype_id, int> fuels = v.fuels_left();
        if( fuels.empty() ) {
            ImGui::TextDisabled( "燃料/能源：无" );
        } else {
            for( const auto &kv : fuels ) {
                const int cap = v.fuel_capacity( kv.first );
                ImGui::BulletText( "%s：%d / %d",
                                   kv.first->nname( kv.second ).c_str(),
                                   kv.second, cap );
            }
        }

        ImGui::Separator();
        int shown = 0;
        for( const vpart_reference &vpr : v.get_all_parts() ) {
            const vehicle_part &pt = vpr.part();
            const int max_hp = vpr.info().durability;
            const bool is_damaged = pt.is_broken() || ( max_hp > 0 && pt.hp() < max_hp );
            if( g_damaged_only && !is_damaged ) {
                continue;
            }
            const std::string pname = vpr.info().name();
            if( !contains_ci( pname, g_filter ) ) {
                continue;
            }
            ++shown;
            if( pt.is_broken() ) {
                ImGui::TextColored( ImVec4( 1.0f, 0.45f, 0.45f, 1.0f ),
                                    "[已损坏] %s (0 / %d)", pname.c_str(), max_hp );
            } else if( max_hp > 0 && pt.hp() < max_hp ) {
                ImGui::TextColored( ImVec4( 1.0f, 0.80f, 0.35f, 1.0f ),
                                    "[受损] %s (%d / %d)", pname.c_str(), pt.hp(), max_hp );
            } else {
                ImGui::Text( "%s (%d / %d)", pname.c_str(), pt.hp(), max_hp );
            }
        }
        if( shown == 0 ) {
            ImGui::TextDisabled( "没有符合条件的部件。" );
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace veh
} // namespace ui2

#endif // TILES

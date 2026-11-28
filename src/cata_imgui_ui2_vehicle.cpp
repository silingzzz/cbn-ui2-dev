#include "cata_imgui_ui2_vehicle.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "game.h"
#include "itype.h"
#include "map/map.h"
#include "player_activity.h"
#include "units_mass.h"
#include "vehicle/veh_interact.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vehicle_part.h"
#include "vehicle/vpart_position.h"
#include "vehicle/vpart_range.h"

namespace
{

enum class deferred_kind {
    none,
    interact,
    controls,
};

bool g_open = false;
vehicle *g_selected = nullptr;
vehicle *g_pending_veh = nullptr;
deferred_kind g_deferred = deferred_kind::none;
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

void draw_feature_button( vehicle &v, const char *label, const char *feature )
{
    if( !ui2::veh::has_feature( v, feature ) ) {
        return;
    }
    const bool enabled = ui2::veh::is_feature_enabled( v, feature );
    if( enabled ) {
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.20f, 0.52f, 0.30f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.26f, 0.62f, 0.38f, 1.0f ) );
    }
    char btn_label[64];
    std::snprintf( btn_label, sizeof( btn_label ), "%s: %s###btn_%s",
                   label, enabled ? "开" : "关", feature );
    if( ImGui::Button( btn_label ) ) {
        ui2::veh::toggle_feature( v, feature );
    }
    if( enabled ) {
        ImGui::PopStyleColor( 2 );
    }
    ImGui::SameLine();
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
    g_pending_veh = nullptr;
    g_deferred = deferred_kind::none;
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

bool has_feature( const ::vehicle &v, const std::string &feature )
{
    for( const vpart_reference &vpr : v.get_all_parts() ) {
        if( !vpr.part().is_broken() && vpr.info().has_flag( feature ) ) {
            return true;
        }
    }
    return false;
}

bool is_feature_enabled( const ::vehicle &v, const std::string &feature )
{
    for( const vpart_reference &vpr : v.get_all_parts() ) {
        const vehicle_part &pt = vpr.part();
        if( !pt.is_broken() && vpr.info().has_flag( feature ) && pt.enabled ) {
            return true;
        }
    }
    return false;
}

bool toggle_feature( ::vehicle &v, const std::string &feature )
{
    if( !has_feature( v, feature ) ) {
        return false;
    }
    const bool next_state = !is_feature_enabled( v, feature );
    for( const vpart_reference &vpr : v.get_all_parts() ) {
        vehicle_part &pt = v.part( vpr.part_index() );
        if( !pt.is_broken() && vpr.info().has_flag( feature ) ) {
            pt.enabled = next_state;
        }
    }
    v.refresh();
    if( g != nullptr ) {
        get_map().invalidate_lightmap_caches();
    }
    return true;
}

bool take_deferred_action()
{
    if( g == nullptr || g_pending_veh == nullptr || g_deferred == deferred_kind::none ) {
        return false;
    }
    vehicle *target = g_pending_veh;
    const deferred_kind kind = g_deferred;
    g_pending_veh = nullptr;
    g_deferred = deferred_kind::none;
    if( kind == deferred_kind::interact ) {
        std::unique_ptr<player_activity> act =
            veh_interact::run( *target, tripoint_mnt_veh::zero() );
        if( act ) {
            get_avatar().assign_activity( std::move( act ) );
        }
        return true;
    }
    if( kind == deferred_kind::controls ) {
        target->use_controls( get_avatar().bub_pos() );
        return true;
    }
    return false;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 132.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 940.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "载具总览与控制###ui2_vehicle_window", &g_open ) ) {
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

    ImGui::BeginChild( "##veh_list", ImVec2( 250.0f, 0.0f ), true );
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
        ImGui::TextDisabled( "选择左侧载具查看总览与控制。" );
    } else {
        vehicle &v = *g_selected;
        avatar &you = get_avatar();
        const int damaged = count_damaged_parts( v );
        const auto mass_kg = units::to_kilogram( v.total_mass() );
        ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "%s", v.name.c_str() );
        ImGui::SameLine();
        if( ImGui::Button( "打开完整改装界面…" ) ) {
            g_pending_veh = &v;
            g_deferred = deferred_kind::interact;
        }
        if( v.player_in_control( you ) ) {
            ImGui::SameLine();
            if( ImGui::Button( "车辆控制菜单…" ) ) {
                g_pending_veh = &v;
                g_deferred = deferred_kind::controls;
            }
        }

        ImGui::Text( "速度：%d / 安全速度 %d  │  总质量：%lld kg  │  受损部件：%d  │  引擎：%s",
                     v.velocity, v.safe_velocity(),
                     static_cast<long long>( mass_kg ), damaged,
                     v.engine_on ? "运行中" : "熄火" );

        draw_feature_button( v, "前灯", "CONE_LIGHT" );
        draw_feature_button( v, "探照灯", "CIRCLE_LIGHT" );
        draw_feature_button( v, "顶灯", "DOME_LIGHT" );
        draw_feature_button( v, "冰箱", "FRIDGE" );
        draw_feature_button( v, "冷冻柜", "FREEZER" );
        draw_feature_button( v, "充电站", "RECHARGER" );
        draw_feature_button( v, "音响", "STEREO" );
        ImGui::NewLine();

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

#include "cata_imgui.h"

#include <imgui.h>

#include "cata_imgui_ui2_map.h"

#if defined( TILES )

#include "clzones.h"
#include "creature.h"
#include "game.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "npc.h"
#include "vehicle/vehicle.h"

#include <optional>

namespace
{

std::optional<tripoint_bub_ms> g_hover;

} // namespace

namespace ui2
{
namespace mapview
{

void set_hover( const tripoint_bub_ms &pos )
{
    g_hover = pos;
}

void clear_hover()
{
    g_hover.reset();
}

void draw()
{
    if( !g_hover || ImGui::GetIO().WantCaptureMouse ) {
        return;
    }
    const tripoint_bub_ms p = *g_hover;
    map &here = get_map();

    ImGui::SetNextWindowPos( ImVec2( 1250.0f, 690.0f ), ImGuiCond_Always );
    ImGui::SetNextWindowBgAlpha( 0.88f );
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize;
    if( !ImGui::Begin( "##map_hover", nullptr, flags ) ) {
        ImGui::End();
        return;
    }

    ImGui::TextColored( ImVec4( 0.7f, 0.9f, 1.0f, 1.0f ), "地图对象" );
    const tripoint_abs_ms p_abs = bub_to_abs( p );
    ImGui::Text( "(%d, %d, %d) 绝对格", p_abs.x(), p_abs.y(), p_abs.z() );
    ImGui::Separator();

    const ter_id terrain = here.ter( p );
    if( terrain ) {
        ImGui::Text( "地形：%s", terrain->name() );
        ImGui::TextDisabled( "  %s", terrain.id().str().c_str() );
    }
    const furn_id furn = here.furn( p );
    if( furn ) {
        ImGui::Text( "家具：%s", furn.obj().name() );
        ImGui::TextDisabled( "  %s", furn.id().str().c_str() );
    }

    const optional_vpart_position vp = here.veh_at( p );
    if( vp ) {
        ImGui::Text( "载具：%s", vp->vehicle().disp_name().c_str() );
    }

    const map_stack items = here.i_at( p );
    if( !items.empty() ) {
        ImGui::Separator();
        ImGui::Text( "物品：%u 件", static_cast<unsigned>( items.size() ) );
        unsigned shown = 0;
        for( const item *it : items ) {
            ImGui::BulletText( "%s ×%d", it->tname().c_str(), it->count() );
            if( ++shown >= 6 ) {
                if( items.size() > shown ) {
                    ImGui::TextDisabled( "  …还有 %u 件",
                                         static_cast<unsigned>( items.size() - shown ) );
                }
                break;
            }
        }
    }

    Creature *critter = g->critter_at<Creature>( p );
    if( critter != nullptr ) {
        ImGui::Separator();
        if( npc *who = dynamic_cast<npc *>( critter ) ) {
            ImGui::Text( "NPC：%s", who->get_name().c_str() );
            ImGui::TextDisabled( "  态度：%s", npc_attitude_name( who->get_attitude() ).c_str() );
        } else {
            ImGui::Text( "生物：%s", critter->disp_name().c_str() );
        }
    }

    const zone_manager &mgr = zone_manager::get_manager();
    if( const zone_data *zone = mgr.get_zone_at( bub_to_abs( p ) ) ) {
        ImGui::Separator();
        ImGui::Text( "区域：%s（%s，%s）", zone->get_name().c_str(),
                     zone->get_type().c_str(), zone->get_enabled() ? "启用" : "停用" );
    }

    ImGui::End();
}

} // namespace mapview
} // namespace ui2

#endif // TILES

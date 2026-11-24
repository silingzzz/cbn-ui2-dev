#include <imgui.h>
#include <SDL3/SDL_keycode.h>

#include "cata_imgui_ui2_world.h"

#if defined( TILES )

#include <algorithm>
#include <optional>
#include <string>

#include "avatar.h"
#include "cata_imgui.h"
#include "cata_imgui_ui2_style.h"
#include "clzones.h"
#include "creature.h"
#include "game.h"
#include "item.h"
#include "line.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "npc.h"
#include "output.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_position.h"

namespace
{

bool g_open = false;
std::optional<tripoint_bub_ms> g_hover;
std::optional<tripoint_bub_ms> g_selected;
ui2::world::request g_request;
std::string g_result;
bool g_result_error = false;

bool g_context_requested = false;
bool g_context_open = false;
ImVec2 g_context_pos( 0.0f, 0.0f );
ImVec2 g_panel_min( 0.0f, 0.0f );
ImVec2 g_panel_max( 0.0f, 0.0f );

auto adjacent( const avatar &you, const tripoint_bub_ms &p ) -> bool
{
    return square_dist( p.xy(), you.bub_pos().xy() ) <= 1;
}

auto inside_for( const avatar &you ) -> bool
{
    return !get_map().is_outside( you.bub_pos() );
}

auto can_open_at( const avatar &you, const tripoint_bub_ms &p ) -> bool
{
    return get_map().can_open_door( &you, p, inside_for( you ) );
}

auto can_close_at( const avatar &you, const tripoint_bub_ms &p ) -> bool
{
    return get_map().close_door( p, inside_for( you ), true );
}

auto has_items_at( const tripoint_bub_ms &p ) -> bool
{
    return !get_map().i_at( p ).empty();
}

auto action_reason( ui2::world::action_kind kind ) -> std::string
{
    if( kind == ui2::world::action_kind::none ) {
        return {};
    }
    if( !g_selected ) {
        return "先在切图上选择一个地块";
    }
    const avatar &you = get_avatar();
    const tripoint_bub_ms p = *g_selected;
    const bool seen = you.sees( p );
    const bool near = adjacent( you, p );
    if( !seen ) {
        return "看不到这个地块";
    }
    switch( kind ) {
        case ui2::world::action_kind::examine:
            if( !near ) {
                return "太远，需站在相邻格";
            }
            return {};
        case ui2::world::action_kind::pickup:
            if( !near ) {
                return "太远，需站在相邻格";
            }
            if( !has_items_at( p ) ) {
                return "这里没有可拾取的物品";
            }
            return {};
        case ui2::world::action_kind::open:
            if( !near ) {
                return "太远，需站在相邻格";
            }
            if( !can_open_at( you, p ) ) {
                return "这里没有可打开的门";
            }
            return {};
        case ui2::world::action_kind::close:
            if( !near ) {
                return "太远，需站在相邻格";
            }
            if( !can_close_at( you, p ) ) {
                return "这里没有可关闭的门";
            }
            return {};
        case ui2::world::action_kind::walk_to:
            if( p == you.bub_pos() ) {
                return "你已经站在这个地块上";
            }
            return {};
        case ui2::world::action_kind::none:
        default:
            return {};
    }
}

void queue( ui2::world::action_kind kind )
{
    if( !g_selected ) {
        return;
    }
    g_request.kind = kind;
    g_request.target = *g_selected;
}

void queue_default()
{
    if( !g_selected ) {
        return;
    }
    const avatar &you = get_avatar();
    const tripoint_bub_ms p = *g_selected;
    const bool near = adjacent( you, p );
    if( near && can_open_at( you, p ) ) {
        queue( ui2::world::action_kind::open );
    } else if( near && can_close_at( you, p ) ) {
        queue( ui2::world::action_kind::close );
    } else if( has_items_at( p ) && near ) {
        queue( ui2::world::action_kind::pickup );
    } else if( near ) {
        queue( ui2::world::action_kind::examine );
    } else {
        queue( ui2::world::action_kind::walk_to );
    }
}

void move_selection( int dx, int dy, int dz )
{
    if( !g_selected ) {
        g_selected = get_avatar().bub_pos();
    }
    tripoint_bub_ms p = *g_selected;
    p.x() += dx;
    p.y() += dy;
    p.z() += dz;
    if( get_map().inbounds( p ) ) {
        g_selected = p;
        g_result.clear();
    }
}

void draw_tile_contents( const tripoint_bub_ms &p )
{
    map &here = get_map();
    const tripoint_abs_ms p_abs = bub_to_abs( p );
    ImGui::Text( "(%d, %d, %d)  ·  绝对 (%d, %d, %d)", p.x(), p.y(), p.z(),
                 p_abs.x(), p_abs.y(), p_abs.z() );

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

    if( const zone_data *zone = zone_manager::get_manager().get_zone_at( p_abs ) ) {
        ImGui::Text( "区域：%s（%s，%s）", zone->get_name().c_str(),
                     zone->get_type().c_str(), zone->get_enabled() ? "启用" : "停用" );
    }

    const map_stack items = here.i_at( p );
    if( !items.empty() ) {
        ImGui::Separator();
        ImGui::Text( "地面物品：%u 件", static_cast<unsigned>( items.size() ) );
        if( ImGui::BeginChild( "##world_items", ImVec2( 0.0f, 92.0f ), true ) ) {
            unsigned shown = 0;
            for( const item *it : items ) {
                ImGui::BulletText( "%s ×%d",
                                   remove_color_tags( it->type_name( it->count() ) ).c_str(),
                                   it->count() );
                if( ++shown >= 8 ) {
                    if( items.size() > shown ) {
                        ImGui::TextDisabled( "  …还有 %u 件",
                                             static_cast<unsigned>( items.size() - shown ) );
                    }
                    break;
                }
            }
        }
        ImGui::EndChild();
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
}

void draw_action_button( const char *label, ui2::world::action_kind kind )
{
    const std::string reason = action_reason( kind );
    ImGui::BeginDisabled( !reason.empty() );
    if( ImGui::Button( label, ImVec2( 62.0f, 28.0f ) ) ) {
        queue( kind );
    }
    ImGui::EndDisabled();
    if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) && !reason.empty() ) {
        ImGui::SetTooltip( "%s", reason.c_str() );
    }
}

void draw_context_menu()
{
    if( g_context_requested ) {
        g_context_requested = false;
        ImGui::OpenPopup( "##world_actions" );
        g_context_open = true;
    }
    if( !g_context_open ) {
        return;
    }
    ImGui::SetNextWindowPos( g_context_pos, ImGuiCond_Appearing );
    if( !ImGui::BeginPopup( "##world_actions" ) ) {
        g_context_open = false;
        return;
    }
    ImGui::TextDisabled( "地块动作" );
    ImGui::Separator();

    auto item = []( const char *label, ui2::world::action_kind kind ) {
        const std::string reason = action_reason( kind );
        if( !reason.empty() ) {
            ImGui::BeginDisabled();
        }
        if( ImGui::MenuItem( label ) ) {
            queue( kind );
            ImGui::CloseCurrentPopup();
        }
        if( !reason.empty() ) {
            ImGui::EndDisabled();
            if( ImGui::IsItemHovered( ImGuiHoveredFlags_AllowWhenDisabled ) ) {
                ImGui::SetTooltip( "%s", reason.c_str() );
            }
        }
    };
    item( "查看", ui2::world::action_kind::examine );
    item( "拾取", ui2::world::action_kind::pickup );
    item( "开门", ui2::world::action_kind::open );
    item( "关门", ui2::world::action_kind::close );
    item( "走到", ui2::world::action_kind::walk_to );
    ImGui::Separator();
    if( ImGui::MenuItem( "取消" ) ) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

} // namespace

namespace ui2
{
namespace world
{

auto is_open() -> bool
{
    return g_open;
}

auto mouse_over_panel( float x, float y ) -> bool
{
    return g_open && x >= g_panel_min.x && x <= g_panel_max.x &&
           y >= g_panel_min.y && y <= g_panel_max.y;
}

void set_open( bool open )
{
    g_open = open;
    cata_imgui::set_mouse_passthrough( open );
    if( !open ) {
        g_hover.reset();
        g_selected.reset();
        g_request = request{};
        g_result.clear();
        g_result_error = false;
        g_context_requested = false;
        g_context_open = false;
    }
}

void set_hover( const tripoint_bub_ms &pos )
{
    g_hover = pos;
}

void clear_hover()
{
    g_hover.reset();
}

void on_left_click( const tripoint_bub_ms &pos )
{
    if( g_context_open ) {
        g_context_open = false;
    }
    if( !g_selected || *g_selected != pos ) {
        g_selected = pos;
        g_result.clear();
        return;
    }
    queue_default();
}

void on_right_click( const tripoint_bub_ms &pos )
{
    g_selected = pos;
    g_result.clear();
    g_context_pos = ImGui::GetIO().MousePos;
    g_context_requested = true;
}

auto handle_escape() -> bool
{
    if( g_context_open ) {
        g_context_open = false;
        return true;
    }
    if( g_selected ) {
        g_selected.reset();
        g_result.clear();
        return true;
    }
    if( !g_result.empty() ) {
        g_result.clear();
        return true;
    }
    return false;
}

auto handle_key( int sdl_key, bool shift ) -> bool
{
    if( !g_open ) {
        return false;
    }
    switch( sdl_key ) {
        case SDLK_UP:
            move_selection( 0, -1, 0 );
            return true;
        case SDLK_DOWN:
            move_selection( 0, 1, 0 );
            return true;
        case SDLK_LEFT:
            move_selection( -1, 0, 0 );
            return true;
        case SDLK_RIGHT:
            move_selection( 1, 0, 0 );
            return true;
        case SDLK_COMMA:
            if( shift ) {
                move_selection( 0, 0, -1 );
                return true;
            }
            return false;
        case SDLK_PERIOD:
            if( shift ) {
                move_selection( 0, 0, 1 );
                return true;
            }
            return false;
        case 'e':
        case 'E':
            queue( action_kind::examine );
            return true;
        case 'g':
        case 'G':
            queue( action_kind::pickup );
            return true;
        case 'o':
        case 'O':
            queue( action_kind::open );
            return true;
        case 'c':
        case 'C':
            queue( action_kind::close );
            return true;
        case 'w':
        case 'W':
            queue( action_kind::walk_to );
            return true;
        default:
            return false;
    }
}

void draw()
{    if( !g_open ) {
        return;
    }
    const ImGuiIO &io = ImGui::GetIO();
    const float panel_width = 360.0f;
    const float panel_height = std::min( 560.0f, io.DisplaySize.y - 150.0f );
    // Place the panel over the map's upper-right quadrant, clear of both the
    // character sidebar and the cabinet row the player usually inspects.
    const float panel_x = std::max( 16.0f, io.DisplaySize.x - 1100.0f );
    g_panel_min = ImVec2( panel_x, 116.0f );
    g_panel_max = ImVec2( panel_x + panel_width, 116.0f + panel_height );
    ImGui::SetNextWindowPos( g_panel_min, ImGuiCond_Always );
    ImGui::SetNextWindowSize( ImVec2( panel_width, panel_height ), ImGuiCond_Always );
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if( !ImGui::Begin( "##world_inspect", nullptr, flags ) ) {
        ImGui::End();
        return;
    }

    ImGui::TextColored( style::accent, "世界查看" );
    ImGui::SameLine();
    if( ImGui::SmallButton( "×" ) ) {
        set_open( false );
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "Esc 逐层退出" );
    ImGui::TextDisabled( "悬停高亮 · 单击选格 · 再次单击执行默认动作 · 右键动作菜单" );
    ImGui::Separator();

    if( !g_selected ) {
        ImGui::TextColored( style::warning, "未选中地块" );
        if( g_hover ) {
            ImGui::TextDisabled( "悬停格（点击选中）" );
            draw_tile_contents( *g_hover );
        } else {
            ImGui::TextWrapped( "把鼠标移到地图上，或按方向键移动选中格。" );
        }
    } else {
        draw_tile_contents( *g_selected );
        ImGui::Separator();
        ImGui::TextDisabled( "可用动作（禁用项会显示原因）" );
        draw_action_button( "查看", action_kind::examine );
        ImGui::SameLine();
        draw_action_button( "拾取", action_kind::pickup );
        ImGui::SameLine();
        draw_action_button( "开门", action_kind::open );
        ImGui::SameLine();
        draw_action_button( "关门", action_kind::close );
        ImGui::SameLine();
        draw_action_button( "走到", action_kind::walk_to );

        const avatar &you = get_avatar();
        const bool near = adjacent( you, *g_selected );
        if( can_open_at( you, *g_selected ) ) {
            ImGui::TextDisabled( "门：当前可开" );
        } else if( can_close_at( you, *g_selected ) ) {
            ImGui::TextDisabled( "门：当前可关" );
        } else if( near ) {
            ImGui::TextDisabled( "门：相邻格没有可开关的门" );
        }

        if( !near ) {
            ImGui::TextColored( style::warning, "距离：%d 格，查看/拾取/开门/关门需相邻",
                                square_dist( g_selected->xy(), you.bub_pos().xy() ) );
        }
        ImGui::TextDisabled( "快捷键：E 查看 · G 拾取 · O 开门 · C 关门 · W 走到" );
    }

    if( !g_result.empty() ) {
        ImGui::Separator();
        ImGui::TextColored( g_result_error ? style::danger : style::accent, "%s",
                            g_result.c_str() );
    }

    ImGui::End();

    if( g_open && !ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId |
                                       ImGuiPopupFlags_AnyPopupLevel ) &&
        !ImGui::IsAnyItemActive() ) {
        if( ImGui::IsKeyPressed( ImGuiKey_UpArrow ) ) {
            move_selection( 0, -1, 0 );
        } else if( ImGui::IsKeyPressed( ImGuiKey_DownArrow ) ) {
            move_selection( 0, 1, 0 );
        } else if( ImGui::IsKeyPressed( ImGuiKey_LeftArrow ) ) {
            move_selection( -1, 0, 0 );
        } else if( ImGui::IsKeyPressed( ImGuiKey_RightArrow ) ) {
            move_selection( 1, 0, 0 );
        } else if( ImGui::GetIO().KeyShift && ImGui::IsKeyPressed( ImGuiKey_Comma ) ) {
            move_selection( 0, 0, -1 );
        } else if( ImGui::GetIO().KeyShift && ImGui::IsKeyPressed( ImGuiKey_Period ) ) {
            move_selection( 0, 0, 1 );
        } else if( ImGui::IsKeyPressed( ImGuiKey_E ) ) {
            queue( action_kind::examine );
        } else if( ImGui::IsKeyPressed( ImGuiKey_G ) ) {
            queue( action_kind::pickup );
        } else if( ImGui::IsKeyPressed( ImGuiKey_O ) ) {
            queue( action_kind::open );
        } else if( ImGui::IsKeyPressed( ImGuiKey_C ) ) {
            queue( action_kind::close );
        } else if( ImGui::IsKeyPressed( ImGuiKey_W ) ) {
            queue( action_kind::walk_to );
        }
    }

    draw_context_menu();
}

void highlight_tile()
{
    if( !g_open ) {
        return;
    }
    if( g_selected ) {
        g->draw_highlight( *g_selected );
    } else if( g_hover ) {
        g->draw_highlight( *g_hover );
    }
}

auto has_request() -> bool
{
    return g_request.kind != action_kind::none;
}

auto take_request( request &out ) -> bool
{
    if( g_request.kind == action_kind::none ) {
        return false;
    }
    out = g_request;
    g_request = request{};
    return true;
}

void set_result( const std::string &text, bool error )
{
    g_result = text;
    g_result_error = error;
}

} // namespace world
} // namespace ui2

#endif // TILES

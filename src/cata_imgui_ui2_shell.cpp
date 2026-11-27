#include "cata_imgui_ui2_shell.h"

#if defined( TILES )

#include <imgui.h>
#include <imgui_internal.h>

#include "avatar.h"
#include "cata_imgui.h"
#include "cata_imgui_ui2_aim.h"
#include "cata_imgui_ui2_craft.h"
#include "cata_imgui_ui2_gear.h"
#include "cata_imgui_ui2_messages.h"
#include "cata_imgui_ui2_vehicle.h"
#include "cata_imgui_ui2_zone.h"
#include "cata_imgui_ui2_world.h"
#include "game.h"

namespace ui2
{
namespace shell
{

namespace
{

auto panel_name( panel p ) -> const char *
{
    switch( p ) {
        case panel::aim:
            return "物品";
        case panel::gear:
            return "装备";
        case panel::craft:
            return "制作";
        case panel::messages:
            return "消息";
        case panel::zone:
            return "区域";
        case panel::world:
            return "世界";
        case panel::vehicle:
            return "载具";
        case panel::none:
        default:
            return "无";
    }
}


void set_open( panel p, bool open )
{
    switch( p ) {
        case panel::aim:
            ui2::aim::set_open( open );
            break;
        case panel::gear:
            ui2::gear::set_open( open );
            break;
        case panel::craft:
            ui2::craft::set_open( open );
            break;
        case panel::messages:
            ui2::messages::set_open( open );
            break;
        case panel::zone:
            ui2::zone::set_open( open );
            break;
        case panel::world:
            ui2::world::set_open( open );
            break;
        case panel::vehicle:
            ui2::veh::set_open( open );
            break;
        case panel::none:
        default:
            break;
    }
}

void draw_status()
{
    if( g == nullptr ) {
        ImGui::TextDisabled( "CBN UI 2.0  ·  正在载入世界…" );
        return;
    }
    const avatar &you = get_avatar();
    // During pack loading the game allocates `g` before body parts exist.
    // get_hp()/get_hp_max() resolves body-part ids, so only touch them once
    // the avatar actually has a body.
    if( you.get_body().empty() ) {
        ImGui::TextDisabled( "CBN UI 2.0  ·  正在载入世界…" );
        return;
    }
    ImGui::TextColored( ImVec4( 0.55f, 0.82f, 1.0f, 1.0f ), "CBN UI 2.0" );
    ImGui::SameLine();
    ImGui::Text( "│ %s", you.get_name().c_str() );
    ImGui::SameLine();
    ImGui::Text( "│ HP %d / %d", you.get_hp(), you.get_hp_max() );
    if( you.get_hp_max() > 0 ) {
        ImGui::SameLine();
        ImGui::ProgressBar( static_cast<float>( you.get_hp() ) /
                            static_cast<float>( you.get_hp_max() ),
                            ImVec2( 120.0f, 0.0f ) );
    }
}

void draw_panel_button( const char *label, panel target )
{
    const bool on = active() == target;
    if( on ) {
        ImGui::PushStyleColor( ImGuiCol_Button, ImVec4( 0.18f, 0.38f, 0.58f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_ButtonHovered, ImVec4( 0.22f, 0.46f, 0.68f, 1.0f ) );
    }
    if( ImGui::Button( label, ImVec2( 74.0f, 28.0f ) ) ) {
        if( on ) {
            close_all();
        } else {
            open( target );
        }
    }
    if( on ) {
        ImGui::PopStyleColor( 2 );
    }
}

} // namespace

auto active() -> panel
{
    if( ui2::aim::is_open() ) {
        return panel::aim;
    }
    if( ui2::gear::is_open() ) {
        return panel::gear;
    }
    if( ui2::craft::is_open() ) {
        return panel::craft;
    }
    if( ui2::messages::is_open() ) {
        return panel::messages;
    }
    if( ui2::zone::is_open() ) {
        return panel::zone;
    }
    if( ui2::world::is_open() ) {
        return panel::world;
    }
    if( ui2::veh::is_open() ) {
        return panel::vehicle;
    }
    return panel::none;
}

auto any_open() -> bool
{
    return active() != panel::none;
}

void close_all()
{
    for( panel p : { panel::aim, panel::gear, panel::craft, panel::messages, panel::zone,
                     panel::world, panel::vehicle } ) {
        set_open( p, false );
    }
    cata_imgui::clear_widget_focus();
}

void open( panel target )
{
    close_all();
    if( target != panel::none ) {
        set_open( target, true );
    }
}

void draw()
{
    // Escape is the single return path: close the active panel first, then
    // close the overlay. Popups and focused inputs keep their own Escape.
    const bool popup_open = ImGui::IsPopupOpen( "", ImGuiPopupFlags_AnyPopupId |
                            ImGuiPopupFlags_AnyPopupLevel );
    if( ImGui::IsKeyPressed( ImGuiKey_Escape ) && !popup_open && !ImGui::IsAnyItemActive() ) {
        if( ui2::world::is_open() && ui2::world::handle_escape() ) {
            // The world panel consumes the first Escape to clear selection or
            // the last result; the next Escape closes the panel itself.
        } else if( any_open() ) {
            close_all();
        } else if( cata_imgui::is_open() ) {
            cata_imgui::toggle();
        }
    }

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 12.0f ),
                             ImGuiCond_Always, ImVec2( 0.0f, 0.0f ) );
    ImGui::SetNextWindowBgAlpha( 0.92f );
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoScrollbar |
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoSavedSettings;

    if( ImGui::Begin( "##ui2_shell", nullptr, flags ) ) {
        draw_status();
        ImGui::Separator();

        draw_panel_button( "物品", panel::aim );
        ImGui::SameLine();
        draw_panel_button( "装备", panel::gear );
        ImGui::SameLine();
        draw_panel_button( "制作", panel::craft );
        ImGui::SameLine();
        draw_panel_button( "消息", panel::messages );
        ImGui::SameLine();
        draw_panel_button( "区域", panel::zone );
        ImGui::SameLine();
        draw_panel_button( "世界", panel::world );
        ImGui::SameLine();
        draw_panel_button( "载具", panel::vehicle );
        ImGui::SameLine();
        ImGui::TextDisabled( "│ 当前：%s", panel_name( active() ) );

        ImGui::SameLine();
        ImGui::BeginDisabled( !any_open() );
        if( ImGui::Button( "全部关闭", ImVec2( 92.0f, 28.0f ) ) ) {
            close_all();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if( ImGui::Button( "隐藏 (F10)", ImVec2( 100.0f, 28.0f ) ) ) {
            cata_imgui::toggle();
        }

        if( any_open() ) {
            ImGui::TextDisabled( "Esc 返回游戏  ·  一次只开一个窗口  ·  旧 UI 仍可用作回退" );
        } else {
            ImGui::TextDisabled( "选一个系统开始  ·  Esc / F10 返回游戏" );
        }
    }
    ImGui::End();
}

} // namespace shell
} // namespace ui2

#endif // TILES

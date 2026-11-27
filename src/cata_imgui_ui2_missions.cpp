#include "cata_imgui_ui2_missions.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "calendar.h"
#include "game.h"
#include "mission.h"
#include "npc.h"
#include "string_formatter.h"
#include "string_utils.h"
#include "translations.h"

namespace
{

bool g_open = false;
ui2::missions::tab_kind g_tab = ui2::missions::tab_kind::active;
int g_selected_uid = -1;
char g_filter[64] = "";
std::string g_notice;

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

std::string format_tokenized_description( const mission &m )
{
    std::string formatted = m.get_description();
    for( const auto &reward : m.get_likely_rewards() ) {
        const std::string token = "<reward_count:" + reward.second.str() + ">";
        formatted = replace_all( formatted, token, string_format( "%d", reward.first ) );
    }
    return formatted;
}

const char *tab_label( ui2::missions::tab_kind tab )
{
    switch( tab ) {
        case ui2::missions::tab_kind::active:
            return "进行中";
        case ui2::missions::tab_kind::completed:
            return "已完成";
        case ui2::missions::tab_kind::failed:
            return "已失败";
        default:
            return "进行中";
    }
}

std::vector<mission *> collect_missions( const avatar &you, ui2::missions::tab_kind tab )
{
    switch( tab ) {
        case ui2::missions::tab_kind::active:
            return you.get_active_missions();
        case ui2::missions::tab_kind::completed:
            return you.get_completed_missions();
        case ui2::missions::tab_kind::failed:
            return you.get_failed_missions();
        default:
            return {};
    }
}

} // namespace

namespace ui2
{
namespace missions
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
    if( !open ) {
        g_notice.clear();
    }
}

void reset_session()
{
    g_open = false;
    g_tab = tab_kind::active;
    g_selected_uid = -1;
    g_filter[0] = '\0';
    g_notice.clear();
}

void set_tab( tab_kind tab )
{
    if( g_tab != tab ) {
        g_tab = tab;
        g_selected_uid = -1;
        g_notice.clear();
    }
}

tab_kind current_tab()
{
    return g_tab;
}

bool track_mission( mission *target )
{
    if( g == nullptr || target == nullptr ) {
        return false;
    }
    avatar &you = get_avatar();
    const std::vector<mission *> active = you.get_active_missions();
    if( std::find( active.begin(), active.end(), target ) == active.end() ) {
        return false;
    }
    you.set_active_mission( *target );
    g_selected_uid = target->get_id();
    g_notice = string_format( "已将“%s”设为当前追踪任务。", target->name() );
    return true;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 132.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 900.0f, 540.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "任务追踪 (只读)###ui2_missions_window", &g_open ) ) {
        ImGui::End();
        return;
    }

    if( g == nullptr ) {
        ImGui::TextDisabled( "世界尚未载入。" );
        ImGui::End();
        return;
    }

    avatar &you = get_avatar();
    mission *tracked = you.get_active_mission();

    if( ImGui::Button( "进行中", ImVec2( 90.0f, 0.0f ) ) ) {
        set_tab( tab_kind::active );
    }
    ImGui::SameLine();
    if( ImGui::Button( "已完成", ImVec2( 90.0f, 0.0f ) ) ) {
        set_tab( tab_kind::completed );
    }
    ImGui::SameLine();
    if( ImGui::Button( "已失败", ImVec2( 90.0f, 0.0f ) ) ) {
        set_tab( tab_kind::failed );
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "│ 当前分类：%s", tab_label( g_tab ) );
    ImGui::SameLine();
    ImGui::SetNextItemWidth( 220.0f );
    ImGui::InputTextWithHint( "##missions_filter", "搜索任务名称或描述…",
                              g_filter, IM_ARRAYSIZE( g_filter ) );
    if( g_filter[0] != '\0' ) {
        ImGui::SameLine();
        if( ImGui::Button( "清空" ) ) {
            g_filter[0] = '\0';
        }
    }

    if( tracked != nullptr ) {
        ImGui::TextColored( ImVec4( 0.45f, 0.90f, 0.55f, 1.0f ),
                            "当前追踪：%s", tracked->name().c_str() );
    } else {
        ImGui::TextDisabled( "当前没有追踪中的任务。" );
    }
    if( !g_notice.empty() ) {
        ImGui::SameLine();
        ImGui::TextColored( ImVec4( 0.45f, 0.85f, 1.0f, 1.0f ), "│ %s", g_notice.c_str() );
    }

    ImGui::Separator();

    const std::vector<mission *> raw_list = collect_missions( you, g_tab );
    std::vector<mission *> filtered;
    filtered.reserve( raw_list.size() );
    for( mission *m : raw_list ) {
        if( m == nullptr ) {
            continue;
        }
        const std::string haystack = m->name() + " " + m->get_description();
        if( contains_ci( haystack, g_filter ) ) {
            filtered.push_back( m );
        }
    }

    mission *selected = nullptr;
    for( mission *m : filtered ) {
        if( m->get_id() == g_selected_uid ) {
            selected = m;
            break;
        }
    }
    if( selected == nullptr && !filtered.empty() ) {
        selected = filtered.front();
        g_selected_uid = selected->get_id();
    }

    ImGui::BeginChild( "##missions_list", ImVec2( 320.0f, 0.0f ), true );
    if( filtered.empty() ) {
        ImGui::TextDisabled( "当前分类下没有匹配的任务。" );
    } else {
        for( mission *m : filtered ) {
            const bool is_tracked = ( m == tracked );
            const bool is_sel = ( m == selected );
            char label[256];
            std::snprintf( label, sizeof( label ), "%s%s##m_%d",
                           is_tracked ? "[追踪中] " : "",
                           m->name().c_str(),
                           m->get_id() );
            if( ImGui::Selectable( label, is_sel ) ) {
                g_selected_uid = m->get_id();
                selected = m;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild( "##missions_detail", ImVec2( 0.0f, 0.0f ), true );
    if( selected == nullptr ) {
        ImGui::TextDisabled( "选择左侧任务查看详情。" );
    } else {
        ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "%s", selected->name().c_str() );
        if( selected->get_npc_id().is_valid() ) {
            if( const npc *guy = g->find_npc( selected->get_npc_id() ) ) {
                ImGui::Text( "委托人：%s", guy->disp_name().c_str() );
            }
        }
        ImGui::Text( "状态：%s%s", tab_label( g_tab ),
                     selected == tracked ? "（当前追踪）" : "" );

        if( selected->has_deadline() ) {
            const time_point deadline = selected->get_deadline();
            ImGui::Text( "截止时间：%s", to_string( deadline ).c_str() );
            if( g_tab != tab_kind::completed ) {
                const time_duration remaining = deadline - calendar::turn;
                if( remaining <= 0_turns ) {
                    ImGui::TextColored( ImVec4( 1.0f, 0.45f, 0.45f, 1.0f ), "剩余时间：已过期" );
                } else if( you.has_watch() ) {
                    ImGui::Text( "剩余时间：%s", to_string( remaining ).c_str() );
                } else {
                    ImGui::TextDisabled( "剩余时间：需要手表查看" );
                }
            }
        } else {
            ImGui::TextDisabled( "截止时间：无限制" );
        }

        if( selected->has_target() ) {
            const tripoint_abs_omt target = selected->get_target();
            ImGui::Text( "目标大地图坐标：(%d, %d, %d)",
                         target.x(), target.y(), target.z() );
        }

        if( g_tab == tab_kind::active ) {
            ImGui::Separator();
            const bool already_tracked = ( selected == tracked );
            ImGui::BeginDisabled( already_tracked );
            if( ImGui::Button( already_tracked ? "当前正在追踪" : "设为当前追踪",
                               ImVec2( 160.0f, 0.0f ) ) ) {
                track_mission( selected );
            }
            ImGui::EndDisabled();
        }

        ImGui::Separator();
        const std::string desc = format_tokenized_description( *selected );
        if( desc.empty() ) {
            ImGui::TextDisabled( "无任务描述。" );
        } else {
            ImGui::TextWrapped( "%s", desc.c_str() );
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace missions
} // namespace ui2

#endif // TILES

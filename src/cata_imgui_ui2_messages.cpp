#include "cata_imgui_ui2_messages.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>

#include "messages.h"
#include "output.h"

namespace
{

bool g_open = false;
char g_search[128] = {};
int g_selected = -1;
bool g_clear_pending = false;
bool g_confirm_clear = false;
std::string g_notice;

bool contains_ci( const std::string &haystack, const std::string &needle )
{
    if( needle.empty() ) {
        return true;
    }
    auto it = std::search( haystack.begin(), haystack.end(),
                           needle.begin(), needle.end(),
    []( char a, char b ) {
        return std::tolower( static_cast<unsigned char>( a ) ) ==
               std::tolower( static_cast<unsigned char>( b ) );
    } );
    return it != haystack.end();
}

std::string clean( const std::string &text )
{
    return remove_color_tags( text );
}

} // namespace

namespace ui2
{
namespace messages
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

bool has_deferred_action()
{
    return g_clear_pending;
}

bool take_deferred_action()
{
    if( !g_clear_pending ) {
        return false;
    }
    g_clear_pending = false;
    Messages::clear_messages();
    g_selected = -1;
    g_notice = "消息记录已清空";
    return true;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    std::vector<std::pair<std::string, std::string>> all = Messages::recent_messages( 300 );
    std::vector<int> visible;
    visible.reserve( all.size() );
    for( int i = 0; i < static_cast<int>( all.size() ); ++i ) {
        const std::string timestamp = clean( all[i].first );
        const std::string text = clean( all[i].second );
        if( contains_ci( timestamp + " " + text, g_search ) ) {
            visible.push_back( i );
        }
    }
    if( g_selected >= static_cast<int>( all.size() ) ) {
        g_selected = -1;
    }

    ImGui::SetNextWindowPos( ImVec2( 980.0f, 110.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 700.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "消息记录", &g_open ) ) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth( 260.0f );
    ImGui::InputTextWithHint( "##msg_search", "搜索时间或内容…", g_search, IM_ARRAYSIZE( g_search ) );
    ImGui::SameLine();
    ImGui::TextDisabled( "显示 %d / %d 条", static_cast<int>( visible.size() ),
                         static_cast<int>( all.size() ) );
    // Keep the destructive action on its own row.  Sharing the search row put
    // its hit box underneath the wide search widget on some DPI/font metrics.
    ImGui::NewLine();
    if( ImGui::Button( "清空记录" ) ) {
        g_confirm_clear = true;
    }
    if( g_confirm_clear ) {
        ImGui::TextColored( ImVec4( 0.95f, 0.60f, 0.55f, 1.0f ),
                            "确认清空当前消息记录？不可撤销。" );
        ImGui::SameLine();
        if( ImGui::Button( "确认清空" ) ) {
            g_clear_pending = true;
            g_confirm_clear = false;
            g_notice.clear();
        }
        ImGui::SameLine();
        if( ImGui::Button( "取消" ) ) {
            g_confirm_clear = false;
        }
    }
    if( !g_notice.empty() ) {
        ImGui::TextColored( ImVec4( 0.55f, 0.85f, 0.55f, 1.0f ), "%s", g_notice.c_str() );
    }
    ImGui::Separator();

    const float list_height = ImGui::GetContentRegionAvail().y * 0.48f;
    if( ImGui::BeginChild( "msg_list", ImVec2( 0.0f, list_height ), true,
    ImGuiWindowFlags_AlwaysVerticalScrollbar ) ) {
        if( visible.empty() ) {
            ImGui::TextDisabled( "（没有匹配的消息）" );
        }
        // Newest first, while retaining the original index for selection.
        for( auto it = visible.rbegin(); it != visible.rend(); ++it ) {
            const int idx = *it;
            const std::string timestamp = clean( all[idx].first );
            const std::string text = clean( all[idx].second );
            char label[512];
            std::snprintf( label, sizeof( label ), "%s##msg%d", text.c_str(), idx );
            if( ImGui::Selectable( label, g_selected == idx ) ) {
                g_selected = idx;
            }
            if( ImGui::IsItemHovered() ) {
                ImGui::SetTooltip( "%s", timestamp.c_str() );
            }
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    const float detail_height = ImGui::GetContentRegionAvail().y;
    if( ImGui::BeginChild( "msg_detail", ImVec2( 0.0f, detail_height ), true,
                           ImGuiWindowFlags_AlwaysVerticalScrollbar ) ) {
        if( g_selected < 0 ) {
            ImGui::TextDisabled( "选择一条消息查看详情。" );
        } else {
            ImGui::TextDisabled( "时间：%s", clean( all[g_selected].first ).c_str() );
            ImGui::Separator();
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted( clean( all[g_selected].second ).c_str() );
            ImGui::PopTextWrapPos();
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace messages
} // namespace ui2

#endif // TILES

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

    ImGui::SetNextWindowPos( ImVec2( 980.0f, 180.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 700.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "消息记录（只读预览）", &g_open ) ) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth( 260.0f );
    ImGui::InputTextWithHint( "##msg_search", "搜索时间或内容…", g_search, IM_ARRAYSIZE( g_search ) );
    ImGui::SameLine();
    ImGui::TextDisabled( "显示 %d / %d 条", static_cast<int>( visible.size() ),
                         static_cast<int>( all.size() ) );
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

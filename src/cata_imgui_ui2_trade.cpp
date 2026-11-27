#include "cata_imgui_ui2_trade.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "game.h"
#include "item.h"
#include "line.h"
#include "npc.h"
#include "output.h"
#include "units.h"

namespace
{

bool g_open = false;
npc *g_selected = nullptr;
ui2::trade::list_tab g_tab = ui2::trade::list_tab::theirs;
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

void draw_pricing_list( const std::vector<item_pricing> &items )
{
    int shown = 0;
    for( size_t i = 0; i < items.size(); ++i ) {
        const item_pricing &ip = items[i];
        if( ip.locs.empty() || ip.locs.front() == nullptr ) {
            continue;
        }
        const item *it = ip.locs.front();
        const std::string name = it->display_name();
        if( !contains_ci( name, g_filter ) ) {
            continue;
        }
        ++shown;
        const int qty = ip.charges > 0 ? ip.charges : ip.count;
        const double unit_val = static_cast<double>( ip.price ) / 100.0;
        const double total_val = unit_val * std::max( qty, 1 );
        const int weight_g = units::to_gram( ip.weight );
        const int vol_ml = units::to_milliliter( ip.vol );
        ImGui::BulletText( "%s x%d  │  单价 $%.2f / 小计 $%.2f  │  %dg / %dml",
                           name.c_str(), qty, unit_val, total_val,
                           weight_g, vol_ml );
    }
    if( shown == 0 ) {
        ImGui::TextDisabled( "没有符合条件的可交易物品。" );
    }
}

} // namespace

namespace ui2
{
namespace trade
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
    g_tab = list_tab::theirs;
    g_filter[0] = '\0';
}

void set_tab( list_tab tab )
{
    g_tab = tab;
}

list_tab current_tab()
{
    return g_tab;
}

auto collect_nearby_tradable_npcs( int max_range ) -> std::vector<npc *>
{
    std::vector<npc *> result;
    if( g == nullptr ) {
        return result;
    }
    const avatar &you = get_avatar();
    const tripoint_bub_ms you_pos = you.bub_pos();
    for( npc &guy : g->all_npcs() ) {
        if( guy.is_dead() || guy.is_hallucination() || guy.guaranteed_hostile() ) {
            continue;
        }
        if( rl_dist( you_pos, guy.bub_pos() ) <= max_range ) {
            result.push_back( &guy );
        }
    }
    std::sort( result.begin(), result.end(), [&]( const npc * a, const npc * b ) {
        return rl_dist( you_pos, a->bub_pos() ) < rl_dist( you_pos, b->bub_pos() );
    } );
    return result;
}

auto build_preview( npc &target, int cost ) -> npc_trading::trade_state
{
    npc_trading::trade_state state;
    if( g == nullptr ) {
        return state;
    }
    npc_trading::setup_trade_state( state, cost, target );
    return state;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    npc *pending_legacy_trade = nullptr;

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 132.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 920.0f, 540.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "交易预览 (只读)###ui2_trade_window", &g_open ) ) {
        ImGui::End();
        return;
    }

    if( g == nullptr ) {
        ImGui::TextDisabled( "世界尚未载入。" );
        ImGui::End();
        return;
    }

    const std::vector<npc *> nearby = collect_nearby_tradable_npcs();
    if( std::find( nearby.begin(), nearby.end(), g_selected ) == nearby.end() ) {
        g_selected = nearby.empty() ? nullptr : nearby.front();
    }

    ImGui::SetNextItemWidth( 240.0f );
    ImGui::InputTextWithHint( "##trade_item_filter", "过滤物品名称…",
                              g_filter, IM_ARRAYSIZE( g_filter ) );
    if( g_filter[0] != '\0' ) {
        ImGui::SameLine();
        if( ImGui::Button( "清空" ) ) {
            g_filter[0] = '\0';
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "│ 周边可交易 NPC：%d 人（10 格内）", static_cast<int>( nearby.size() ) );

    ImGui::Separator();

    const avatar &you = get_avatar();
    const tripoint_bub_ms you_pos = you.bub_pos();

    ImGui::BeginChild( "##trade_npc_list", ImVec2( 250.0f, 0.0f ), true );
    if( nearby.empty() ) {
        ImGui::TextDisabled( "附近 10 格内没有可交易的 NPC。" );
    } else {
        for( size_t i = 0; i < nearby.size(); ++i ) {
            npc *guy = nearby[i];
            const int dist = rl_dist( you_pos, guy->bub_pos() );
            const char *tag = guy->will_exchange_items_freely() ? "自由" :
                              ( guy->is_shopkeeper() ? "店主" : "普通" );
            char label[256];
            std::snprintf( label, sizeof( label ), "%s [%s · %d格]##npc_%zu",
                           guy->get_name().c_str(), tag, dist, i );
            if( ImGui::Selectable( label, g_selected == guy ) ) {
                g_selected = guy;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild( "##trade_detail", ImVec2( 0.0f, 0.0f ), true );
    if( g_selected == nullptr ) {
        ImGui::TextDisabled( "选择左侧 NPC 查看只读交易估值预览。" );
    } else {
        npc &target = *g_selected;
        const npc_trading::trade_state preview = build_preview( target );
        const double adj = npc_trading::net_price_adjustment( you, target );
        const double owed = static_cast<double>( target.op_of_u.owed ) / 100.0;
        const double max_credit = static_cast<double>( target.max_credit_extended() ) / 100.0;

        ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "%s",
                            target.get_name().c_str() );
        ImGui::SameLine();
        if( ImGui::Button( "打开完整交易界面…" ) ) {
            pending_legacy_trade = &target;
        }
        ImGui::Text( "议价倍率：%.2fx  │  当前欠款：$%.2f  │  可赊账上限：$%.2f  │  %s",
                     adj, owed, max_credit,
                     target.will_exchange_items_freely() ? "友方自由交换" : "常规以物易物" );

        ImGui::Separator();

        if( ImGui::BeginTabBar( "##trade_preview_tabs" ) ) {
            char theirs_title[64];
            std::snprintf( theirs_title, sizeof( theirs_title ),
                           "对方可售 (%zu)###tab_theirs", preview.theirs.size() );
            if( ImGui::BeginTabItem( theirs_title ) ) {
                g_tab = list_tab::theirs;
                draw_pricing_list( preview.theirs );
                ImGui::EndTabItem();
            }

            char yours_title[64];
            std::snprintf( yours_title, sizeof( yours_title ),
                           "我方可售 (%zu)###tab_yours", preview.yours.size() );
            if( ImGui::BeginTabItem( yours_title ) ) {
                g_tab = list_tab::yours;
                draw_pricing_list( preview.yours );
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    if( pending_legacy_trade != nullptr ) {
        npc_trading::trade( *pending_legacy_trade, 0, "" );
    }
}

} // namespace trade
} // namespace ui2

#endif // TILES

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
npc *g_cached_npc = nullptr;
npc_trading::trade_state g_state;
bool g_state_valid = false;
std::string g_status_msg;
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

int max_qty_for( const item_pricing &ip )
{
    return ip.charges > 0 ? ip.charges : std::max( ip.count, 1 );
}

int selected_qty_for( const item_pricing &ip, bool is_theirs )
{
    if( ip.charges > 0 ) {
        return is_theirs ? ip.u_charges : ip.npc_charges;
    }
    return is_theirs ? ip.u_has : ip.npc_has;
}

void refresh_cached_state( npc &target )
{
    g_state = ui2::trade::build_preview( target );
    g_cached_npc = &target;
    g_state_valid = true;
}

void draw_pricing_list( npc_trading::trade_state &state, const npc &target, bool is_theirs )
{
    std::vector<item_pricing> &items = is_theirs ? state.theirs : state.yours;
    int shown = 0;
    for( size_t i = 0; i < items.size(); ++i ) {
        item_pricing &ip = items[i];
        if( ip.locs.empty() || ip.locs.front() == nullptr ) {
            continue;
        }
        const item *it = ip.locs.front();
        const std::string name = it->display_name();
        if( !contains_ci( name, g_filter ) ) {
            continue;
        }
        ++shown;
        const int max_qty = max_qty_for( ip );
        const int cur_qty = selected_qty_for( ip, is_theirs );
        bool checked = cur_qty > 0;

        ImGui::PushID( static_cast<int>( i ) );
        if( ImGui::Checkbox( "##sel", &checked ) ) {
            ui2::trade::set_item_selection( state, target, is_theirs, i, checked ? max_qty : 0 );
        }
        if( max_qty > 1 ) {
            ImGui::SameLine();
            ImGui::BeginDisabled( cur_qty <= 0 );
            if( ImGui::SmallButton( "-" ) ) {
                ui2::trade::set_item_selection( state, target, is_theirs, i, cur_qty - 1 );
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text( "%d/%d", cur_qty, max_qty );
            ImGui::SameLine();
            ImGui::BeginDisabled( cur_qty >= max_qty );
            if( ImGui::SmallButton( "+" ) ) {
                ui2::trade::set_item_selection( state, target, is_theirs, i, cur_qty + 1 );
            }
            ImGui::EndDisabled();
        }
        ImGui::SameLine();
        const double unit_val = static_cast<double>( ip.price ) / 100.0;
        const double total_val = unit_val * max_qty;
        const int weight_g = units::to_gram( ip.weight );
        const int vol_ml = units::to_milliliter( ip.vol );
        ImGui::Text( "%s x%d  │  单价 $%.2f / 小计 $%.2f  │  %dg / %dml",
                     name.c_str(), max_qty, unit_val, total_val,
                     weight_g, vol_ml );
        ImGui::PopID();
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
    g_cached_npc = nullptr;
    g_state = npc_trading::trade_state{};
    g_state_valid = false;
    g_status_msg.clear();
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
    state.volume_left = target.volume_capacity() - target.volume_carried();
    state.weight_left = target.weight_capacity() - target.weight_carried();
    return state;
}

void recalculate_balance_and_capacity( npc_trading::trade_state &state, const npc &target,
                                       int cost )
{
    state.your_balance = target.will_exchange_items_freely() ? 0 : ( target.op_of_u.owed - cost );
    state.volume_left = target.volume_capacity() - target.volume_carried();
    state.weight_left = target.weight_capacity() - target.weight_carried();

    const bool free_trade = target.will_exchange_items_freely();
    for( const item_pricing &ip : state.theirs ) {
        const int qty = selected_qty_for( ip, true );
        if( qty <= 0 ) {
            continue;
        }
        if( !free_trade ) {
            state.your_balance -= static_cast<int>( ip.price * qty );
        }
        state.volume_left += ip.vol * qty;
        state.weight_left += ip.weight * qty;
    }
    for( const item_pricing &ip : state.yours ) {
        const int qty = selected_qty_for( ip, false );
        if( qty <= 0 ) {
            continue;
        }
        if( !free_trade ) {
            state.your_balance += static_cast<int>( ip.price * qty );
        }
        state.volume_left -= ip.vol * qty;
        state.weight_left -= ip.weight * qty;
    }
}

bool set_item_selection( npc_trading::trade_state &state, const npc &target, bool is_theirs,
                         size_t index, int qty )
{
    std::vector<item_pricing> &items = is_theirs ? state.theirs : state.yours;
    if( index >= items.size() ) {
        return false;
    }
    item_pricing &ip = items[index];
    const int max_qty = max_qty_for( ip );
    const int clamped = std::clamp( qty, 0, max_qty );
    if( ip.charges > 0 ) {
        if( is_theirs ) {
            ip.u_charges = clamped;
            ip.u_has = 0;
        } else {
            ip.npc_charges = clamped;
            ip.npc_has = 0;
        }
    } else {
        if( is_theirs ) {
            ip.u_has = clamped;
            ip.u_charges = 0;
        } else {
            ip.npc_has = clamped;
            ip.npc_charges = 0;
        }
    }
    ip.selected = clamped > 0;
    recalculate_balance_and_capacity( state, target );
    return true;
}

bool has_selected_items( const npc_trading::trade_state &state )
{
    const auto any_sel = []( const std::vector<item_pricing> &list ) {
        return std::any_of( list.begin(), list.end(), []( const item_pricing & ip ) {
            return ip.selected && ( ip.u_has + ip.npc_has + ip.u_charges + ip.npc_charges ) > 0;
        } );
    };
    return any_sel( state.theirs ) || any_sel( state.yours );
}

bool can_commit( const npc_trading::trade_state &state, const npc &target )
{
    if( !has_selected_items( state ) ) {
        return false;
    }
    if( !npc_trading::npc_will_accept_trade( state, target ) ) {
        return false;
    }
    if( !target.will_exchange_items_freely() &&
        ( state.volume_left < 0_ml || state.weight_left < 0_gram ) ) {
        return false;
    }
    return true;
}

bool commit_trade( npc_trading::trade_state &state, npc &target )
{
    if( g == nullptr || !can_commit( state, target ) ) {
        return false;
    }
    avatar &you = get_avatar();
    npc_trading::transfer_items( state.yours, you, target, false );
    npc_trading::transfer_items( state.theirs, target, you, true );
    if( !target.will_exchange_items_freely() ) {
        npc_trading::update_npc_owed( state, target );
    }
    return true;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    npc *pending_legacy_trade = nullptr;

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 132.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 940.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "交易 (实时交互)###ui2_trade_window", &g_open ) ) {
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
        g_state_valid = false;
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

    ImGui::BeginChild( "##trade_npc_list", ImVec2( 240.0f, 0.0f ), true );
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
                g_state_valid = false;
                g_status_msg.clear();
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild( "##trade_detail", ImVec2( 0.0f, 0.0f ), true );
    if( g_selected == nullptr ) {
        ImGui::TextDisabled( "选择左侧 NPC 进行实时交易。" );
    } else {
        npc &target = *g_selected;
        if( !g_state_valid || g_cached_npc != &target ) {
            refresh_cached_state( target );
        }

        const double adj = npc_trading::net_price_adjustment( you, target );
        const double balance = static_cast<double>( g_state.your_balance ) / 100.0;
        const double max_credit = static_cast<double>( target.max_credit_extended() ) / 100.0;
        const bool acceptable = can_commit( g_state, target );

        ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "%s",
                            target.get_name().c_str() );
        ImGui::SameLine();
        ImGui::BeginDisabled( !acceptable );
        if( ImGui::Button( "确认交易" ) ) {
            if( commit_trade( g_state, target ) ) {
                refresh_cached_state( target );
                g_status_msg = "交易已成交并实时刷新清单。";
            } else {
                g_status_msg = "当前条件未满足，无法成交。";
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if( ImGui::Button( "重置选择" ) ) {
            refresh_cached_state( target );
            g_status_msg = "已清空当前选择。";
        }
        ImGui::SameLine();
        if( ImGui::Button( "打开完整交易界面…" ) ) {
            pending_legacy_trade = &target;
        }

        ImGui::Text( "议价倍率：%.2fx  │  结算差额：$%.2f  │  可赊账上限：$%.2f  │  %s",
                     adj, balance, max_credit,
                     target.will_exchange_items_freely() ? "友方自由交换" :
                     ( npc_trading::npc_will_accept_trade( g_state, target ) ? "NPC 可接受" : "差额不足" ) );
        if( !g_status_msg.empty() ) {
            ImGui::TextColored( ImVec4( 0.45f, 0.92f, 0.55f, 1.0f ), "%s",
                                g_status_msg.c_str() );
        }

        ImGui::Separator();

        if( ImGui::BeginTabBar( "##trade_preview_tabs" ) ) {
            char theirs_title[64];
            std::snprintf( theirs_title, sizeof( theirs_title ),
                           "对方可售 (%zu)###tab_theirs", g_state.theirs.size() );
            if( ImGui::BeginTabItem( theirs_title ) ) {
                g_tab = list_tab::theirs;
                draw_pricing_list( g_state, target, true );
                ImGui::EndTabItem();
            }

            char yours_title[64];
            std::snprintf( yours_title, sizeof( yours_title ),
                           "我方可售 (%zu)###tab_yours", g_state.yours.size() );
            if( ImGui::BeginTabItem( yours_title ) ) {
                g_tab = list_tab::yours;
                draw_pricing_list( g_state, target, false );
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::EndChild();

    ImGui::End();

    if( pending_legacy_trade != nullptr ) {
        npc_trading::trade( *pending_legacy_trade, 0, "" );
        g_state_valid = false;
    }
}

} // namespace trade
} // namespace ui2

#endif // TILES

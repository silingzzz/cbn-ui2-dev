#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_TRADE_H
#define CATA_SRC_CATA_IMGUI_UI2_TRADE_H

#if defined( TILES )

#include <cstddef>
#include <vector>

#include "npctrade.h"

class npc;

namespace ui2
{
namespace trade
{

enum class list_tab : int {
    theirs = 0,
    yours = 1,
};

bool is_open();
void toggle();
void set_open( bool open );
void reset_session();
void draw();
bool take_deferred_action();

// Read-only query & interactive trade helpers exposed for UI and Catch2 tests.
auto collect_nearby_tradable_npcs( int max_range = 10 ) -> std::vector<npc *>;
auto build_preview( npc &target, int cost = 0 ) -> npc_trading::trade_state;
void recalculate_balance_and_capacity( npc_trading::trade_state &state, const npc &target,
                                       int cost = 0 );
bool set_item_selection( npc_trading::trade_state &state, const npc &target, bool is_theirs,
                         size_t index, int qty );
bool has_selected_items( const npc_trading::trade_state &state );
bool can_commit( const npc_trading::trade_state &state, const npc &target );
bool commit_trade( npc_trading::trade_state &state, npc &target );
void set_tab( list_tab tab );
list_tab current_tab();

} // namespace trade
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_TRADE_H

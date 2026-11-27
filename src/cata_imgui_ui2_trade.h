#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_TRADE_H
#define CATA_SRC_CATA_IMGUI_UI2_TRADE_H

#if defined( TILES )

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

// Read-only query helpers exposed for UI and Catch2 tests.
auto collect_nearby_tradable_npcs( int max_range = 10 ) -> std::vector<npc *>;
auto build_preview( npc &target, int cost = 0 ) -> npc_trading::trade_state;
void set_tab( list_tab tab );
list_tab current_tab();

} // namespace trade
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_TRADE_H

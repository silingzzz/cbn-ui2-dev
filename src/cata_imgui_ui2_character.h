#pragma once
#ifndef CATA_SRC_CATA_IMGUI_UI2_CHARACTER_H
#define CATA_SRC_CATA_IMGUI_UI2_CHARACTER_H

#if defined( TILES )

#include "type_id.h"

namespace ui2
{
namespace character
{

enum class tab_kind : int {
    stats = 0,
    skills = 1,
    traits = 2,
    bionics = 3,
    effects = 4,
};

struct stats_snapshot {
    int str = 0;
    int str_base = 0;
    int dex = 0;
    int dex_base = 0;
    int intel = 0;
    int intel_base = 0;
    int per = 0;
    int per_base = 0;
    int speed = 0;
    int pain = 0;
    int stamina = 0;
    int stamina_max = 0;
    int power_kj = 0;
    int max_power_kj = 0;
};

bool is_open();
void toggle();
void set_open( bool open );
void reset_session();
void draw();

// Read-only query & interactive helpers exposed for UI and Catch2 tests.
auto read_stats_snapshot() -> stats_snapshot;
bool is_skill_training( const skill_id &id );
bool toggle_skill_training( const skill_id &id );
void set_tab( tab_kind tab );
tab_kind current_tab();

} // namespace character
} // namespace ui2

#endif // TILES

#endif // CATA_SRC_CATA_IMGUI_UI2_CHARACTER_H

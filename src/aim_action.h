#pragma once
#ifndef CATA_SRC_AIM_ACTION_H
#define CATA_SRC_AIM_ACTION_H

#include <array>

#include "coordinates.h"
#include "type_id.h"

class Character;

/// PR-2A AIM-2: one-item move command.
///
/// This layer only builds the existing game activity request. It deliberately
/// performs no capacity, weight, ownership or placement checks: `ACT_MOVE_ITEMS`
/// decides all of that when the activity runs, so the UI cannot fork the rules.
enum class aim_move_result {
    started,
    /// The stack slot no longer exists, is empty, or now holds another type.
    stale_selection,
};

struct aim_move_target {
    tripoint_rel_ms offset;
    const char *label;
};

/// The nine map tiles under and around the player, in screen order.
auto aim_move_targets() -> const std::array<aim_move_target, 9> &;

/// Move exactly one item (or one charge of a charge-counted stack) out of the
/// player's backpack to `dest`, relative to the player.
///
/// Returns `started` after the activity has been assigned. The item is still in
/// the backpack until the game processes the activity.
auto aim_move_one( Character &who, int stack_index, const itype_id &type,
                   tripoint_rel_ms dest ) -> aim_move_result;

#endif // CATA_SRC_AIM_ACTION_H

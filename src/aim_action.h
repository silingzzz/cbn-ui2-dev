#pragma once
#ifndef CATA_SRC_AIM_ACTION_H
#define CATA_SRC_AIM_ACTION_H

#include <array>
#include <vector>

#include "coordinates.h"
#include "type_id.h"

class Character;

/// PR-2A AIM-2: one-to-N item move command.
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

struct aim_move_request {
    int stack_index = -1;
    itype_id type;
    int amount = 0;
};

/// The nine map tiles under and around the player, in screen order.
auto aim_move_targets() -> const std::array<aim_move_target, 9> &;

/// Move a batch of stacks out of the player's backpack to `dest`, relative to
/// the player. Each request is validated against the current inventory before
/// any pointer is put into the activity, so a stale batch is rejected as a
/// whole instead of half-starting.
///
/// Returns `started` after the activity has been assigned. The items are still
/// in the backpack until the game processes the activity.
auto aim_move_items( Character &who, const std::vector<aim_move_request> &requests,
                     tripoint_rel_ms dest ) -> aim_move_result;

/// Single-stack wrapper used by drag/drop and the row context menu.
auto aim_move_items( Character &who, int stack_index, const itype_id &type,
                     int amount, tripoint_rel_ms dest ) -> aim_move_result;

/// Convenience wrapper for `aim_move_items( ..., 1, ... )`.
auto aim_move_one( Character &who, int stack_index, const itype_id &type,
                   tripoint_rel_ms dest ) -> aim_move_result;

/// Ground-to-backpack pickup request.  The index/type pair is validated against
/// the map stack at execution time, mirroring the inventory requests above.
struct aim_ground_request {
    int stack_index = -1;
    itype_id type;
    int amount = 0;
};

enum class aim_pickup_result {
    started,
    stale_selection,
};

/// Assign `ACT_PICKUP` for items lying on `ground`.  Volume, weight, ownership
/// and thief prompts stay inside `pickup_activity_actor`; the UI only selects.
auto aim_pickup_items( Character &who, const tripoint_bub_ms &ground,
                       const std::vector<aim_ground_request> &requests ) -> aim_pickup_result;

#endif // CATA_SRC_AIM_ACTION_H

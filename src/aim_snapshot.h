#pragma once
#ifndef CATA_SRC_AIM_SNAPSHOT_H
#define CATA_SRC_AIM_SNAPSHOT_H

#include <cstdint>
#include <string>
#include <vector>

#include "type_id.h"
#include "units.h"

class Character;

/// PR-2A AIM-1: read-only snapshot of the player's backpack.
///
/// This layer deliberately holds NO item pointers: the game reallocates item
/// containers freely, so a snapshot only keeps lightweight display fields. The
/// UI rebuilds it every frame and resolves item details from the live inventory
/// inside that same frame.
struct aim_row {
    std::string name;          ///< item::tname()
    std::string category_id;   ///< item::get_category_id()
    std::string category_name; ///< item::get_category().name()
    itype_id type_id;          ///< stable to the *type* level, not per instance
    int stack_index = 0;       ///< position of the stack inside Character::inv
    int count = 0;             ///< items in the stack, or charges for charge items
    bool by_charges = false;
    units::mass weight = 0_gram;   ///< summed over the whole stack
    units::volume volume = 0_ml;   ///< summed over the whole stack
};

struct aim_snapshot {
    std::vector<aim_row> rows;
    units::mass total_weight = 0_gram;
    units::volume total_volume = 0_ml;
    /// Incremented on every rebuild; lets the UI notice that its cached
    /// selection may no longer refer to the same thing.
    std::uint64_t build_id = 0;
};

/// Build the snapshot from the real backpack only (Character::inv).
/// Worn, wielded, ground and vehicle items are intentionally out of scope.
aim_snapshot build_aim_snapshot( Character &who );

/// Case-insensitive substring match on the display name. UTF-8 bytes that are
/// not ASCII are passed through untouched, so CJK still matches exactly.
auto aim_row_matches( const aim_row &row, const std::string &needle ) -> bool;

/// Indices of rows to display, grouped by category then sorted by name.
/// Empty category_id means "all categories".
auto aim_visible_rows( const aim_snapshot &snap, const std::string &category_id,
                       const std::string &needle ) -> std::vector<std::size_t>;

/// Category ids present in the snapshot, in first-seen order.
auto aim_categories( const aim_snapshot &snap ) -> std::vector<std::string>;

#endif // CATA_SRC_AIM_SNAPSHOT_H

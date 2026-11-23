#include "aim_action.h"

#include <memory>
#include <vector>

#include "activity_actor_definitions.h"
#include "character.h"
#include "inventory.h"
#include "item.h"
#include "player_activity.h"

namespace
{

const std::array<aim_move_target, 9> g_targets = { {
        { tripoint_rel_ms::zero(), "脚下" },
        { tripoint_rel_ms::north(), "北" },
        { tripoint_rel_ms::north_east(), "东北" },
        { tripoint_rel_ms::east(), "东" },
        { tripoint_rel_ms::south_east(), "东南" },
        { tripoint_rel_ms::south(), "南" },
        { tripoint_rel_ms::south_west(), "西南" },
        { tripoint_rel_ms::west(), "西" },
        { tripoint_rel_ms::north_west(), "西北" },
    }
};

} // namespace

auto aim_move_targets() -> const std::array<aim_move_target, 9> &
{
    return g_targets;
}

auto aim_move_one( Character &who, int stack_index, const itype_id &type,
                   tripoint_rel_ms dest ) -> aim_move_result
{
    location_inventory &inv = who.get_inv();
    if( stack_index < 0 || stack_index >= static_cast<int>( inv.size() ) ) {
        return aim_move_result::stale_selection;
    }

    const std::vector<item *> &stack = inv.const_stack( stack_index );
    if( stack.empty() || stack.front() == nullptr || stack.front()->typeId() != type ) {
        return aim_move_result::stale_selection;
    }

    const item &first = *stack.front();
    if( first.count_by_charges() && first.charges <= 0 ) {
        return aim_move_result::stale_selection;
    }

    // Charges move one charge; ordinary items move the whole first instance.
    // The actor's zero quantity means "the entire item", which for a stack of
    // non-charge items is still exactly one item instance.
    std::vector<item *> targets = { stack.front() };
    std::vector<int> quantities = { first.count_by_charges() ? 1 : 0 };

    who.assign_activity( std::make_unique<player_activity>(
                             std::make_unique<move_items_activity_actor>(
                                 targets, quantities, false, dest ) ) );
    return aim_move_result::started;
}

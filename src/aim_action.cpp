#include "aim_action.h"

#include <algorithm>
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

auto aim_move_items( Character &who, const std::vector<aim_move_request> &requests,
                     tripoint_rel_ms dest ) -> aim_move_result
{
    if( requests.empty() ) {
        return aim_move_result::stale_selection;
    }

    location_inventory &inv = who.get_inv();
    std::vector<int> used_stacks;
    used_stacks.reserve( requests.size() );

    for( const aim_move_request &request : requests ) {
        if( request.amount <= 0 || request.stack_index < 0 ||
            request.stack_index >= static_cast<int>( inv.size() ) ) {
            return aim_move_result::stale_selection;
        }
        if( std::find( used_stacks.begin(), used_stacks.end(), request.stack_index ) !=
            used_stacks.end() ) {
            return aim_move_result::stale_selection;
        }

        const std::vector<item *> &stack = inv.const_stack( request.stack_index );
        if( stack.empty() || stack.front() == nullptr || stack.front()->typeId() != request.type ) {
            return aim_move_result::stale_selection;
        }

        const item &first = *stack.front();
        const int available = first.count_by_charges() ? first.charges :
                              static_cast<int>( stack.size() );
        if( available < request.amount ) {
            return aim_move_result::stale_selection;
        }
        used_stacks.push_back( request.stack_index );
    }

    std::vector<item *> targets;
    std::vector<int> quantities;
    for( const aim_move_request &request : requests ) {
        const std::vector<item *> &stack = inv.const_stack( request.stack_index );
        const item &first = *stack.front();
        if( first.count_by_charges() ) {
            // One request carrying the charge count; the actor splits it when it runs.
            targets.push_back( stack.front() );
            quantities.push_back( request.amount );
        } else {
            // One instance per request; the actor's zero quantity means "the whole item".
            for( int i = 0; i < request.amount; ++i ) {
                if( stack[static_cast<std::size_t>( i )] == nullptr ) {
                    return aim_move_result::stale_selection;
                }
                targets.push_back( stack[static_cast<std::size_t>( i )] );
                quantities.push_back( 0 );
            }
        }
    }

    who.assign_activity( std::make_unique<player_activity>(
                             std::make_unique<move_items_activity_actor>(
                                 targets, quantities, false, dest ) ) );
    return aim_move_result::started;
}

auto aim_move_items( Character &who, int stack_index, const itype_id &type,
                     int amount, tripoint_rel_ms dest ) -> aim_move_result
{
    aim_move_request request;
    request.stack_index = stack_index;
    request.type = type;
    request.amount = amount;
    return aim_move_items( who, std::vector<aim_move_request>{ request }, dest );
}

auto aim_move_one( Character &who, int stack_index, const itype_id &type,
                   tripoint_rel_ms dest ) -> aim_move_result
{
    return aim_move_items( who, stack_index, type, 1, dest );
}

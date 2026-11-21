#include <string>

#include "aim_snapshot.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "inventory.h"
#include "item.h"
#include "item_category.h"
#include "player_helpers.h"

// PR-2A AIM-1: unit tests for the read-only backpack snapshot layer.
// These cover the pure data/adaptation logic; anything that needs a live game
// world (real rendering, pick up / drop refresh) moves to save-game acceptance.

namespace
{

/// Adds `count` separate light items so stacking behaviour can be observed.
void add_items( avatar &who, const std::string &type, int count )
{
    for( int i = 0; i < count; ++i ) {
        who.i_add( item::spawn( type ) );
    }
}

auto find_row( const aim_snapshot &snap, const std::string &type_id ) -> const aim_row *
{
    for( const aim_row &row : snap.rows ) {
        if( row.type_id.str() == type_id ) {
            return &row;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE( "aim snapshot of an empty backpack", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();

    const aim_snapshot snap = build_aim_snapshot( dummy );
    CHECK( snap.rows.empty() );
    CHECK( snap.total_weight == 0_gram );
    CHECK( snap.total_volume == 0_ml );
    CHECK( snap.build_id > 0 );
}

TEST_CASE( "aim snapshot of a single item", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "bandages", 1 );

    const aim_snapshot snap = build_aim_snapshot( dummy );
    REQUIRE( snap.rows.size() == 1 );
    const aim_row &row = snap.rows.front();
    CHECK( row.type_id.str() == "bandages" );
    CHECK( row.by_charges == dummy.get_inv().const_stack( 0 ).front()->count_by_charges() );
    // Compare against the live item rather than assuming a non-empty category.
    const item &live = *dummy.get_inv().const_stack( 0 ).front();
    CHECK( row.name == live.tname() );
    CHECK( row.category_id == live.get_category_id() );
    CHECK( row.category_name == live.get_category().name() );
    CHECK( !row.name.empty() );
}

TEST_CASE( "aim snapshot groups multiple categories", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "bandages", 1 );
    add_items( dummy, "hammer", 1 );
    add_items( dummy, "rag", 1 );

    const aim_snapshot snap = build_aim_snapshot( dummy );
    CHECK( snap.rows.size() == 3 );

    const std::vector<std::string> categories = aim_categories( snap );
    CHECK( categories.size() >= 2 );
    // Every row's category must match the live item it was built from.
    const location_inventory &inv = dummy.get_inv();
    for( const aim_row &row : snap.rows ) {
        REQUIRE( row.stack_index >= 0 );
        REQUIRE( row.stack_index < static_cast<int>( inv.size() ) );
        const item &live = *inv.const_stack( row.stack_index ).front();
        CHECK( row.category_id == live.get_category_id() );
    }
}

TEST_CASE( "aim snapshot records stacked item counts", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "hammer", 3 );

    const aim_snapshot snap = build_aim_snapshot( dummy );
    const aim_row *row = find_row( snap, "hammer" );
    REQUIRE( row != nullptr );
    // Either the game merged them into one stack, or three separate stacks; the
    // total count must be three in both cases.
    int total = 0;
    for( const aim_row &r : snap.rows ) {
        if( r.type_id.str() == "hammer" ) {
            total += r.count;
        }
    }
    CHECK( total == 3 );
}

TEST_CASE( "aim snapshot weight and volume match the items", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "bandages", 1 );
    add_items( dummy, "hammer", 1 );

    const aim_snapshot snap = build_aim_snapshot( dummy );
    units::mass live_weight = 0_gram;
    units::volume live_volume = 0_ml;
    const location_inventory &inv = dummy.get_inv();
    for( int i = 0; i < static_cast<int>( inv.size() ); ++i ) {
        for( const item *it : inv.const_stack( i ) ) {
            live_weight += it->weight();
            live_volume += it->volume();
        }
    }
    CHECK( snap.total_weight == live_weight );
    CHECK( snap.total_volume == live_volume );
}

TEST_CASE( "aim search and category filters", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "bandages", 1 );
    add_items( dummy, "hammer", 1 );

    const aim_snapshot snap = build_aim_snapshot( dummy );
    REQUIRE( snap.rows.size() == 2 );

    // No filter: everything is visible.
    CHECK( aim_visible_rows( snap, "", "" ).size() == 2 );

    // Search is a case-insensitive substring match on the display name.
    const aim_row &first = snap.rows.front();
    CHECK( aim_row_matches( first, "" ) );
    CHECK( aim_row_matches( first, first.name ) );
    CHECK_FALSE( aim_row_matches( first, "zzz-no-such-item" ) );
    CHECK( aim_visible_rows( snap, "", "zzz-no-such-item" ).empty() );

    // Category filter keeps only rows of that category.
    const std::vector<std::size_t> only_category = aim_visible_rows( snap, first.category_id, "" );
    REQUIRE( !only_category.empty() );
    for( const std::size_t idx : only_category ) {
        CHECK( snap.rows[idx].category_id == first.category_id );
    }
}

TEST_CASE( "aim snapshot rebuild notices inventory changes", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "bandages", 1 );

    const aim_snapshot before = build_aim_snapshot( dummy );
    REQUIRE( before.rows.size() == 1 );

    add_items( dummy, "hammer", 1 );
    const aim_snapshot after = build_aim_snapshot( dummy );
    CHECK( after.rows.size() == 2 );
    CHECK( after.build_id > before.build_id );
}

TEST_CASE( "aim selection recovers safely when the item disappears", "[aim][snapshot]" )
{
    clear_avatar();
    avatar &dummy = get_avatar();
    add_items( dummy, "bandages", 1 );
    add_items( dummy, "hammer", 1 );

    const aim_snapshot before = build_aim_snapshot( dummy );
    REQUIRE( before.rows.size() == 2 );

    // Remember the identity a UI would keep: (category, type, stack slot).
    const aim_row &target = before.rows.front();
    const std::string key = target.category_id + "|" + target.type_id.str();

    // Remove everything; the remembered identity must no longer resolve.
    dummy.inv_dump_remove();
    const aim_snapshot after = build_aim_snapshot( dummy );

    bool found = false;
    for( const aim_row &row : after.rows ) {
        if( ( row.category_id + "|" + row.type_id.str() ) == key ) {
            found = true;
            break;
        }
    }
    CHECK_FALSE( found );
    // The UI contract: when the remembered row is gone, selection is dropped.
    CHECK( after.rows.empty() );
}

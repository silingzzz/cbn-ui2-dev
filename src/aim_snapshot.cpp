#include "aim_snapshot.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "character.h"
#include "inventory.h"
#include "item.h"
#include "item_category.h"

namespace
{

std::uint64_t g_build_counter = 0;

auto lower_ascii( const std::string &text ) -> std::string
{
    std::string out = text;
    for( char &c : out ) {
        const unsigned char uc = static_cast<unsigned char>( c );
        if( uc < 128 ) {
            c = static_cast<char>( std::tolower( uc ) );
        }
    }
    return out;
}

} // namespace

aim_snapshot build_aim_snapshot( Character &who )
{
    aim_snapshot snap;
    snap.build_id = ++g_build_counter;

    const location_inventory &inv = who.get_inv();
    const int stacks = static_cast<int>( inv.size() );
    snap.rows.reserve( static_cast<std::size_t>( stacks ) );

    for( int i = 0; i < stacks; ++i ) {
        const std::vector<item *> &stack = inv.const_stack( i );
        if( stack.empty() || stack.front() == nullptr ) {
            continue;
        }
        const item &first = *stack.front();

        aim_row row;
        row.name = first.tname();
        row.category_id = first.get_category_id();
        row.category_name = first.get_category().name();
        row.type_id = first.typeId();
        row.stack_index = i;
        row.by_charges = first.count_by_charges();

        units::mass weight = 0_gram;
        units::volume volume = 0_ml;
        for( const item *it : stack ) {
            if( it == nullptr ) {
                continue;
            }
            weight += it->weight();
            volume += it->volume();
        }
        row.weight = weight;
        row.volume = volume;
        row.count = row.by_charges ? first.charges : static_cast<int>( stack.size() );

        if( row.count <= 0 ) {
            continue;
        }

        snap.total_weight += weight;
        snap.total_volume += volume;
        snap.rows.push_back( row );
    }

    return snap;
}

auto aim_row_matches( const aim_row &row, const std::string &needle ) -> bool
{
    if( needle.empty() ) {
        return true;
    }
    return lower_ascii( row.name ).find( lower_ascii( needle ) ) != std::string::npos;
}

auto aim_visible_rows( const aim_snapshot &snap, const std::string &category_id,
                       const std::string &needle ) -> std::vector<std::size_t>
{
    std::vector<std::size_t> out;
    for( std::size_t i = 0; i < snap.rows.size(); ++i ) {
        const aim_row &row = snap.rows[i];
        if( !category_id.empty() && row.category_id != category_id ) {
            continue;
        }
        if( !aim_row_matches( row, needle ) ) {
            continue;
        }
        out.push_back( i );
    }
    std::stable_sort( out.begin(), out.end(), [&snap]( std::size_t a, std::size_t b ) {
        const aim_row &ra = snap.rows[a];
        const aim_row &rb = snap.rows[b];
        if( ra.category_name != rb.category_name ) {
            return ra.category_name < rb.category_name;
        }
        return ra.name < rb.name;
    } );
    return out;
}

auto aim_categories( const aim_snapshot &snap ) -> std::vector<std::string>
{
    std::vector<std::string> out;
    for( const aim_row &row : snap.rows ) {
        if( std::find( out.begin(), out.end(), row.category_id ) == out.end() ) {
            out.push_back( row.category_id );
        }
    }
    return out;
}

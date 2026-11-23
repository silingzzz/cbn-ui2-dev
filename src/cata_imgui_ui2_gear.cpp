#include "cata_imgui_ui2_gear.h"

#if defined( TILES )

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "character.h"
#include "item.h"
#include "item_contents.h"
#include "units.h"

namespace
{

bool g_open = false;
item *g_selected = nullptr;

struct gear_row {
    item *ptr = nullptr;
    const char *slot = "";
};

auto build_rows() -> std::vector<gear_row>
{
    std::vector<gear_row> rows;
    Character &you = get_avatar();
    for( item *wielded : you.wielded_items() ) {
        if( wielded != nullptr ) {
            rows.push_back( { wielded, "持握" } );
        }
    }
    for( auto iter = you.worn.begin(); iter != you.worn.end(); ++iter ) {
        item *worn = *iter;
        if( worn != nullptr ) {
            rows.push_back( { worn, "穿戴" } );
        }
    }
    return rows;
}

auto row_of( const std::vector<gear_row> &rows, item *ptr ) -> int
{
    for( std::size_t i = 0; i < rows.size(); ++i ) {
        if( rows[i].ptr == ptr ) {
            return static_cast<int>( i );
        }
    }
    return -1;
}

auto contents_of( item &it ) -> std::vector<item *>
{
    return it.contents.all_items_top();
}

auto plain_name( const item &it ) -> std::string
{
    const std::string raw = it.tname();
    std::string out;
    std::size_t pos = 0;
    while( pos < raw.size() ) {
        const std::size_t open = raw.find( '<', pos );
        if( open == std::string::npos ) {
            out += raw.substr( pos );
            break;
        }
        out += raw.substr( pos, open - pos );
        const std::size_t close = raw.find( '>', open );
        if( close == std::string::npos ) {
            break;
        }
        pos = close + 1;
    }
    return out;
}

void draw_item_tree( item &it )
{
    const std::vector<item *> kids = contents_of( it );
    const ImGuiTreeNodeFlags flags = kids.empty() ? ImGuiTreeNodeFlags_Leaf :
                                     ImGuiTreeNodeFlags_OpenOnArrow;
    if( ImGui::TreeNodeEx( static_cast<void *>( &it ), flags, "%s", plain_name( it ).c_str() ) ) {
        for( item *kid : kids ) {
            if( kid != nullptr ) {
                draw_item_tree( *kid );
            }
        }
        ImGui::TreePop();
    }
}

} // namespace

namespace ui2
{
namespace gear
{

bool is_open()
{
    return g_open;
}

void toggle()
{
    g_open = !g_open;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    const std::vector<gear_row> rows = build_rows();
    if( row_of( rows, g_selected ) < 0 ) {
        g_selected = rows.empty() ? nullptr : rows.front().ptr;
    }

    ImGui::SetNextWindowPos( ImVec2( 980.0f, 60.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 500.0f, 560.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "装备与容器（只读）", &g_open ) ) {
        ImGui::End();
        return;
    }

    const int wielded_count = static_cast<int>( std::count_if( rows.begin(), rows.end(),
    []( const gear_row & row ) {
        return std::string( row.slot ) == "持握";
    } ) );
    const int worn_count = static_cast<int>( rows.size() ) - wielded_count;
    ImGui::TextDisabled( "持握 %d ｜ 穿戴 %d ｜ 只读，不改动装备", wielded_count, worn_count );
    ImGui::Separator();

    const float inner_height = ImGui::GetContentRegionAvail().y;
    if( ImGui::BeginChild( "gear_list", ImVec2( 220.0f, inner_height ), true ) ) {
        if( rows.empty() ) {
            ImGui::TextDisabled( "（没有持握或穿戴物品）" );
        }
        std::string last_slot;
        for( std::size_t i = 0; i < rows.size(); ++i ) {
            const gear_row &row = rows[i];
            if( last_slot != row.slot ) {
                last_slot = row.slot;
                ImGui::Spacing();
                ImGui::TextDisabled( "▸ %s", last_slot.c_str() );
            }
            char label[256];
            std::snprintf( label, sizeof( label ), "%s##gear%zu", plain_name( *row.ptr ).c_str(), i );
            if( ImGui::Selectable( label, g_selected == row.ptr ) ) {
                g_selected = row.ptr;
            }
            ImGui::SameLine();
            const std::size_t kids = contents_of( *row.ptr ).size();
            ImGui::TextDisabled( kids == 0 ? "空" : "%zu 件", kids );
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if( ImGui::BeginChild( "gear_detail", ImVec2( 0.0f, inner_height ), true ) ) {
        if( g_selected == nullptr ) {
            ImGui::Dummy( ImVec2( 0.0f, 40.0f ) );
            ImGui::TextDisabled( "先选中左侧装备。" );
        } else {
            item &it = *g_selected;
            ImGui::TextUnformatted( plain_name( it ).c_str() );
            ImGui::TextDisabled( "重量 %.2f kg ｜ 体积 %.2f L ｜ 内容 %zu 件",
                                 units::to_gram( it.weight() ) / 1000.0,
                                 units::to_milliliter( it.volume() ) / 1000.0,
                                 contents_of( it ).size() );
            ImGui::Separator();
            const std::vector<item *> kids = contents_of( it );
            if( kids.empty() ) {
                ImGui::TextDisabled( "（这个物品不是容器，或容器为空）" );
            } else {
                ImGui::TextDisabled( "容器内容（只读，可逐层展开）" );
                for( item *kid : kids ) {
                    if( kid != nullptr ) {
                        draw_item_tree( *kid );
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

} // namespace gear
} // namespace ui2

#endif // TILES

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
item *g_selected_parent = nullptr;

enum class pending_kind {
    none,
    wield,
    takeoff,
    unwield,
    wear,
    move_child,
    take_child,
};

struct pending_action {
    pending_kind kind = pending_kind::none;
    item *subject = nullptr;
    item *from = nullptr;
    item *dest = nullptr;
};

pending_action g_pending;

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

void draw_item_tree( item &it, item *parent )
{
    const std::vector<item *> kids = contents_of( it );
    const ImGuiTreeNodeFlags flags = kids.empty() ? ImGuiTreeNodeFlags_Leaf :
                                     ImGuiTreeNodeFlags_OpenOnArrow;
    const bool open = ImGui::TreeNodeEx( static_cast<void *>( &it ), flags, "%s",
                                         plain_name( it ).c_str() );
    if( ImGui::IsItemClicked() ) {
        g_selected = &it;
        g_selected_parent = parent;
    }
    if( open ) {
        for( item *kid : kids ) {
            if( kid != nullptr ) {
                draw_item_tree( *kid, &it );
            }
        }
        ImGui::TreePop();
    }
}

void take_off_to_inventory( Character &you, item &it )
{
    if( !you.can_takeoff( it ).success() ) {
        you.add_msg_if_player( m_info, "现在不能卸下 %s。", plain_name( it ) );
        return;
    }
    std::vector<detached_ptr<item>> detached;
    if( you.takeoff( it, &detached ) ) {
        for( detached_ptr<item> &piece : detached ) {
            if( piece ) {
                you.i_add( std::move( piece ) );
            }
        }
    }
}

void move_child( Character &you, item &parent, item &child, item &dest )
{
    if( !dest.is_container() ) {
        you.add_msg_if_player( m_info, "%s 不是容器。", plain_name( dest ) );
        return;
    }
    if( &dest == &parent ) {
        return;
    }
    if( dest.contents.item_size_modifier() + child.volume() > dest.get_total_capacity() ) {
        you.add_msg_if_player( m_info, "%s 装不下 %s。", plain_name( dest ), plain_name( child ) );
        return;
    }
    const std::string child_name = plain_name( child );
    const std::string dest_name = plain_name( dest );
    detached_ptr<item> piece = parent.contents.remove_top( &child );
    if( !piece ) {
        return;
    }
    const ret_val<bool> inserted = dest.contents.insert_item( std::move( piece ) );
    if( !inserted.success() ) {
        you.add_msg_if_player( m_warning, "移动到 %s 失败。", dest_name );
        return;
    }
    you.add_msg_if_player( m_info, "已把 %s 移入 %s。", child_name, dest_name );
}

void take_child_to_inventory( Character &you, item &parent, item &child )
{
    const std::string child_name = plain_name( child );
    detached_ptr<item> piece = parent.contents.remove_top( &child );
    if( !piece ) {
        return;
    }
    you.i_add_or_drop( std::move( piece ) );
    you.add_msg_if_player( m_info, "已取出 %s。", child_name );
}

void apply_pending()
{
    if( g_pending.kind == pending_kind::none || g_pending.subject == nullptr ) {
        g_pending = {};
        return;
    }
    Character &you = get_avatar();
    item &it = *g_pending.subject;
    const std::string name = plain_name( it );
    switch( g_pending.kind ) {
        case pending_kind::wield:
            if( !you.primary_weapon().is_null() ) {
                detached_ptr<item> old = you.remove_primary_weapon();
                if( old ) {
                    you.i_add_or_drop( std::move( old ) );
                }
            }
            if( !you.wield( it ) ) {
                you.add_msg_if_player( m_info, "不能持握 %s。", name );
            }
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        case pending_kind::takeoff:
            take_off_to_inventory( you, it );
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        case pending_kind::unwield: {
            detached_ptr<item> piece = you.remove_primary_weapon();
            if( piece ) {
                you.i_add_or_drop( std::move( piece ) );
            } else {
                you.add_msg_if_player( m_warning, "不能收起 %s。", name );
            }
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        }
        case pending_kind::wear: {
            const auto check = you.can_wear( it );
            if( !check.success() ) {
                you.add_msg_if_player( m_info, "%s", check.c_str() );
            } else {
                detached_ptr<item> piece = you.remove_primary_weapon();
                detached_ptr<item> left = you.wear_item( std::move( piece ), true );
                if( left ) {
                    you.i_add( std::move( left ) );
                    you.add_msg_if_player( m_warning, "穿不上 %s。", name );
                }
            }
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        }
        case pending_kind::move_child:
            if( g_pending.from != nullptr && g_pending.dest != nullptr ) {
                move_child( you, *g_pending.from, it, *g_pending.dest );
                g_selected = g_pending.subject;
                g_selected_parent = g_pending.dest;
            }
            break;
        case pending_kind::take_child:
            if( g_pending.from != nullptr ) {
                take_child_to_inventory( you, *g_pending.from, it );
                g_selected = nullptr;
                g_selected_parent = nullptr;
            }
            break;
        default:
            break;
    }
    g_pending = {};
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
    if( g_selected == nullptr && !rows.empty() ) {
        g_selected = rows.front().ptr;
        g_selected_parent = nullptr;
    }

    ImGui::SetNextWindowPos( ImVec2( 980.0f, 60.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 520.0f, 600.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "装备与容器", &g_open ) ) {
        ImGui::End();
        return;
    }

    const int wielded_count = static_cast<int>( std::count_if( rows.begin(), rows.end(),
    []( const gear_row & row ) {
        return std::string( row.slot ) == "持握";
    } ) );
    const int worn_count = static_cast<int>( rows.size() ) - wielded_count;
    ImGui::TextDisabled( "持握 %d ｜ 穿戴 %d ｜ 点击物品可持握/卸下，容器可转移", wielded_count,
                         worn_count );
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
                g_selected_parent = nullptr;
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
            if( g_selected_parent != nullptr ) {
                ImGui::TextDisabled( "位于：%s", plain_name( *g_selected_parent ).c_str() );
                ImGui::Separator();
                if( ImGui::Button( "取出到背包", ImVec2( 130.0f, 0.0f ) ) ) {
                    g_pending = { pending_kind::take_child, &it, g_selected_parent, nullptr };
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth( 180.0f );
                if( ImGui::BeginCombo( "移入容器", "选择目标容器" ) ) {
                    for( std::size_t i = 0; i < rows.size(); ++i ) {
                        item *dest = rows[i].ptr;
                        if( dest == g_selected_parent || !dest->is_container() ) {
                            continue;
                        }
                        char label[256];
                        std::snprintf( label, sizeof( label ), "%s##dest%zu", plain_name( *dest ).c_str(), i );
                        if( ImGui::Selectable( label ) ) {
                            g_pending = { pending_kind::move_child, &it, g_selected_parent, dest };
                        }
                    }
                    ImGui::EndCombo();
                }
            } else {
                const int top_row_index = row_of( rows, &it );
                if( top_row_index >= 0 ) {
                    const bool wielded = std::string( rows[top_row_index].slot ) == "持握";
                    ImGui::Separator();
                    if( wielded ) {
                        if( ImGui::Button( "收起（放回背包）", ImVec2( 170.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::unwield, &it, nullptr, nullptr };
                        }
                        ImGui::SameLine();
                        if( ImGui::Button( "穿上", ImVec2( 90.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::wear, &it, nullptr, nullptr };
                        }
                    } else {
                        if( ImGui::Button( "持握", ImVec2( 90.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::wield, &it, nullptr, nullptr };
                        }
                        ImGui::SameLine();
                        if( ImGui::Button( "卸下（放回背包）", ImVec2( 170.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::takeoff, &it, nullptr, nullptr };
                        }
                    }
                } else {
                    ImGui::TextDisabled( "（该物品已不在装备栏）" );
                }
            }
            ImGui::Separator();
            const std::vector<item *> kids = contents_of( it );
            if( kids.empty() ) {
                ImGui::TextDisabled( "（这个物品不是容器，或容器为空）" );
            } else {
                ImGui::TextDisabled( "容器内容（点击子物品可移动）" );
                for( item *kid : kids ) {
                    if( kid != nullptr ) {
                        draw_item_tree( *kid, &it );
                    }
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();

    apply_pending();
}

} // namespace gear
} // namespace ui2

#endif // TILES

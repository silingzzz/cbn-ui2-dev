#include "cata_imgui_ui2_gear.h"

#if defined( TILES )

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "avatar_action.h"
#include "cata_imgui.h"
#include "character.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "units.h"

namespace
{

bool g_open = false;
item *g_selected = nullptr;
item *g_selected_parent = nullptr;

/// Restore the gear window after the game's own item UI returns.
bool g_reopen_after_use = false;

enum class pending_kind {
    none,
    wield,
    takeoff,
    unwield,
    wear,
    move_child,
    take_child,
    use,
    drop,
};

struct pending_action {
    pending_kind kind = pending_kind::none;
    item *subject = nullptr;
    item *from = nullptr;
    item *dest = nullptr;
};

pending_action g_pending;
std::string g_notice;
bool g_notice_error = false;

struct gear_drag_payload {
    item *subject = nullptr;
    item *parent = nullptr;
};

struct container_choice {
    item *ptr = nullptr;
    std::string path;
};

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
    const location_inventory &inv = you.get_inv();
    for( int i = 0; i < static_cast<int>( inv.size() ); ++i ) {
        const std::vector<item *> &stack = inv.const_stack( i );
        if( !stack.empty() && stack.front() != nullptr ) {
            rows.push_back( { stack.front(), "背包" } );
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

bool can_store_items( const item &it )
{
    return it.is_container() || it.get_storage() > 0_ml;
}

bool can_accept( const item &dest, const item &child )
{
    if( dest.is_container() ) {
        return dest.can_contain( child );
    }
    // Wearable storage in this codebase is separate from type->container.
    // Keep liquids in proper liquid containers rather than clothing storage.
    return dest.get_storage() > 0_ml && !child.made_of( LIQUID );
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

bool contains_item( const item &root, const item *needle )
{
    if( &root == needle ) {
        return true;
    }
    for( const item *kid : root.contents.all_items_top() ) {
        if( kid != nullptr && contains_item( *kid, needle ) ) {
            return true;
        }
    }
    return false;
}

void collect_containers( item &root, const std::string &path,
                         std::vector<container_choice> &out )
{
    if( can_store_items( root ) ) {
        out.push_back( { &root, path } );
    }
    for( item *kid : contents_of( root ) ) {
        if( kid != nullptr ) {
            collect_containers( *kid, path + " / " + plain_name( *kid ), out );
        }
    }
}

void draw_drag_source( item &it, item *parent )
{
    if( ImGui::BeginDragDropSource() ) {
        const gear_drag_payload payload{ &it, parent };
        ImGui::SetDragDropPayload( "GEAR_ITEM", &payload, sizeof( payload ) );
        ImGui::Text( "移动：%s", plain_name( it ).c_str() );
        ImGui::EndDragDropSource();
    }
}

void accept_container_drop( item &dest )
{
    if( !can_store_items( dest ) ) {
        return;
    }
    if( ImGui::BeginDragDropTarget() ) {
        if( const ImGuiPayload *payload = ImGui::AcceptDragDropPayload( "GEAR_ITEM" ) ) {
            if( payload->DataSize == sizeof( gear_drag_payload ) ) {
                const auto dropped = *static_cast<const gear_drag_payload *>( payload->Data );
                if( dropped.subject != nullptr ) {
                    g_pending = { pending_kind::move_child, dropped.subject, dropped.parent, &dest };
                }
            }
        }
        ImGui::EndDragDropTarget();
    }
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
    draw_drag_source( it, parent );
    accept_container_drop( it );
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

void move_child( Character &you, item &child, item &dest )
{
    if( !can_store_items( dest ) ) {
        you.add_msg_if_player( m_info, "%s 不是容器。", plain_name( dest ) );
        return;
    }
    if( child.parent_item() == &dest || contains_item( child, &dest ) ) {
        you.add_msg_if_player( m_info, "不能把物品移入自身或它自己的子容器。" );
        return;
    }
    if( !can_accept( dest, child ) ) {
        you.add_msg_if_player( m_info, "%s 不能收纳 %s。", plain_name( dest ), plain_name( child ) );
        return;
    }
    if( dest.contents.item_size_modifier() + child.volume() > dest.get_total_capacity() ) {
        you.add_msg_if_player( m_info, "%s 装不下 %s。", plain_name( dest ), plain_name( child ) );
        return;
    }
    const std::string child_name = plain_name( child );
    const std::string dest_name = plain_name( dest );
    detached_ptr<item> piece = child.detach();
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

void take_child_to_inventory( Character &you, item & /*parent*/, item &child )
{
    if( child.made_of( LIQUID ) ) {
        you.add_msg_if_player( m_info, "液体需要先倒入合适的容器，不能直接放入背包。" );
        return;
    }
    const std::string child_name = plain_name( child );
    detached_ptr<item> piece = child.detach();
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
    avatar &you = get_avatar();
    item &it = *g_pending.subject;
    const std::string name = plain_name( it );
    g_notice.clear();
    g_notice_error = false;
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
                g_notice = "不能持握 " + name;
                g_notice_error = true;
            } else {
                g_notice = "已持握 " + name;
            }
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        case pending_kind::takeoff:
            take_off_to_inventory( you, it );
            g_notice = "已卸下 " + name;
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        case pending_kind::unwield: {
            detached_ptr<item> piece = you.remove_primary_weapon();
            if( piece ) {
                you.i_add_or_drop( std::move( piece ) );
                g_notice = "已收起 " + name;
            } else {
                you.add_msg_if_player( m_warning, "不能收起 %s。", name );
                g_notice = "不能收起 " + name;
                g_notice_error = true;
            }
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        }
        case pending_kind::wear: {
            const auto check = you.can_wear( it );
            if( !check.success() ) {
                you.add_msg_if_player( m_info, "%s", check.c_str() );
                g_notice = "不能穿戴 " + name + "：" + check.c_str();
                g_notice_error = true;
            } else {
                detached_ptr<item> piece = it.detach();
                detached_ptr<item> left = you.wear_item( std::move( piece ), true );
                if( left ) {
                    you.i_add( std::move( left ) );
                    you.add_msg_if_player( m_warning, "穿不上 %s。", name );
                    g_notice = "穿不上 " + name;
                    g_notice_error = true;
                } else {
                    g_notice = "已穿上 " + name;
                }
            }
            g_selected = nullptr;
            g_selected_parent = nullptr;
            break;
        }
        case pending_kind::move_child:
            if( g_pending.dest != nullptr ) {
                move_child( you, it, *g_pending.dest );
                g_notice = "已移入 " + plain_name( *g_pending.dest );
                g_selected = g_pending.subject;
                g_selected_parent = g_pending.dest;
            } else {
                g_notice = "没有可用的目标容器";
                g_notice_error = true;
            }
            break;
        case pending_kind::take_child:
            if( g_pending.from != nullptr ) {
                take_child_to_inventory( you, *g_pending.from, it );
                g_notice = "已取出 " + name;
                g_selected = nullptr;
                g_selected_parent = nullptr;
            } else {
                g_notice = "原容器已失效";
                g_notice_error = true;
            }
            break;
        case pending_kind::use: {
            bool still_owned = false;
            for( const gear_row &row : build_rows() ) {
                if( row.ptr != nullptr && contains_item( *row.ptr, &it ) ) {
                    still_owned = true;
                    break;
                }
            }
            if( !still_owned ) {
                you.add_msg_if_player( m_info, "该物品已不在装备栏，取消使用。" );
                g_notice = "该物品已不在装备栏";
                g_notice_error = true;
                g_reopen_after_use = false;
                break;
            }
            // This function is now called from game::handle_action(), outside
            // ImGui's draw callback. It is safe to let the game open its own UI.
            g_selected = nullptr;
            g_selected_parent = nullptr;
            g_notice = "正在使用 " + name;
            avatar_action::use_item( you, &it );
            if( g_reopen_after_use ) {
                g_reopen_after_use = false;
                g_open = true;
                if( !cata_imgui::is_open() ) {
                    cata_imgui::toggle();
                }
            }
            break;
        }
        case pending_kind::drop: {
            bool still_owned = false;
            for( const gear_row &row : build_rows() ) {
                if( row.ptr != nullptr && contains_item( *row.ptr, &it ) ) {
                    still_owned = true;
                    break;
                }
            }
            if( !still_owned ) {
                you.add_msg_if_player( m_info, "该物品已不在装备栏，取消丢弃。" );
                g_notice = "该物品已不在装备栏";
                g_notice_error = true;
            } else {
                g_selected = nullptr;
                g_selected_parent = nullptr;
                you.drop( it, you.bub_pos() );
                g_notice = "已丢下 " + name;
            }
            break;
        }
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

void set_open( bool open )
{
    g_open = open;
}

bool has_deferred_action()
{
    return g_pending.kind != pending_kind::none;
}

bool take_deferred_action()
{
    if( g_pending.kind == pending_kind::none ) {
        return false;
    }
    apply_pending();
    return true;
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

    ImGui::SetNextWindowPos( ImVec2( 980.0f, 110.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 520.0f, 600.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "装备与容器", &g_open ) ) {
        ImGui::End();
        return;
    }

    const int wielded_count = static_cast<int>( std::count_if( rows.begin(), rows.end(),
    []( const gear_row & row ) {
        return std::string( row.slot ) == "持握";
    } ) );
    const int worn_count = static_cast<int>( std::count_if( rows.begin(), rows.end(),
    []( const gear_row & row ) {
        return std::string( row.slot ) == "穿戴";
    } ) );
    const int backpack_count = static_cast<int>( std::count_if( rows.begin(), rows.end(),
    []( const gear_row & row ) {
        return std::string( row.slot ) == "背包";
    } ) );
    ImGui::TextDisabled( "持握 %d ｜ 穿戴 %d ｜ 背包 %d ｜ 点击选择，拖到容器可移入",
                         wielded_count, worn_count, backpack_count );
    if( !g_notice.empty() ) {
        ImGui::TextColored( g_notice_error ? ImVec4( 0.95f, 0.45f, 0.35f, 1.0f )
                            : ImVec4( 0.55f, 0.85f, 0.55f, 1.0f ),
                            "%s", g_notice.c_str() );
    }
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
            draw_drag_source( *row.ptr, nullptr );
            accept_container_drop( *row.ptr );
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
            auto draw_move_combo = [&]() {
                ImGui::SetNextItemWidth( 240.0f );
                if( ImGui::BeginCombo( "移入容器", "选择目标容器（含嵌套）" ) ) {
                    std::vector<container_choice> choices;
                    for( const gear_row &row : rows ) {
                        if( row.ptr != nullptr ) {
                            collect_containers( *row.ptr, plain_name( *row.ptr ), choices );
                        }
                    }
                    for( std::size_t i = 0; i < choices.size(); ++i ) {
                        item *dest = choices[i].ptr;
                        if( dest == g_selected_parent || dest == &it || contains_item( it, dest ) ) {
                            continue;
                        }
                        char label[512];
                        std::snprintf( label, sizeof( label ), "%s##dest%zu", choices[i].path.c_str(), i );
                        if( ImGui::Selectable( label ) ) {
                            g_pending = { pending_kind::move_child, &it, g_selected_parent, dest };
                        }
                    }
                    ImGui::EndCombo();
                }
            };
            if( g_selected_parent != nullptr ) {
                ImGui::TextDisabled( "位于：%s", plain_name( *g_selected_parent ).c_str() );
                ImGui::Separator();
                if( it.made_of( LIQUID ) ) {
                    ImGui::BeginDisabled();
                }
                if( ImGui::Button( "取出到背包", ImVec2( 130.0f, 0.0f ) ) ) {
                    g_pending = { pending_kind::take_child, &it, g_selected_parent, nullptr };
                }
                if( it.made_of( LIQUID ) ) {
                    ImGui::EndDisabled();
                    ImGui::TextDisabled( "液体需倒入合适容器，不能直接取出到背包。" );
                } else {
                    ImGui::SameLine();
                }
                draw_move_combo();
            } else {
                const int top_row_index = row_of( rows, &it );
                if( top_row_index >= 0 ) {
                    const std::string slot = rows[top_row_index].slot;
                    const bool wielded = slot == "持握";
                    ImGui::Separator();
                    if( wielded ) {
                        if( ImGui::Button( "收起（放回背包）", ImVec2( 170.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::unwield, &it, nullptr, nullptr };
                        }
                    } else {
                        if( ImGui::Button( "持握", ImVec2( 90.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::wield, &it, nullptr, nullptr };
                        }
                        ImGui::SameLine();
                        if( slot == "穿戴" ) {
                            if( ImGui::Button( "卸下（放回背包）", ImVec2( 170.0f, 0.0f ) ) ) {
                                g_pending = { pending_kind::takeoff, &it, nullptr, nullptr };
                            }
                        }
                    }
                    if( it.is_armor() && slot != "穿戴" ) {
                        if( wielded ) {
                            ImGui::SameLine();
                        }
                        if( ImGui::Button( "穿上", ImVec2( 90.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::wear, &it, nullptr, nullptr };
                        }
                    }
                    ImGui::Spacing();
                    if( ImGui::Button( "使用", ImVec2( 90.0f, 0.0f ) ) ) {
                        g_pending = { pending_kind::use, &it, nullptr, nullptr };
                        g_reopen_after_use = true;
                        // The game action opens its own UI; hide the overlay so the
                        // player sees that UI rather than this paused frame.
                        g_open = false;
                        if( cata_imgui::is_open() ) {
                            cata_imgui::toggle();
                        }
                    }
                    if( ImGui::IsItemHovered() ) {
                        ImGui::SetTooltip( "关闭覆盖层后调用游戏原有的使用动作；会打开物品对应界面。" );
                    }
                    if( slot == "背包" ) {
                        draw_move_combo();
                        ImGui::Spacing();
                        if( ImGui::Button( "丢下到地面", ImVec2( 130.0f, 0.0f ) ) ) {
                            g_pending = { pending_kind::drop, &it, nullptr, nullptr };
                        }
                        if( ImGui::IsItemHovered() ) {
                            ImGui::SetTooltip( "在当前位置丢下；面板保持打开，由游戏原有掉落动作结算。" );
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
}

} // namespace gear
} // namespace ui2

#endif // TILES

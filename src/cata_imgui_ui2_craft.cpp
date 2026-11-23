#include "cata_imgui_ui2_craft.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "character.h"
#include "crafting.h"
#include "inventory.h"
#include "item.h"
#include "messages.h"
#include "player_activity.h"
#include "recipe.h"
#include "recipe_dictionary.h"
#include "requirements.h"

namespace
{

bool g_open = false;
bool g_only_craftable = false;
const recipe *g_selected = nullptr;
int g_batch = 1;
char g_search[128] = "";
// Total steps of the craft started from this window; used for the progress bar.
int g_activity_total_moves = 0;
bool g_pending_cancel = false;

auto plain_text( const std::string &raw ) -> std::string
{
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

auto lower_copy( std::string text ) -> std::string
{
    std::transform( text.begin(), text.end(), text.begin(),
    []( unsigned char c ) {
        return static_cast<char>( std::tolower( c ) );
    } );
    return text;
}

auto build_rows() -> std::vector<const recipe *>
{
    Character &you = get_avatar();
    const recipe_subset available = you.get_available_recipes( you.crafting_inventory() );
    std::vector<const recipe *> rows;
    rows.reserve( available.size() );
    for( const recipe *rec : available ) {
        if( rec != nullptr && !rec->is_nested() ) {
            rows.push_back( rec );
        }
    }
    const std::string query = lower_copy( g_search );
    if( !query.empty() ) {
        std::erase_if( rows, [&query]( const recipe * rec ) {
            return lower_copy( plain_text( rec->result_name() ) ).find( query ) ==
                   std::string::npos;
        } );
    }
    if( g_only_craftable ) {
        const inventory &inv = you.crafting_inventory();
        std::erase_if( rows, [&inv]( const recipe * rec ) {
            return !rec->deduped_requirements().can_make_with_inventory(
                       inv, rec->get_component_filter(), g_batch );
        } );
    }
    std::sort( rows.begin(), rows.end(), []( const recipe * a, const recipe * b ) {
        return lower_copy( plain_text( a->result_name() ) ) <
               lower_copy( plain_text( b->result_name() ) );
    } );
    return rows;
}

void start_craft( const recipe &rec, int batch )
{
    Character &you = get_avatar();
    g_activity_total_moves = rec.batch_time( batch, 1.0f, 0 );
    you.make_craft( rec.ident(), batch, you.bub_pos() );
    add_msg( m_info, "新版制作：提交 %s ×%d，组件与工具仍由原制作流程检查。",
             plain_text( rec.result_name() ).c_str(), batch );
}

} // namespace

namespace ui2
{
namespace craft
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

    const std::vector<const recipe *> rows = build_rows();
    Character &you = get_avatar();
    if( std::find( rows.begin(), rows.end(), g_selected ) == rows.end() ) {
        g_selected = rows.empty() ? nullptr : rows.front();
    }

    ImGui::SetNextWindowPos( ImVec2( 60.0f, 520.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 1180.0f, 470.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "制作（PR-4 预览）", &g_open ) ) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth( 280.0f );
    ImGui::InputTextWithHint( "##craft_search", "搜索产物名称…", g_search, IM_ARRAYSIZE( g_search ) );
    ImGui::SameLine();
    ImGui::TextDisabled( "可用配方 %d ｜ 批量 %d ｜ 组件/工具由游戏检查",
                         static_cast<int>( rows.size() ), g_batch );
    ImGui::SameLine();
    ImGui::Checkbox( "仅材料齐全", &g_only_craftable );
    ImGui::Separator();

    const float inner_height = ImGui::GetContentRegionAvail().y;
    if( ImGui::BeginChild( "craft_list", ImVec2( 520.0f, inner_height ), true ) ) {
        if( rows.empty() ) {
            ImGui::TextDisabled( "（没有匹配的可用配方）" );
        } else {
            for( std::size_t i = 0; i < rows.size(); ++i ) {
                const recipe *rec = rows[i];
                char label[256];
                std::snprintf( label, sizeof( label ), "%s##craft%zu",
                               plain_text( rec->result_name() ).c_str(), i );
                if( ImGui::Selectable( label, g_selected == rec ) ) {
                    g_selected = rec;
                }
                ImGui::SameLine( 360.0f );
                ImGui::TextDisabled( "难度 %d ｜ %d 步", rec->difficulty,
                                     rec->batch_time( 1, 1.0f, 0 ) );
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();
    if( ImGui::BeginChild( "craft_detail", ImVec2( 0.0f, inner_height ), true ) ) {
        if( g_selected == nullptr ) {
            ImGui::TextDisabled( "先选中左侧配方。" );
        } else {
            const recipe &rec = *g_selected;
            ImGui::TextUnformatted( plain_text( rec.result_name() ).c_str() );
            ImGui::TextDisabled( "分类：%s ｜ 难度：%d ｜ 制作步数：%d",
                                 rec.category.c_str(), rec.difficulty,
                                 rec.batch_time( 1, 1.0f, 0 ) );
            ImGui::TextDisabled( "技能：%s",
                                 plain_text( rec.primary_skill_string( &get_avatar(), true ) ).c_str() );
            ImGui::TextDisabled( "相关技能：%s",
                                 plain_text( rec.required_all_skills_string() ).c_str() );

            const inventory &inv = you.crafting_inventory();
            const int batch = std::max( 1, std::min( g_batch, 50 ) );
            const bool can_make = rec.deduped_requirements().can_make_with_inventory(
                                      inv, rec.get_component_filter(), batch );
            ImGui::TextColored( can_make ? ImVec4( 0.55f, 0.90f, 0.55f, 1.0f )
                                 : ImVec4( 0.95f, 0.60f, 0.55f, 1.0f ),
                                 can_make ? "当前批次 %d：材料与工具齐全" : "当前批次 %d：材料或工具不足",
                                 batch );
            if( !can_make ) {
                const std::string missing =
                    plain_text( rec.simple_requirements().list_missing() );
                if( !missing.empty() ) {
                    ImGui::TextWrapped( "%s", missing.c_str() );
                }
            }
            if( ImGui::TreeNode( "全部需求（组件 / 工具 / 质量）" ) ) {
                const std::string all = plain_text( rec.simple_requirements().list_all() );
                ImGui::TextWrapped( "%s", all.empty() ? "（无组件或工具要求）" : all.c_str() );
                ImGui::TreePop();
            }
            if( you.activity ) {
                const std::string act_id = you.activity->id().str();
                ImGui::TextDisabled( "当前活动：%s", act_id.c_str() );
                if( act_id == "ACT_CRAFT" ) {
                    const int moves_left = std::max( 0, you.activity->get_moves_left() );
                    if( g_activity_total_moves > 0 ) {
                        const float pct = std::clamp(
                                              1.0f - static_cast<float>( moves_left ) /
                                              static_cast<float>( g_activity_total_moves ),
                                              0.0f, 1.0f );
                        const char *overlay = pct >= 1.0f ? "即将完成" : nullptr;
                        ImGui::ProgressBar( pct, ImVec2( -1.0f, 0.0f ), overlay );
                        ImGui::TextDisabled( "剩余 %d 步（总 %d 步）", moves_left,
                                             g_activity_total_moves );
                    } else {
                        ImGui::TextDisabled( "剩余 %d 步", moves_left );
                    }
                    if( ImGui::Button( "取消当前制作" ) ) {
                        g_pending_cancel = true;
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled( "取消会丢弃已预扣的组件，原样走游戏 canceled 回调。" );
                }
            } else {
                ImGui::TextDisabled( "当前没有进行中的活动" );
            }
            ImGui::Separator();
            ImGui::SetNextItemWidth( 100.0f );
            ImGui::InputInt( "##craft_batch", &g_batch, 1, 10 );
            g_batch = std::max( 1, std::min( g_batch, 50 ) );
            ImGui::SameLine();
            if( !can_make ) {
                ImGui::BeginDisabled();
            }
            if( ImGui::Button( "制作 1 次" ) ) {
                start_craft( rec, 1 );
            }
            ImGui::SameLine();
            if( ImGui::Button( "制作 N 次" ) ) {
                start_craft( rec, g_batch );
            }
            if( !can_make ) {
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::TextDisabled( "（材料或工具不足，已禁用）" );
            }
            ImGui::TextDisabled( "批量制作仍走原 make_craft；队列、优先级与状态由游戏活动系统管理。" );
        }
    }
    ImGui::EndChild();

    ImGui::End();

    // A write must not run inside the ImGui frame: cancelling can tear down the
    // activity and invalidate the inventory the frame is still reading.
    if( g_pending_cancel ) {
        g_pending_cancel = false;
        if( you.activity && you.activity->id().str() == "ACT_CRAFT" ) {
            you.cancel_activity();
            you.invalidate_crafting_inventory();
            g_activity_total_moves = 0;
            add_msg( m_info, "新版制作：已取消当前制作。" );
        }
    }
}

} // namespace craft
} // namespace ui2

#endif // TILES

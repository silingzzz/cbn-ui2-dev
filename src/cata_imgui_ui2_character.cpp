#include "cata_imgui_ui2_character.h"

#if defined( TILES )

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui.h>

#include "avatar.h"
#include "bionics.h"
#include "character_display.h"
#include "effect.h"
#include "game.h"
#include "mutation.h"
#include "perk.h"
#include "skill.h"
#include "units.h"

namespace
{

bool g_open = false;
ui2::character::tab_kind g_tab = ui2::character::tab_kind::stats;
bool g_only_trained_skills = false;
char g_filter[64] = "";

bool contains_ci( const std::string &haystack, const std::string &needle )
{
    if( needle.empty() ) {
        return true;
    }
    const auto it = std::search( haystack.begin(), haystack.end(),
                                 needle.begin(), needle.end(),
    []( char a, char b ) {
        return std::tolower( static_cast<unsigned char>( a ) ) ==
               std::tolower( static_cast<unsigned char>( b ) );
    } );
    return it != haystack.end();
}

void draw_stat_row( const char *label, int cur, int base )
{
    if( cur > base ) {
        ImGui::TextColored( ImVec4( 0.45f, 0.92f, 0.55f, 1.0f ),
                            "%s：%d (基础 %d)", label, cur, base );
    } else if( cur < base ) {
        ImGui::TextColored( ImVec4( 1.0f, 0.55f, 0.45f, 1.0f ),
                            "%s：%d (基础 %d)", label, cur, base );
    } else {
        ImGui::Text( "%s：%d (基础 %d)", label, cur, base );
    }
}

void draw_stats_tab( const avatar &you )
{
    const ui2::character::stats_snapshot snap = ui2::character::read_stats_snapshot();

    ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "基础属性" );
    draw_stat_row( "力量", snap.str, snap.str_base );
    draw_stat_row( "敏捷", snap.dex, snap.dex_base );
    draw_stat_row( "智力", snap.intel, snap.intel_base );
    draw_stat_row( "感知", snap.per, snap.per_base );

    ImGui::Separator();
    ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "即时状态" );
    ImGui::Text( "速度：%d  │  痛感：%d  │  耐力：%d / %d",
                 snap.speed, snap.pain, snap.stamina, snap.stamina_max );
    ImGui::Text( "负重：%.1f / %.1f kg  │  体积：%.2f / %.2f L",
                 static_cast<double>( units::to_gram( you.weight_carried() ) ) / 1000.0,
                 static_cast<double>( units::to_gram( you.weight_capacity() ) ) / 1000.0,
                 static_cast<double>( units::to_milliliter( you.volume_carried() ) ) / 1000.0,
                 static_cast<double>( units::to_milliliter( you.volume_capacity() ) ) / 1000.0 );
    if( snap.max_power_kj > 0 ) {
        ImGui::Text( "义体能量：%d / %d kJ", snap.power_kj, snap.max_power_kj );
    } else {
        ImGui::TextDisabled( "义体能量：无储能" );
    }

    ImGui::Separator();
    ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ), "部位生命值" );
    for( const auto &bp : you.get_body() ) {
        const int hp = bp.second.get_hp_cur();
        const int max_hp = bp.second.get_hp_max();
        ImGui::BulletText( "%s：%d / %d", bp.first->name.translated().c_str(), hp, max_hp );
    }
}

void draw_skills_tab( const avatar &you )
{
    ImGui::Checkbox( "仅看已提升技能 (>0级)", &g_only_trained_skills );
    ImGui::Separator();

    const std::vector<const Skill *> sorted_skills = Skill::get_skills_sorted_by(
    []( const Skill & a, const Skill & b ) {
        return a.name() < b.name();
    } );

    int shown = 0;
    for( const Skill *sk : sorted_skills ) {
        if( sk == nullptr ) {
            continue;
        }
        const int lvl = you.get_skill_level( sk->ident() );
        if( g_only_trained_skills && lvl <= 0 ) {
            continue;
        }
        const std::string name = sk->name();
        if( !contains_ci( name, g_filter ) ) {
            continue;
        }
        ++shown;
        const int pct = you.get_skill_level_object( sk->ident() ).exercise();
        ImGui::BulletText( "%s：Lv %d (%d%%)", name.c_str(), lvl, pct );
    }
    if( shown == 0 ) {
        ImGui::TextDisabled( "没有符合条件的技能。" );
    }
}

void draw_traits_tab( const avatar &you )
{
    const std::vector<perk_id> perks = you.get_perks();
    const std::vector<trait_id> muts = you.get_mutations( false );

    ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ),
                        "专长 (%zu)", perks.size() );
    if( perks.empty() ) {
        ImGui::TextDisabled( "当前无专长。" );
    } else {
        for( const perk_id &pk : perks ) {
            const std::string name = pk.obj().get_name();
            if( contains_ci( name, g_filter ) ) {
                ImGui::BulletText( "%s", name.c_str() );
            }
        }
    }

    ImGui::Separator();
    ImGui::TextColored( ImVec4( 0.65f, 0.88f, 1.0f, 1.0f ),
                        "特质 / 变异 (%zu)", muts.size() );
    if( muts.empty() ) {
        ImGui::TextDisabled( "当前无特质或变异。" );
    } else {
        for( const trait_id &mut : muts ) {
            const std::string name = mut.obj().name();
            if( contains_ci( name, g_filter ) ) {
                ImGui::BulletText( "%s", name.c_str() );
            }
        }
    }
}

void draw_bionics_tab( const avatar &you )
{
    if( you.my_bionics->empty() ) {
        ImGui::TextDisabled( "当前未安装任何生化义体。" );
        return;
    }
    int shown = 0;
    for( const bionic &bio : *you.my_bionics ) {
        const std::string name = bio.info().name.translated();
        if( !contains_ci( name, g_filter ) ) {
            continue;
        }
        ++shown;
        const char *state = bio.info().activated ? ( bio.powered ? "[开启]" : "[关闭]" ) : "[被动]";
        ImGui::BulletText( "%s %s", state, name.c_str() );
    }
    if( shown == 0 ) {
        ImGui::TextDisabled( "没有符合条件的义体。" );
    }
}

void draw_effects_tab( const avatar &you )
{
    int shown = 0;
    for( const auto &elem : you.get_all_effects() ) {
        for( const auto &bp_eff : elem.second ) {
            const effect &eff = bp_eff.second;
            const std::string name = eff.disp_name();
            if( !contains_ci( name, g_filter ) ) {
                continue;
            }
            ++shown;
            ImGui::BulletText( "%s", name.c_str() );
        }
    }
    if( shown == 0 ) {
        ImGui::TextDisabled( "当前没有生效的状态效果。" );
    }
}

} // namespace

namespace ui2
{
namespace character
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

void reset_session()
{
    g_open = false;
    g_tab = tab_kind::stats;
    g_only_trained_skills = false;
    g_filter[0] = '\0';
}

void set_tab( tab_kind tab )
{
    g_tab = tab;
}

tab_kind current_tab()
{
    return g_tab;
}

auto read_stats_snapshot() -> stats_snapshot
{
    stats_snapshot snap;
    if( g == nullptr ) {
        return snap;
    }
    const avatar &you = get_avatar();
    snap.str = you.get_str();
    snap.str_base = you.get_str_base();
    snap.dex = you.get_dex();
    snap.dex_base = you.get_dex_base();
    snap.intel = you.get_int();
    snap.intel_base = you.get_int_base();
    snap.per = you.get_per();
    snap.per_base = you.get_per_base();
    snap.speed = you.get_speed();
    snap.pain = you.get_pain();
    snap.stamina = you.get_stamina();
    snap.stamina_max = you.get_stamina_max();
    snap.power_kj = static_cast<int>( units::to_kilojoule( you.get_power_level() ) );
    snap.max_power_kj = static_cast<int>( units::to_kilojoule( you.get_max_power_level() ) );
    return snap;
}

void draw()
{
    if( !g_open ) {
        return;
    }

    ImGui::SetNextWindowPos( ImVec2( 12.0f, 132.0f ), ImGuiCond_FirstUseEver );
    ImGui::SetNextWindowSize( ImVec2( 920.0f, 540.0f ), ImGuiCond_FirstUseEver );
    if( !ImGui::Begin( "人物总览 (只读)###ui2_character_window", &g_open ) ) {
        ImGui::End();
        return;
    }

    if( g == nullptr ) {
        ImGui::TextDisabled( "世界尚未载入。" );
        ImGui::End();
        return;
    }

    bool open_legacy_character = false;
    avatar &you = get_avatar();
    if( you.get_body().empty() ) {
        ImGui::TextDisabled( "角色尚未初始化。" );
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth( 240.0f );
    ImGui::InputTextWithHint( "##char_filter", "过滤技能/特质/义体/状态…",
                              g_filter, IM_ARRAYSIZE( g_filter ) );
    if( g_filter[0] != '\0' ) {
        ImGui::SameLine();
        if( ImGui::Button( "清空" ) ) {
            g_filter[0] = '\0';
        }
    }
    ImGui::SameLine();
    if( ImGui::Button( "打开完整人物界面 (@)…" ) ) {
        open_legacy_character = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled( "│ %s", you.get_name().c_str() );

    ImGui::Separator();

    if( ImGui::BeginTabBar( "##char_overview_tabs" ) ) {
        if( ImGui::BeginTabItem( "属性与状态###char_stats" ) ) {
            g_tab = tab_kind::stats;
            draw_stats_tab( you );
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( "技能###char_skills" ) ) {
            g_tab = tab_kind::skills;
            draw_skills_tab( you );
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( "专长与特质###char_traits" ) ) {
            g_tab = tab_kind::traits;
            draw_traits_tab( you );
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( "义体###char_bionics" ) ) {
            g_tab = tab_kind::bionics;
            draw_bionics_tab( you );
            ImGui::EndTabItem();
        }
        if( ImGui::BeginTabItem( "状态效果###char_effects" ) ) {
            g_tab = tab_kind::effects;
            draw_effects_tab( you );
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    if( open_legacy_character ) {
        character_display::disp_info( you );
    }
}

} // namespace character
} // namespace ui2

#endif // TILES

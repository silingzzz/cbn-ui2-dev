#include "scores_ui.h"

#include <algorithm>
#include <cassert>
#include <iterator>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "achievement.h"
#include "color.h"
#include "cursesdef.h"
#include "cursesport.h"
#include "event_statistics.h"
#include "input.h"
#include "kill_tracker.h"
#include "output.h"
#include "point.h"
#include "stats_tracker.h"
#include "translations.h"
#include "ui.h"
#include "ui_manager.h"

static std::string get_achievements_text( const achievements_tracker &achievements )
{
    std::string os;
    std::vector<const achievement *> valid_achievements = achievements.valid_achievements();
    valid_achievements.erase(
        std::remove_if( valid_achievements.begin(), valid_achievements.end(),
    [&]( const achievement * a ) {
        return achievements.is_hidden( a );
    } ), valid_achievements.end() );
    using sortable_achievement =
        std::tuple<achievement_completion, std::string, const achievement *>;
    std::vector<sortable_achievement> sortable_achievements;
    std::transform( valid_achievements.begin(), valid_achievements.end(),
                    std::back_inserter( sortable_achievements ),
    [&]( const achievement * ach ) {
        achievement_completion comp = achievements.is_completed( ach->id );
        return std::make_tuple( comp, ach->name().translated(), ach );
    } );
    std::sort( sortable_achievements.begin(), sortable_achievements.end(), localized_compare );
    for( const sortable_achievement &ach : sortable_achievements ) {
        os += achievements.ui_text_for( std::get<const achievement *>( ach ) ) + "\n";
    }
    if( valid_achievements.empty() ) {
        os += _( "This game has no valid achievements.\n" );
    }
    os += _( "Note that only achievements that existed when you started this game and still "
             "exist now will appear here." );
    return os;
}

static std::string get_scores_text( stats_tracker &stats )
{
    std::string os;
    std::vector<const score *> valid_scores = stats.valid_scores();
    for( const score *scr : valid_scores ) {
        os += scr->description( stats ) + "\n";
    }
    if( valid_scores.empty() ) {
        os += _( "This game has no valid scores.\n" );
    }
    os += _( "\nNote that only scores that existed when you started this game and still exist now "
             "will appear here." );
    return os;
}

void show_scores_ui( const achievements_tracker &achievements, stats_tracker &stats,
                     const kill_tracker &kills )
{
    catacurses::window w;

    enum class tab_mode {
        achievements,
        scores,
        kills,
        num_tabs,
        first_tab = achievements,
    };

    tab_mode tab = static_cast<tab_mode>( 0 );
    input_context ctxt( "SCORES" );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    // Mouse: click/hover the tabs, wheel scrolls the text view.
    ctxt.register_action( "MOUSE_MOVE" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );

    catacurses::window w_view;
    scrolling_text_view view( w_view );
    bool new_tab = true;

    ui_adaptor ui;
    const auto &init_windows = [&]( ui_adaptor & ui ) {
        w = new_centered_win( TERMY - 2, FULL_SCREEN_WIDTH );
        w_view = catacurses::newwin( getmaxy( w ) - 4, getmaxx( w ) - 1,
                                     point( getbegx( w ), getbegy( w ) + 3 ) );
        ui.position_from_window( w );
    };
    ui.on_screen_resize( init_windows );
    // initialize explicitly here since w_view is used before first redraw
    init_windows( ui );

    const std::vector<std::pair<tab_mode, std::string>> tabs = {
        { tab_mode::achievements, _( "ACHIEVEMENTS" ) },
        { tab_mode::scores, _( "SCORES" ) },
        { tab_mode::kills, _( "KILLS" ) },
    };

    std::vector<std::string> tab_texts;
    tab_texts.reserve( tabs.size() );
    for( const std::pair<tab_mode, std::string> &tab_entry : tabs ) {
        tab_texts.push_back( tab_entry.second );
    }
    // Mouse hit areas of the tab bar, recorded while drawing.
    std::vector<std::pair<inclusive_rectangle<point>, size_t>> tab_hit_rects;

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w );
        draw_tabs( w, tabs, tab );
        tab_hit_rects = tab_rects( w, tab_texts, static_cast<size_t>( tab ) );
        draw_border_below_tabs( w );
        wnoutrefresh( w );

        view.draw( c_white );
    } );

    while( true ) {
        if( new_tab ) {
            switch( tab ) {
                case tab_mode::achievements:
                    view.set_text( get_achievements_text( achievements ) );
                    break;
                case tab_mode::scores:
                    view.set_text( get_scores_text( stats ) );
                    break;
                case tab_mode::kills:
                    view.set_text( kills.get_kills_text() );
                    break;
                case tab_mode::num_tabs:
                    assert( false );
                    break;
            }
        }

        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        new_tab = false;
        if( action == "MOUSE_MOVE" || action == "SELECT" ) {
            const point cell = mouse_pixel_to_cell( ctxt.get_raw_input().mouse_pos );
            for( const auto &hit : tab_hit_rects ) {
                if( hit.first.contains( cell ) ) {
                    tab = static_cast<tab_mode>( hit.second );
                    new_tab = true;
                    break;
                }
            }
        }
        if( action == "SCROLL_UP" ) {
            view.scroll_up();
        } else if( action == "SCROLL_DOWN" ) {
            view.scroll_down();
        } else if( action == "RIGHT" || action == "NEXT_TAB" ) {
            tab = static_cast<tab_mode>( static_cast<int>( tab ) + 1 );
            if( tab >= tab_mode::num_tabs ) {
                tab = tab_mode::first_tab;
            }
            new_tab = true;
        } else if( action == "LEFT" || action == "PREV_TAB" ) {
            tab = static_cast<tab_mode>( static_cast<int>( tab ) - 1 );
            if( tab < tab_mode::first_tab ) {
                tab = static_cast<tab_mode>( static_cast<int>( tab_mode::num_tabs ) - 1 );
            }
            new_tab = true;
        } else if( action == "DOWN" ) {
            view.scroll_down();
        } else if( action == "UP" ) {
            view.scroll_up();
        } else if( action == "PAGE_DOWN" ) {
            view.page_down();
        } else if( action == "PAGE_UP" ) {
            view.page_up();
        } else if( action == "CONFIRM" || action == "QUIT" ) {
            break;
        }
    }
}

void show_kills( kill_tracker &kills )
{
    catacurses::window w;

    input_context ctxt( "SCORES" );
    ctxt.register_cardinal();
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    // Mouse: wheel scrolls the kill list.
    ctxt.register_action( "SCROLL_UP" );
    ctxt.register_action( "SCROLL_DOWN" );

    catacurses::window w_view;
    scrolling_text_view view( w_view );

    ui_adaptor ui;
    const auto &init_windows = [&]( ui_adaptor & ui ) {
        w = new_centered_win( TERMY - 2, FULL_SCREEN_WIDTH );
        w_view = catacurses::newwin( getmaxy( w ) - 4, getmaxx( w ) - 1,
                                     point( getbegx( w ), getbegy( w ) + 3 ) );
        ui.position_from_window( w );
        view.set_text( kills.get_kills_text() );
    };
    ui.on_screen_resize( init_windows );
    // initialize explicitly here since w_view is used before first redraw
    init_windows( ui );

    ui.on_redraw( [&]( const ui_adaptor & ) {
        werase( w );
        draw_border( w );
        wnoutrefresh( w );
        view.draw( c_white );
    } );

    while( true ) {
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        if( action == "DOWN" || action == "SCROLL_DOWN" ) {
            view.scroll_down();
        } else if( action == "UP" || action == "SCROLL_UP" ) {
            view.scroll_up();
        } else if( action == "PAGE_DOWN" ) {
            view.page_down();
        } else if( action == "PAGE_UP" ) {
            view.page_up();
        } else if( action == "CONFIRM" || action == "QUIT" ) {
            break;
        }
    }
}

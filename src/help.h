#pragma once

#include <map>
#include <string>
#include <vector>
#include <utility>

#include "cuboid_rectangle.h"
#include "cursesdef.h"
#include "input.h"
#include "point.h"

class JsonIn;

class help
{
    public:
        void load();
        void display_help();

    private:
        void deserialize( JsonIn &jsin );
        void draw_menu( const catacurses::window &win );
        std::string get_note_colors();
        std::string get_dir_grid();

        std::map<int, std::pair<std::string, std::vector<std::string> > > help_texts;
        std::vector< std::vector<std::string> > hotkeys;

        /// Mouse hit areas of the help topic lines, recorded by draw_menu().
        std::vector<std::pair<inclusive_rectangle<point>, size_t>> menu_hits;

        input_context ctxt;
};

help &get_help();

std::string get_hint();



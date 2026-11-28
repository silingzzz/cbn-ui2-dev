#include "cata_imgui_ui2.h"
#include "cata_imgui_ui2_aim.h"
#include "cata_imgui_ui2_craft.h"
#include "cata_imgui_ui2_gear.h"
#include "cata_imgui_ui2_messages.h"
#include "cata_imgui_ui2_missions.h"
#include "cata_imgui_ui2_trade.h"
#include "cata_imgui_ui2_vehicle.h"
#include "cata_imgui_ui2_character.h"
#include "cata_imgui_ui2_zone.h"
#include "cata_imgui_ui2_shell.h"

#if defined( TILES )

#include "cata_imgui_ui2_map.h"
#include "cata_imgui_ui2_world.h"

namespace ui2
{

void reset_session()
{
    ui2::gear::reset_session();
    ui2::craft::reset_session();
    ui2::messages::reset_session();
    ui2::missions::reset_session();
    ui2::trade::reset_session();
    ui2::veh::reset_session();
    ui2::character::reset_session();
    ui2::world::reset_session();
}

void draw()
{
    // Platform layer first: it owns navigation and consumes Escape before the
    // business windows draw their own frame.
    shell::draw();
    // PR-2A AIM-1: read-only real backpack window.
    ui2::aim::draw();
    // PR-3A: read-only worn/wielded equipment and nested containers.
    ui2::gear::draw();
    // PR-4A: recipe browser with a direct path into the existing craft flow.
    ui2::craft::draw();
    // PR-5A: read-only zone inspector with a deferred enable toggle.
    ui2::zone::draw();
    // Message history viewer with a mouse-driven list and detail pane.
    ui2::messages::draw();
    // World inspect: non-modal map selection plus a small action panel.
    ui2::world::draw();
    // P3 Missions: read-only mission tracker with instant active-mission switching.
    ui2::missions::draw();
    // P3 Trade: read-only trade valuation preview with legacy popup handoff.
    ui2::trade::draw();
    // P3 Vehicle: read-only nearby vehicle overview.
    ui2::veh::draw();
    // Stage 3 Character: read-only stats, skills, perks/traits, bionics, and effects.
    ui2::character::draw();
    // PR-6A: hover card for the tile under the game cursor.
    ui2::mapview::draw();
}

} // namespace ui2

#endif // TILES

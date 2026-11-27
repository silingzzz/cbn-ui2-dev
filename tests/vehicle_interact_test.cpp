#include "../src/map/map.h"
#include "../src/vehicle/veh_interact.h"
#include "../src/vehicle/vehicle_part.h"
#include "../src/vehicle/vpart_position.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_imgui.h"
#include "catch/catch.hpp"
#include "coordinates.h"
#include "game.h"
#include "inventory.h"
#include "item.h"
#include "map_helpers.h"
#include "player_activity.h"
#include "player_helpers.h"
#include "requirements.h"
#include "state_helpers.h"
#include "type_id.h"
#include "vehicle/veh_type.h"
#include "vehicle/vehicle.h"
#include "vehicle/vpart_range.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

static const activity_id ACT_VEHICLE("ACT_VEHICLE");
static const trait_id trait_DEBUG_HS("DEBUG_HS");

static void test_repair(std::vector<detached_ptr<item>>& tools, bool expect_craftable) {

    const tripoint_bub_ms test_origin(60, 60, 0);
    g->u.setpos(test_origin);
    g->u.wear_item(item::spawn("backpack"), false);
    for (detached_ptr<item>& gear : tools) { g->u.i_add(std::move(gear)); }

    const tripoint_bub_ms vehicle_origin = test_origin + tripoint_rel_ms::south_east();
    vehicle* veh_ptr =
        get_map().add_vehicle(vproto_id("bicycle"), vehicle_origin, -90_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);
    // Find the frame at the origin.
    vehicle_part* origin_frame = nullptr;
    for (vehicle_part* part : veh_ptr->get_parts_at(vehicle_origin, "", part_status_flag::any)) {
        if (part->info().location == "structure") {
            origin_frame = part;
            break;
        }
    }
    REQUIRE(origin_frame != nullptr);
    REQUIRE(origin_frame->hp() == origin_frame->info().durability);
    veh_ptr->mod_hp(*origin_frame, -100);
    REQUIRE(origin_frame->hp() < origin_frame->info().durability);

    const vpart_info& vp = origin_frame->info();
    // Assertions about frame part?

    requirement_data reqs = vp.repair_requirements();
    // Bust cache on crafting_inventory()
    g->u.mod_moves(1);
    inventory crafting_inv = g->u.crafting_inventory();
    bool can_repair = vp.repair_requirements().can_make_with_inventory(
        g->u.crafting_inventory(), is_crafting_component);
    CHECK(can_repair == expect_craftable);
}

TEST_CASE("repair_vehicle_part") {
    clear_all_state();
    const time_point bday = calendar::start_of_cataclysm;
    SECTION("welder") {
        std::vector<detached_ptr<item>> tools;
        tools.push_back(item::spawn("welder", bday, 500));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, true);
    }
    SECTION("UPS_modded_welder") {
        std::vector<detached_ptr<item>> tools;
        detached_ptr<item> welder = item::spawn("welder", bday, 0);
        welder->put_in(item::spawn("battery_ups"));
        tools.push_back(std::move(welder));
        tools.push_back(item::spawn("UPS_off", bday, 500));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, true);
    }
    SECTION("welder_missing_goggles") {
        std::vector<detached_ptr<item>> tools;
        tools.push_back(item::spawn("welder", bday, 500));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, false);
    }
    SECTION("welder_missing_charge") {
        std::vector<detached_ptr<item>> tools;
        tools.push_back(item::spawn("welder", bday, 5));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, false);
    }
    SECTION("UPS_modded_welder_missing_charges") {
        std::vector<detached_ptr<item>> tools;
        detached_ptr<item> welder = item::spawn("welder", bday, 0);
        welder->put_in(item::spawn("battery_ups"));
        tools.push_back(std::move(welder));
        tools.push_back(item::spawn("UPS_off", bday, 5));
        tools.push_back(item::spawn("goggles_welding"));
        tools.push_back(item::spawn("material_aluminium_ingot", bday, 10));
        test_repair(tools, false);
    }
}

TEST_CASE("debug_hammerspace_installs_full_vehicle_battery", "[vehicle][veh_interact]") {
    clear_all_state();

    map& here = get_map();
    avatar& you = get_avatar();
    clear_avatar();
    you.toggle_trait(trait_DEBUG_HS);
    you.set_body();

    const tripoint_bub_ms vehicle_origin(60, 60, 0);
    you.setpos(vehicle_origin + point_south);

    vehicle* veh_ptr = here.add_vehicle(vproto_id("bicycle"), vehicle_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    const auto install_part_id = vpart_id("storage_battery");
    const auto reference_part_index = 0;
    const auto reference_part = &veh_ptr->part(reference_part_index);
    const auto reference_pos = map_local_to_abs(here, veh_ptr->bub_part_location(*reference_part));

    you.assign_activity(ACT_VEHICLE, 1, static_cast<int>('i'));
    you.activity->values =
        {reference_pos.x(), reference_pos.y(), reference_pos.z(), 0, 0, 0, reference_part_index};
    you.activity->str_values.push_back(install_part_id.str());
    for (const tripoint_abs_ms& p : veh_ptr->get_points(true)) {
        you.activity->coord_set.insert(p);
    }

    veh_interact::complete_vehicle(you);

    const auto all_parts = veh_ptr->get_all_parts();
    const auto installed_battery = std::find_if(
        all_parts.begin(), all_parts.end(), [&install_part_id](const vpart_reference& part) {
            return part.info().get_id() == install_part_id;
        });

    REQUIRE(installed_battery != all_parts.end());
    CHECK(installed_battery->part().ammo_remaining() == installed_battery->part().ammo_capacity());
}

#if defined( TILES )
TEST_CASE("ui2_vehicle_select_part_single_and_legacy_popup_scope", "[ui][vehicle][mission]") {
    clear_all_state();
    const bool was_open = cata_imgui::is_open();
    if (!was_open) {
        cata_imgui::toggle();
    }
    REQUIRE(cata_imgui::is_open());

    map& here = get_map();
    const tripoint_bub_ms vehicle_origin(60, 60, 0);
    vehicle* veh_ptr = here.add_vehicle(vproto_id("bicycle"), vehicle_origin, 0_degrees, 0, 0);
    REQUIRE(veh_ptr != nullptr);

    // Matching 0 parts or 1 part must return without opening overview or hiding UI2.
    veh_interact::select_part(
        *veh_ptr,
        [](const vehicle_part&) { return false; },
        "No part");
    CHECK(cata_imgui::is_open());

    // Find a part id that occurs exactly once on the bicycle so select_part
    // auto-selects it without opening overview or hiding UI2.
    vpart_id unique_part;
    for (const vpart_reference& vpr : veh_ptr->get_all_parts()) {
        const vpart_id pid = vpr.info().get_id();
        int count = 0;
        for (const vpart_reference& other : veh_ptr->get_all_parts()) {
            if (other.info().get_id() == pid) {
                ++count;
            }
        }
        if (count == 1) {
            unique_part = pid;
            break;
        }
    }
    REQUIRE(unique_part != vpart_id::NULL_ID());

    const vehicle_part& chosen = veh_interact::select_part(
        *veh_ptr,
        [&unique_part](const vehicle_part& pt) { return pt.info().get_id() == unique_part; },
        "Unique part");
    CHECK(chosen.info().get_id() == unique_part);
    CHECK(cata_imgui::is_open());

    // Nested legacy_popup_scope (e.g., veh_interact::run -> select_part / list_missions)
    // stays hidden until the outermost scope exits.
    {
        cata_imgui::legacy_popup_scope outer_gate;
        CHECK_FALSE(cata_imgui::is_open());
        {
            cata_imgui::legacy_popup_scope inner_gate;
            CHECK_FALSE(cata_imgui::is_open());
        }
        CHECK_FALSE(cata_imgui::is_open());
    }
    CHECK(cata_imgui::is_open());

    if (cata_imgui::is_open() != was_open) {
        cata_imgui::toggle();
    }
}
#endif

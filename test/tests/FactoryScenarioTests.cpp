/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Writes the trailer scenarios in data/factory/scenarios when FT_SCENARIO_OUT names a
// directory: `FT_SCENARIO_OUT=$PWD/../data/factory/scenarios ./OpenRCT2Tests --gtest_filter='FactoryScenarios.*'`.
// Each scenario is built on a fresh flat map from actions and plain state, so the files can be regenerated.

#include "TestData.h"

#include <cstdlib>
#include <filesystem>
#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/footpath/FootpathPlaceAction.h>
#include <openrct2/actions/park/ParkEntrancePlaceAction.h>
#include <openrct2/actions/peep/PeepSpawnPlaceAction.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/Objectives.h>
#include <openrct2/factory/Planet.h>
#include <openrct2/factory/WorldManager.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/management/Research.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/scenario/ScenarioObjective.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapOwnership.h>
#include <openrct2/world/ParkData.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <string>
#include <vector>

using namespace OpenRCT2;

namespace
{
    constexpr int32_t kSize = 64;
    constexpr int32_t kRoadY = kSize / 2;

    ObjectEntryIndex load(const std::string& id)
    {
        auto& objectManager = GetContext()->GetObjectManager();
        auto* object = objectManager.LoadObject(id);
        return object != nullptr ? objectManager.GetLoadedObjectEntryIndex(object) : kObjectEntryIndexNull;
    }

    ObjectEntryIndex proto(const std::string& name)
    {
        return load("factory-tour.factory_prototype." + name);
    }

    int32_t groundZ(int32_t tx, int32_t ty)
    {
        return MapGetSurfaceElementAt(TileCoordsXY{ tx, ty })->getBaseZ();
    }

    // A fresh flat park: owned land east of an entrance at (8, kRoadY), a road in from the west edge with a guest
    // spawn, and a short avenue inside.
    void buildPark(const std::string& name, const std::string& details)
    {
        auto& gameState = getGameState();
        gameStateInitAll(gameState, { kSize, kSize });
        gameState.cheats.sandboxMode = true;
        gameState.scenarioOptions.name = name;
        gameState.scenarioOptions.details = details;
        gameState.park.name = name;
        gameState.park.cash = 25000.00_GBP;
        gameState.scenarioOptions.initialCash = gameState.park.cash;
        gameState.park.bankLoan = 0;
        gameState.park.maxBankLoan = 20000.00_GBP;
        gameState.researchProgressStage = RESEARCH_STAGE_FINISHED_ALL; // rides not gated by technologies are invented
        gameState.park.flags.unset(ParkFlag::noMoney);
        // Open on the entrance and the factory site.
        gameState.savedView = Translate3DTo2DWithZ(
            0, CoordsXYZ{ 20 * kCoordsXYStep, kRoadY * kCoordsXYStep, 14 * kCoordsZStep });
        gameState.savedViewZoom = ZoomLevel{ 0 };
        gameState.savedViewRotation = 0;
        for (int32_t y = 1; y < kSize - 1; y++)
            for (int32_t x = 9; x < kSize - 1; x++)
                MapGetSurfaceElementAt(TileCoordsXY{ x, y })->setOwnership(OwnershipFlags{ OwnershipFlag::landOwned });

        const auto surface = load("rct2.footpath_surface.tarmac");
        const auto railings = load("rct2.footpath_railings.wood");
        const auto entrance = load("rct2.park_entrance.pkent1");
        ASSERT_NE(surface, kObjectEntryIndexNull);
        ASSERT_NE(entrance, kObjectEntryIndexNull);
        const int32_t z = groundZ(8, kRoadY);
        auto place = GameActions::ParkEntrancePlaceAction(
            CoordsXYZD{ 8 * kCoordsXYStep, kRoadY * kCoordsXYStep, z, 0 }, surface, entrance, false);
        ASSERT_EQ(GameActions::ExecuteNested(&place, gameState).error, GameActions::Status::ok);
        for (int32_t x = 1; x <= 16; x++)
        {
            if (x == 8)
                continue;
            auto path = GameActions::FootpathPlaceAction(
                CoordsXYZ{ x * kCoordsXYStep, kRoadY * kCoordsXYStep, z }, {}, surface, railings);
            ASSERT_EQ(GameActions::ExecuteNested(&path, gameState).error, GameActions::Status::ok) << x;
        }
        auto spawn = GameActions::PeepSpawnPlaceAction(CoordsXYZD{ kCoordsXYStep + 16, kRoadY * kCoordsXYStep + 16, z, 2 });
        ASSERT_EQ(GameActions::ExecuteNested(&spawn, gameState).error, GameActions::Status::ok);
    }

    void paintOre(const char* ore, int32_t cx, int32_t cy, int32_t radius, uint32_t amount)
    {
        auto& gameState = getGameState();
        const auto entry = proto(ore);
        ASSERT_NE(entry, kObjectEntryIndexNull);
        gameState.factory.ore.resize(gameState.mapSize);
        for (int32_t dy = -radius; dy <= radius; dy++)
            for (int32_t dx = -radius; dx <= radius; dx++)
                if (dx * dx + dy * dy <= radius * radius)
                    gameState.factory.ore.set({ cx + dx, cy + dy }, { entry, 0, amount });
        gameState.factory.topologyVersion++;
    }

    void save(const char* fileName)
    {
        const auto dir = std::getenv("FT_SCENARIO_OUT");
        Factory::Worlds::activate(Factory::Worlds::kPrimaryWorld);
        const auto path = (std::filesystem::path(dir) / fileName).string();
        ASSERT_EQ(ScenarioSave(getGameState(), path, SaveFlags{ SaveFlag::scenario }), 1) << path;
    }

    const std::vector<std::string> kBasics = {
        "belt_basic",
        "inserter_basic",
        "chest_wooden",
        "burner_drill",
        "stone_furnace",
        "iron_ore",
        "iron_plate",
        "iron_plate_smelting",
        "coal",
        "iron_ore_patch",
        "coal_patch",
        "iron_gear",
        "iron_gear_recipe",
        "assembling_machine",
        "burner_generator",
        "small_pole",
        "underground_belt_basic",
        "splitter_basic",
        "warehouse_depot",
        "export_depot",
        "copper_ore_patch",
        "copper_ore",
        "copper_plate",
        "copper_plate_smelting",
    };
} // namespace

TEST(FactoryScenarios, WriteTrailerScenarios)
{
    if (std::getenv("FT_SCENARIO_OUT") == nullptr)
        GTEST_SKIP() << "FT_SCENARIO_OUT not set";
    gOpenRCT2Headless = true;
    gOpenRCT2NoGraphics = true;
    auto context = CreateContext();
    ASSERT_TRUE(context->Initialise());
    ASSERT_TRUE(context->LoadParkFromFile(TestData::GetParkPath("tile-element-tests.sv6")));
    GameLoadInit();
    for (const auto& name : kBasics)
        ASSERT_NE(proto(name), kObjectEntryIndexNull) << name;

    // 1. First Shift: get a smelting line going.
    buildPark(
        "Factory Tour: First Shift",
        "The park’s new owners bought it for the iron underneath. Mine it, smelt it and turn out 500 iron plates.");
    paintOre("iron_ore_patch", 24, 20, 4, 800);
    paintOre("coal_patch", 30, 26, 3, 800);
    paintOre("copper_ore_patch", 40, 18, 3, 600);
    {
        auto& objective = getGameState().scenarioOptions.objective;
        objective.Type = Scenario::ObjectiveType::produceItemsBy;
        objective.Year = 2;
        objective.NumGuests = proto("iron_plate");
        objective.Currency = 500;
    }
    save("Factory Tour - First Shift.park");

    // 2. Tour Season: the factory becomes the attraction.
    for (const auto& name : { "factory_model", "factory_model_recipe", "gear_keyring", "gear_keyring_recipe" })
        ASSERT_NE(proto(name), kObjectEntryIndexNull) << name;
    ASSERT_NE(load("factory-tour.ride.tour_tram"), kObjectEntryIndexNull);
    ASSERT_NE(load("factory-tour.ride.gift_shop"), kObjectEntryIndexNull);
    ASSERT_NE(load("factory-tour.footpath_surface.exhibit"), kObjectEntryIndexNull);
    buildPark(
        "Factory Tour: Tour Season",
        "Visitors will pay to watch machines work. Build tours and exhibit walkways and show 150 guests around.");
    paintOre("iron_ore_patch", 30, 22, 4, 1000);
    paintOre("coal_patch", 36, 28, 3, 1000);
    getGameState().park.flags.set(ParkFlag::factoryAffectsRating, true);
    {
        auto& objective = getGameState().scenarioOptions.objective;
        objective.Type = Scenario::ObjectiveType::guestsTouredFactory;
        objective.Year = 3;
        objective.NumGuests = 150;
    }
    save("Factory Tour - Tour Season.park");

    // 3. Twin Worlds: research, rockets and a weird dimension full of void crystal.
    for (const auto& name : { "research_kit",
                              "research_kit_recipe",
                              "lab",
                              "tech_logistics",
                              "tech_steel",
                              "tech_electronics",
                              "tech_freight",
                              "tech_interworld",
                              "steel_plate",
                              "steel_smelting",
                              "engineering_kit",
                              "engineering_kit_recipe",
                              "copper_cable",
                              "copper_cable_recipe",
                              "electronic_circuit",
                              "electronic_circuit_recipe",
                              "rocket_part",
                              "rocket_part_recipe",
                              "launch_pad",
                              "landing_pad",
                              "void_crystal",
                              "void_crystal_patch",
                              "void_lens",
                              "void_lens_recipe",
                              "freight_loader",
                              "freight_unloader" })
        ASSERT_NE(proto(name), kObjectEntryIndexNull) << name;
    ASSERT_NE(load("rct2.terrain_surface.martian"), kObjectEntryIndexNull);
    buildPark(
        "Factory Tour: Twin Worlds", "Research rocketry, open a route to the weird dimension and ship home 25 void lenses.");
    paintOre("iron_ore_patch", 26, 20, 4, 1500);
    paintOre("coal_patch", 32, 26, 3, 1500);
    paintOre("copper_ore_patch", 40, 20, 3, 1200);
    {
        auto& objective = getGameState().scenarioOptions.objective;
        objective.Type = Scenario::ObjectiveType::produceItemsBy;
        objective.Year = 4;
        objective.NumGuests = proto("void_lens");
        objective.Currency = 25;
    }
    const auto weird = Factory::Worlds::create({ 48, 48 });
    ASSERT_EQ(weird, 1);
    {
        Factory::Worlds::Scope inWeird(weird);
        Factory::applyWorldPreset(getGameState(), Factory::WorldPreset::weird);
    }
    save("Factory Tour - Twin Worlds.park");

    // The files load back with their objectives (and Twin Worlds with its second world).
    const auto dir = std::filesystem::path(std::getenv("FT_SCENARIO_OUT"));
    ASSERT_TRUE(context->LoadParkFromFile((dir / "Factory Tour - Twin Worlds.park").string()));
    GameLoadInit();
    EXPECT_EQ(getGameState().scenarioOptions.objective.Type, Scenario::ObjectiveType::produceItemsBy);
    EXPECT_EQ(Factory::Worlds::count(), 2u);
    EXPECT_EQ(Factory::Worlds::state(1).factory.parkExt.planet.preset, static_cast<uint8_t>(Factory::WorldPreset::weird));
    Factory::Worlds::activate(Factory::Worlds::kPrimaryWorld);
    Factory::Worlds::adoptActiveAsPrimary();
}

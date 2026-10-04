/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The fork replay pack: recorded games that exercise every fork action over a running
// factory, played back with the sync checksum compared at every recorded interval (like upstream's ReplayTests).
//
// Re-record after an intended simulation or save-format change:
//   FT_RECORD_REPLAYS=1 ./OpenRCT2Tests --gtest_filter='FactoryReplayTests.Record*'   (from build/)
// which rewrites test/tests/testdata/factory-replays/*.parkrep.

#include "TestData.h"

#include <cstdlib>
#include <functional>
#include <gtest/gtest.h>
#include <map>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/ReplayManager.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/core/File.h>
#include <openrct2/core/FileScanner.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/WorldManager.h>
#include <openrct2/factory/actions/FactoryCreateWorldAction.h>
#include <openrct2/factory/actions/FactoryDamageAction.h>
#include <openrct2/factory/actions/FactoryPlaceAction.h>
#include <openrct2/factory/actions/FactoryPlaceBeltLineAction.h>
#include <openrct2/factory/actions/FactoryRemoveAction.h>
#include <openrct2/factory/actions/FactoryRotateAction.h>
#include <openrct2/factory/actions/FactorySetFilterAction.h>
#include <openrct2/factory/actions/FactorySetOreAction.h>
#include <openrct2/factory/actions/FactorySetParkOptionAction.h>
#include <openrct2/factory/actions/FactorySetRecipeAction.h>
#include <openrct2/factory/actions/FactoryThreatSpawnAction.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/tile_element/FactoryElement.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <string>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;
using namespace OpenRCT2::GameActions;

namespace
{
    std::string ReplayDir()
    {
        return Path::GetAbsolute(Path::Combine(TestData::GetBasePath(), u8"factory-replays"));
    }

    /**
     * Records one replay: loads the test park, flattens the work area, lets `setup` stock the initial factory
     * directly (the replay embeds the park as it is when recording starts), then runs `ticks` game ticks executing
     * `script[tick]` before each one. Actions go through GameActions::Execute so the replay manager records them.
     */
    class Recorder
    {
    public:
        static constexpr int32_t kX0 = 3;
        static constexpr int32_t kX1 = 17;
        static constexpr int32_t kY0 = 7;
        static constexpr int32_t kY1 = 16;

        explicit Recorder(IContext& context)
            : _context(context)
        {
        }

        ObjectEntryIndex Load(const char* name)
        {
            auto& objectManager = _context.GetObjectManager();
            auto* object = objectManager.LoadObject(std::string("factory-tour.factory_prototype.") + name);
            EXPECT_NE(object, nullptr) << name;
            return object != nullptr ? objectManager.GetLoadedObjectEntryIndex(object) : kObjectEntryIndexNull;
        }

        int32_t GroundZ() const
        {
            return MapGetSurfaceElementAt(TileCoordsXY{ kX0, kY0 })->getBaseZ();
        }

        CoordsXYZ At(int32_t tx, int32_t ty) const
        {
            return CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ() };
        }

        void PrepareGround()
        {
            const int32_t z = GroundZ();
            for (int32_t ty = kY0; ty <= kY1; ty++)
            {
                for (int32_t tx = kX0; tx <= kX1; tx++)
                {
                    while (auto* element = findFactoryElement(At(tx, ty), true))
                        removeElement(getGameState(), *element, At(tx, ty));
                    auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, ty });
                    surface->setSlope(0);
                    surface->setBaseZ(z);
                    surface->setClearanceZ(z);
                    surface->setWaterHeight(0);
                }
            }
            getGameState().factory.reset();
            getGameState().factory.ore.resize(getGameState().mapSize);
            getGameState().cheats.sandboxMode = true; // the park does not own all of this land
        }

        FactoryElement* PlaceDirect(int32_t tx, int32_t ty, Direction dir, ObjectEntryIndex entry)
        {
            return placeElement(getGameState(), At(tx, ty), dir, entry, false);
        }

        template<typename TAction>
        void Do(TAction action)
        {
            auto result = Execute(&action, getGameState());
            EXPECT_EQ(result.error, Status::ok) << action.GetName() << " at tick " << _tick;
        }

        void Record(const std::string& name, int32_t ticks, const std::map<int32_t, std::function<void()>>& script)
        {
            auto* replayManager = _context.GetReplayManager();
            const auto path = Path::Combine(ReplayDir(), name + u8".parkrep");
            ASSERT_TRUE(replayManager->StartRecording(path, static_cast<uint32_t>(ticks)));
            for (_tick = 0; _tick < ticks; _tick++)
            {
                auto it = script.find(_tick);
                if (it != script.end())
                {
                    // Playback checks a tick's checksum, then runs that tick's actions at once, then the rest of the
                    // tick. Mirror it: take the checksum now (the tick's own Update() finds it already taken) and run
                    // the actions as if inside the tick, so they execute (and are recorded) immediately instead of
                    // being queued for the end of the tick.
                    replayManager->Update();
                    gInUpdateCode = true;
                    it->second();
                    gInUpdateCode = false;
                }
                gameStateUpdateLogic();
            }
            if (replayManager->IsRecording())
                replayManager->StopRecording();
            EXPECT_TRUE(File::Exists(path));
        }

    private:
        IContext& _context;
        int32_t _tick = 0;
    };

    std::unique_ptr<IContext> OpenTestPark()
    {
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        auto context = CreateContext();
        if (!context->Initialise() || !context->LoadParkFromFile(TestData::GetParkPath("tile-element-tests.sv6")))
            return nullptr;
        GameLoadInit();
        return context;
    }

    bool Recording()
    {
        return std::getenv("FT_RECORD_REPLAYS") != nullptr;
    }
} // namespace

TEST(FactoryReplayTests, RecordLogistics)
{
    if (!Recording())
        GTEST_SKIP() << "FT_RECORD_REPLAYS not set";
    auto context = OpenTestPark();
    ASSERT_NE(context, nullptr);
    Recorder r(*context);
    const auto belt = r.Load("belt_basic");
    const auto inserter = r.Load("inserter_basic");
    const auto chest = r.Load("chest_wooden");
    const auto splitter = r.Load("splitter_basic");
    const auto underground = r.Load("underground_belt_basic");
    const auto plate = r.Load("iron_plate");
    const auto gear = r.Load("iron_gear");
    r.PrepareGround();
    // A chest of plates and gears at the west end.
    auto* source = r.PlaceDirect(4, 10, 2, chest);
    ASSERT_NE(source, nullptr);
    auto& slots = getGameState().factory.containers.get(source->getRecordId())->slots;
    for (size_t i = 0; i < slots.size(); i++)
        slots[i] = { i % 2 == 0 ? plate : gear, 100 };

    const Direction east = 2;
    r.Record(
        "FactoryLogistics", 2400,
        {
            { 2, [&] { r.Do(FactoryPlaceAction(r.At(5, 10), east, inserter)); } },
            { 4, [&] { r.Do(FactoryPlaceBeltLineAction(r.At(6, 10), r.At(9, 10), east, belt)); } },
            // An underground pair passing under a crossing belt.
            { 6, [&] { r.Do(FactoryPlaceAction(r.At(10, 10), east, underground)); } },
            { 7, [&] { r.Do(FactoryPlaceAction(r.At(11, 10), 1, belt)); } },
            { 8, [&] { r.Do(FactoryPlaceAction(r.At(12, 10), east, underground)); } },
            { 10, [&] { r.Do(FactoryPlaceAction(r.At(13, 10), east, splitter)); } },
            { 12, [&] { r.Do(FactoryPlaceBeltLineAction(r.At(14, 10), r.At(16, 10), east, belt)); } },
            { 14, [&] { r.Do(FactoryPlaceBeltLineAction(r.At(14, 9), r.At(16, 9), east, belt)); } },
            { 16, [&] { r.Do(FactoryPlaceAction(r.At(17, 10), east, chest)); } },
            { 300, [&] { r.Do(FactorySetFilterAction(r.At(13, 10), gear, kSplitterPriorityNone, kSplitterPriorityRight)); } },
            // Break and mend the line, then turn the source inserter away and back.
            { 900, [&] { r.Do(FactoryRemoveAction(r.At(8, 10))); } },
            { 1000, [&] { r.Do(FactoryPlaceAction(r.At(8, 10), east, belt)); } },
            { 1200, [&] { r.Do(FactoryRotateAction(r.At(5, 10))); } },
            { 1300,
              [&] {
                  for (int i = 0; i < 3; i++)
                      r.Do(FactoryRotateAction(r.At(5, 10)));
              } },
            { 1800, [&] { r.Do(FactorySetFilterAction(r.At(13, 10), kObjectEntryIndexNull, kSplitterPriorityLeft, 0)); } },
        });
}

TEST(FactoryReplayTests, RecordProduction)
{
    if (!Recording())
        GTEST_SKIP() << "FT_RECORD_REPLAYS not set";
    auto context = OpenTestPark();
    ASSERT_NE(context, nullptr);
    Recorder r(*context);
    const auto belt = r.Load("belt_basic");
    const auto inserter = r.Load("inserter_basic");
    const auto chest = r.Load("chest_wooden");
    const auto drill = r.Load("burner_drill");
    const auto furnace = r.Load("stone_furnace");
    const auto orePatch = r.Load("iron_ore_patch");
    const auto coal = r.Load("coal");
    r.Load("iron_ore");
    r.Load("iron_plate");
    r.Load("iron_plate_smelting");
    r.PrepareGround();
    // Two coal chests: one feeds the drill, one the furnace.
    for (auto [tx, ty] : { std::pair{ 7, 15 }, std::pair{ 12, 15 } })
    {
        auto* coalChest = r.PlaceDirect(tx, ty, 2, chest);
        ASSERT_NE(coalChest, nullptr);
        for (auto& slot : getGameState().factory.containers.get(coalChest->getRecordId())->slots)
            slot = { coal, 50 };
    }

    const Direction east = 2;
    const Direction north = 3;
    r.Record(
        "FactoryProduction", 3600,
        {
            { 2, [&] { r.Do(FactorySetOreAction(MapRange{ r.At(6, 12), r.At(8, 14) }, orePatch, 40)); } },
            { 4, [&] { r.Do(FactoryPlaceAction(r.At(7, 13), east, drill)); } },
            { 5, [&] { r.Do(FactoryPlaceAction(r.At(7, 14), north, inserter)); } },
            { 6, [&] { r.Do(FactoryPlaceBeltLineAction(r.At(8, 13), r.At(10, 13), east, belt)); } },
            { 8, [&] { r.Do(FactoryPlaceAction(r.At(11, 13), east, inserter)); } },
            { 9, [&] { r.Do(FactoryPlaceAction(r.At(12, 13), east, furnace)); } },
            { 10, [&] { r.Do(FactoryPlaceAction(r.At(12, 14), north, inserter)); } },
            { 11, [&] { r.Do(FactoryPlaceAction(r.At(13, 13), east, inserter)); } },
            { 12, [&] { r.Do(FactoryPlaceAction(r.At(14, 13), east, chest)); } },
            // Remove and rebuild the furnace mid-run (its contents are lost).
            { 2000, [&] { r.Do(FactoryRemoveAction(r.At(12, 13))); } },
            { 2100, [&] { r.Do(FactoryPlaceAction(r.At(12, 13), east, furnace)); } },
        });
}

TEST(FactoryReplayTests, RecordSteamAndPower)
{
    if (!Recording())
        GTEST_SKIP() << "FT_RECORD_REPLAYS not set";
    auto context = OpenTestPark();
    ASSERT_NE(context, nullptr);
    Recorder r(*context);
    const auto inserter = r.Load("inserter_basic");
    const auto chest = r.Load("chest_wooden");
    const auto pipe = r.Load("pipe_basic");
    const auto pump = r.Load("offshore_pump");
    const auto boiler = r.Load("boiler");
    const auto engine = r.Load("steam_engine");
    const auto pole = r.Load("small_pole");
    const auto assembler = r.Load("assembling_machine");
    const auto gearRecipe = r.Load("iron_gear_recipe");
    const auto coal = r.Load("coal");
    const auto plate = r.Load("iron_plate");
    r.Load("iron_gear");
    r.Load("water");
    r.Load("steam");
    r.PrepareGround();
    auto* shore = MapGetSurfaceElementAt(TileCoordsXY{ 3, 10 });
    shore->setWaterHeight(shore->getBaseZ() + 2 * kCoordsZStep);
    auto stock = [&](int32_t tx, int32_t ty, ObjectEntryIndex item, uint16_t count) {
        auto* element = r.PlaceDirect(tx, ty, 2, chest);
        ASSERT_NE(element, nullptr);
        for (auto& slot : getGameState().factory.containers.get(element->getRecordId())->slots)
            slot = { item, count };
    };
    stock(7, 12, coal, 50);
    stock(12, 9, plate, 100);

    const Direction east = 2;
    const Direction north = 3;
    const Direction south = 1;
    const Direction west = 0;
    r.Record(
        "FactorySteamAndPower", 3600,
        {
            { 2, [&] { r.Do(FactoryPlaceAction(r.At(4, 10), east, pump)); } },
            { 3, [&] { r.Do(FactoryPlaceAction(r.At(5, 10), east, pipe)); } },
            { 4, [&] { r.Do(FactoryPlaceAction(r.At(6, 10), east, pipe)); } },
            { 5, [&] { r.Do(FactoryPlaceAction(r.At(7, 10), north, boiler)); } },
            { 6, [&] { r.Do(FactoryPlaceAction(r.At(7, 11), north, inserter)); } },
            { 7, [&] { r.Do(FactoryPlaceAction(r.At(7, 9), north, engine)); } },
            { 8, [&] { r.Do(FactoryPlaceAction(r.At(8, 9), north, pole)); } },
            { 9, [&] { r.Do(FactoryPlaceAction(r.At(10, 9), east, assembler)); } },
            { 10, [&] { r.Do(FactorySetRecipeAction(r.At(10, 9), gearRecipe)); } },
            { 11, [&] { r.Do(FactoryPlaceAction(r.At(11, 9), west, inserter)); } },
            { 12, [&] { r.Do(FactoryPlaceAction(r.At(10, 10), south, inserter)); } },
            { 13, [&] { r.Do(FactoryPlaceAction(r.At(10, 11), east, chest)); } },
            // Cut the water line for a while, then mend it.
            { 1500, [&] { r.Do(FactoryRemoveAction(r.At(5, 10))); } },
            { 2200, [&] { r.Do(FactoryPlaceAction(r.At(5, 10), east, pipe)); } },
        });
}

TEST(FactoryReplayTests, RecordCombatAndResearch)
{
    if (!Recording())
        GTEST_SKIP() << "FT_RECORD_REPLAYS not set";
    auto context = OpenTestPark();
    ASSERT_NE(context, nullptr);
    Recorder r(*context);
    const auto inserter = r.Load("inserter_basic");
    const auto chest = r.Load("chest_wooden");
    const auto pole = r.Load("small_pole");
    const auto generator = r.Load("burner_generator");
    const auto lab = r.Load("lab");
    const auto kit = r.Load("research_kit");
    const auto logistics = r.Load("tech_logistics");
    const auto turret = r.Load("bolt_turret");
    const auto magazine = r.Load("bolt_magazine");
    const auto crawler = r.Load("scrap_crawler");
    const auto furnace = r.Load("stone_furnace");
    const auto coal = r.Load("coal");
    r.Load("splitter_basic");
    r.Load("underground_belt_basic");
    r.PrepareGround();
    auto stock = [&](int32_t tx, int32_t ty, ObjectEntryIndex item, uint16_t count) {
        auto* element = r.PlaceDirect(tx, ty, 2, chest);
        ASSERT_NE(element, nullptr);
        for (auto& slot : getGameState().factory.containers.get(element->getRecordId())->slots)
            slot = { item, count };
    };
    stock(6, 10, coal, 50);
    stock(9, 13, kit, 100);
    stock(12, 13, magazine, 20);
    auto worldX = [&](int32_t tx) { return tx * kCoordsXYStep + 16; };

    const Direction east = 2;
    const Direction north = 3;
    r.Record(
        "FactoryCombatAndResearch", 3600,
        {
            { 2, [&] { r.Do(FactoryPlaceAction(r.At(8, 10), east, generator)); } },
            { 3, [&] { r.Do(FactoryPlaceAction(r.At(7, 10), east, inserter)); } },
            { 4, [&] { r.Do(FactoryPlaceAction(r.At(9, 10), east, pole)); } },
            { 5, [&] { r.Do(FactoryPlaceAction(r.At(9, 11), east, lab)); } },
            { 6, [&] { r.Do(FactoryPlaceAction(r.At(9, 12), north, inserter)); } },
            { 7, [&] { r.Do(FactorySetParkOptionAction(FactoryParkOption::researchTarget, logistics)); } },
            { 8, [&] { r.Do(FactoryPlaceAction(r.At(12, 11), east, turret)); } },
            { 9, [&] { r.Do(FactoryPlaceAction(r.At(12, 12), north, inserter)); } },
            { 10, [&] { r.Do(FactoryPlaceAction(r.At(5, 6), east, furnace)); } },
            // Threats from two sides: some reach the furnace, the turret shoots those that pass it.
            { 300, [&] { r.Do(FactoryThreatSpawnAction(crawler, worldX(2), worldX(6))); } },
            { 600, [&] { r.Do(FactoryThreatSpawnAction(crawler, worldX(18), worldX(11))); } },
            { 900, [&] { r.Do(FactoryThreatSpawnAction(crawler, worldX(17), worldX(14))); } },
            { 1500, [&] { r.Do(FactoryDamageAction(0, 0, 50, 3)); } },
            { 2000, [&] { r.Do(FactoryThreatSpawnAction(crawler, worldX(2), worldX(14))); } },
        });
    // The recording exercised research, turret kills and machine damage.
    const auto& state = getGameState().factory;
    EXPECT_GT(state.research.unitsDone(logistics) + (state.research.isResearched(logistics) ? 10u : 0u), 0u);
    EXPECT_LT(state.threats.aliveCount(), 4u);
    bool damaged = false;
    state.machines.forEach([&](RecordId, const MachineRecord& machine) {
        auto* proto = getPrototype(machine.entry);
        damaged |= proto != nullptr && machine.health < proto->getMachine().health;
    });
    EXPECT_TRUE(damaged);
}

TEST(FactoryReplayTests, RecordTwoWorlds)
{
    if (!Recording())
        GTEST_SKIP() << "FT_RECORD_REPLAYS not set";
    auto context = OpenTestPark();
    ASSERT_NE(context, nullptr);
    Recorder r(*context);
    const auto chest = r.Load("chest_wooden");
    const auto furnace = r.Load("stone_furnace");
    const auto crawler = r.Load("scrap_crawler");
    r.PrepareGround();
    namespace W = Factory::Worlds;
    // World 1's tiles at ground level of its own flat map; actions issued in its scope are stamped with it.
    auto at1 = [&](int32_t tx, int32_t ty) {
        const CoordsXY loc{ tx * kCoordsXYStep, ty * kCoordsXYStep };
        return CoordsXYZ{ loc, MapGetSurfaceElementAt(loc)->getBaseZ() };
    };
    auto inWorld1 = [&](auto&& fn) {
        W::Scope scope(1);
        fn();
    };
    const Direction east = 2;
    std::map<int32_t, std::function<void()>> script = {
        { 2, [&] { r.Do(FactoryCreateWorldAction(40, 0)); } },
        { 4, [&] { inWorld1([&] { r.Do(FactoryPlaceAction(at1(20, 20), east, furnace)); }); } },
        { 5,
          [&] {
              inWorld1([&] { r.Do(FactoryThreatSpawnAction(crawler, 12 * kCoordsXYStep + 16, 20 * kCoordsXYStep + 16)); });
          } },
        { 900, [&] { inWorld1([&] { r.Do(FactoryThreatSpawnAction(crawler, 28 * kCoordsXYStep, 14 * kCoordsXYStep)); }); } },
    };
    // World 0 builds at the same time.
    for (int32_t i = 0; i < 6; i++)
        script[10 + i] = [&r, chest, east, i] { r.Do(FactoryPlaceAction(r.At(6 + i, 10), east, chest)); };
    r.Record("FactoryTwoWorlds", 2400, script);
    EXPECT_EQ(W::count(), 2u);
    {
        W::Scope scope(1);
        // The crawlers reached the furnace and damaged it.
        bool damaged = false;
        getGameState().factory.machines.forEach(
            [&](RecordId, const MachineRecord& machine) { damaged |= machine.health < 300; });
        EXPECT_TRUE(damaged);
    }
    W::activate(W::kPrimaryWorld);
    W::adoptActiveAsPrimary();
}

TEST(FactoryReplayTests, ForkReplayPackPlaysBackInSync)
{
    gOpenRCT2Headless = true;
    gOpenRCT2NoGraphics = true;
    auto scanner = OpenRCT2::Path::scanDirectory(Path::Combine(ReplayDir(), u8"*.parkrep"), true);
    std::vector<std::string> files;
    while (scanner->next())
        files.push_back(Path::GetAbsolute(scanner->getPath()));
    ASSERT_GE(files.size(), 5u) << "fork replay pack missing from " << ReplayDir();
    for (const auto& file : files)
    {
        SCOPED_TRACE(file);
        auto context = CreateContext();
        ASSERT_TRUE(context->Initialise());
        auto* replayManager = context->GetReplayManager();
        ASSERT_NO_THROW(replayManager->StartPlayback(file));
        uint32_t ticks = 0;
        while (replayManager->IsReplaying())
        {
            gameStateUpdateLogic();
            ticks++;
            if (replayManager->IsPlaybackStateMismatching())
                break;
        }
        EXPECT_FALSE(replayManager->IsPlaybackStateMismatching());
        EXPECT_FALSE(replayManager->IsReplaying());
        EXPECT_GT(ticks, 1000u);
        // The replay really built a factory.
        EXPECT_GT(getGameState().factory.recordCount(), 5u);
    }
}

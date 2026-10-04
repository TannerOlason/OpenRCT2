/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Places factory elements on a real map and runs the simulation:
// chest -> inserter -> belt -> inserter -> chest, the M1 "hello conveyor" slice without the UI.

#include "TestData.h"

#include <cstdlib>
#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Date.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/ride/RideCreateAction.h>
#include <openrct2/actions/ride/RideDemolishAction.h>
#include <openrct2/actions/ride/RideEntranceExitPlaceAction.h>
#include <openrct2/actions/ride/RideSetStatusAction.h>
#include <openrct2/actions/track/TrackPlaceAction.h>
#include <openrct2/core/DataSerialiser.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/entity/Guest.h>
#include <openrct2/factory/Belts.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactorySerialisation.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Fluids.h>
#include <openrct2/factory/GuestFactory.h>
#include <openrct2/factory/Market.h>
#include <openrct2/factory/Objectives.h>
#include <openrct2/factory/ParkExt.h>
#include <openrct2/factory/Pollution.h>
#include <openrct2/factory/RideRatingsFactory.h>
#include <openrct2/factory/SyncChecksum.h>
#include <openrct2/factory/actions/FactoryMarketSellAction.h>
#include <openrct2/factory/actions/FactorySetFilterAction.h>
#include <openrct2/factory/actions/FactorySetParkOptionAction.h>
#include <openrct2/management/Research.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/RideObject.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideConstruction.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/RideRatings.h>
#include <openrct2/ride/ShopItem.h>
#include <openrct2/ride/ted/TrackElemType.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/scenario/ScenarioObjective.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/Park.h>
#include <openrct2/world/tile_element/FactoryElement.h>
#include <openrct2/world/tile_element/SurfaceElement.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;

class FactoryTopologyTests : public testing::Test
{
protected:
    static void SetUpTestCase()
    {
        std::string parkPath = TestData::GetParkPath("tile-element-tests.sv6");
        gOpenRCT2Headless = true;
        gOpenRCT2NoGraphics = true;
        _context = CreateContext();
        ASSERT_TRUE(_context->Initialise());
        ASSERT_TRUE(_context->LoadParkFromFile(parkPath));
        GameLoadInit();

        auto& objectManager = _context->GetObjectManager();
        _plate = Index(objectManager.LoadObject("factory-tour.factory_prototype.iron_plate"));
        _belt = Index(objectManager.LoadObject("factory-tour.factory_prototype.belt_basic"));
        _inserter = Index(objectManager.LoadObject("factory-tour.factory_prototype.inserter_basic"));
        _chest = Index(objectManager.LoadObject("factory-tour.factory_prototype.chest_wooden"));
        _ironOrePatch = Index(objectManager.LoadObject("factory-tour.factory_prototype.iron_ore_patch"));
        _ironOre = Index(objectManager.LoadObject("factory-tour.factory_prototype.iron_ore"));
        _coal = Index(objectManager.LoadObject("factory-tour.factory_prototype.coal"));
        _drill = Index(objectManager.LoadObject("factory-tour.factory_prototype.burner_drill"));
        _furnace = Index(objectManager.LoadObject("factory-tour.factory_prototype.stone_furnace"));
        _smelting = Index(objectManager.LoadObject("factory-tour.factory_prototype.iron_plate_smelting"));
        _pole = Index(objectManager.LoadObject("factory-tour.factory_prototype.small_pole"));
        _generator = Index(objectManager.LoadObject("factory-tour.factory_prototype.burner_generator"));
        _assembler = Index(objectManager.LoadObject("factory-tour.factory_prototype.assembling_machine"));
        _gear = Index(objectManager.LoadObject("factory-tour.factory_prototype.iron_gear"));
        _gearRecipe = Index(objectManager.LoadObject("factory-tour.factory_prototype.iron_gear_recipe"));
        _underground = Index(objectManager.LoadObject("factory-tour.factory_prototype.underground_belt_basic"));
        _splitter = Index(objectManager.LoadObject("factory-tour.factory_prototype.splitter_basic"));
        _water = Index(objectManager.LoadObject("factory-tour.factory_prototype.water"));
        _steam = Index(objectManager.LoadObject("factory-tour.factory_prototype.steam"));
        _pipe = Index(objectManager.LoadObject("factory-tour.factory_prototype.pipe_basic"));
        _pump = Index(objectManager.LoadObject("factory-tour.factory_prototype.offshore_pump"));
        _boiler = Index(objectManager.LoadObject("factory-tour.factory_prototype.boiler"));
        _steamEngine = Index(objectManager.LoadObject("factory-tour.factory_prototype.steam_engine"));
        _electricDrill = Index(objectManager.LoadObject("factory-tour.factory_prototype.electric_drill"));
        ASSERT_NE(_plate, kObjectEntryIndexNull);
        ASSERT_NE(_belt, kObjectEntryIndexNull);
        ASSERT_NE(_inserter, kObjectEntryIndexNull);
        ASSERT_NE(_chest, kObjectEntryIndexNull);
    }

    static void TearDownTestCase()
    {
        _context.reset();
    }

    void SetUp() override
    {
        // Each test starts from an empty factory on a flat strip of land.
        auto& gameState = getGameState();
        ClearStrip();
        gameState.factory.reset();
    }

    static ObjectEntryIndex Index(Object* object)
    {
        if (object == nullptr)
            return kObjectEntryIndexNull;
        return GetContext()->GetObjectManager().GetLoadedObjectEntryIndex(object);
    }

    // A row of tiles along +x at y = kRowY, flattened to one height.
    static constexpr int32_t kRowY = 10;
    static constexpr int32_t kRowX0 = 4;
    static constexpr int32_t kRowLength = 12;

    static int32_t GroundZ(int32_t tx)
    {
        auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, kRowY });
        return surface != nullptr ? surface->getBaseZ() : 0;
    }

    static CoordsXYZ Tile(int32_t tx)
    {
        return CoordsXYZ{ tx * kCoordsXYStep, kRowY * kCoordsXYStep, GroundZ(tx) };
    }

    static void ClearStrip()
    {
        // Tests also build on the rows around the strip; clear everything so no element outlives its record.
        for (int32_t ty = kRowY - 2; ty <= kRowY + 6; ty++)
        {
            for (int32_t tx = kRowX0 - 1; tx <= kRowX0 + kRowLength + 1; tx++)
            {
                const CoordsXYZ at{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ(kRowX0) };
                while (auto* element = findFactoryElement(at, true))
                {
                    removeElement(getGameState(), *element, at);
                }
            }
        }
        // The strip must be flat for neighbours to connect; the test park's surface is uniform here.
        for (int32_t tx = kRowX0; tx < kRowX0 + kRowLength; tx++)
        {
            auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, kRowY });
            ASSERT_NE(surface, nullptr);
            surface->setSlope(0);
            surface->setBaseZ(GroundZ(kRowX0));
            surface->setClearanceZ(GroundZ(kRowX0));
        }
    }

    static FactoryElement* Place(int32_t tx, Direction dir, ObjectEntryIndex entry, bool ghost = false)
    {
        return placeElement(getGameState(), Tile(tx), dir, entry, ghost);
    }

    // Places on an arbitrary tile after flattening it to the row's height.
    static FactoryElement* PlaceAt(int32_t tx, int32_t ty, Direction dir, ObjectEntryIndex entry)
    {
        auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, ty });
        if (surface == nullptr)
            return nullptr;
        surface->setSlope(0);
        surface->setBaseZ(GroundZ(kRowX0));
        surface->setClearanceZ(GroundZ(kRowX0));
        while (auto* existing = findFactoryElement(CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ(kRowX0) }, true))
            removeElement(getGameState(), *existing, CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ(kRowX0) });
        return placeElement(
            getGameState(), CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ(kRowX0) }, dir, entry, false);
    }

    static void Tick(int32_t ticks)
    {
        for (int32_t i = 0; i < ticks; i++)
            update(getGameState());
    }

    // A Factory Tour ride on a 4x2 flat circuit: a two-tile station at (x0, y0) and (x0 + 1, y0) heading east, four
    // one-tile left turns and two straights, with its entrance and exit north of the station. Returns RideId::GetNull()
    // on failure.
    static RideId BuildFactoryTourLoop(int32_t x0, int32_t y0)
    {
        auto& gameState = getGameState();
        gameState.cheats.sandboxMode = true;
        auto& objectManager = GetContext()->GetObjectManager();
        auto* tram = objectManager.LoadObject("factory-tour.ride.tour_tram");
        if (tram == nullptr)
            return RideId::GetNull();
        const auto entry = objectManager.GetLoadedObjectEntryIndex(tram);
        RideTypeSetInvented(RIDE_TYPE_FACTORY_TOUR);
        RideEntrySetInvented(entry);
        auto create = GameActions::RideCreateAction(RIDE_TYPE_FACTORY_TOUR, entry, 0, 0, 0, RideInspection::every10Minutes);
        auto created = GameActions::ExecuteNested(&create, gameState);
        if (created.error != GameActions::Status::ok)
            return RideId::GetNull();
        const RideId rideId = created.getData<RideId>();
        const int32_t z = GroundZ(kRowX0);
        for (int32_t ty = y0 - 1; ty <= y0 + 1; ty++)
            for (int32_t tx = x0 - 1; tx <= x0 + 2; tx++)
            {
                auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, ty });
                surface->setSlope(0);
                surface->setBaseZ(z);
                surface->setClearanceZ(z);
            }
        struct Piece
        {
            TrackElemType type;
            int32_t tx, ty;
            Direction dir;
        };
        const Piece pieces[] = {
            { TrackElemType::endStation, x0, y0, 2 },
            { TrackElemType::endStation, x0 + 1, y0, 2 },
            { TrackElemType::leftQuarterTurn1Tile, x0 + 2, y0, 2 },
            { TrackElemType::leftQuarterTurn1Tile, x0 + 2, y0 + 1, 1 },
            { TrackElemType::flat, x0 + 1, y0 + 1, 0 },
            { TrackElemType::flat, x0, y0 + 1, 0 },
            { TrackElemType::leftQuarterTurn1Tile, x0 - 1, y0 + 1, 0 },
            { TrackElemType::leftQuarterTurn1Tile, x0 - 1, y0, 3 },
        };
        for (const auto& piece : pieces)
        {
            auto place = GameActions::TrackPlaceAction(
                rideId, piece.type, RIDE_TYPE_FACTORY_TOUR,
                CoordsXYZD{ piece.tx * kCoordsXYStep, piece.ty * kCoordsXYStep, z, piece.dir }, 0, 0, 0, {}, false);
            if (GameActions::ExecuteNested(&place, gameState).error != GameActions::Status::ok)
                return RideId::GetNull();
        }
        // Entrance and exit on the station's north side (-y), facing it.
        for (auto [tx, isExit] : { std::pair{ x0, false }, std::pair{ x0 + 1, true } })
        {
            bool placed = false;
            for (Direction dir : { Direction{ 1 }, Direction{ 3 } })
            {
                auto entrance = GameActions::RideEntranceExitPlaceAction(
                    CoordsXY{ tx * kCoordsXYStep, (y0 - 1) * kCoordsXYStep }, dir, rideId, StationIndex::FromUnderlying(0),
                    isExit);
                if (GameActions::ExecuteNested(&entrance, gameState).error == GameActions::Status::ok)
                {
                    placed = true;
                    break;
                }
            }
            if (!placed)
                return RideId::GetNull();
        }
        return rideId;
    }

    static std::shared_ptr<IContext> _context;
    static ObjectEntryIndex _plate;
    static ObjectEntryIndex _belt;
    static ObjectEntryIndex _inserter;
    static ObjectEntryIndex _chest;
    static ObjectEntryIndex _ironOrePatch;
    static ObjectEntryIndex _ironOre;
    static ObjectEntryIndex _coal;
    static ObjectEntryIndex _drill;
    static ObjectEntryIndex _furnace;
    static ObjectEntryIndex _smelting;
    static ObjectEntryIndex _pole;
    static ObjectEntryIndex _generator;
    static ObjectEntryIndex _assembler;
    static ObjectEntryIndex _gear;
    static ObjectEntryIndex _gearRecipe;
    static ObjectEntryIndex _underground;
    static ObjectEntryIndex _splitter;
    static ObjectEntryIndex _water;
    static ObjectEntryIndex _steam;
    static ObjectEntryIndex _pipe;
    static ObjectEntryIndex _pump;
    static ObjectEntryIndex _boiler;
    static ObjectEntryIndex _steamEngine;
    static ObjectEntryIndex _electricDrill;
};

std::shared_ptr<IContext> FactoryTopologyTests::_context;
ObjectEntryIndex FactoryTopologyTests::_plate = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_belt = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_inserter = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_chest = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_ironOrePatch = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_ironOre = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_coal = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_drill = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_furnace = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_smelting = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_pole = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_generator = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_assembler = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_gear = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_gearRecipe = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_underground = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_splitter = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_water = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_steam = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_pipe = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_pump = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_boiler = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_steamEngine = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_electricDrill = kObjectEntryIndexNull;

TEST_F(FactoryTopologyTests, PlacingBeltsInARowFormsOneSegment)
{
    auto& state = getGameState().factory;
    const Direction east = 2; // +x
    for (int32_t i = 0; i < 5; i++)
    {
        auto* element = Place(kRowX0 + i, east, _belt);
        ASSERT_NE(element, nullptr);
        EXPECT_TRUE(element->hasRecord());
        EXPECT_EQ(element->getFootprintIndex(), i);
    }
    EXPECT_EQ(state.beltSegments.aliveCount(), 1u);
    auto* segment = state.beltSegments.get(findBeltElement(Tile(kRowX0))->getRecordId());
    ASSERT_NE(segment, nullptr);
    EXPECT_EQ(segment->tiles.size(), 5u);
    EXPECT_EQ(segment->next, kNullRecord);
    EXPECT_EQ(getBeltShape(*findBeltElement(Tile(kRowX0 + 2))), BeltShape::straight);

    // Placing a belt in front of an existing run merges it in; placing behind extends it.
    Place(kRowX0 + 5, east, _belt);
    Place(kRowX0 - 1, east, _belt);
    EXPECT_EQ(state.beltSegments.aliveCount(), 1u);
    EXPECT_EQ(findBeltElement(Tile(kRowX0 + 5))->getFootprintIndex(), 6);
    EXPECT_EQ(findBeltElement(Tile(kRowX0 - 1))->getFootprintIndex(), 0);
    EXPECT_EQ(findBeltElement(Tile(kRowX0 + 2))->getFootprintIndex(), 3);
}

TEST_F(FactoryTopologyTests, RemovingAMiddleBeltSplitsTheSegmentAndKeepsItems)
{
    auto& state = getGameState().factory;
    const Direction east = 2;
    for (int32_t i = 0; i < 6; i++)
        Place(kRowX0 + i, east, _belt);
    auto* segment = state.beltSegments.get(findBeltElement(Tile(kRowX0))->getRecordId());
    const int32_t length = segmentLength(*segment);
    laneInsertAt(segment->lanes[0], length, 1 * 256 + 100, _plate); // tile 1
    laneInsertAt(segment->lanes[0], length, 3 * 256 + 50, _plate);  // tile 3 (removed)
    laneInsertAt(segment->lanes[1], length, 5 * 256 + 10, _plate);  // tile 5

    auto* middle = findBeltElement(Tile(kRowX0 + 3));
    removeElement(getGameState(), *middle, Tile(kRowX0 + 3));

    EXPECT_EQ(state.beltSegments.aliveCount(), 2u);
    auto* left = state.beltSegments.get(findBeltElement(Tile(kRowX0))->getRecordId());
    auto* right = state.beltSegments.get(findBeltElement(Tile(kRowX0 + 4))->getRecordId());
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_NE(left, right);
    EXPECT_EQ(left->tiles.size(), 3u);
    EXPECT_EQ(right->tiles.size(), 2u);
    EXPECT_EQ(findBeltElement(Tile(kRowX0 + 4))->getFootprintIndex(), 0);
    EXPECT_EQ(findBeltElement(Tile(kRowX0 + 5))->getFootprintIndex(), 1);
    ASSERT_EQ(left->lanes[0].items.size(), 1u);
    EXPECT_EQ(lanePosition(left->lanes[0], segmentLength(*left), 0), 1 * 256 + 100);
    EXPECT_EQ(right->lanes[0].items.size(), 0u);
    ASSERT_EQ(right->lanes[1].items.size(), 1u);
    EXPECT_EQ(lanePosition(right->lanes[1], segmentLength(*right), 0), 1 * 256 + 10);
}

TEST_F(FactoryTopologyTests, CurvesGetShapesAndJoinSegments)
{
    const Direction east = 2;  // +x
    const Direction south = 1; // +y
    // Belt heading east into a tile that turns to head +y: travelling (d + 1) & 3 = east into d = south is a
    // turn left (clockwise on screen) for the belt whose output direction is south.
    Place(kRowX0, east, _belt);
    auto* corner = Place(kRowX0 + 1, south, _belt);
    ASSERT_NE(corner, nullptr);
    EXPECT_EQ(getBeltShape(*corner), BeltShape::turnLeft);
    EXPECT_EQ(getGameState().factory.beltSegments.aliveCount(), 1u);
    EXPECT_EQ(corner->getFootprintIndex(), 1);
}

TEST_F(FactoryTopologyTests, GhostsOccupyNoRecords)
{
    auto& state = getGameState().factory;
    auto* ghost = Place(kRowX0, 2, _belt, true);
    ASSERT_NE(ghost, nullptr);
    EXPECT_TRUE(ghost->isGhost());
    EXPECT_FALSE(ghost->hasRecord());
    EXPECT_TRUE(state.isEmpty());
    EXPECT_EQ(findBeltElement(Tile(kRowX0)), nullptr);
    EXPECT_EQ(findBeltElement(Tile(kRowX0), true), ghost);
    removeElement(getGameState(), *ghost, Tile(kRowX0));
    EXPECT_TRUE(state.isEmpty());
}

TEST_F(FactoryTopologyTests, HelloConveyorMovesPlatesBetweenChests)
{
    auto& state = getGameState().factory;
    const Direction east = 2;
    // chest(0) -> inserter(1) -> belt(2..5) -> inserter(6) -> chest(7), all facing east
    auto* sourceChest = Place(kRowX0 + 0, east, _chest);
    Place(kRowX0 + 1, east, _inserter);
    for (int32_t i = 2; i <= 5; i++)
        Place(kRowX0 + i, east, _belt);
    Place(kRowX0 + 6, east, _inserter);
    auto* sinkChest = Place(kRowX0 + 7, east, _chest);
    ASSERT_NE(sourceChest, nullptr);
    ASSERT_NE(sinkChest, nullptr);
    ASSERT_EQ(state.beltSegments.aliveCount(), 1u);
    ASSERT_EQ(state.inserters.aliveCount(), 2u);
    ASSERT_EQ(state.containers.aliveCount(), 2u);

    auto* source = state.containers.get(sourceChest->getRecordId());
    auto* sink = state.containers.get(sinkChest->getRecordId());
    ASSERT_NE(source, nullptr);
    ASSERT_NE(sink, nullptr);
    source->slots[0] = { _plate, 20 };

    // 20 plates at one inserter cycle each (24 + 24 ticks) plus belt transit (4 tiles at 12 units/tick).
    Tick(40 * 40);

    int remaining = 0;
    for (auto& slot : source->slots)
        remaining += slot.count;
    int arrived = 0;
    for (auto& slot : sink->slots)
        arrived += slot.count;
    EXPECT_EQ(remaining, 0);
    EXPECT_EQ(arrived, 20);
    EXPECT_EQ(sink->slots[0].item, _plate);

    // Nothing is left on the belt and both inserters are idle.
    auto* segment = state.beltSegments.get(findBeltElement(Tile(kRowX0 + 2))->getRecordId());
    EXPECT_TRUE(segment->lanes[0].items.empty());
    EXPECT_TRUE(segment->lanes[1].items.empty());
    state.inserters.forEach([](RecordId, InserterRecord& inserter) {
        EXPECT_EQ(inserter.phase, kInserterPhaseWaitingForItem);
        EXPECT_TRUE(inserter.hand.isEmpty());
    });
}

TEST_F(FactoryTopologyTests, SimulationIsDeterministicAcrossRuns)
{
    const Direction east = 2;
    auto build = [&]() {
        auto& state = getGameState().factory;
        auto* sourceChest = Place(kRowX0 + 0, east, _chest);
        Place(kRowX0 + 1, east, _inserter);
        for (int32_t i = 2; i <= 5; i++)
            Place(kRowX0 + i, east, _belt);
        Place(kRowX0 + 6, east, _inserter);
        Place(kRowX0 + 7, east, _chest);
        state.containers.get(sourceChest->getRecordId())->slots[0] = { _plate, 50 };
    };

    std::vector<std::string> first;
    build();
    for (int i = 0; i < 10; i++)
    {
        Tick(100);
        first.push_back(computeSyncChecksum(getGameState()).toString());
    }
    ClearStrip();
    getGameState().factory.reset();
    build();
    for (int i = 0; i < 10; i++)
    {
        Tick(100);
        EXPECT_EQ(computeSyncChecksum(getGameState()).toString(), first[i]) << "diverged at window " << i;
    }
}

// Writes a park with the slice mid-run when FT_SLICE_PARK_OUT names a path, so it can be rendered with
// `openrct2-cli screenshot` to check the painter. Skipped otherwise.
TEST_F(FactoryTopologyTests, SaveSliceParkForScreenshot)
{
    const char* out = std::getenv("FT_SLICE_PARK_OUT");
    if (out == nullptr)
        GTEST_SKIP() << "FT_SLICE_PARK_OUT not set";

    auto& gameState = getGameState();
    auto& state = gameState.factory;
    const Direction east = 2;
    auto* sourceChest = Place(kRowX0 + 0, east, _chest);
    Place(kRowX0 + 1, east, _inserter);
    for (int32_t i = 2; i <= 7; i++)
        Place(kRowX0 + i, east, _belt);
    // Turn south at the end, then drop into a chest.
    Place(kRowX0 + 8, 1, _belt);
    Place(kRowX0 + 9, east, _inserter);
    Place(kRowX0 + 10, east, _chest);
    ASSERT_NE(sourceChest, nullptr);
    state.containers.get(sourceChest->getRecordId())->slots[0] = { _plate, 200 };

    // A second row two tiles south: ore patch, burner drill feeding a furnace through a short belt.
    state.ore.resize(gameState.mapSize);
    for (int32_t dx = -1; dx <= 1; dx++)
        for (int32_t dy = -1; dy <= 1; dy++)
            state.ore.set({ kRowX0 + dx, kRowY + 3 + dy }, { _ironOrePatch, 0, 50 });
    auto* drillElement = PlaceAt(kRowX0, kRowY + 3, east, _drill);
    PlaceAt(kRowX0 + 1, kRowY + 3, east, _belt);
    PlaceAt(kRowX0 + 2, kRowY + 3, east, _belt);
    PlaceAt(kRowX0 + 3, kRowY + 3, east, _inserter);
    auto* furnaceElement = PlaceAt(kRowX0 + 4, kRowY + 3, east, _furnace);

    // Logistics row: belt -> underground under a crossing belt -> splitter with two output belts.
    PlaceAt(kRowX0 + 0, kRowY + 5, east, _belt);
    PlaceAt(kRowX0 + 1, kRowY + 5, east, _underground);
    PlaceAt(kRowX0 + 2, kRowY + 5, 1, _belt);
    PlaceAt(kRowX0 + 3, kRowY + 5, east, _underground);
    PlaceAt(kRowX0 + 4, kRowY + 5, east, _belt);
    PlaceAt(kRowX0 + 5, kRowY + 5, east, _splitter);
    PlaceAt(kRowX0 + 6, kRowY + 5, east, _belt);
    PlaceAt(kRowX0 + 6, kRowY + 4, east, _belt);
    PlaceAt(kRowX0 + 7, kRowY + 5, east, _belt);
    PlaceAt(kRowX0 + 7, kRowY + 4, east, _belt);
    if (auto* logistics = state.beltSegments.get(
            findBeltElement(CoordsXYZ{ kRowX0 * kCoordsXYStep, (kRowY + 5) * kCoordsXYStep, GroundZ(kRowX0) })->getRecordId()))
    {
        for (int i = 0; i < 3; i++)
            laneInsertAt(logistics->lanes[0], segmentLength(*logistics), 20 + i * 80, _plate);
    }
    // A 3x3 electric drill east of the drill row, with a belt leaving its east edge.
    placeElement(
        gameState, CoordsXYZ{ (kRowX0 + 9) * kCoordsXYStep, (kRowY + 3) * kCoordsXYStep, GroundZ(kRowX0) }, east,
        _electricDrill, false);
    PlaceAt(kRowX0 + 12, kRowY + 4, east, _belt);

    // A Factory Tour circuit just south of the logistics row, testing so its trams run past the belts.
    const RideId tourId = BuildFactoryTourLoop(kRowX0 + 5, kRowY + 7);
    EXPECT_FALSE(tourId.IsNull());
    if (!tourId.IsNull())
    {
        auto testing = GameActions::RideSetStatusAction(tourId, RideStatus::testing);
        EXPECT_EQ(GameActions::ExecuteNested(&testing, gameState).error, GameActions::Status::ok);
    }

    // Fluid row two tiles north: a pipe run with a tee into a boiler facing south and a steam engine below it.
    for (int32_t i = 0; i <= 4; i++)
        PlaceAt(kRowX0 + i, kRowY - 2, east, _pipe);
    PlaceAt(kRowX0 + 2, kRowY - 1, east, _pipe);
    PlaceAt(kRowX0 + 5, kRowY - 2, 1, _boiler);
    PlaceAt(kRowX0 + 5, kRowY - 1, 1, _steamEngine);
    // Power: a fuelled burner generator and a pole feeding an assembler that a chest and inserter keep busy.
    auto* generatorElement = PlaceAt(kRowX0 + 6, kRowY - 2, east, _generator);
    PlaceAt(kRowX0 + 6, kRowY - 1, east, _pole);
    auto* assemblerElement = PlaceAt(kRowX0 + 7, kRowY - 1, east, _assembler);
    PlaceAt(kRowX0 + 8, kRowY - 1, 0, _inserter);
    auto* plateChest = PlaceAt(kRowX0 + 9, kRowY - 1, east, _chest);
    if (generatorElement != nullptr && assemblerElement != nullptr && plateChest != nullptr)
    {
        for (int i = 0; i < 20; i++)
            machineInsertInput(state, *state.machines.get(generatorElement->getRecordId()), _coal);
        state.machines.get(assemblerElement->getRecordId())->recipe = _gearRecipe;
        state.containers.get(plateChest->getRecordId())->slots[0] = { _plate, 100 };
    }

    if (drillElement != nullptr && furnaceElement != nullptr)
    {
        for (int i = 0; i < 5; i++)
        {
            machineInsertInput(state, *state.machines.get(drillElement->getRecordId()), _coal);
            machineInsertInput(state, *state.machines.get(furnaceElement->getRecordId()), _coal);
        }
    }
    Tick(600);

    // A park-side side table entry rides along in its own chunk.
    state.parkExt.addGuestFlags(7, kGuestTouredFactory);

    ASSERT_EQ(ScenarioSave(getGameState(), out, {}), 1);

    // Round trip: the park must come back with the same elements and records.
    const auto beltsBefore = state.beltSegments.aliveCount();
    const auto insertersBefore = state.inserters.aliveCount();
    const auto checksumBefore = computeSyncChecksum(getGameState()).toString();
    ASSERT_TRUE(GetContext()->LoadParkFromFile(out));
    GameLoadInit();
    auto& loaded = getGameState().factory;
    EXPECT_EQ(loaded.beltSegments.aliveCount(), beltsBefore);
    EXPECT_EQ(loaded.inserters.aliveCount(), insertersBefore);
    EXPECT_EQ(loaded.containers.aliveCount(), 3u);
    EXPECT_EQ(loaded.machines.aliveCount(), 7u);
    EXPECT_EQ(loaded.pipes.aliveCount(), 6u);
    EXPECT_EQ(loaded.parkExt.guestFlags(7), kGuestTouredFactory);
    EXPECT_NE(findBeltElement(Tile(kRowX0 + 2)), nullptr);
    EXPECT_NE(findFactoryElement(Tile(kRowX0 + 1)), nullptr);
    EXPECT_EQ(computeSyncChecksum(getGameState()).toString(), checksumBefore);
}

TEST_F(FactoryTopologyTests, DrillMinesOreAndFurnaceSmeltsIt)
{
    ASSERT_NE(_ironOrePatch, kObjectEntryIndexNull);
    ASSERT_NE(_drill, kObjectEntryIndexNull);
    ASSERT_NE(_furnace, kObjectEntryIndexNull);
    ASSERT_NE(_smelting, kObjectEntryIndexNull);
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    const Direction east = 2;

    // Ore under and around the drill at tile 0.
    state.ore.resize(gameState.mapSize);
    for (int32_t dx = -1; dx <= 1; dx++)
        for (int32_t dy = -1; dy <= 1; dy++)
            state.ore.set({ kRowX0 + dx, kRowY + dy }, { _ironOrePatch, 0, 10 });
    EXPECT_EQ(state.ore.nonEmptyCount(), 9u);

    // drill(0) -> belt(1..3) -> inserter(4) -> furnace(5) -> inserter(6) -> chest(7)
    auto* drillElement = Place(kRowX0 + 0, east, _drill);
    for (int32_t i = 1; i <= 3; i++)
        Place(kRowX0 + i, east, _belt);
    Place(kRowX0 + 4, east, _inserter);
    auto* furnaceElement = Place(kRowX0 + 5, east, _furnace);
    Place(kRowX0 + 6, east, _inserter);
    auto* chestElement = Place(kRowX0 + 7, east, _chest);
    ASSERT_NE(drillElement, nullptr);
    ASSERT_NE(furnaceElement, nullptr);
    ASSERT_NE(chestElement, nullptr);
    ASSERT_EQ(state.machines.aliveCount(), 2u);

    auto* drill = state.machines.get(drillElement->getRecordId());
    auto* furnace = state.machines.get(furnaceElement->getRecordId());
    auto* chest = state.containers.get(chestElement->getRecordId());
    ASSERT_NE(drill, nullptr);
    ASSERT_NE(furnace, nullptr);
    EXPECT_EQ(drill->getKind(), MachineKind::drill);
    EXPECT_EQ(furnace->getKind(), MachineKind::furnace);

    // Without fuel nothing happens.
    Tick(50);
    EXPECT_EQ(drill->getStatus(), MachineStatus::noFuel);
    EXPECT_EQ(state.ore.nonEmptyCount(), 9u);

    // Fuel both burners: coal goes to the fuel slot. One coal burns for 1600 ticks; mining 90 ore takes 9000
    // ticks and smelting them 11520, so ten coal each is plenty.
    for (int i = 0; i < 10; i++)
    {
        EXPECT_TRUE(machineInsertInput(state, *drill, _coal));
        EXPECT_TRUE(machineInsertInput(state, *furnace, _coal));
    }
    EXPECT_EQ(drill->fuel.item, _coal);
    EXPECT_EQ(drill->fuel.count, 10);
    EXPECT_FALSE(machineAcceptsInput(state, *drill, _ironOre)); // drills take nothing but fuel
    EXPECT_TRUE(machineAcceptsInput(state, *furnace, _ironOre));

    // 90 ore in the patch; mining takes 100 ticks each plus transit and smelting (128 ticks each).
    Tick(100 * 90 + 128 * 90 + 2000);

    int plates = 0;
    for (auto& slot : chest->slots)
        if (slot.item == _plate)
            plates += slot.count;
    EXPECT_EQ(state.ore.nonEmptyCount(), 0u);
    EXPECT_EQ(drill->getStatus(), MachineStatus::noOre);
    EXPECT_EQ(plates, 90);
    // An idle furnace forgets its auto-selected recipe until the next input arrives.
    EXPECT_EQ(furnace->getStatus(), MachineStatus::noInput);
    EXPECT_EQ(furnace->recipe, kObjectEntryIndexNull);
}

TEST_F(FactoryTopologyTests, AssemblerNeedsAPoweredNetworkAndMakesGears)
{
    ASSERT_NE(_pole, kObjectEntryIndexNull);
    ASSERT_NE(_generator, kObjectEntryIndexNull);
    ASSERT_NE(_assembler, kObjectEntryIndexNull);
    ASSERT_NE(_gearRecipe, kObjectEntryIndexNull);
    auto& state = getGameState().factory;
    const Direction east = 2;
    const int32_t ax = kRowX0 + 2; // assembler column

    // Row: chest(0) -> inserter(1) -> assembler(2) -> inserter(3) -> chest(4).
    auto* sourceChest = Place(kRowX0 + 0, east, _chest);
    Place(kRowX0 + 1, east, _inserter);
    auto* assemblerElement = Place(ax, east, _assembler);
    Place(kRowX0 + 3, east, _inserter);
    auto* sinkChest = Place(kRowX0 + 4, east, _chest);
    ASSERT_NE(assemblerElement, nullptr);
    auto* assembler = state.machines.get(assemblerElement->getRecordId());
    ASSERT_NE(assembler, nullptr);
    state.containers.get(sourceChest->getRecordId())->slots[0] = { _plate, 40 };

    // Without a recipe nothing is accepted; with one and no network the machine reports noPower.
    EXPECT_FALSE(machineAcceptsInput(state, *assembler, _plate));
    assembler->recipe = _gearRecipe;
    EXPECT_TRUE(machineAcceptsInput(state, *assembler, _plate));
    Tick(200);
    EXPECT_EQ(assembler->getStatus(), MachineStatus::noPower);
    EXPECT_EQ(state.powerNetworks.aliveCount(), 0u);

    // Pole A one tile south of the assembler (radius 2), pole B four tiles south (within wire reach 7) with
    // the generator next to it.
    PlaceAt(ax, kRowY + 1, east, _pole);
    PlaceAt(ax, kRowY + 4, east, _pole);
    auto* generatorElement = PlaceAt(ax, kRowY + 5, east, _generator);
    ASSERT_NE(generatorElement, nullptr);
    // Pool storage may move when a record is added: re-fetch pointers after placements.
    assembler = state.machines.get(assemblerElement->getRecordId());
    auto* generator = state.machines.get(generatorElement->getRecordId());
    ASSERT_NE(assembler, nullptr);
    ASSERT_NE(generator, nullptr);
    EXPECT_TRUE(state.powerDirty);
    Tick(1);
    EXPECT_FALSE(state.powerDirty);
    EXPECT_EQ(state.powerNetworks.aliveCount(), 1u);
    ASSERT_NE(assembler->powerNetwork, kNullRecord);
    EXPECT_EQ(assembler->powerNetwork, generator->powerNetwork);
    auto* network = state.powerNetworks.get(assembler->powerNetwork);
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->poleCount, 2);
    EXPECT_EQ(network->consumerCount, 1);
    EXPECT_EQ(network->generatorCount, 1);

    // The assembler has a craft waiting, so it draws power; the empty generator reports noFuel.
    Tick(10);
    EXPECT_EQ(generator->getStatus(), MachineStatus::noFuel);
    EXPECT_EQ(assembler->getStatus(), MachineStatus::noPower);
    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(machineInsertInput(state, *generator, _coal));

    // 20 gears from 40 plates: each craft is 20 ticks at speed 0.5 -> 40 ticks, plus inserter cycles.
    Tick(40 * 20 + 48 * 60 + 500);
    int gears = 0;
    for (auto& slot : state.containers.get(sinkChest->getRecordId())->slots)
        if (slot.item == _gear)
            gears += slot.count;
    EXPECT_EQ(gears, 20);
    EXPECT_LT(generator->fuel.count, 5);                    // it burnt something
    EXPECT_EQ(generator->getStatus(), MachineStatus::idle); // nothing left to craft, nothing drawn

    // Removing pole B splits the network: the generator is stranded and the assembler loses power.
    removeElement(
        getGameState(), *findFactoryElement(CoordsXYZ{ ax * kCoordsXYStep, (kRowY + 4) * kCoordsXYStep, GroundZ(kRowX0) }),
        CoordsXYZ{ ax * kCoordsXYStep, (kRowY + 4) * kCoordsXYStep, GroundZ(kRowX0) });
    state.containers.get(sourceChest->getRecordId())->slots[0] = { _plate, 40 };
    Tick(300);
    EXPECT_EQ(assembler->getStatus(), MachineStatus::noPower);
    EXPECT_EQ(generator->powerNetwork, kNullRecord);
}

TEST_F(FactoryTopologyTests, SideloadMergesOntoTheNearLane)
{
    auto& state = getGameState().factory;
    const Direction east = 2;  // +x
    const Direction south = 1; // +y
    // Main belt heading south through column kRowX0 + 3, rows kRowY-1 .. kRowY+2. A feeder heading east along
    // the row ends at the main belt's side.
    const int32_t mx = kRowX0 + 3;
    for (int32_t y = kRowY - 1; y <= kRowY + 2; y++)
        PlaceAt(mx, y, south, _belt);
    for (int32_t x = kRowX0; x < mx; x++)
        PlaceAt(x, kRowY, east, _belt);

    auto* feederEnd = findBeltElement(Tile(mx - 1));
    auto* mainTile = findBeltElement(Tile(mx));
    ASSERT_NE(feederEnd, nullptr);
    ASSERT_NE(mainTile, nullptr);
    auto* feeder = state.beltSegments.get(feederEnd->getRecordId());
    auto* main = state.beltSegments.get(mainTile->getRecordId());
    ASSERT_NE(feeder, nullptr);
    ASSERT_NE(main, nullptr);
    EXPECT_NE(feeder, main);
    EXPECT_EQ(feeder->getNextKind(), BeltLinkKind::sideload);
    EXPECT_EQ(feeder->next, mainTile->getRecordId());
    // Entering from the main belt's left (east is rightOf(south)) lands on its left lane.
    EXPECT_EQ(feeder->nextLane, kLaneLeft);
    EXPECT_EQ(feeder->nextPos, segmentTileStart(*main, mainTile->getFootprintIndex()) + kBeltUnitsPerTile / 2);

    laneInsertAt(feeder->lanes[0], segmentLength(*feeder), 100, _plate);
    laneInsertAt(feeder->lanes[1], segmentLength(*feeder), 20, _gear);
    Tick(200);
    EXPECT_TRUE(feeder->lanes[0].items.empty());
    EXPECT_TRUE(feeder->lanes[1].items.empty());
    // Both arrived on the main belt's left lane, nothing on its right lane; they compress at its dead end.
    EXPECT_EQ(main->lanes[kLaneLeft].items.size(), 2u);
    EXPECT_TRUE(main->lanes[kLaneRight].items.empty());
    EXPECT_EQ(lanePosition(main->lanes[kLaneLeft], segmentLength(*main), 0), segmentLength(*main));
}

TEST_F(FactoryTopologyTests, UndergroundBeltPairsAndPassesUnderACrossingBelt)
{
    auto& state = getGameState().factory;
    const Direction east = 2;
    const Direction south = 1;
    // belt(0) -> underground entrance(1) ... crossing belt at (3) heading south ... exit(4) -> belt(5)
    Place(kRowX0 + 0, east, _belt);
    auto* entrance = Place(kRowX0 + 1, east, _underground);
    ASSERT_NE(entrance, nullptr);
    EXPECT_FALSE(entrance->hasRecord());
    EXPECT_FALSE(isUndergroundExit(*entrance));
    Place(kRowX0 + 3, south, _belt);
    auto* exit = Place(kRowX0 + 4, east, _underground);
    ASSERT_NE(exit, nullptr);
    entrance = findFactoryElement(Tile(kRowX0 + 1));
    ASSERT_TRUE(entrance->hasRecord());
    EXPECT_TRUE(isUndergroundExit(*exit));
    EXPECT_FALSE(isUndergroundExit(*entrance));
    EXPECT_EQ(entrance->getRecordId(), exit->getRecordId());
    auto* tunnel = state.beltSegments.get(exit->getRecordId());
    ASSERT_NE(tunnel, nullptr);
    EXPECT_EQ(tunnel->tiles.size(), 2u);
    EXPECT_EQ(tunnel->extraLength, 2 * kBeltUnitsPerTile); // tiles 2 and 3 are spanned
    EXPECT_EQ(segmentLength(*tunnel), 4 * kBeltUnitsPerTile);
    const RecordId tunnelId = exit->getRecordId();
    Place(kRowX0 + 5, east, _belt);
    // Placing allocates a segment, which may move the pool; re-fetch every record by id.
    tunnel = state.beltSegments.get(tunnelId);
    ASSERT_NE(tunnel, nullptr);

    auto* first = state.beltSegments.get(findBeltElement(Tile(kRowX0))->getRecordId());
    auto* last = state.beltSegments.get(findBeltElement(Tile(kRowX0 + 5))->getRecordId());
    ASSERT_NE(first, nullptr);
    ASSERT_NE(last, nullptr);
    EXPECT_EQ(first->getNextKind(), BeltLinkKind::segment);
    EXPECT_EQ(first->next, tunnelId);
    EXPECT_EQ(tunnel->getNextKind(), BeltLinkKind::segment);
    EXPECT_EQ(tunnel->next, findBeltElement(Tile(kRowX0 + 5))->getRecordId());

    laneInsertAt(first->lanes[0], segmentLength(*first), 50, _plate);
    Tick(300);
    EXPECT_TRUE(first->lanes[0].items.empty());
    EXPECT_TRUE(tunnel->lanes[0].items.empty());
    ASSERT_EQ(last->lanes[0].items.size(), 1u);
    // The crossing belt was untouched.
    auto* crossing = state.beltSegments.get(findBeltElement(Tile(kRowX0 + 3))->getRecordId());
    ASSERT_NE(crossing, nullptr);
    EXPECT_TRUE(crossing->lanes[0].items.empty());

    // Removing the exit unpairs the entrance.
    removeElement(getGameState(), *findFactoryElement(Tile(kRowX0 + 4)), Tile(kRowX0 + 4));
    entrance = findFactoryElement(Tile(kRowX0 + 1));
    ASSERT_NE(entrance, nullptr);
    EXPECT_FALSE(entrance->hasRecord());
}

TEST_F(FactoryTopologyTests, SplitterAlternatesBetweenOutputs)
{
    auto& state = getGameState().factory;
    const Direction east = 2;
    // Input belt along the row into side 0 of a splitter at column kRowX0 + 2 (side 1 is one tile north:
    // rightOf(east) = direction 3 = -y). Output belts ahead of both sides.
    Place(kRowX0 + 0, east, _belt);
    Place(kRowX0 + 1, east, _belt);
    auto* splitterElement = Place(kRowX0 + 2, east, _splitter);
    ASSERT_NE(splitterElement, nullptr);
    auto* second = findFactoryElement(CoordsXYZ{ (kRowX0 + 2) * kCoordsXYStep, (kRowY - 1) * kCoordsXYStep, GroundZ(kRowX0) });
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->getSubtype(), FactoryElementSubtype::splitter);
    EXPECT_EQ(second->getFootprintIndex(), 1);
    EXPECT_EQ(second->getRecordId(), splitterElement->getRecordId());
    EXPECT_EQ(state.splitters.aliveCount(), 1u);

    Place(kRowX0 + 3, east, _belt);
    Place(kRowX0 + 4, east, _belt);
    PlaceAt(kRowX0 + 3, kRowY - 1, east, _belt);
    PlaceAt(kRowX0 + 4, kRowY - 1, east, _belt);

    auto* input = state.beltSegments.get(findBeltElement(Tile(kRowX0))->getRecordId());
    ASSERT_NE(input, nullptr);
    EXPECT_EQ(input->getNextKind(), BeltLinkKind::splitter);
    EXPECT_EQ(input->nextLane, 0);

    const int32_t inputLength = segmentLength(*input);
    for (int i = 0; i < 6; i++)
        laneInsertAt(input->lanes[0], inputLength, 10 + i * 70, _plate);
    Tick(400);
    EXPECT_TRUE(input->lanes[0].items.empty());
    auto* out0 = state.beltSegments.get(findBeltElement(Tile(kRowX0 + 3))->getRecordId());
    auto* out1 = state.beltSegments.get(
        findBeltElement(CoordsXYZ{ (kRowX0 + 3) * kCoordsXYStep, (kRowY - 1) * kCoordsXYStep, GroundZ(kRowX0) })
            ->getRecordId());
    ASSERT_NE(out0, nullptr);
    ASSERT_NE(out1, nullptr);
    EXPECT_EQ(out0->lanes[0].items.size(), 3u);
    EXPECT_EQ(out1->lanes[0].items.size(), 3u);

    // Removing one splitter tile removes the whole splitter.
    removeElement(getGameState(), *findFactoryElement(Tile(kRowX0 + 2)), Tile(kRowX0 + 2));
    EXPECT_EQ(state.splitters.aliveCount(), 0u);
    EXPECT_EQ(
        findFactoryElement(CoordsXYZ{ (kRowX0 + 2) * kCoordsXYStep, (kRowY - 1) * kCoordsXYStep, GroundZ(kRowX0) }), nullptr);
}

TEST_F(FactoryTopologyTests, PipeNetworksConserveFluidWhenSplitAndJoined)
{
    ASSERT_NE(_pipe, kObjectEntryIndexNull);
    ASSERT_NE(_water, kObjectEntryIndexNull);
    auto& state = getGameState().factory;
    for (int32_t i = 0; i < 5; i++)
        ASSERT_NE(Place(kRowX0 + i, 0, _pipe), nullptr);
    // Pipes connect regardless of their own direction; the middle one joins west (0) and east (2).
    EXPECT_EQ(findFactoryElement(Tile(kRowX0 + 2))->getConnectionCache(), 0b0101);
    EXPECT_EQ(findFactoryElement(Tile(kRowX0))->getConnectionCache(), 0b0100);
    EXPECT_TRUE(state.fluidDirty);
    Tick(1);
    ASSERT_EQ(state.fluidNetworks.aliveCount(), 1u);
    auto* network = state.fluidNetworks.get(state.pipes.get(findFactoryElement(Tile(kRowX0))->getRecordId())->network);
    ASSERT_NE(network, nullptr);
    EXPECT_EQ(network->pipeCount, 5);
    EXPECT_EQ(network->capacity, 5000u);
    network->fluid = _water;
    network->amount = 4000;

    // Removing the middle pipe shares the volume over the surviving pipes by capacity (never above it).
    removeElement(getGameState(), *findFactoryElement(Tile(kRowX0 + 2)), Tile(kRowX0 + 2));
    Tick(1);
    ASSERT_EQ(state.fluidNetworks.aliveCount(), 2u);
    auto networkAt = [&](int32_t i) {
        return state.fluidNetworks.get(state.pipes.get(findFactoryElement(Tile(kRowX0 + i))->getRecordId())->network);
    };
    ASSERT_NE(networkAt(0), nullptr);
    ASSERT_NE(networkAt(4), nullptr);
    EXPECT_NE(networkAt(0), networkAt(4));
    EXPECT_EQ(networkAt(0)->amount, 2000u);
    EXPECT_EQ(networkAt(4)->amount, 2000u);
    EXPECT_EQ(networkAt(0)->fluid, _water);

    // Rejoining sums the halves back into one volume.
    Place(kRowX0 + 2, 0, _pipe);
    Tick(1);
    ASSERT_EQ(state.fluidNetworks.aliveCount(), 1u);
    EXPECT_EQ(networkAt(0)->amount, 4000u);
    EXPECT_EQ(networkAt(0)->capacity, 5000u);

    // Fluid volumes are part of the sync checksum.
    const auto checksum = computeSyncChecksum(getGameState()).toString();
    networkAt(0)->amount--;
    EXPECT_NE(computeSyncChecksum(getGameState()).toString(), checksum);
}

TEST_F(FactoryTopologyTests, SteamChainPowersAnAssembler)
{
    ASSERT_NE(_pump, kObjectEntryIndexNull);
    ASSERT_NE(_boiler, kObjectEntryIndexNull);
    ASSERT_NE(_steamEngine, kObjectEntryIndexNull);
    auto& state = getGameState().factory;
    const Direction east = 2;
    const Direction north = 3; // -y
    const int32_t bx = kRowX0 + 2;

    // Water on the tile west of the pump; without it the pump cannot be placed (or work).
    auto* shore = MapGetSurfaceElementAt(TileCoordsXY{ kRowX0 - 1, kRowY });
    ASSERT_NE(shore, nullptr);
    EXPECT_FALSE(hasWaterBehind(Tile(kRowX0), east));
    shore->setWaterHeight(shore->getBaseZ() + 2 * kCoordsZStep);
    EXPECT_TRUE(hasWaterBehind(Tile(kRowX0), east));

    // pump(0) faces east -> pipe(1) -> boiler(2) facing north takes water from its west and east sides and
    // sends steam north into a steam engine whose front and back are a pass-through box.
    auto* pumpElement = Place(kRowX0, east, _pump);
    Place(kRowX0 + 1, east, _pipe);
    auto* boilerElement = Place(bx, north, _boiler);
    auto* engineElement = PlaceAt(bx, kRowY - 1, north, _steamEngine);
    // A pole next to the engine powers an assembler two tiles further east.
    PlaceAt(bx + 1, kRowY - 1, north, _pole);
    auto* assemblerElement = PlaceAt(bx + 3, kRowY - 1, north, _assembler);
    ASSERT_NE(pumpElement, nullptr);
    ASSERT_NE(boilerElement, nullptr);
    ASSERT_NE(engineElement, nullptr);
    ASSERT_NE(assemblerElement, nullptr);
    // The pipe joins the pump's front (west) and the boiler's west side, not north or south.
    EXPECT_EQ(findFactoryElement(Tile(kRowX0 + 1))->getConnectionCache(), 0b0101);

    Tick(1);
    auto* pump = state.machines.get(pumpElement->getRecordId());
    auto* boiler = state.machines.get(boilerElement->getRecordId());
    auto* engine = state.machines.get(engineElement->getRecordId());
    auto* assembler = state.machines.get(assemblerElement->getRecordId());
    ASSERT_NE(pump, nullptr);
    ASSERT_NE(boiler, nullptr);
    ASSERT_NE(engine, nullptr);
    ASSERT_NE(assembler, nullptr);
    EXPECT_EQ(state.fluidNetworks.aliveCount(), 2u);
    ASSERT_EQ(boiler->fluidNetworks.size(), 2u);
    EXPECT_EQ(pump->fluidNetworks[0], boiler->fluidNetworks[0]);   // water: pump box, pipe, boiler input
    EXPECT_EQ(engine->fluidNetworks[0], boiler->fluidNetworks[1]); // steam: boiler output, engine box
    auto* water = state.fluidNetworks.get(boiler->fluidNetworks[0]);
    auto* steam = state.fluidNetworks.get(boiler->fluidNetworks[1]);
    ASSERT_NE(water, nullptr);
    ASSERT_NE(steam, nullptr);
    EXPECT_EQ(water->pipeCount, 1);
    EXPECT_EQ(water->boxCount, 2);
    EXPECT_EQ(engine->powerNetwork, assembler->powerNetwork);
    ASSERT_NE(engine->powerNetwork, kNullRecord);

    // Water fills its network; the unfuelled boiler makes no steam.
    Tick(40);
    EXPECT_EQ(pump->getStatus(), MachineStatus::outputFull);
    EXPECT_EQ(water->amount, water->capacity);
    EXPECT_EQ(water->fluid, _water);
    EXPECT_EQ(boiler->getStatus(), MachineStatus::noFuel);
    EXPECT_EQ(steam->amount, 0u);

    for (int i = 0; i < 5; i++)
        EXPECT_TRUE(machineInsertInput(state, *boiler, _coal));
    Tick(100);
    EXPECT_EQ(steam->fluid, _steam);
    EXPECT_GT(steam->amount, 0u);
    // Nothing draws power yet, so the engine idles and keeps its steam.
    EXPECT_EQ(engine->getStatus(), MachineStatus::idle);

    // An assembler with work draws power; the engine burns steam and the assembler makes gears.
    assembler->recipe = _gearRecipe;
    for (int i = 0; i < 4; i++)
        EXPECT_TRUE(machineInsertInput(state, *assembler, _plate));
    Tick(10);
    EXPECT_EQ(engine->getStatus(), MachineStatus::working);
    EXPECT_EQ(assembler->getStatus(), MachineStatus::working);
    Tick(200);
    int gears = 0;
    for (auto& slot : assembler->outputs)
        if (slot.item == _gear)
            gears += slot.count;
    EXPECT_EQ(gears, 2);
    EXPECT_LE(steam->amount, steam->capacity);
    EXPECT_LT(boiler->fuel.count, 5);

    // Without water behind it the pump stops.
    shore->setWaterHeight(0);
    Tick(1);
    EXPECT_EQ(pump->getStatus(), MachineStatus::noInput);
}

TEST_F(FactoryTopologyTests, SplitterFilterAndPrioritiesRouteItems)
{
    auto& state = getGameState().factory;
    const Direction east = 2;
    const CoordsXYZ north1{ (kRowX0 + 3) * kCoordsXYStep, (kRowY - 1) * kCoordsXYStep, GroundZ(kRowX0) };
    Place(kRowX0 + 0, east, _belt);
    Place(kRowX0 + 1, east, _belt);
    auto* splitterElement = Place(kRowX0 + 2, east, _splitter);
    ASSERT_NE(splitterElement, nullptr);
    Place(kRowX0 + 3, east, _belt);
    Place(kRowX0 + 4, east, _belt);
    PlaceAt(kRowX0 + 3, kRowY - 1, east, _belt);
    PlaceAt(kRowX0 + 4, kRowY - 1, east, _belt);
    const RecordId splitterId = findFactoryElement(Tile(kRowX0 + 2))->getRecordId();

    // Gears go to the right-hand output (side 1), everything else to the left. Fluids are refused as filters.
    auto setFilter = [&](ObjectEntryIndex filter, uint8_t in, uint8_t out) {
        auto action = GameActions::FactorySetFilterAction(Tile(kRowX0 + 2), filter, in, out);
        return GameActions::ExecuteNested(&action, getGameState()).error;
    };
    EXPECT_EQ(setFilter(_water, 0, kSplitterPriorityRight), GameActions::Status::invalidParameters);
    EXPECT_EQ(setFilter(_gear, 0, kSplitterPriorityRight), GameActions::Status::ok);
    EXPECT_EQ(state.splitters.get(splitterId)->filter, _gear);

    auto* input = state.beltSegments.get(findBeltElement(Tile(kRowX0))->getRecordId());
    ASSERT_NE(input, nullptr);
    const int32_t inputLength = segmentLength(*input);
    for (int i = 0; i < 6; i++)
        laneInsertAt(input->lanes[0], inputLength, 10 + i * 70, i % 2 == 0 ? _plate : _gear);
    Tick(400);
    EXPECT_TRUE(input->lanes[0].items.empty());
    auto* out0 = state.beltSegments.get(findBeltElement(Tile(kRowX0 + 3))->getRecordId());
    auto* out1 = state.beltSegments.get(findBeltElement(north1)->getRecordId());
    ASSERT_NE(out0, nullptr);
    ASSERT_NE(out1, nullptr);
    ASSERT_EQ(out0->lanes[0].items.size(), 3u);
    ASSERT_EQ(out1->lanes[0].items.size(), 3u);
    for (auto& item : out0->lanes[0].items)
        EXPECT_EQ(item.item, _plate);
    for (auto& item : out1->lanes[0].items)
        EXPECT_EQ(item.item, _gear);

    // Without a filter, a left output priority sends everything left while it has room.
    EXPECT_EQ(setFilter(kObjectEntryIndexNull, 0, kSplitterPriorityLeft), GameActions::Status::ok);
    for (int i = 0; i < 4; i++)
        laneInsertAt(input->lanes[0], inputLength, 10 + i * 70, _gear);
    Tick(400);
    EXPECT_EQ(out0->lanes[0].items.size(), 7u);
    EXPECT_EQ(out1->lanes[0].items.size(), 3u);
}

TEST(FactoryFootprintTests, GeometryHelpers)
{
    const CoordsXYZ origin{ 10 * kCoordsXYStep, 20 * kCoordsXYStep, 64 };
    EXPECT_EQ(footprintCentre(origin, 3), (CoordsXYZ{ 11 * kCoordsXYStep, 21 * kCoordsXYStep, 64 }));
    // Just beyond each edge centre: -x, +y, +x, -y.
    EXPECT_EQ(footprintEdgeNeighbour(origin, 3, 0), (CoordsXYZ{ 9 * kCoordsXYStep, 21 * kCoordsXYStep, 64 }));
    EXPECT_EQ(footprintEdgeNeighbour(origin, 3, 1), (CoordsXYZ{ 11 * kCoordsXYStep, 23 * kCoordsXYStep, 64 }));
    EXPECT_EQ(footprintEdgeNeighbour(origin, 3, 2), (CoordsXYZ{ 13 * kCoordsXYStep, 21 * kCoordsXYStep, 64 }));
    EXPECT_EQ(footprintEdgeNeighbour(origin, 3, 3), (CoordsXYZ{ 11 * kCoordsXYStep, 19 * kCoordsXYStep, 64 }));
    // A 1x1 footprint degenerates to the plain neighbour.
    for (Direction d = 0; d < 4; d++)
        EXPECT_EQ(footprintEdgeNeighbour(origin, 1, d), neighbourTile(origin, d));
    EXPECT_EQ(distanceToFootprint(11, 21, 10, 20, 3), 0);
    EXPECT_EQ(distanceToFootprint(14, 21, 10, 20, 3), 2);
    EXPECT_EQ(distanceToFootprint(8, 17, 10, 20, 3), 3);
    // Every rotation maps the nine tiles onto the nine slices exactly once; rotation 0 is row-major map order.
    for (uint8_t rotation = 0; rotation < 4; rotation++)
    {
        uint32_t seen = 0;
        for (uint8_t index = 0; index < 9; index++)
            seen |= 1u << footprintViewSlice(index, 3, rotation);
        EXPECT_EQ(seen, 0x1FFu) << int(rotation);
    }
    for (uint8_t index = 0; index < 9; index++)
        EXPECT_EQ(footprintViewSlice(index, 3, 0), index);
}

TEST_F(FactoryTopologyTests, ElectricDrillMinesAroundItsCentreAndDropsAheadOfItsFront)
{
    ASSERT_NE(_electricDrill, kObjectEntryIndexNull);
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    const Direction east = 2;
    auto at = [](int32_t tx, int32_t ty) { return CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ(kRowX0) }; };

    // Ore only on the far corners of the 5x5 mining area around the centre (kRowX0 + 2, kRowY + 1).
    state.ore.resize(gameState.mapSize);
    state.ore.set({ kRowX0, kRowY - 1 }, { _ironOrePatch, 0, 3 });
    state.ore.set({ kRowX0 + 4, kRowY + 3 }, { _ironOrePatch, 0, 3 });
    auto* drillElement = placeElement(gameState, at(kRowX0 + 1, kRowY), east, _electricDrill, false);
    ASSERT_NE(drillElement, nullptr);
    const RecordId drillId = drillElement->getRecordId();
    // The output lands on the belt beyond the east edge's centre.
    PlaceAt(kRowX0 + 4, kRowY + 1, east, _belt);
    PlaceAt(kRowX0 + 5, kRowY + 1, east, _belt);
    PlaceAt(kRowX0 + 6, kRowY + 1, east, _belt);

    // Unpowered, nothing happens. A pole touching the footprint's south edge and a fuelled generator power it.
    Tick(50);
    EXPECT_EQ(state.machines.get(drillId)->getStatus(), MachineStatus::noPower);
    PlaceAt(kRowX0 + 1, kRowY + 3, east, _pole);
    auto* generatorElement = PlaceAt(kRowX0 + 1, kRowY + 4, east, _generator);
    ASSERT_NE(generatorElement, nullptr);
    for (int i = 0; i < 5; i++)
        machineInsertInput(state, *state.machines.get(generatorElement->getRecordId()), _coal);
    Tick(1);
    EXPECT_NE(state.machines.get(drillId)->powerNetwork, kNullRecord);
    EXPECT_EQ(state.machines.get(drillId)->powerNetwork, state.machines.get(generatorElement->getRecordId())->powerNetwork);

    Tick(1200);
    // All six ore came out onto the belt (it compresses at the dead end, two lanes' worth of room).
    EXPECT_EQ(state.ore.get({ kRowX0, kRowY - 1 }).amount, 0u);
    EXPECT_EQ(state.ore.get({ kRowX0 + 4, kRowY + 3 }).amount, 0u);
    auto* belt = state.beltSegments.get(findBeltElement(at(kRowX0 + 4, kRowY + 1))->getRecordId());
    ASSERT_NE(belt, nullptr);
    EXPECT_EQ(belt->lanes[0].items.size() + belt->lanes[1].items.size(), 6u);
    EXPECT_EQ(state.machines.get(drillId)->getStatus(), MachineStatus::noOre);

    removeElement(gameState, *findFactoryElement(at(kRowX0 + 3, kRowY + 2)), at(kRowX0 + 3, kRowY + 2));
    EXPECT_EQ(state.machines.get(drillId), nullptr);
    EXPECT_EQ(findFactoryElement(at(kRowX0 + 1, kRowY)), nullptr);
}

TEST(FactoryRideRatingTests, ProximityScoreGrowsWithDensityVarietyAndActivity)
{
    const RatingsModifier modifier{ RatingsModifierType::bonusFactoryProximity, 0, 300, 60, 30 };
    EXPECT_EQ(factoryProximityScore(RideProximityStats{}, modifier).excitement, 0);
    // One belt beside each of ten pieces: a sixth of the density bonus plus one kind of variety.
    RideProximityStats sparse{ 10, 10, 0, 0, 1u << 0 };
    auto low = factoryProximityScore(sparse, modifier);
    // Density is Q16 fixed point: 10 / 60 = 10922 / 65536, so 300 of it rounds down to 49.
    EXPECT_EQ(low.excitement, 49 + 300 / 64);
    EXPECT_EQ(low.intensity, 9);
    // Dense, varied and all working saturates density and adds variety and activity.
    RideProximityStats busy{ 10, 120, 40, 40, 0xFFu | (0xFu << 16) };
    auto high = factoryProximityScore(busy, modifier);
    EXPECT_EQ(high.excitement, 300 + 300 * 8 / 64 + 300 / 8);
    EXPECT_EQ(high.intensity, 60);
    EXPECT_EQ(high.nausea, 30);
    // Idle machines are worth less than working ones.
    RideProximityStats idle = busy;
    idle.working = 0;
    EXPECT_LT(factoryProximityScore(idle, modifier).excitement, high.excitement);
}

TEST_F(FactoryTopologyTests, FactoryTourRideCountsMachineryNearItsTrack)
{
    auto& gameState = getGameState();
    auto& objectManager = GetContext()->GetObjectManager();
    auto* tram = objectManager.LoadObject("factory-tour.ride.tour_tram");
    ASSERT_NE(tram, nullptr);
    auto action = GameActions::RideCreateAction(
        RIDE_TYPE_FACTORY_TOUR, objectManager.GetLoadedObjectEntryIndex(tram), 0, 0, 0, RideInspection::every10Minutes);
    auto result = GameActions::ExecuteNested(&action, gameState);
    ASSERT_EQ(result.error, GameActions::Status::ok);
    auto* ride = GetRide(result.getData<RideId>());
    ASSERT_NE(ride, nullptr);
    EXPECT_TRUE(rideHasFactoryProximity(*ride));
    EXPECT_STREQ(ride->getRideTypeDescriptor().Name.data(), "factory_tour");

    // A furnace and two belts within two tiles of a "track piece" at the start of the row; a chest further away.
    Place(kRowX0 + 1, 2, _furnace);
    Place(kRowX0 + 2, 2, _belt);
    Place(kRowX0 + 3, 2, _belt);
    Place(kRowX0 + 6, 2, _chest);
    rideRatingsBegin(gameState, *ride);
    rideRatingsScorePiece(gameState, *ride, Tile(kRowX0 + 1));
    const auto* stats = rideProximityStats(gameState, ride->id.ToUnderlying());
    ASSERT_NE(stats, nullptr);
    EXPECT_EQ(stats->pieces, 1);
    EXPECT_EQ(stats->factoryTiles, 3);
    EXPECT_EQ(stats->machines, 1);
    EXPECT_EQ(stats->working, 0);
    const RatingsModifier modifier{ RatingsModifierType::bonusFactoryProximity, 0, 300, 60, 30 };
    EXPECT_GT(rideRatingsTakeBonus(gameState, *ride, modifier).excitement, 0);
    EXPECT_EQ(rideProximityStats(gameState, ride->id.ToUnderlying()), nullptr); // taken once
    ride->remove();
}

TEST_F(FactoryTopologyTests, FactoryTourExcitementRisesBesideWorkingMachinery)
{
    auto& gameState = getGameState();
    gameState.cheats.sandboxMode = true;
    const int32_t x0 = kRowX0 + 5;
    const int32_t y0 = kRowY + 3;
    const RideId rideId = BuildFactoryTourLoop(x0, y0);
    ASSERT_FALSE(rideId.IsNull());
    auto* ride = GetRide(rideId);
    ASSERT_NE(ride, nullptr);
    // Rated without opening or running test laps: the comparison only needs the track walk.
    ride->status = RideStatus::testing;
    ride->flags.set(RideFlag::tested);

    RideRating::UpdateRide(*ride);
    const auto bareExcitement = ride->ratings.excitement;

    // Working machinery two tiles south of the circuit: fuelled furnaces fed by an inserter from a chest of ore.
    auto* oreChest = PlaceAt(x0 - 1, y0 + 3, 2, _chest);
    PlaceAt(x0, y0 + 3, 2, _inserter);
    auto* furnace = PlaceAt(x0 + 1, y0 + 3, 2, _furnace);
    ASSERT_NE(oreChest, nullptr);
    ASSERT_NE(furnace, nullptr);
    {
        auto& state = gameState.factory;
        state.containers.get(oreChest->getRecordId())->slots[0] = { _ironOre, 50 };
        for (int i = 0; i < 5; i++)
            machineInsertInput(state, *state.machines.get(furnace->getRecordId()), _coal);
    }
    Tick(80); // the inserter delivers ore and the furnace starts working
    ASSERT_EQ(gameState.factory.machines.get(furnace->getRecordId())->getStatus(), MachineStatus::working);
    RideRating::UpdateRide(*ride);
    const auto factoryExcitement = ride->ratings.excitement;
    EXPECT_GT(factoryExcitement, bareExcitement);
    // The totals were consumed by the calculation.
    EXPECT_EQ(rideProximityStats(gameState, rideId.ToUnderlying()), nullptr);

    auto demolish = GameActions::RideDemolishAction(rideId, GameActions::RideModifyType::demolish);
    EXPECT_EQ(GameActions::ExecuteNested(&demolish, gameState).error, GameActions::Status::ok);
}

TEST(FactoryPollutionTests, SpreadsDecaysAndRoundTripsSparsely)
{
    PollutionLayer layer;
    layer.ensureSize({ 64, 64 }); // 8x8 cells
    layer.add({ 20, 20 }, 1600);  // cell (2, 2)
    EXPECT_EQ(layer.at({ 17, 23 }), 1600u);
    EXPECT_EQ(layer.at({ 30, 20 }), 0u);
    layer.spread();
    // Each neighbour gets a sixteenth; the cell keeps the rest less a thirty-second.
    EXPECT_EQ(layer.at({ 20, 20 }), 1600u - 4 * 100 - 50);
    EXPECT_EQ(layer.at({ 28, 20 }), 100u);
    EXPECT_EQ(layer.at({ 20, 12 }), 100u);
    EXPECT_EQ(layer.total(), 1600u - 50);
    for (int i = 0; i < 400; i++)
        layer.spread();
    EXPECT_TRUE(layer.isEmpty()); // integer decay always reaches zero

    layer.add({ 3, 60 }, 7);
    layer.add({ 63, 0 }, 9);
    State state;
    state.pollution = layer;
    std::vector<uint8_t> bytes;
    {
        MemoryStream ms;
        DataSerialiser ds(true, ms);
        serialise(state, ds);
        auto* data = static_cast<const uint8_t*>(ms.GetData());
        bytes.assign(data, data + ms.GetLength());
    }
    State loaded;
    MemoryStream in(bytes);
    in.SetPosition(0);
    DataSerialiser ds(false, in);
    serialise(loaded, ds);
    EXPECT_EQ(loaded.pollution.at({ 3, 60 }), 7u);
    EXPECT_EQ(loaded.pollution.at({ 63, 0 }), 9u);
    EXPECT_EQ(loaded.pollution.total(), 16u);
}

TEST_F(FactoryTopologyTests, GuestsNoticeImpressiveSmellyAndNoisyFactories)
{
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    const CoordsXYZ guest = Tile(kRowX0 + 2);
    // No factory: no thought, so the guest falls back to upstream's assessment.
    EXPECT_FALSE(assessGuestSurroundings(gameState, guest).hasThought);

    // Four working machines of two kinds nearby impress (the furnaces stay quiet enough).
    std::vector<RecordId> machines;
    for (int32_t i = 0; i < 3; i++)
        machines.push_back(PlaceAt(kRowX0 + i, kRowY + 3, 2, _furnace)->getRecordId());
    machines.push_back(PlaceAt(kRowX0 + 3, kRowY + 3, 2, _assembler)->getRecordId());
    for (auto id : machines)
        state.machines.get(id)->status = static_cast<uint8_t>(MachineStatus::working);
    auto verdict = assessGuestSurroundings(gameState, guest);
    ASSERT_TRUE(verdict.hasThought);
    EXPECT_EQ(verdict.thought, PeepThoughtType::factoryImpressive);
    EXPECT_GT(verdict.happiness, 0);

    // Loud machines close by drown that out.
    auto* generator = PlaceAt(kRowX0 + 2, kRowY + 1, 2, _generator);
    auto* drill = PlaceAt(kRowX0 + 3, kRowY + 1, 2, _drill);
    state.machines.get(generator->getRecordId())->status = static_cast<uint8_t>(MachineStatus::working);
    state.machines.get(drill->getRecordId())->status = static_cast<uint8_t>(MachineStatus::working);
    verdict = assessGuestSurroundings(gameState, guest);
    EXPECT_EQ(verdict.thought, PeepThoughtType::factoryNoise);
    EXPECT_LT(verdict.happiness, 0);

    // Enough pollution in the guest's cell beats everything and makes them queasy.
    state.pollution.ensureSize(gameState.mapSize);
    state.pollution.add(TileCoordsXY{ CoordsXY(guest) }, kSmellPollution);
    verdict = assessGuestSurroundings(gameState, guest);
    EXPECT_EQ(verdict.thought, PeepThoughtType::factorySmell);
    EXPECT_GT(verdict.nausea, 0);
}

TEST_F(FactoryTopologyTests, WorkingMachinesPollute)
{
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    auto* furnace = Place(kRowX0 + 2, 2, _furnace);
    ASSERT_NE(furnace, nullptr);
    for (int i = 0; i < 5; i++)
        machineInsertInput(state, *state.machines.get(furnace->getRecordId()), _coal);
    for (int i = 0; i < 20; i++)
        machineInsertInput(state, *state.machines.get(furnace->getRecordId()), _ironOre);
    Tick(200);
    EXPECT_GT(state.pollution.at({ kRowX0 + 2, kRowY }), 1000u);
}

TEST(FactoryParkExtTests, GuestFlagsStaySortedCountAndPrune)
{
    ParkExt ext;
    EXPECT_TRUE(ext.isEmpty());
    ext.addGuestFlags(30, kGuestTouredFactory);
    ext.addGuestFlags(10, kGuestTouredFactory);
    ext.addGuestFlags(20, 0);
    ext.addGuestFlags(20, kGuestTouredFactory);
    ASSERT_EQ(ext.guests.size(), 3u);
    EXPECT_EQ(ext.guests[0].id, 10);
    EXPECT_EQ(ext.guests[2].id, 30);
    EXPECT_EQ(ext.guestFlags(20), kGuestTouredFactory);
    EXPECT_EQ(ext.guestFlags(99), 0);
    EXPECT_EQ(ext.countGuestsWith(kGuestTouredFactory), 3u);
}

TEST_F(FactoryTopologyTests, WatchableMachinesAndParkExtPruning)
{
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    auto* furnace = Place(kRowX0 + 2, 2, _furnace);
    ASSERT_NE(furnace, nullptr);
    EXPECT_FALSE(isWatchableMachine(gameState, *furnace)); // idle
    state.machines.get(furnace->getRecordId())->status = static_cast<uint8_t>(MachineStatus::working);
    EXPECT_TRUE(isWatchableMachine(gameState, *findFactoryElement(Tile(kRowX0 + 2))));
    EXPECT_FALSE(isWatchableMachine(gameState, *Place(kRowX0 + 4, 2, _belt)));

    // The test park has no guest with this id, so pruning drops the entry.
    markGuestToured(gameState, 4321);
    EXPECT_EQ(state.parkExt.guestFlags(4321), kGuestTouredFactory);
    pruneParkExt(gameState);
    EXPECT_TRUE(state.parkExt.guests.empty());
    EXPECT_EQ(state.parkExt.guestsToured, 1u); // the all-time count stays
}

TEST_F(FactoryTopologyTests, InsertersFillTheWarehouseThroughADepot)
{
    auto& state = getGameState().factory;
    auto* depotObject = GetContext()->GetObjectManager().LoadObject("factory-tour.factory_prototype.warehouse_depot");
    ASSERT_NE(depotObject, nullptr);
    const auto depot = GetContext()->GetObjectManager().GetLoadedObjectEntryIndex(depotObject);
    auto* source = Place(kRowX0, 2, _chest);
    Place(kRowX0 + 1, 2, _inserter);
    Place(kRowX0 + 2, 2, depot);
    state.containers.get(source->getRecordId())->slots[0] = { _plate, 5 };
    Tick(48 * 6);
    EXPECT_EQ(state.warehouse.count(_plate), 5u);
    EXPECT_TRUE(state.containers.get(source->getRecordId())->slots[0].isEmpty());
}

TEST_F(FactoryTopologyTests, MarketPricesFallWithSalesAndRecoverDaily)
{
    auto& gameState = getGameState();
    auto& market = gameState.factory.market;
    gameState.park.flags.unset(ParkFlag::noMoney);
    const auto base = getPrototype(_plate)->getItem().marketPrice;
    ASSERT_GT(base, 0);
    EXPECT_EQ(market.price(_plate), base);
    EXPECT_EQ(market.price(_furnace), 0); // not an item

    const auto cashBefore = gameState.park.cash;
    const auto income = market.sell(_plate, 10, true);
    EXPECT_GT(income, 0);
    EXPECT_LT(income, base * 10); // each unit sold for a little less
    EXPECT_EQ(gameState.park.cash, cashBefore + income);
    EXPECT_EQ(market.goodsSold, income);
    EXPECT_EQ(market.saturation(_plate), 10 * getPrototype(_plate)->getItem().marketSaturation);
    EXPECT_LT(market.price(_plate), base);

    // Flooding the market bottoms out at an eighth of the price.
    market.sell(_plate, 1000, false);
    EXPECT_EQ(market.price(_plate), base * (kMarketSaturationMax - kMarketSaturationFloor) / kMarketSaturationMax);

    // A new day recovers an eighth of the saturation.
    const auto flooded = market.saturation(_plate);
    market.lastDayIndex = -2;
    market.update(gameState);
    EXPECT_EQ(market.saturation(_plate), flooded - flooded / 8);
    market.update(gameState); // same day: nothing more
    EXPECT_EQ(market.saturation(_plate), flooded - flooded / 8);
    gameState.park.flags.set(ParkFlag::noMoney);
}

TEST_F(FactoryTopologyTests, ExportDepotSellsAndTheWarehouseSellsByAction)
{
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    gameState.park.flags.unset(ParkFlag::noMoney);
    auto& objectManager = GetContext()->GetObjectManager();
    auto* exportObject = objectManager.LoadObject("factory-tour.factory_prototype.export_depot");
    ASSERT_NE(exportObject, nullptr);
    auto* source = Place(kRowX0, 2, _chest);
    Place(kRowX0 + 1, 2, _inserter);
    Place(kRowX0 + 2, 2, objectManager.GetLoadedObjectEntryIndex(exportObject));
    state.containers.get(source->getRecordId())->slots[0] = { _plate, 4 };
    state.containers.get(source->getRecordId())->slots[1] = { _furnace, 1 }; // not sellable: refused
    const auto cashBefore = gameState.park.cash;
    Tick(48 * 6);
    EXPECT_EQ(state.market.saturation(_plate), 4 * getPrototype(_plate)->getItem().marketSaturation);
    EXPECT_GT(gameState.park.cash, cashBefore);
    // The inserter picked up the furnace too and now waits, unable to drop it.
    auto* arm = state.inserters.get(findFactoryElement(Tile(kRowX0 + 1))->getRecordId());
    ASSERT_NE(arm, nullptr);
    EXPECT_EQ(arm->hand.item, _furnace);
    EXPECT_EQ(arm->phase, kInserterPhaseWaitingToDrop);

    // Selling from the warehouse by action pays the park through the usual money path.
    state.warehouse.deposit(_gear, 5);
    GameActions::FactoryMarketSellAction sell(_gear, 5);
    const auto cashMid = gameState.park.cash;
    gInUpdateCode = true;
    auto res = GameActions::Execute(&sell, gameState);
    gInUpdateCode = false;
    ASSERT_EQ(res.error, GameActions::Status::ok);
    EXPECT_LT(res.cost, 0);
    EXPECT_EQ(gameState.park.cash, cashMid - res.cost);
    EXPECT_EQ(state.warehouse.count(_gear), 0u);
    GameActions::FactoryMarketSellAction nothing(_gear, 1);
    EXPECT_EQ(GameActions::Query(&nothing, gameState).error, GameActions::Status::invalidParameters);
    gameState.park.flags.set(ParkFlag::noMoney);
}

TEST_F(FactoryTopologyTests, WarehouseStocksShopsAndTheGiftShopSellsSouvenirs)
{
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    auto& objectManager = GetContext()->GetObjectManager();
    EXPECT_TRUE(GetShopItemDescriptor(ShopItem::factoryModel).IsSouvenir());
    EXPECT_EQ(RideObject::ParseShopItem("gear_keyring"), ShopItem::gearKeyring);

    auto* shop = static_cast<RideObject*>(objectManager.LoadObject("factory-tour.ride.gift_shop"));
    ASSERT_NE(shop, nullptr);
    EXPECT_EQ(shop->GetEntry().shop_item[0], ShopItem::factoryModel);
    EXPECT_EQ(shop->GetEntry().shop_item[1], ShopItem::gearKeyring);
    auto* modelObject = objectManager.LoadObject("factory-tour.factory_prototype.factory_model");
    ASSERT_NE(modelObject, nullptr);
    const auto model = objectManager.GetLoadedObjectEntryIndex(modelObject);

    // A factory element so the state is not empty; infinite stock mode leaves shops alone.
    Place(kRowX0, 2, _chest);
    EXPECT_EQ(shopStockItem(gameState, EnumValue(ShopItem::factoryModel)), kObjectEntryIndexNull);
    EXPECT_FALSE(shopItemSoldOut(gameState, EnumValue(ShopItem::factoryModel)));

    // Warehouse stock mode: sold out until the factory delivers, then each sale takes one.
    state.parkExt.shopStockMode = 1;
    EXPECT_EQ(shopStockItem(gameState, EnumValue(ShopItem::factoryModel)), model);
    EXPECT_TRUE(shopItemSoldOut(gameState, EnumValue(ShopItem::factoryModel)));
    state.warehouse.deposit(model, 2);
    EXPECT_FALSE(shopItemSoldOut(gameState, EnumValue(ShopItem::factoryModel)));
    EXPECT_TRUE(takeShopStock(gameState, EnumValue(ShopItem::factoryModel)));
    EXPECT_TRUE(takeShopStock(gameState, EnumValue(ShopItem::factoryModel)));
    EXPECT_TRUE(shopItemSoldOut(gameState, EnumValue(ShopItem::factoryModel)));
    // Items no factory item makes are bought in as usual.
    EXPECT_FALSE(shopItemSoldOut(gameState, EnumValue(ShopItem::burger)));
    EXPECT_FALSE(takeShopStock(gameState, EnumValue(ShopItem::burger)));
    state.parkExt.shopStockMode = 0;
}

TEST_F(FactoryTopologyTests, ParkRatingTermFollowsMachineUptimeOnlyWhenEnabled)
{
    auto& gameState = getGameState();
    auto& park = gameState.park;
    auto& state = gameState.factory;
    auto* a = Place(kRowX0, 2, _furnace);
    auto* b = Place(kRowX0 + 2, 2, _furnace);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    const auto ratingWithoutTerm = Park::CalculateParkRating(park, gameState);
    // Off (the default): exactly upstream's rating.
    EXPECT_EQ(parkRatingAdjustment(park, gameState), 0);

    park.flags.set(ParkFlag::factoryAffectsRating);
    EXPECT_EQ(parkRatingAdjustment(park, gameState), -25); // both idle
    state.machines.get(a->getRecordId())->status = static_cast<uint8_t>(MachineStatus::working);
    EXPECT_EQ(parkRatingAdjustment(park, gameState), 0);
    state.machines.get(b->getRecordId())->status = static_cast<uint8_t>(MachineStatus::working);
    EXPECT_EQ(parkRatingAdjustment(park, gameState), 25);
    EXPECT_EQ(Park::CalculateParkRating(park, gameState), std::clamp(ratingWithoutTerm + 25, 0, 999));
    park.flags.unset(ParkFlag::factoryAffectsRating);
    EXPECT_EQ(Park::CalculateParkRating(park, gameState), ratingWithoutTerm);
}

TEST_F(FactoryTopologyTests, FactoryObjectivesCountProductionAndTours)
{
    auto& gameState = getGameState();
    auto& state = gameState.factory;
    Place(kRowX0, 2, _chest); // a non-empty factory
    const auto savedDate = gameState.date;
    gameState.date = Date::FromYMD(1); // the test park is decades old; objective years count from its start
    Scenario::Objective objective{};
    objective.Type = Scenario::ObjectiveType::produceItemsBy;
    objective.Year = 200; // far in the future: undecided until reached
    objective.NumGuests = _plate;
    objective.Currency = 10;
    EXPECT_EQ(checkObjective(objective, gameState), Scenario::ObjectiveStatus::undecided);
    state.production.add(_plate, 9);
    EXPECT_EQ(checkObjective(objective, gameState), Scenario::ObjectiveStatus::undecided);
    state.production.add(_plate, 1);
    EXPECT_EQ(checkObjective(objective, gameState), Scenario::ObjectiveStatus::success);
    objective.Year = 0; // the deadline has passed
    objective.Currency = 1000;
    EXPECT_EQ(checkObjective(objective, gameState), Scenario::ObjectiveStatus::failure);

    // Tours count each guest once, even after their entry is pruned.
    objective.Type = Scenario::ObjectiveType::guestsTouredFactory;
    objective.Year = 200;
    objective.NumGuests = 2;
    markGuestToured(gameState, 11);
    markGuestToured(gameState, 11);
    EXPECT_EQ(state.parkExt.guestsToured, 1u);
    markGuestToured(gameState, 12);
    pruneParkExt(gameState);
    EXPECT_EQ(state.parkExt.guestsToured, 2u);
    EXPECT_EQ(checkObjective(objective, gameState), Scenario::ObjectiveStatus::success);

    // A furnace's plates are counted as they are made.
    auto* furnace = Place(kRowX0 + 2, 2, _furnace);
    const auto before = state.production.count(_plate);
    for (int i = 0; i < 5; i++)
        machineInsertInput(state, *state.machines.get(furnace->getRecordId()), _coal);
    for (int i = 0; i < 3; i++)
        machineInsertInput(state, *state.machines.get(furnace->getRecordId()), _ironOre);
    Tick(600);
    EXPECT_EQ(state.production.count(_plate), before + 3);
    gameState.date = savedDate;
}

TEST_F(FactoryTopologyTests, ParkOptionActionSetsRatingFlagAndObjectiveItem)
{
    auto& gameState = getGameState();
    auto run = [&](GameActions::FactoryParkOption option, uint16_t value) {
        auto action = GameActions::FactorySetParkOptionAction(option, value);
        return GameActions::ExecuteNested(&action, gameState).error;
    };
    EXPECT_EQ(run(GameActions::FactoryParkOption::affectsRating, 1), GameActions::Status::ok);
    EXPECT_TRUE(gameState.park.flags.has(ParkFlag::factoryAffectsRating));
    EXPECT_EQ(run(GameActions::FactoryParkOption::affectsRating, 0), GameActions::Status::ok);
    EXPECT_FALSE(gameState.park.flags.has(ParkFlag::factoryAffectsRating));
    EXPECT_EQ(run(GameActions::FactoryParkOption::objectiveItem, _gear), GameActions::Status::ok);
    EXPECT_EQ(gameState.scenarioOptions.objective.NumGuests, _gear);
    EXPECT_EQ(run(GameActions::FactoryParkOption::objectiveItem, _furnace), GameActions::Status::invalidParameters);
    EXPECT_EQ(run(GameActions::FactoryParkOption::objectiveItem, _water), GameActions::Status::invalidParameters);
    gameState.scenarioOptions.objective.NumGuests = 0;
}

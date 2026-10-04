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
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/factory/Belts.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/SyncChecksum.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/scenario/Scenario.h>
#include <openrct2/world/Map.h>
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
        for (int32_t tx = kRowX0 - 1; tx <= kRowX0 + kRowLength + 1; tx++)
        {
            while (auto* element = findFactoryElement(Tile(tx), true))
            {
                removeElement(getGameState(), *element, Tile(tx));
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

    static void Tick(int32_t ticks)
    {
        for (int32_t i = 0; i < ticks; i++)
            update(getGameState());
    }

    static std::shared_ptr<IContext> _context;
    static ObjectEntryIndex _plate;
    static ObjectEntryIndex _belt;
    static ObjectEntryIndex _inserter;
    static ObjectEntryIndex _chest;
};

std::shared_ptr<IContext> FactoryTopologyTests::_context;
ObjectEntryIndex FactoryTopologyTests::_plate = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_belt = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_inserter = kObjectEntryIndexNull;
ObjectEntryIndex FactoryTopologyTests::_chest = kObjectEntryIndexNull;

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

    auto& state = getGameState().factory;
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
    Tick(600);

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
    EXPECT_EQ(loaded.containers.aliveCount(), 2u);
    EXPECT_NE(findBeltElement(Tile(kRowX0 + 2)), nullptr);
    EXPECT_NE(findFactoryElement(Tile(kRowX0 + 1)), nullptr);
    EXPECT_EQ(computeSyncChecksum(getGameState()).toString(), checksumBefore);
}

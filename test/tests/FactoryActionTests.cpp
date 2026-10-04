/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Fork game actions through the upstream runner and registry.

#include "TestData.h"

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/actions/GameAction.hpp>
#include <openrct2/actions/GameActionRegistry.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/terraform/LandRaiseAction.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Materials.h>
#include <openrct2/factory/actions/FactoryActionRegistry.h>
#include <openrct2/factory/actions/FactoryPlaceAction.h>
#include <openrct2/factory/actions/FactoryPlaceBeltLineAction.h>
#include <openrct2/factory/actions/FactoryRemoveAction.h>
#include <openrct2/factory/actions/FactoryRotateAction.h>
#include <openrct2/factory/actions/FactorySetParkOptionAction.h>
#include <openrct2/network/NetworkAction.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/tile_element/FactoryElement.h>
#include <openrct2/world/tile_element/SurfaceElement.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;
using namespace OpenRCT2::GameActions;

class FactoryActionTests : public testing::Test
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
        auto index = [&](const char* id) {
            auto* object = objectManager.LoadObject(id);
            return object != nullptr ? objectManager.GetLoadedObjectEntryIndex(object) : kObjectEntryIndexNull;
        };
        _belt = index("factory-tour.factory_prototype.belt_basic");
        _chest = index("factory-tour.factory_prototype.chest_wooden");
        _plate = index("factory-tour.factory_prototype.iron_plate");
        _drill = index("factory-tour.factory_prototype.electric_drill");
        ASSERT_NE(_belt, kObjectEntryIndexNull);
        ASSERT_NE(_chest, kObjectEntryIndexNull);
        ASSERT_NE(_plate, kObjectEntryIndexNull);
    }

    static void TearDownTestCase()
    {
        _context.reset();
    }

    void SetUp() override
    {
        auto& gameState = getGameState();
        gameState.cheats.sandboxMode = true; // the test park's land is not all owned
        for (int32_t tx = kX0 - 1; tx <= kX0 + 8; tx++)
        {
            while (auto* element = findFactoryElement(Tile(tx), true))
                removeElement(gameState, *element, Tile(tx));
            auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, kY });
            ASSERT_NE(surface, nullptr);
            surface->setSlope(0);
            surface->setBaseZ(GroundZ());
            surface->setClearanceZ(GroundZ());
        }
        gameState.factory.reset();
    }

    static constexpr int32_t kY = 12;
    static constexpr int32_t kX0 = 4;

    static int32_t GroundZ()
    {
        return MapGetSurfaceElementAt(TileCoordsXY{ kX0, kY })->getBaseZ();
    }

    static CoordsXYZ Tile(int32_t tx)
    {
        return CoordsXYZ{ tx * kCoordsXYStep, kY * kCoordsXYStep, GroundZ() };
    }

    template<typename TAction>
    static Result Run(TAction& action, CommandFlags flags = {})
    {
        action.SetFlags(flags);
        return ExecuteNested(&action, getGameState());
    }

    static std::shared_ptr<IContext> _context;
    static ObjectEntryIndex _belt;
    static ObjectEntryIndex _chest;
    static ObjectEntryIndex _plate;
    static ObjectEntryIndex _drill;
};

std::shared_ptr<IContext> FactoryActionTests::_context;
ObjectEntryIndex FactoryActionTests::_belt = kObjectEntryIndexNull;
ObjectEntryIndex FactoryActionTests::_chest = kObjectEntryIndexNull;
ObjectEntryIndex FactoryActionTests::_plate = kObjectEntryIndexNull;
ObjectEntryIndex FactoryActionTests::_drill = kObjectEntryIndexNull;

TEST_F(FactoryActionTests, RegistryResolvesForkCommands)
{
    const auto place = toGameCommand(FactoryCommand::place);
    EXPECT_TRUE(isFactoryCommand(place));
    EXPECT_FALSE(isFactoryCommand(GameCommand::placeBanner));
    EXPECT_TRUE(IsValidId(static_cast<uint32_t>(place)));
    EXPECT_FALSE(IsValidId(static_cast<uint32_t>(kFactoryCommandBase + 99)));
    EXPECT_STREQ(GetName(place), "FactoryPlaceAction");

    auto created = Create(toGameCommand(FactoryCommand::rotate));
    ASSERT_NE(created, nullptr);
    EXPECT_EQ(created->GetType(), toGameCommand(FactoryCommand::rotate));
    EXPECT_STREQ(created->GetName(), "FactoryRotateAction");

    EXPECT_EQ(GameActions::Factory::commandFromScriptName("factoryremove"), toGameCommand(FactoryCommand::remove));
    EXPECT_EQ(GameActions::Factory::scriptNameFromCommand(place), "factoryplace");

#ifndef DISABLE_NETWORK
    EXPECT_EQ(Network::NetworkActions::findCommand(place), Network::Permission::factory);
#endif
}

TEST_F(FactoryActionTests, PlaceRemoveAndRotateRoundTrip)
{
    auto& state = getGameState().factory;
    FactoryPlaceAction place(Tile(kX0), 2, _belt);
    auto res = Run(place);
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_EQ(res.cost, 20);

    auto* element = findBeltElement(Tile(kX0));
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->getDirection(), 2);
    EXPECT_EQ(state.beltSegments.aliveCount(), 1u);

    // Occupied tile is refused.
    FactoryPlaceAction again(Tile(kX0), 2, _chest);
    EXPECT_EQ(Run(again).error, Status::itemAlreadyPlaced);

    // A non-placeable prototype is refused.
    FactoryPlaceAction item(Tile(kX0 + 1), 0, _plate);
    EXPECT_EQ(Run(item).error, Status::invalidParameters);

    FactoryRotateAction rotate(Tile(kX0));
    ASSERT_EQ(Run(rotate).error, Status::ok);
    element = findBeltElement(Tile(kX0));
    ASSERT_NE(element, nullptr);
    EXPECT_EQ(element->getDirection(), 3);
    EXPECT_EQ(state.beltSegments.aliveCount(), 1u);

    FactoryRemoveAction remove(Tile(kX0));
    res = Run(remove);
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_EQ(res.cost, -15);
    EXPECT_EQ(findBeltElement(Tile(kX0)), nullptr);
    EXPECT_EQ(state.beltSegments.aliveCount(), 0u);

    FactoryRemoveAction removeAgain(Tile(kX0));
    EXPECT_EQ(Run(removeAgain).error, Status::invalidParameters);
}

TEST_F(FactoryActionTests, GhostsLeaveNoRecordAndRemoveOnlyGhosts)
{
    auto& state = getGameState().factory;
    FactoryPlaceAction ghost(Tile(kX0), 1, _chest);
    ASSERT_EQ(Run(ghost, { CommandFlag::ghost }).error, Status::ok);
    auto* element = findFactoryElement(Tile(kX0), true);
    ASSERT_NE(element, nullptr);
    EXPECT_TRUE(element->isGhost());
    EXPECT_TRUE(state.isEmpty());
    EXPECT_EQ(findFactoryElement(Tile(kX0)), nullptr);

    // A real placement on the same tile ignores the ghost and succeeds.
    FactoryPlaceAction real(Tile(kX0), 1, _chest);
    ASSERT_EQ(Run(real).error, Status::ok);
    EXPECT_EQ(state.containers.aliveCount(), 1u);

    // Ghost removal only removes ghosts.
    FactoryRemoveAction removeGhost(Tile(kX0));
    ASSERT_EQ(Run(removeGhost, { CommandFlag::ghost }).error, Status::ok);
    EXPECT_EQ(state.containers.aliveCount(), 1u);
    EXPECT_NE(findFactoryElement(Tile(kX0)), nullptr);
    FactoryRemoveAction removeGhostAgain(Tile(kX0));
    EXPECT_EQ(Run(removeGhostAgain, { CommandFlag::ghost }).error, Status::invalidParameters);
}

TEST_F(FactoryActionTests, SlopedOrOffMapTilesAreRefused)
{
    auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ kX0 + 2, kY });
    surface->setSlope(1);
    FactoryPlaceAction sloped(Tile(kX0 + 2), 0, _belt);
    EXPECT_EQ(Run(sloped).error, Status::invalidParameters);
    surface->setSlope(0);

    FactoryPlaceAction offMap(CoordsXYZ{ -32, -32, 0 }, 0, _belt);
    EXPECT_EQ(Run(offMap).error, Status::invalidParameters);

    FactoryPlaceAction badDirection(Tile(kX0 + 2), 7, _belt);
    EXPECT_EQ(Run(badDirection).error, Status::invalidParameters);
}

TEST_F(FactoryActionTests, BeltLinePlacesARunAndSkipsOccupiedTiles)
{
    auto& state = getGameState().factory;
    // A chest in the middle of the run is skipped; the belts either side face east along the drag.
    FactoryPlaceAction chest(Tile(kX0 + 2), 0, _chest);
    ASSERT_EQ(Run(chest).error, Status::ok);

    FactoryPlaceBeltLineAction line(Tile(kX0), Tile(kX0 + 4), 0, _belt);
    auto ghost = Run(line, { CommandFlag::ghost, CommandFlag::allowDuringPaused, CommandFlag::noSpend });
    ASSERT_EQ(ghost.error, Status::ok);
    EXPECT_EQ(ghost.cost, 4 * 20);
    EXPECT_EQ(state.beltSegments.aliveCount(), 0u);
    for (int32_t i = 0; i < 5; i++)
    {
        auto* element = findFactoryElement(Tile(kX0 + i), true);
        ASSERT_NE(element, nullptr);
        EXPECT_EQ(element->isGhost(), i != 2);
    }
    for (int32_t i = 0; i < 5; i++)
    {
        if (auto* element = findFactoryElement(Tile(kX0 + i), true); element != nullptr && element->isGhost())
            removeElement(getGameState(), *element, Tile(kX0 + i));
    }

    auto res = Run(line);
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_EQ(res.cost, 4 * 20);
    for (int32_t i : { 0, 1, 3, 4 })
    {
        auto* belt = findBeltElement(Tile(kX0 + i));
        ASSERT_NE(belt, nullptr) << i;
        EXPECT_EQ(belt->getDirection(), 2);
    }
    EXPECT_EQ(state.beltSegments.aliveCount(), 2u);

    // Dragging the same run again changes nothing and costs nothing.
    auto again = Run(line);
    EXPECT_EQ(again.error, Status::ok);
    EXPECT_EQ(again.cost, 0);

    // A single tile uses the fallback direction; a westward drag faces west.
    FactoryPlaceBeltLineAction single(Tile(kX0 + 6), Tile(kX0 + 6), 3, _belt);
    ASSERT_EQ(Run(single).error, Status::ok);
    EXPECT_EQ(findBeltElement(Tile(kX0 + 6))->getDirection(), 3);
    Direction dir;
    auto tiles = FactoryPlaceBeltLineAction::lineTiles(Tile(kX0 + 5), Tile(kX0 + 1), 0, dir);
    EXPECT_EQ(dir, 0);
    EXPECT_EQ(tiles.size(), 5u);
    EXPECT_EQ(tiles.front(), Tile(kX0 + 5));
    EXPECT_EQ(tiles.back(), Tile(kX0 + 1));

    // Like a single belt, a run cannot be built while the game is paused.
    FactoryPlaceBeltLineAction paused(Tile(kX0 + 8), Tile(kX0 + 10), 0, _belt);
    gGamePaused = 1;
    auto pausedRes = Query(&paused, getGameState()); // top level: nested runs skip the pause check
    gGamePaused = 0;
    EXPECT_EQ(pausedRes.error, Status::gamePaused);
    EXPECT_EQ(findFactoryElement(Tile(kX0 + 8)), nullptr);
}

TEST_F(FactoryActionTests, MultiTileMachinesNeedEveryTileFreeAndGoAsAWhole)
{
    ASSERT_NE(_drill, kObjectEntryIndexNull);
    auto& state = getGameState().factory;
    auto at = [](int32_t tx, int32_t ty) { return CoordsXYZ{ tx * kCoordsXYStep, ty * kCoordsXYStep, GroundZ() }; };
    for (int32_t ty = kY; ty <= kY + 2; ty++)
    {
        for (int32_t tx = kX0 + 4; tx <= kX0 + 6; tx++)
        {
            while (auto* element = findFactoryElement(at(tx, ty), true))
                removeElement(getGameState(), *element, at(tx, ty));
            auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ tx, ty });
            ASSERT_NE(surface, nullptr);
            surface->setSlope(0);
            surface->setBaseZ(GroundZ());
            surface->setClearanceZ(GroundZ());
        }
    }

    // A chest under the far corner blocks the 3x3 drill; with it gone the drill takes all nine tiles.
    FactoryPlaceAction chest(at(kX0 + 6, kY + 2), 0, _chest);
    ASSERT_EQ(Run(chest).error, Status::ok);
    FactoryPlaceAction drill(at(kX0 + 4, kY), 2, _drill);
    EXPECT_EQ(Run(drill).error, Status::itemAlreadyPlaced);
    FactoryRemoveAction removeChest(at(kX0 + 6, kY + 2));
    ASSERT_EQ(Run(removeChest).error, Status::ok);
    auto res = Run(drill);
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_EQ(res.cost, 200);
    ASSERT_EQ(state.machines.aliveCount(), 1u);
    const auto record = findFactoryElement(at(kX0 + 4, kY))->getRecordId();
    for (int32_t index = 0; index < 9; index++)
    {
        auto* element = findFactoryElement(at(kX0 + 4 + index % 3, kY + index / 3));
        ASSERT_NE(element, nullptr) << index;
        EXPECT_EQ(element->getSubtype(), FactoryElementSubtype::machine);
        EXPECT_EQ(element->getRecordId(), record);
        EXPECT_EQ(element->getFootprintIndex(), index);
        EXPECT_EQ((element->getFactoryFlags() & FACTORY_ELEMENT_FLAG_ORIGIN) != 0, index == 0);
        EXPECT_EQ(footprintOrigin(*element, at(kX0 + 4 + index % 3, kY + index / 3)), at(kX0 + 4, kY));
    }

    // Removing it from any tile removes every tile and the record.
    FactoryRemoveAction removeDrill(at(kX0 + 5, kY + 2));
    ASSERT_EQ(Run(removeDrill).error, Status::ok);
    EXPECT_EQ(state.machines.aliveCount(), 0u);
    for (int32_t index = 0; index < 9; index++)
        EXPECT_EQ(findFactoryElement(at(kX0 + 4 + index % 3, kY + index / 3), true), nullptr) << index;
}

TEST(FactoryWarehouseTests, DepositTakeCoverAndConsume)
{
    Warehouse warehouse;
    warehouse.deposit(5, 10);
    warehouse.deposit(2, 3);
    warehouse.deposit(5, 4);
    ASSERT_EQ(warehouse.stock.size(), 2u);
    EXPECT_EQ(warehouse.stock[0].item, 2); // sorted by item
    EXPECT_EQ(warehouse.count(5), 14u);
    EXPECT_EQ(warehouse.take(2, 10), 3u);
    EXPECT_EQ(warehouse.count(2), 0u);
    EXPECT_EQ(warehouse.stock.size(), 1u); // emptied entries go
    const MaterialBill bill{ { 5, 12 } };
    EXPECT_TRUE(warehouse.canCover(bill));
    EXPECT_FALSE(warehouse.canCover({ { 5, 15 } }));
    warehouse.consume(bill);
    EXPECT_EQ(warehouse.count(5), 2u);
    warehouse.depositBill({ { 7, 1 } });
    EXPECT_EQ(warehouse.count(7), 1u);
}

TEST_F(FactoryActionTests, ConstructionModesBillTheWarehouse)
{
    auto& gameState = getGameState();
    auto& factory = gameState.factory;
    gameState.park.flags.unset(ParkFlag::noMoney);
    gameState.park.cash = 100000.00_GBP;
    // Construction bills only apply to upstream construction, here raising a tile.
    const auto tile = Tile(kX0 + 3);
    LandRaiseAction action(tile, MapRange{ tile, tile }, MapSelectType::full);

    // Money mode (the default): upstream behaviour.
    auto res = Query(&action, gameState);
    ASSERT_EQ(res.error, Status::ok);
    const auto cost = res.cost;
    ASSERT_GT(cost, 0);
    EXPECT_EQ(
        billFromCost(cost, ExpenditureType::landscaping).front().count,
        static_cast<uint32_t>((cost + kMoneyPerBillItem - 1) / kMoneyPerBillItem));
    EXPECT_TRUE(billFromCost(cost, ExpenditureType::shopStock).empty());

    // Materials mode with an empty warehouse refuses; with plates it is free in money and takes the bill.
    FactorySetParkOptionAction setMode(FactoryParkOption::constructionMode, static_cast<uint8_t>(ConstructionMode::materials));
    ASSERT_EQ(Run(setMode).error, Status::ok);
    EXPECT_EQ(Query(&action, gameState).error, Status::insufficientMaterials);
    const uint32_t billCount = billFromCost(cost, ExpenditureType::landscaping).front().count;
    factory.warehouse.deposit(_plate, billCount + 5);
    res = Query(&action, gameState);
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_EQ(res.cost, 0);
    const auto cashBefore = gameState.park.cash;
    gInUpdateCode = true; // execute now instead of queueing for the end of the tick
    res = Execute(&action, gameState);
    gInUpdateCode = false;
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_EQ(factory.warehouse.count(_plate), 5u);
    EXPECT_EQ(gameState.park.cash, cashBefore);

    // Hybrid mode pays both.
    FactorySetParkOptionAction hybrid(FactoryParkOption::constructionMode, static_cast<uint8_t>(ConstructionMode::hybrid));
    ASSERT_EQ(Run(hybrid).error, Status::ok);
    factory.warehouse.deposit(_plate, 1000);
    const auto platesBefore = factory.warehouse.count(_plate);
    gInUpdateCode = true;
    res = Execute(&action, gameState);
    gInUpdateCode = false;
    ASSERT_EQ(res.error, Status::ok);
    EXPECT_LT(factory.warehouse.count(_plate), platesBefore);
    EXPECT_LT(gameState.park.cash, cashBefore);

    // Fork actions are never billed.
    FactoryPlaceAction belt(Tile(kX0 + 6), 0, _belt);
    EXPECT_EQ(Query(&belt, gameState).error, Status::ok);

    // Back to money mode: the side table empties again.
    FactorySetParkOptionAction money(FactoryParkOption::constructionMode, 0);
    ASSERT_EQ(Run(money).error, Status::ok);
    EXPECT_FALSE(materialsActive(gameState));
    gameState.park.flags.set(ParkFlag::noMoney);
}

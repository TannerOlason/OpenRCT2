/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/GameState.h>
#include <openrct2/core/DataSerialiser.h>
#include <openrct2/core/MemoryStream.h>
#include <openrct2/factory/FactoryPool.hpp>
#include <openrct2/factory/FactorySerialisation.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/SyncChecksum.h>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;

namespace
{
    std::vector<uint8_t> SerialiseToBytes(State& state)
    {
        MemoryStream ms;
        DataSerialiser ds(true, ms);
        serialise(state, ds);
        auto* data = static_cast<const uint8_t*>(ms.GetData());
        return std::vector<uint8_t>(data, data + ms.GetLength());
    }

    void DeserialiseFromBytes(State& state, const std::vector<uint8_t>& bytes)
    {
        MemoryStream ms(bytes);
        ms.SetPosition(0);
        DataSerialiser ds(false, ms);
        serialise(state, ds);
    }

    struct Probe
    {
        int32_t value{};
        template<typename V>
        void visit(V& v)
        {
            v(value);
        }
    };
} // namespace

TEST(FactoryPoolTests, AllocatesLowestFreeIdAndIteratesAscending)
{
    Pool<Probe> pool;
    EXPECT_EQ(pool.allocate(), 0u);
    EXPECT_EQ(pool.allocate(), 1u);
    EXPECT_EQ(pool.allocate(), 2u);
    EXPECT_EQ(pool.allocate(), 3u);
    EXPECT_EQ(pool.aliveCount(), 4u);

    pool.release(2);
    pool.release(1);
    EXPECT_EQ(pool.aliveCount(), 2u);
    EXPECT_FALSE(pool.isAlive(1));
    EXPECT_EQ(pool.get(1), nullptr);

    // Lowest free id first, regardless of release order.
    EXPECT_EQ(pool.allocate(), 1u);
    EXPECT_EQ(pool.allocate(), 2u);
    EXPECT_EQ(pool.allocate(), 4u);

    std::vector<RecordId> order;
    pool.forEach([&](RecordId id, Probe&) { order.push_back(id); });
    EXPECT_EQ(order, (std::vector<RecordId>{ 0, 1, 2, 3, 4 }));
}

TEST(FactoryPoolTests, ReleasingTailSlotsTrimsStorage)
{
    Pool<Probe> pool;
    pool.allocate();
    pool.allocate();
    pool.allocate();
    EXPECT_EQ(pool.slotCount(), 3u);

    pool.release(2);
    EXPECT_EQ(pool.slotCount(), 2u);

    // A hole in the middle is kept, then trimmed once everything after it is gone.
    pool.release(0);
    EXPECT_EQ(pool.slotCount(), 2u);
    pool.release(1);
    EXPECT_EQ(pool.slotCount(), 0u);
    EXPECT_EQ(pool.aliveCount(), 0u);
    EXPECT_EQ(pool.allocate(), 0u);
}

TEST(FactoryStateTests, EmptyStateIsEmptyAndResetRestoresIt)
{
    State state;
    EXPECT_TRUE(state.isEmpty());
    EXPECT_EQ(state.recordCount(), 0u);

    state.containers.allocate();
    state.topologyVersion++;
    EXPECT_FALSE(state.isEmpty());

    state.reset();
    EXPECT_TRUE(state.isEmpty());

    // Research alone (a park that researched before building anything) must still be saved.
    state.research.researched.push_back(1);
    EXPECT_FALSE(state.isEmpty());
    state.reset();
    EXPECT_TRUE(state.isEmpty());
}

TEST(FactoryStateTests, SerialisationRoundTripsEveryRecordKind)
{
    State state;
    state.topologyVersion = 7;
    state.research.researched = { 3, 8 };
    state.research.progress = { { 12, 4 } };
    state.research.current = 12;

    auto chestId = state.containers.allocate();
    auto* chest = state.containers.get(chestId);
    chest->setLocation({ 10, 20, 14 });
    chest->direction = 3;
    chest->entry = 5;
    chest->slots.push_back({ 1, 40 });
    chest->slots.push_back({});
    chest->slots.push_back({ 2, 1 });

    // Leave a hole so the dense layout with alive flags is exercised.
    auto holeId = state.inserters.allocate();
    auto armId = state.inserters.allocate();
    state.inserters.release(holeId);
    auto* arm = state.inserters.get(armId);
    arm->setLocation({ 11, 20, 14 });
    arm->direction = 1;
    arm->entry = 6;
    arm->phase = 2;
    arm->progress = 1234;
    arm->hand = { 1, 1 };
    arm->source = { static_cast<uint8_t>(FactoryElementSubtype::container), chestId };
    arm->target = { static_cast<uint8_t>(FactoryElementSubtype::belt), 0 };
    arm->topologyVersionSeen = 7;

    auto beltId = state.beltSegments.allocate();
    auto* belt = state.beltSegments.get(beltId);
    belt->tiles = { { 12, 20, 14 }, { 13, 20, 14 }, { 13, 21, 14 } };
    belt->entry = 7;
    belt->speed = 12;
    belt->lanes[0].items = { { 1, 64 }, { 1, 64 }, { 2, 200 } };
    belt->lanes[1].items = { { 3, 100 } };
    belt->next = kNullRecord;

    RecordId pipeId;
    state.pipes.allocateRecord(pipeId).setLocation({ 14, 20, 14 });
    RecordId fluidId;
    auto& fluid = state.fluidNetworks.allocateRecord(fluidId);
    fluid.fluid = 9;
    fluid.amount = 1500;
    fluid.capacity = 2000;
    state.pipes.get(pipeId)->network = fluidId;
    RecordId machineId;
    auto& machine = state.machines.allocateRecord(machineId);
    machine.fluidNetworks = { fluidId, kNullRecord };
    RecordId powerId;
    state.powerNetworks.allocateRecord(powerId).lastSupply = 450;
    state.fluidDirty = true;

    auto bytes = SerialiseToBytes(state);
    ASSERT_FALSE(bytes.empty());

    State loaded;
    DeserialiseFromBytes(loaded, bytes);

    EXPECT_EQ(loaded.topologyVersion, 7u);
    EXPECT_EQ(loaded.containers.aliveCount(), 1u);
    EXPECT_EQ(loaded.inserters.aliveCount(), 1u);
    EXPECT_EQ(loaded.inserters.slotCount(), 2u);
    EXPECT_FALSE(loaded.inserters.isAlive(holeId));
    EXPECT_EQ(loaded.beltSegments.aliveCount(), 1u);

    auto* loadedChest = loaded.containers.get(chestId);
    ASSERT_NE(loadedChest, nullptr);
    EXPECT_EQ(loadedChest->location(), (TileCoordsXYZ{ 10, 20, 14 }));
    EXPECT_EQ(loadedChest->direction, 3);
    EXPECT_EQ(loadedChest->entry, 5);
    ASSERT_EQ(loadedChest->slots.size(), 3u);
    EXPECT_EQ(loadedChest->slots[0].item, 1);
    EXPECT_EQ(loadedChest->slots[0].count, 40);
    EXPECT_TRUE(loadedChest->slots[1].isEmpty());

    auto* loadedArm = loaded.inserters.get(armId);
    ASSERT_NE(loadedArm, nullptr);
    EXPECT_EQ(loadedArm->progress, 1234);
    EXPECT_EQ(loadedArm->source.getKind(), FactoryElementSubtype::container);
    EXPECT_EQ(loadedArm->source.id, chestId);
    EXPECT_EQ(loadedArm->target.getKind(), FactoryElementSubtype::belt);

    auto* loadedBelt = loaded.beltSegments.get(beltId);
    ASSERT_NE(loadedBelt, nullptr);
    ASSERT_EQ(loadedBelt->tiles.size(), 3u);
    EXPECT_EQ(loadedBelt->tiles[2], (TileCoordsXYZ{ 13, 21, 14 }));
    ASSERT_EQ(loadedBelt->lanes[0].items.size(), 3u);
    EXPECT_EQ(loadedBelt->lanes[0].items[2].gap, 200);
    ASSERT_EQ(loadedBelt->lanes[1].items.size(), 1u);
    EXPECT_EQ(loadedBelt->next, kNullRecord);

    ASSERT_NE(loaded.pipes.get(pipeId), nullptr);
    EXPECT_EQ(loaded.pipes.get(pipeId)->network, fluidId);
    ASSERT_NE(loaded.fluidNetworks.get(fluidId), nullptr);
    EXPECT_EQ(loaded.fluidNetworks.get(fluidId)->fluid, 9);
    EXPECT_EQ(loaded.fluidNetworks.get(fluidId)->amount, 1500u);
    EXPECT_EQ(loaded.fluidNetworks.get(fluidId)->space(), 500u);
    ASSERT_NE(loaded.machines.get(machineId), nullptr);
    EXPECT_EQ(loaded.machines.get(machineId)->fluidNetworks, (std::vector<RecordId>{ fluidId, kNullRecord }));
    EXPECT_EQ(loaded.powerNetworks.get(powerId)->lastSupply, 450u);
    EXPECT_TRUE(loaded.fluidDirty);
    EXPECT_EQ(loaded.research.researched, (std::vector<ObjectEntryIndex>{ 3, 8 }));
    EXPECT_EQ(loaded.research.unitsDone(12), 4u);
    EXPECT_EQ(loaded.research.current, 12);

    // The hole was reused as the lowest free id after loading.
    EXPECT_EQ(loaded.inserters.allocate(), holeId);

    // Re-serialising the loaded state (before the extra allocation would matter) must be byte-identical.
    loaded.inserters.release(holeId);
    EXPECT_EQ(SerialiseToBytes(loaded), bytes);
}

TEST(FactorySyncChecksumTests, EmptyFactoryMatchesUpstreamEntityChecksum)
{
    auto gameState = std::make_unique<GameState_t>();
    gameState->entities.resetAllEntities();
    gameState->factory.reset();

    auto upstream = gameState->entities.getAllEntitiesChecksum();
    auto fork = computeSyncChecksum(*gameState);
    EXPECT_EQ(fork.raw, upstream.raw);

    gameState->factory.containers.allocate();
    auto withFactory = computeSyncChecksum(*gameState);
#ifndef DISABLE_NETWORK
    EXPECT_NE(withFactory.raw, upstream.raw);
#else
    EXPECT_EQ(withFactory.raw, upstream.raw);
#endif
}

TEST(FactoryStateTests, ProductionHistorySamplesAndForgetsIdleItems)
{
    ProductionStats stats;
    stats.add(5, 3);
    stats.consume(5, 1);
    stats.consume(9, 4);
    EXPECT_EQ(stats.count(5), 3u);
    EXPECT_EQ(stats.consumedCount(9), 4u);
    ASSERT_NE(stats.historyOf(5), nullptr);
    EXPECT_EQ(stats.historyOf(5)->produced[stats.head], 3u);

    // Closing the period keeps it as the last sample and opens an empty one.
    const auto first = stats.head;
    stats.advanceSample();
    EXPECT_NE(stats.head, first);
    EXPECT_EQ(stats.historyOf(5)->produced[first], 3u);
    EXPECT_EQ(stats.historyOf(5)->produced[stats.head], 0u);

    // After a full window without activity an item drops out of the history but keeps its totals.
    for (size_t i = 0; i < kProductionSamples; i++)
        stats.advanceSample();
    EXPECT_EQ(stats.historyOf(5), nullptr);
    EXPECT_EQ(stats.count(5), 3u);
    EXPECT_TRUE(stats.history.empty());
}

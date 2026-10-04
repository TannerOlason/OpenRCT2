/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The factory-bench world: it builds, it produces, and two builds run identically.

#include <gtest/gtest.h>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/factory/FactoryBench.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/SyncChecksum.h>
#include <string>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;

namespace
{
    std::string RunBench(int32_t cells, int32_t ticks, BenchCounts& counts, uint32_t& items)
    {
        int32_t width = 0;
        int32_t height = 0;
        benchMapSize(cells, width, height);
        auto& gameState = getGameState();
        gameStateInitAll(gameState, { std::max(width, height), std::max(width, height) });
        std::string error;
        EXPECT_TRUE(buildBenchFactory(gameState, cells, counts, error)) << error;
        for (int32_t i = 0; i < ticks; i++)
            update(gameState);
        // Everything that reached a sink chest (plates from the furnaces, gears from the assemblers).
        items = 0;
        gameState.factory.containers.forEach([&](RecordId, ContainerRecord& chest) {
            for (auto& slot : chest.slots)
                if (slot.count > 0 && slot.count < 100)
                    items += slot.count;
        });
        return computeSyncChecksum(gameState).toString();
    }
} // namespace

TEST(FactoryBenchTests, BenchFactoryProducesAndIsDeterministic)
{
    gOpenRCT2Headless = true;
    gOpenRCT2NoGraphics = true;
    auto context = CreateContext();
    ASSERT_TRUE(context->Initialise());

    BenchCounts counts;
    uint32_t items = 0;
    const auto first = RunBench(6, 1500, counts, items);
    EXPECT_EQ(counts.cells, 6);
    EXPECT_EQ(counts.belts, 6u * 12u);
    EXPECT_EQ(counts.segments, 6u);
    EXPECT_EQ(counts.inserters, 6u * 4u);
    EXPECT_EQ(counts.machines, 6u * 4u);
    EXPECT_EQ(counts.poles, 6u);
    EXPECT_EQ(counts.pipes, 6u * 6u);
    // Every cell mines, smelts and crafts within 1500 ticks.
    EXPECT_GE(items, 6u * 2u);
    EXPECT_EQ(getGameState().factory.powerNetworks.aliveCount(), 6u);
    EXPECT_EQ(getGameState().factory.fluidNetworks.aliveCount(), 6u);

    uint32_t itemsAgain = 0;
    BenchCounts countsAgain;
    EXPECT_EQ(RunBench(6, 1500, countsAgain, itemsAgain), first);
    EXPECT_EQ(itemsAgain, items);

    std::string error;
    EXPECT_FALSE(buildBenchFactory(getGameState(), 0, counts, error));
    EXPECT_FALSE(error.empty());
}

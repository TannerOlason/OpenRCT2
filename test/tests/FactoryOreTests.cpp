/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Pure ore layer tests.

#include <gtest/gtest.h>
#include <openrct2/factory/Ore.h>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;

TEST(FactoryOreTests, SetGetTakeAndCounters)
{
    OreLayer layer;
    layer.resize({ 8, 6 });
    EXPECT_TRUE(layer.isEmpty());
    EXPECT_EQ(layer.hash(), 0u);
    EXPECT_TRUE(layer.get({ 3, 3 }).isEmpty());
    EXPECT_TRUE(layer.get({ 99, 1 }).isEmpty());

    layer.set({ 3, 3 }, { 5, 10, 100 });
    EXPECT_FALSE(layer.isEmpty());
    EXPECT_EQ(layer.nonEmptyCount(), 1u);
    EXPECT_EQ(layer.get({ 3, 3 }).amount, 100u);
    const auto hashOne = layer.hash();
    EXPECT_NE(hashOne, 0u);

    layer.set({ 4, 3 }, { 5, 10, 7 });
    EXPECT_EQ(layer.nonEmptyCount(), 2u);
    EXPECT_EQ(layer.take({ 4, 3 }, 10), 7u);
    EXPECT_TRUE(layer.get({ 4, 3 }).isEmpty());
    EXPECT_EQ(layer.nonEmptyCount(), 1u);
    // Removing what was added restores the order-independent hash exactly.
    EXPECT_EQ(layer.hash(), hashOne);

    EXPECT_EQ(layer.take({ 3, 3 }, 30), 30u);
    EXPECT_EQ(layer.get({ 3, 3 }).amount, 70u);
    EXPECT_NE(layer.hash(), hashOne);

    // Out-of-bounds writes are ignored.
    layer.set({ -1, 0 }, { 5, 10, 100 });
    layer.set({ 8, 0 }, { 5, 10, 100 });
    EXPECT_EQ(layer.nonEmptyCount(), 1u);
}

TEST(FactoryOreTests, AssignRebuildsHashIdenticallyToIncrementalBuild)
{
    OreLayer incremental;
    incremental.resize({ 5, 5 });
    std::vector<OreCell> cells(25);
    for (int32_t i = 0; i < 25; i += 3)
    {
        OreCell cell{ static_cast<ObjectEntryIndex>(1 + i % 2), static_cast<uint16_t>(i), static_cast<uint32_t>(10 + i) };
        incremental.set({ i % 5, i / 5 }, cell);
        cells[i] = cell;
    }
    OreLayer assigned;
    assigned.assign({ 5, 5 }, std::move(cells));
    EXPECT_EQ(assigned.hash(), incremental.hash());
    EXPECT_EQ(assigned.nonEmptyCount(), incremental.nonEmptyCount());
    EXPECT_EQ(assigned.get({ 3, 0 }).amount, incremental.get({ 3, 0 }).amount);
}

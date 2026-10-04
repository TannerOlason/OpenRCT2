/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Pure belt lane tests, no game context needed.

#include <gtest/gtest.h>
#include <openrct2/factory/Belts.h>
#include <vector>

using namespace OpenRCT2;
using namespace OpenRCT2::Factory;

namespace
{
    constexpr ObjectEntryIndex kPlate = 1;
    constexpr ObjectEntryIndex kGear = 2;

    std::vector<int32_t> Positions(const BeltLane& lane, int32_t length)
    {
        std::vector<int32_t> result;
        for (size_t i = 0; i < lane.items.size(); i++)
            result.push_back(lanePosition(lane, length, i));
        return result;
    }

    BeltSegmentRecord MakeSegment(size_t tiles, uint8_t speed)
    {
        BeltSegmentRecord segment;
        for (size_t i = 0; i < tiles; i++)
            segment.tiles.push_back({ static_cast<int32_t>(i), 0, 0 });
        segment.speed = speed;
        return segment;
    }
} // namespace

TEST(FactoryBeltTests, InsertKeepsFrontFirstOrderAndSpacing)
{
    BeltLane lane;
    const int32_t length = 4 * kBeltUnitsPerTile;

    EXPECT_TRUE(laneInsertAt(lane, length, 500, kPlate));
    EXPECT_TRUE(laneInsertAt(lane, length, 100, kGear));
    EXPECT_TRUE(laneInsertAt(lane, length, 300, kPlate));
    EXPECT_EQ(Positions(lane, length), (std::vector<int32_t>{ 500, 300, 100 }));
    EXPECT_EQ(lane.items[1].item, kPlate);
    EXPECT_EQ(lane.items[2].item, kGear);

    // Too close to an existing item on either side, or off the belt.
    EXPECT_FALSE(laneCanInsertAt(lane, length, 500 + kBeltItemSpacing - 1));
    EXPECT_FALSE(laneCanInsertAt(lane, length, 300 - kBeltItemSpacing + 1));
    EXPECT_FALSE(laneCanInsertAt(lane, length, -1));
    EXPECT_FALSE(laneCanInsertAt(lane, length, length + 1));
    EXPECT_TRUE(laneCanInsertAt(lane, length, 500 + kBeltItemSpacing));
    EXPECT_TRUE(laneCanInsertAt(lane, length, length));
    EXPECT_TRUE(laneCanInsertAt(lane, length, 0));
    EXPECT_EQ(laneRearPosition(lane, length), 100);
}

TEST(FactoryBeltTests, TakeInRangeMergesGaps)
{
    BeltLane lane;
    const int32_t length = 2 * kBeltUnitsPerTile;
    laneInsertAt(lane, length, 400, kPlate);
    laneInsertAt(lane, length, 300, kGear);
    laneInsertAt(lane, length, 100, kPlate);

    EXPECT_FALSE(laneTakeInRange(lane, length, 150, 250).has_value());
    auto taken = laneTakeInRange(lane, length, 250, 350);
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, kGear);
    EXPECT_EQ(Positions(lane, length), (std::vector<int32_t>{ 400, 100 }));

    taken = laneTakeInRange(lane, length, 0, length);
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, kPlate);
    EXPECT_EQ(Positions(lane, length), (std::vector<int32_t>{ 100 }));
}

TEST(FactoryBeltTests, UnblockedLaneMovesEveryItemAtSpeed)
{
    BeltLane lane;
    const int32_t length = 4 * kBeltUnitsPerTile;
    laneInsertAt(lane, length, 600, kPlate);
    laneInsertAt(lane, length, 200, kGear);
    laneInsertAt(lane, length, 100, kPlate);

    tickLane(lane, length, 12, LaneTarget{});
    EXPECT_EQ(Positions(lane, length), (std::vector<int32_t>{ 612, 212, 112 }));
    tickLane(lane, length, 36, LaneTarget{});
    EXPECT_EQ(Positions(lane, length), (std::vector<int32_t>{ 648, 248, 148 }));
}

TEST(FactoryBeltTests, DeadEndCompressesItemsToSpacing)
{
    BeltLane lane;
    const int32_t length = 2 * kBeltUnitsPerTile; // 512
    laneInsertAt(lane, length, 500, kPlate);
    laneInsertAt(lane, length, 400, kGear);
    laneInsertAt(lane, length, 100, kPlate);

    for (int i = 0; i < 100; i++)
        tickLane(lane, length, 12, LaneTarget{});

    // Front item parks at the end, the rest stack up at exactly the minimum spacing.
    EXPECT_EQ(Positions(lane, length), (std::vector<int32_t>{ 512, 512 - 64, 512 - 128 }));
    EXPECT_EQ(lane.items.size(), 3u);

    // Partial compression: the item behind closes part of its gap in one tick.
    BeltLane lane2;
    laneInsertAt(lane2, length, 512, kPlate);
    laneInsertAt(lane2, length, 512 - 70, kGear);
    laneInsertAt(lane2, length, 100, kPlate);
    tickLane(lane2, length, 12, LaneTarget{});
    // The third item is 342 behind the second, so it still moves at full speed.
    EXPECT_EQ(Positions(lane2, length), (std::vector<int32_t>{ 512, 512 - 64, 112 }));
}

TEST(FactoryBeltTests, ItemsTransferToTheNextLaneWithOvershoot)
{
    const int32_t length = kBeltUnitsPerTile;
    BeltLane a;
    BeltLane b;
    laneInsertAt(a, length, 250, kPlate);
    laneInsertAt(a, length, 150, kGear);

    tickLane(a, length, 12, LaneTarget{ &b, length, -1 });
    EXPECT_EQ(Positions(a, length), (std::vector<int32_t>{ 162 }));
    EXPECT_EQ(Positions(b, length), (std::vector<int32_t>{ 6 }));
    EXPECT_EQ(b.items[0].item, kPlate);

    // The receiving lane refuses when its rear item is within the spacing.
    BeltLane c;
    laneInsertAt(c, length, 40, kGear);
    BeltLane d;
    laneInsertAt(d, length, 250, kPlate);
    tickLane(d, length, 12, LaneTarget{ &c, length, -1 });
    EXPECT_EQ(Positions(d, length), (std::vector<int32_t>{ 256 }));
    EXPECT_EQ(c.items.size(), 1u);

    // The parked item has gap 0, so the overshoot is the full speed (12). With the rear item at 70 the
    // receiving lane still refuses (70 - 12 < 64); at 100 it accepts.
    tickLane(c, length, 30, LaneTarget{});
    tickLane(d, length, 12, LaneTarget{ &c, length, -1 });
    EXPECT_EQ(d.items.size(), 1u);
    tickLane(c, length, 30, LaneTarget{});
    tickLane(d, length, 12, LaneTarget{ &c, length, -1 });
    EXPECT_EQ(d.items.size(), 0u);
    EXPECT_EQ(Positions(c, length), (std::vector<int32_t>{ 100, 12 }));
}

TEST(FactoryBeltTests, ThroughputMatchesSpeedOverLongRuns)
{
    // A compressed 32-tile segment at speed 12 (7.5 items per second per lane) delivers 15 items per
    // second (40 ticks) over both lanes into a sink that drains instantly. The feed keeps each lane fully
    // compressed by placing a new item exactly one spacing behind the rearmost one.
    auto source = MakeSegment(32, 12);
    auto sink = MakeSegment(1, 64);
    const int32_t length = segmentLength(source);
    const int32_t warmup = 1000; // 8192 units / 12 per tick is 683 ticks of transit
    const int32_t measured = 4000;
    int delivered = 0;
    for (int t = 0; t < warmup + measured; t++)
    {
        for (auto& lane : source.lanes)
        {
            const int32_t pos = lane.items.empty() ? 0 : laneRearPosition(lane, length) - kBeltItemSpacing;
            if (pos >= 0)
                laneInsertAt(lane, length, pos, kPlate);
        }
        const int32_t sinkLength = segmentLength(sink);
        const LaneTarget targets[kBeltLaneCount] = { { &sink.lanes[0], sinkLength, -1 }, { &sink.lanes[1], sinkLength, -1 } };
        tickSegment(source, targets);
        if (t >= warmup)
        {
            for (auto& lane : sink.lanes)
                delivered += static_cast<int>(lane.items.size());
        }
        for (auto& lane : sink.lanes)
            lane.items.clear();
    }
    const double perSecond = delivered * 40.0 / measured;
    EXPECT_NEAR(perSecond, 15.0, 0.3);
}

TEST(FactoryBeltTests, SideloadTargetInsertsAtAFixedPositionWhenThereIsRoom)
{
    const int32_t length = 2 * kBeltUnitsPerTile;
    BeltLane feeder;
    BeltLane main;
    laneInsertAt(feeder, length, 500, kPlate);
    laneInsertAt(main, length, 300, kGear); // blocks the entry point at 384 - 64 .. 384 + 64? no: 300 is 84 away
    const LaneTarget target{ &main, length, 384 };
    tickLane(feeder, length, 12, target);
    // 500 + 12 = 512 parks exactly at the end; the next tick it sideloads at 384 because 384 - 300 >= 64.
    EXPECT_EQ(feeder.items.size(), 1u);
    tickLane(feeder, length, 12, target);
    EXPECT_TRUE(feeder.items.empty());
    EXPECT_EQ(main.items.size(), 2u);
    EXPECT_EQ(lanePosition(main, length, 0), 384);

    // A second item has to wait while the entry point is occupied.
    laneInsertAt(feeder, length, 505, kPlate);
    tickLane(feeder, length, 12, target);
    EXPECT_EQ(feeder.items.size(), 1u);
    EXPECT_EQ(lanePosition(feeder, length, 0), length);
    for (int i = 0; i < 20; i++)
        tickLane(main, length, 12, LaneTarget{});
    tickLane(feeder, length, 12, target);
    EXPECT_TRUE(feeder.items.empty());
    EXPECT_EQ(main.items.size(), 3u);
}

TEST(FactoryBeltTests, TakeFrontAtEndOnlyTakesParkedItems)
{
    const int32_t length = kBeltUnitsPerTile;
    BeltLane lane;
    laneInsertAt(lane, length, 200, kPlate);
    laneInsertAt(lane, length, 100, kGear);
    EXPECT_FALSE(laneTakeFrontAtEnd(lane).has_value());
    for (int i = 0; i < 10; i++)
        tickLane(lane, length, 12, LaneTarget{});
    auto taken = laneTakeFrontAtEnd(lane);
    ASSERT_TRUE(taken.has_value());
    EXPECT_EQ(*taken, kPlate);
    EXPECT_EQ(lane.items.size(), 1u);
    EXPECT_EQ(lanePosition(lane, length, 0), length - 64);
}

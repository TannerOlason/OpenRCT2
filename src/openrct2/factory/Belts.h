/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Belt lane simulation (pure functions over records, no map access).
//
// A lane stores items front-first: items[0] is the item nearest the segment end. Each item's `gap` is the
// distance from the item ahead of it (or from the segment end for items[0]) to its own centre, in belt units
// (256 per tile). Positions from the segment start are therefore implicit and moving an unblocked lane costs
// one subtraction. Items never come closer than kBeltItemSpacing.

#pragma once

#include "FactoryRecords.h"

#include <cstdint>
#include <optional>

namespace OpenRCT2::Factory
{
    int32_t segmentLength(const BeltSegmentRecord& segment);

    // First belt unit of tile `index` of the segment (underground pairs put their hidden span before the exit).
    int32_t segmentTileStart(const BeltSegmentRecord& segment, size_t index);

    // Position of items[index] measured from the segment start to the item centre.
    int32_t lanePosition(const BeltLane& lane, int32_t length, size_t index);

    // Position of the rearmost item, or `length + kBeltItemSpacing` when the lane is empty.
    int32_t laneRearPosition(const BeltLane& lane, int32_t length);

    bool laneCanInsertAt(const BeltLane& lane, int32_t length, int32_t pos);
    bool laneInsertAt(BeltLane& lane, int32_t length, int32_t pos, ObjectEntryIndex item);

    // Removes and returns the first item whose centre lies in [from, to], searching from the front.
    std::optional<ObjectEntryIndex> laneTakeInRange(BeltLane& lane, int32_t length, int32_t from, int32_t to);

    // Removes and returns the front item when it has reached the end (gap 0).
    std::optional<ObjectEntryIndex> laneTakeFrontAtEnd(BeltLane& lane);

    struct LaneItemView
    {
        int32_t position;
        ObjectEntryIndex item;
    };

    // Calls f(LaneItemView) for every item whose centre lies in [from, to], front first.
    template<typename F>
    void laneForEachInRange(const BeltLane& lane, int32_t length, int32_t from, int32_t to, F f)
    {
        int32_t pos = length;
        for (const auto& item : lane.items)
        {
            pos -= item.gap;
            if (pos < from)
                break;
            if (pos <= to)
                f(LaneItemView{ pos, item.item });
        }
    }

    /**
     * Where a lane hands items over when they reach its end.
     */
    struct LaneTarget
    {
        BeltLane* lane = nullptr; // nullptr: dead end
        int32_t length = 0;
        // -1: append at the target's start carrying the overshoot (a segment join);
        // >= 0: insert at this fixed position (a sideload), waiting until there is room.
        int32_t position = -1;
    };

    /**
     * Advances every item by up to `speed` units. Items reaching the end move onto the target when it has
     * room; otherwise they compress against the end.
     */
    void tickLane(BeltLane& lane, int32_t length, int32_t speed, const LaneTarget& target);

    // Ticks both lanes with per-lane targets (nullptr targets are dead ends).
    void tickSegment(BeltSegmentRecord& segment, const LaneTarget* targets);
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Belts.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    int32_t segmentLength(const BeltSegmentRecord& segment)
    {
        return static_cast<int32_t>(segment.tiles.size()) * kBeltUnitsPerTile + segment.extraLength;
    }

    int32_t segmentTileStart(const BeltSegmentRecord& segment, size_t index)
    {
        return static_cast<int32_t>(index) * kBeltUnitsPerTile + (index > 0 ? segment.extraLength : 0);
    }

    std::optional<ObjectEntryIndex> laneTakeFrontAtEnd(BeltLane& lane)
    {
        if (lane.items.empty() || lane.items.front().gap != 0)
            return std::nullopt;
        auto item = lane.items.front().item;
        lane.items.erase(lane.items.begin());
        // The new front keeps its gap: it was measured from the removed item, which sat exactly at the end.
        return item;
    }

    int32_t lanePosition(const BeltLane& lane, int32_t length, size_t index)
    {
        int32_t pos = length;
        for (size_t i = 0; i <= index && i < lane.items.size(); i++)
        {
            pos -= lane.items[i].gap;
        }
        return pos;
    }

    int32_t laneRearPosition(const BeltLane& lane, int32_t length)
    {
        if (lane.items.empty())
            return length + kBeltItemSpacing;
        return lanePosition(lane, length, lane.items.size() - 1);
    }

    bool laneCanInsertAt(const BeltLane& lane, int32_t length, int32_t pos)
    {
        if (pos < 0 || pos > length)
            return false;
        int32_t ahead = length + kBeltItemSpacing; // a virtual item just past the end never blocks
        for (const auto& item : lane.items)
        {
            int32_t itemPos = ahead == length + kBeltItemSpacing ? length - item.gap : ahead - item.gap;
            if (itemPos < pos)
            {
                // `ahead` is the nearest item in front, `itemPos` the nearest behind.
                return ahead - pos >= kBeltItemSpacing && pos - itemPos >= kBeltItemSpacing;
            }
            if (itemPos == pos)
                return false;
            ahead = itemPos;
        }
        return ahead - pos >= kBeltItemSpacing;
    }

    bool laneInsertAt(BeltLane& lane, int32_t length, int32_t pos, ObjectEntryIndex item)
    {
        if (!laneCanInsertAt(lane, length, pos))
            return false;

        int32_t aheadPos = length;
        size_t index = 0;
        for (; index < lane.items.size(); index++)
        {
            int32_t itemPos = aheadPos - lane.items[index].gap;
            if (itemPos < pos)
                break;
            aheadPos = itemPos;
        }
        // Insert before `index`: the new item's gap is measured from aheadPos, and the item that used to be at
        // `index` now measures its gap from the new item.
        BeltItem inserted{ item, static_cast<uint16_t>(aheadPos - pos) };
        if (index < lane.items.size())
        {
            int32_t behindPos = aheadPos - lane.items[index].gap;
            lane.items[index].gap = static_cast<uint16_t>(pos - behindPos);
        }
        lane.items.insert(lane.items.begin() + static_cast<ptrdiff_t>(index), inserted);
        return true;
    }

    std::optional<ObjectEntryIndex> laneTakeInRange(BeltLane& lane, int32_t length, int32_t from, int32_t to)
    {
        int32_t pos = length;
        for (size_t i = 0; i < lane.items.size(); i++)
        {
            pos -= lane.items[i].gap;
            if (pos < from)
                return std::nullopt;
            if (pos <= to)
            {
                auto item = lane.items[i].item;
                if (i + 1 < lane.items.size())
                {
                    lane.items[i + 1].gap = static_cast<uint16_t>(lane.items[i + 1].gap + lane.items[i].gap);
                }
                lane.items.erase(lane.items.begin() + static_cast<ptrdiff_t>(i));
                return item;
            }
        }
        return std::nullopt;
    }

    static bool laneAcceptsAtStart(const BeltLane& lane, int32_t length, int32_t pos)
    {
        return pos <= length && laneRearPosition(lane, length) - pos >= kBeltItemSpacing;
    }

    static void laneAppendAtStart(BeltLane& lane, int32_t length, int32_t pos, ObjectEntryIndex item)
    {
        int32_t rear = lane.items.empty() ? length : laneRearPosition(lane, length);
        lane.items.push_back({ item, static_cast<uint16_t>(rear - pos) });
    }

    void tickLane(BeltLane& lane, int32_t length, int32_t speed, const LaneTarget& target)
    {
        if (lane.items.empty() || speed <= 0)
            return;

        // Front items that would cross the end this tick move onto the target while it has room.
        while (!lane.items.empty())
        {
            auto& front = lane.items.front();
            if (front.gap >= speed)
                break;
            const int32_t overshoot = speed - front.gap;
            if (target.lane == nullptr)
                break;
            if (target.position < 0)
            {
                if (!laneAcceptsAtStart(*target.lane, target.length, overshoot))
                    break;
                laneAppendAtStart(*target.lane, target.length, overshoot, front.item);
            }
            else
            {
                if (!laneInsertAt(*target.lane, target.length, target.position, front.item))
                    break;
            }
            const uint16_t frontGap = front.gap;
            lane.items.erase(lane.items.begin());
            if (!lane.items.empty())
            {
                lane.items.front().gap = static_cast<uint16_t>(lane.items.front().gap + frontGap);
            }
        }

        // Everything else moves `speed`, or less when it catches up with the item (or end) ahead.
        int32_t movedPrev = 0;
        for (size_t i = 0; i < lane.items.size(); i++)
        {
            auto& item = lane.items[i];
            const int32_t slack = i == 0 ? static_cast<int32_t>(item.gap) : static_cast<int32_t>(item.gap) - kBeltItemSpacing;
            const int32_t actualMove = std::min(speed, movedPrev + std::max(0, slack));
            item.gap = static_cast<uint16_t>(static_cast<int32_t>(item.gap) - (actualMove - movedPrev));
            movedPrev = actualMove;
            if (actualMove == speed)
                break; // every item behind also moves at full speed, gaps unchanged
        }
    }

    void tickSegment(BeltSegmentRecord& segment, const LaneTarget* targets)
    {
        const int32_t length = segmentLength(segment);
        for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
        {
            tickLane(segment.lanes[lane], length, segment.speed, targets != nullptr ? targets[lane] : LaneTarget{});
        }
    }
} // namespace OpenRCT2::Factory

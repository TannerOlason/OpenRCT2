/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryState.h"

#include "../GameState.h"
#include "../profiling/Profiling.h"
#include "../world/tile_element/FactoryElement.h"
#include "Belts.h"
#include "FactoryPrototypeObject.h"
#include "FactoryTopology.h"

namespace OpenRCT2::Factory
{
    void State::reset()
    {
        containers.clear();
        inserters.clear();
        beltSegments.clear();
        topologyVersion = 0;
    }

    bool State::isEmpty() const
    {
        return recordCount() == 0 && topologyVersion == 0;
    }

    size_t State::recordCount() const
    {
        return containers.aliveCount() + inserters.aliveCount() + beltSegments.aliveCount();
    }

    static void updateBelts(State& state)
    {
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& segment) {
            auto* next = segment.next != kNullRecord ? state.beltSegments.get(segment.next) : nullptr;
            tickSegment(segment, next);
        });
    }

    static RecordRef resolveNeighbour(const InserterRecord& inserter, Direction d)
    {
        auto loc = neighbourTile(tileToCoords(inserter.location()), d);
        auto* element = findFactoryElement(loc);
        if (element == nullptr || !element->hasRecord())
            return RecordRef{};
        return RecordRef{ static_cast<uint8_t>(element->getSubtype()), element->getRecordId(), element->getFootprintIndex() };
    }

    static void resolveInserterRefs(State& state, InserterRecord& inserter)
    {
        if (inserter.topologyVersionSeen == state.topologyVersion)
            return;
        inserter.source = resolveNeighbour(inserter, oppositeOf(inserter.direction));
        inserter.target = resolveNeighbour(inserter, inserter.direction);
        inserter.topologyVersionSeen = state.topologyVersion;
    }

    bool containerTakeAny(ContainerRecord& container, ItemStack& hand)
    {
        for (auto& slot : container.slots)
        {
            if (!slot.isEmpty())
            {
                hand.item = slot.item;
                hand.count = 1;
                slot.count--;
                if (slot.count == 0)
                    slot.item = kObjectEntryIndexNull;
                return true;
            }
        }
        return false;
    }

    bool containerInsert(ContainerRecord& container, ObjectEntryIndex item, uint16_t stackSize)
    {
        for (auto& slot : container.slots)
        {
            if (slot.item == item && slot.count < stackSize)
            {
                slot.count++;
                return true;
            }
        }
        for (auto& slot : container.slots)
        {
            if (slot.isEmpty())
            {
                slot.item = item;
                slot.count = 1;
                return true;
            }
        }
        return false;
    }

    static int32_t beltTileStart(const RecordRef& ref)
    {
        return ref.aux * kBeltUnitsPerTile;
    }

    static bool inserterPickUp(State& state, InserterRecord& inserter)
    {
        auto& ref = inserter.source;
        switch (ref.getKind())
        {
            case FactoryElementSubtype::container:
            {
                auto* container = state.containers.get(ref.id);
                return container != nullptr && containerTakeAny(*container, inserter.hand);
            }
            case FactoryElementSubtype::belt:
            {
                auto* segment = state.beltSegments.get(ref.id);
                if (segment == nullptr)
                    return false;
                const int32_t length = segmentLength(*segment);
                const int32_t from = beltTileStart(ref);
                const int32_t to = from + kBeltUnitsPerTile - 1;
                for (auto& lane : segment->lanes)
                {
                    auto item = laneTakeInRange(lane, length, from, to);
                    if (item.has_value())
                    {
                        inserter.hand.item = *item;
                        inserter.hand.count = 1;
                        return true;
                    }
                }
                return false;
            }
            default:
                return false;
        }
    }

    // Items go on the lane farthest from the inserter; when the belt runs along the drop direction, the
    // right lane.
    static uint8_t dropLaneFor(Direction dropDirection, Direction beltDirection)
    {
        if (dropDirection == leftOf(beltDirection))
            return kLaneLeft;
        return kLaneRight;
    }

    static bool inserterDrop(State& state, InserterRecord& inserter)
    {
        auto& ref = inserter.target;
        switch (ref.getKind())
        {
            case FactoryElementSubtype::container:
            {
                auto* container = state.containers.get(ref.id);
                if (container == nullptr)
                    return false;
                auto* itemProto = getPrototype(inserter.hand.item);
                const uint16_t stackSize = itemProto != nullptr ? itemProto->getItem().stackSize : 1;
                return containerInsert(*container, inserter.hand.item, stackSize);
            }
            case FactoryElementSubtype::belt:
            {
                auto* segment = state.beltSegments.get(ref.id);
                if (segment == nullptr || ref.aux >= segment->tiles.size())
                    return false;
                const int32_t length = segmentLength(*segment);
                const int32_t pos = beltTileStart(ref) + kBeltUnitsPerTile / 2;
                auto* beltElement = findBeltElement(tileToCoords(segment->tiles[ref.aux]));
                const Direction beltDirection = beltElement != nullptr ? beltElement->getDirection() : inserter.direction;
                const uint8_t lane = dropLaneFor(inserter.direction, beltDirection);
                return laneInsertAt(segment->lanes[lane], length, pos, inserter.hand.item);
            }
            default:
                return false;
        }
    }

    static void updateInserter(State& state, InserterRecord& inserter)
    {
        resolveInserterRefs(state, inserter);
        auto* proto = getPrototype(inserter.entry);
        const uint16_t swingTicks = proto != nullptr ? proto->getInserter().swingTicks : 24;

        switch (inserter.phase)
        {
            case kInserterPhaseWaitingForItem:
                if (!inserter.source.isNull() && inserterPickUp(state, inserter))
                {
                    inserter.phase = kInserterPhaseSwingingToDrop;
                    inserter.progress = 0;
                }
                break;
            case kInserterPhaseSwingingToDrop:
                if (++inserter.progress >= swingTicks)
                {
                    inserter.phase = kInserterPhaseWaitingToDrop;
                    inserter.progress = swingTicks;
                }
                break;
            case kInserterPhaseWaitingToDrop:
                if (!inserter.target.isNull() && inserterDrop(state, inserter))
                {
                    inserter.hand = ItemStack{};
                    inserter.phase = kInserterPhaseReturning;
                    inserter.progress = 0;
                }
                break;
            case kInserterPhaseReturning:
                if (++inserter.progress >= swingTicks)
                {
                    inserter.phase = kInserterPhaseWaitingForItem;
                    inserter.progress = 0;
                }
                break;
            default:
                inserter.phase = kInserterPhaseWaitingForItem;
                inserter.progress = 0;
                break;
        }
    }

    static void updateInserters(State& state)
    {
        state.inserters.forEach([&](RecordId, InserterRecord& inserter) { updateInserter(state, inserter); });
    }

    void update(GameState_t& gameState)
    {
        PROFILED_FUNCTION();

        auto& state = gameState.factory;
        if (state.isEmpty())
        {
            return;
        }
        // Fixed order: belts move, then inserters pick up and drop. Containers have no per-tick behaviour.
        updateBelts(state);
        updateInserters(state);
    }
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryTopology.h"

#include "../Context.h"
#include "../GameState.h"
#include "../object/ObjectManager.h"
#include "../world/Map.h"
#include "../world/MapAnimation.h"
#include "../world/TileElementsView.h"
#include "../world/tile_element/FactoryElement.h"
#include "Belts.h"

#include <vector>

namespace OpenRCT2::Factory
{
    BeltShape getBeltShape(const FactoryElement& element)
    {
        return static_cast<BeltShape>(element.getConnectionCache() & kBeltShapeMask);
    }

    void setBeltShape(FactoryElement& element, BeltShape shape)
    {
        auto cache = static_cast<uint8_t>(element.getConnectionCache() & ~kBeltShapeMask);
        element.setConnectionCache(static_cast<uint8_t>(cache | (static_cast<uint8_t>(shape) & kBeltShapeMask)));
    }

    CoordsXYZ tileToCoords(const TileCoordsXYZ& tile)
    {
        return CoordsXYZ{ tile.toCoordsXY(), tile.z * kCoordsZStep };
    }

    CoordsXYZ neighbourTile(const CoordsXYZ& loc, Direction d)
    {
        return CoordsXYZ{ CoordsXY(loc) + CoordsDirectionDelta[d & 3], loc.z };
    }

    FactoryElement* findFactoryElement(const CoordsXYZ& loc, bool includeGhost)
    {
        if (!MapIsLocationValid(loc))
            return nullptr;
        for (auto* element : TileElementsView<FactoryElement>(loc))
        {
            if (element->getBaseZ() != loc.z)
                continue;
            if (element->isGhost() && !includeGhost)
                continue;
            return element;
        }
        return nullptr;
    }

    FactoryElement* findBeltElement(const CoordsXYZ& loc, bool includeGhost)
    {
        auto* element = findFactoryElement(loc, includeGhost);
        if (element != nullptr && element->getSubtype() == FactoryElementSubtype::belt)
            return element;
        return nullptr;
    }

    const FactoryPrototypeObject* getPrototype(ObjectEntryIndex entry)
    {
        if (entry == kObjectEntryIndexNull)
            return nullptr;
        auto& objectManager = GetContext()->GetObjectManager();
        return objectManager.GetLoadedObject<FactoryPrototypeObject>(entry);
    }

    const FactoryPrototypeObject* getPrototype(const FactoryElement& element)
    {
        return getPrototype(element.getEntryIndex());
    }

    FactoryElement* findBeltFeeder(const CoordsXYZ& loc, Direction dir, BeltShape& shape)
    {
        shape = BeltShape::straight;
        auto* behind = findBeltElement(neighbourTile(loc, oppositeOf(dir)));
        if (behind != nullptr && behind->getDirection() == dir)
        {
            return behind;
        }

        // A belt on our right side travelling leftOf... is simplest expressed by where items come from:
        // a feeder travelling rightOf(dir) sits at the left neighbour and turns left to continue in dir;
        // a feeder travelling leftOf(dir) sits at the right neighbour and turns right.
        const Direction turnLeftTravel = rightOf(dir);
        const Direction turnRightTravel = leftOf(dir);
        auto* turnLeftFeeder = findBeltElement(neighbourTile(loc, oppositeOf(turnLeftTravel)));
        if (turnLeftFeeder != nullptr && turnLeftFeeder->getDirection() != turnLeftTravel)
            turnLeftFeeder = nullptr;
        auto* turnRightFeeder = findBeltElement(neighbourTile(loc, oppositeOf(turnRightTravel)));
        if (turnRightFeeder != nullptr && turnRightFeeder->getDirection() != turnRightTravel)
            turnRightFeeder = nullptr;

        if (behind != nullptr || (turnLeftFeeder != nullptr && turnRightFeeder != nullptr))
        {
            // Fed straight from behind by a belt not pointing here, or two side feeders: stay straight and
            // treat the side belts as (unsupported) sideloads.
            return nullptr;
        }
        if (turnLeftFeeder != nullptr)
        {
            shape = BeltShape::turnLeft;
            return turnLeftFeeder;
        }
        if (turnRightFeeder != nullptr)
        {
            shape = BeltShape::turnRight;
            return turnRightFeeder;
        }
        return nullptr;
    }

    static void refreshBeltShape(const CoordsXYZ& loc)
    {
        auto* belt = findBeltElement(loc, true);
        if (belt == nullptr)
            return;
        BeltShape shape;
        findBeltFeeder(loc, belt->getDirection(), shape);
        if (getBeltShape(*belt) != shape)
        {
            setBeltShape(*belt, shape);
            MapInvalidateTileFull(loc);
        }
    }

    void refreshBeltShapesAround(const CoordsXYZ& loc)
    {
        refreshBeltShape(loc);
        for (Direction d = 0; d < 4; d++)
        {
            refreshBeltShape(neighbourTile(loc, d));
        }
    }

    static void unlinkSegment(State& state, RecordId id)
    {
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& segment) {
            if (segment.next == id)
                segment.next = kNullRecord;
        });
    }

    static void bindTileToSegment(const TileCoordsXYZ& tile, RecordId segmentId, uint8_t index)
    {
        auto* belt = findBeltElement(tileToCoords(tile));
        if (belt != nullptr)
        {
            belt->setRecordId(segmentId);
            belt->setFootprintIndex(index);
        }
    }

    /**
     * Appends `tail` onto `head` (head's last tile feeds tail's first tile). Tail items keep their gaps:
     * they are nearest the merged end. Head's front item now measures its gap from tail's rearmost item.
     */
    static void mergeSegments(State& state, RecordId headId, RecordId tailId)
    {
        auto* head = state.beltSegments.get(headId);
        auto* tail = state.beltSegments.get(tailId);
        if (head == nullptr || tail == nullptr || headId == tailId)
            return;

        const int32_t tailLength = segmentLength(*tail);
        for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
        {
            auto& headLane = head->lanes[lane];
            auto& tailLane = tail->lanes[lane];
            if (!headLane.items.empty())
            {
                const int32_t rearOfTail = tailLane.items.empty() ? tailLength : laneRearPosition(tailLane, tailLength);
                headLane.items.front().gap = static_cast<uint16_t>(headLane.items.front().gap + rearOfTail);
            }
            tailLane.items.insert(tailLane.items.end(), headLane.items.begin(), headLane.items.end());
            headLane.items = std::move(tailLane.items);
        }

        const auto headTiles = static_cast<uint8_t>(head->tiles.size());
        for (size_t i = 0; i < tail->tiles.size(); i++)
        {
            head->tiles.push_back(tail->tiles[i]);
            bindTileToSegment(tail->tiles[i], headId, static_cast<uint8_t>(headTiles + i));
        }
        head->next = tail->next;
        unlinkSegment(state, tailId);
        state.beltSegments.release(tailId);
    }

    static RecordId placeBelt(State& state, const CoordsXYZ& loc, Direction dir, const FactoryPrototypeObject& proto)
    {
        const TileCoordsXYZ tile(loc);
        BeltShape shape;
        auto* feeder = findBeltFeeder(loc, dir, shape);

        RecordId segmentId = kNullRecord;
        if (feeder != nullptr && feeder->hasRecord())
        {
            auto* feederSegment = state.beltSegments.get(feeder->getRecordId());
            if (feederSegment != nullptr && feederSegment->next == kNullRecord
                && static_cast<size_t>(feeder->getFootprintIndex()) + 1 == feederSegment->tiles.size()
                && feederSegment->tiles.size() < kMaxSegmentTiles && feederSegment->speed == proto.getBelt().speed)
            {
                segmentId = feeder->getRecordId();
                feederSegment->tiles.push_back(tile);
                bindTileToSegment(tile, segmentId, static_cast<uint8_t>(feederSegment->tiles.size() - 1));
            }
        }
        if (segmentId == kNullRecord)
        {
            auto& segment = state.beltSegments.allocateRecord(segmentId);
            segment.tiles.push_back(tile);
            segment.entry = kObjectEntryIndexNull;
            segment.speed = proto.getBelt().speed;
            bindTileToSegment(tile, segmentId, 0);
        }

        // Link or merge with the belt ahead when this tile is its feeder.
        auto aheadLoc = neighbourTile(loc, dir);
        auto* ahead = findBeltElement(aheadLoc);
        if (ahead != nullptr && ahead->hasRecord())
        {
            BeltShape aheadShape;
            auto* aheadFeeder = findBeltFeeder(aheadLoc, ahead->getDirection(), aheadShape);
            if (aheadFeeder != nullptr && aheadFeeder->getRecordId() == segmentId && ahead->getFootprintIndex() == 0)
            {
                auto* segment = state.beltSegments.get(segmentId);
                auto* aheadSegment = state.beltSegments.get(ahead->getRecordId());
                if (segment != nullptr && aheadSegment != nullptr)
                {
                    if (segment->tiles.size() + aheadSegment->tiles.size() <= kMaxSegmentTiles
                        && aheadSegment->speed == segment->speed)
                    {
                        mergeSegments(state, segmentId, ahead->getRecordId());
                    }
                    else
                    {
                        segment->next = ahead->getRecordId();
                    }
                }
            }
        }
        return segmentId;
    }

    static void rebuildLaneFromPositions(BeltLane& lane, int32_t length, const std::vector<LaneItemView>& items)
    {
        lane.items.clear();
        for (const auto& item : items)
        {
            // Positions come from a valid lane, so this only fails for items that were on the removed tile.
            laneInsertAt(lane, length, item.position, item.item);
        }
    }

    static void removeBelt(State& state, FactoryElement& element)
    {
        if (!element.hasRecord())
            return;
        const RecordId segmentId = element.getRecordId();
        auto* segment = state.beltSegments.get(segmentId);
        if (segment == nullptr)
            return;

        const size_t k = element.getFootprintIndex();
        if (k >= segment->tiles.size())
            return;
        const int32_t length = segmentLength(*segment);
        const int32_t removedStart = static_cast<int32_t>(k) * kBeltUnitsPerTile;
        const int32_t removedEnd = removedStart + kBeltUnitsPerTile;

        std::vector<TileCoordsXYZ> rightTiles(segment->tiles.begin() + static_cast<ptrdiff_t>(k + 1), segment->tiles.end());
        const RecordId oldNext = segment->next;

        std::vector<LaneItemView> leftItems[kBeltLaneCount];
        std::vector<LaneItemView> rightItems[kBeltLaneCount];
        for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
        {
            laneForEachInRange(segment->lanes[lane], length, 0, length, [&](LaneItemView view) {
                if (view.position < removedStart)
                    leftItems[lane].push_back(view);
                else if (view.position >= removedEnd)
                    rightItems[lane].push_back({ view.position - removedEnd, view.item });
                // Items on the removed tile are lost.
            });
        }

        // The right part becomes a new segment and keeps the old next link.
        if (!rightTiles.empty())
        {
            const ObjectEntryIndex entry = segment->entry;
            const uint8_t speed = segment->speed;
            RecordId rightId;
            auto& right = state.beltSegments.allocateRecord(rightId);
            right.tiles = rightTiles;
            right.entry = entry;
            right.speed = speed;
            right.next = oldNext;
            const int32_t rightLength = segmentLength(right);
            for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
            {
                rebuildLaneFromPositions(right.lanes[lane], rightLength, rightItems[lane]);
            }
            for (size_t i = 0; i < right.tiles.size(); i++)
            {
                bindTileToSegment(right.tiles[i], rightId, static_cast<uint8_t>(i));
            }
            segment = state.beltSegments.get(segmentId);
        }

        // The left part keeps the id, or the segment dies.
        if (k == 0)
        {
            unlinkSegment(state, segmentId);
            state.beltSegments.release(segmentId);
        }
        else
        {
            segment->tiles.resize(k);
            segment->next = kNullRecord;
            const int32_t leftLength = segmentLength(*segment);
            for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
            {
                rebuildLaneFromPositions(segment->lanes[lane], leftLength, leftItems[lane]);
            }
        }
        element.setRecordId(kNullRecord);
    }

    FactoryElement* placeElement(
        GameState_t& gameState, const CoordsXYZ& loc, Direction dir, ObjectEntryIndex entry, bool ghost)
    {
        auto* proto = getPrototype(entry);
        if (proto == nullptr || !proto->isPlaceable())
            return nullptr;

        auto* element = TileElementInsert<FactoryElement>(loc, 0b1111);
        if (element == nullptr)
            return nullptr;

        element->setClearanceZ(loc.z + proto->getClearance() * kCoordsZStep);
        element->setDirection(dir & 3);
        element->setSubtype(proto->getSubtype());
        element->setEntryIndex(entry);
        element->setRecordId(kNullRecord);
        element->setFootprintIndex(0);
        element->setConnectionCache(0);
        element->setFactoryFlags(FACTORY_ELEMENT_FLAG_ORIGIN);
        element->setGhost(ghost);

        auto& state = gameState.factory;
        if (!ghost)
        {
            switch (proto->getSubtype())
            {
                case FactoryElementSubtype::belt:
                    placeBelt(state, loc, dir, *proto);
                    break;
                case FactoryElementSubtype::container:
                {
                    RecordId id;
                    auto& record = state.containers.allocateRecord(id);
                    record.setLocation(TileCoordsXYZ(loc));
                    record.direction = dir & 3;
                    record.entry = entry;
                    record.slots.assign(proto->getContainer().slots, ItemStack{});
                    element->setRecordId(id);
                    break;
                }
                case FactoryElementSubtype::inserter:
                {
                    RecordId id;
                    auto& record = state.inserters.allocateRecord(id);
                    record.setLocation(TileCoordsXYZ(loc));
                    record.direction = dir & 3;
                    record.entry = entry;
                    element->setRecordId(id);
                    break;
                }
                case FactoryElementSubtype::machine:
                {
                    RecordId id;
                    auto& record = state.machines.allocateRecord(id);
                    record.setLocation(TileCoordsXYZ(loc));
                    record.direction = dir & 3;
                    record.entry = entry;
                    const auto& props = proto->getMachine();
                    record.kind = static_cast<uint8_t>(props.kind);
                    record.inputs.assign(props.inputSlots, ItemStack{});
                    record.outputs.assign(props.outputSlots, ItemStack{});
                    record.status = static_cast<uint8_t>(MachineStatus::idle);
                    element->setRecordId(id);
                    if (props.energy == EnergySource::electric || proto->isGenerator())
                        state.powerDirty = true;
                    break;
                }
                case FactoryElementSubtype::pole:
                {
                    RecordId id;
                    auto& record = state.poles.allocateRecord(id);
                    record.setLocation(TileCoordsXYZ(loc));
                    record.direction = dir & 3;
                    record.entry = entry;
                    element->setRecordId(id);
                    state.powerDirty = true;
                    break;
                }
                default:
                    // Other kinds get records in later milestones; they still occupy the tile.
                    break;
            }
            state.topologyVersion++;
        }

        refreshBeltShapesAround(loc);
        MapInvalidateTileFull(loc);
        MapAnimations::MarkTileForInvalidation(TileCoordsXY(loc));
        return element;
    }

    void removeElement(GameState_t& gameState, FactoryElement& element, const CoordsXYZ& loc)
    {
        auto& state = gameState.factory;
        if (!element.isGhost())
        {
            switch (element.getSubtype())
            {
                case FactoryElementSubtype::belt:
                    removeBelt(state, element);
                    break;
                case FactoryElementSubtype::container:
                    state.containers.release(element.getRecordId());
                    break;
                case FactoryElementSubtype::inserter:
                    state.inserters.release(element.getRecordId());
                    break;
                case FactoryElementSubtype::machine:
                    state.machines.release(element.getRecordId());
                    state.powerDirty = true;
                    break;
                case FactoryElementSubtype::pole:
                    state.poles.release(element.getRecordId());
                    state.powerDirty = true;
                    break;
                default:
                    break;
            }
            state.topologyVersion++;
        }
        MapInvalidateTileFull(loc);
        TileElementRemove(reinterpret_cast<TileElement*>(&element));
        refreshBeltShapesAround(loc);
    }

    void postLoad(GameState_t& gameState)
    {
        auto& state = gameState.factory;
        if (state.isEmpty())
            return;

        // Drop records whose tiles no longer carry a matching element. A consistent save changes nothing, so a
        // joining client ends up with exactly the host's state (topologyVersion included).
        bool changed = false;
        std::vector<RecordId> dead;
        state.beltSegments.forEach([&](RecordId id, BeltSegmentRecord& segment) {
            for (size_t i = 0; i < segment.tiles.size(); i++)
            {
                auto* belt = findBeltElement(tileToCoords(segment.tiles[i]));
                if (belt == nullptr || belt->getRecordId() != id || belt->getFootprintIndex() != i)
                {
                    dead.push_back(id);
                    break;
                }
            }
        });
        for (auto id : dead)
        {
            unlinkSegment(state, id);
            state.beltSegments.release(id);
            changed = true;
        }
        dead.clear();
        state.containers.forEach([&](RecordId id, ContainerRecord& record) {
            auto* element = findFactoryElement(tileToCoords(record.location()));
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::container || element->getRecordId() != id)
                dead.push_back(id);
        });
        for (auto id : dead)
        {
            state.containers.release(id);
            changed = true;
        }
        dead.clear();
        state.inserters.forEach([&](RecordId id, InserterRecord& record) {
            auto* element = findFactoryElement(tileToCoords(record.location()));
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::inserter || element->getRecordId() != id)
                dead.push_back(id);
        });
        for (auto id : dead)
        {
            state.inserters.release(id);
            changed = true;
        }
        dead.clear();
        state.machines.forEach([&](RecordId id, MachineRecord& record) {
            auto* element = findFactoryElement(tileToCoords(record.location()));
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::machine || element->getRecordId() != id)
                dead.push_back(id);
        });
        for (auto id : dead)
        {
            state.machines.release(id);
            changed = true;
        }
        dead.clear();
        state.poles.forEach([&](RecordId id, PoleRecord& record) {
            auto* element = findFactoryElement(tileToCoords(record.location()));
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::pole || element->getRecordId() != id)
                dead.push_back(id);
        });
        for (auto id : dead)
        {
            state.poles.release(id);
            changed = true;
        }
        if (changed)
            state.powerDirty = true;

        if (changed)
        {
            state.topologyVersion++;
        }
    }
} // namespace OpenRCT2::Factory

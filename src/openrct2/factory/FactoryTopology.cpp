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
#include "Fluids.h"

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

    bool isUndergroundExit(const FactoryElement& element)
    {
        return (element.getConnectionCache() & kUndergroundExitFlag) != 0;
    }

    static void setUndergroundExit(FactoryElement& element, bool exit)
    {
        auto cache = element.getConnectionCache();
        element.setConnectionCache(
            static_cast<uint8_t>(exit ? (cache | kUndergroundExitFlag) : (cache & ~kUndergroundExitFlag)));
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

    static bool isBeltLike(const FactoryElement& element)
    {
        const auto subtype = element.getSubtype();
        return subtype == FactoryElementSubtype::belt || subtype == FactoryElementSubtype::undergroundBelt
            || subtype == FactoryElementSubtype::splitter;
    }

    // Any belt-like element at loc (belt, underground tile, splitter tile).
    static FactoryElement* findBeltLikeElement(const CoordsXYZ& loc)
    {
        auto* element = findFactoryElement(loc);
        return element != nullptr && isBeltLike(*element) ? element : nullptr;
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

    // True when `element` (at the tile behind) pushes items straight into a belt travelling `dir`.
    static bool outputsStraightInto(const FactoryElement& element, Direction dir)
    {
        if (element.getDirection() != dir)
            return false;
        switch (element.getSubtype())
        {
            case FactoryElementSubtype::belt:
            case FactoryElementSubtype::splitter:
                return true;
            case FactoryElementSubtype::undergroundBelt:
                return isUndergroundExit(element);
            default:
                return false;
        }
    }

    FactoryElement* findBeltFeeder(const CoordsXYZ& loc, Direction dir, BeltShape& shape)
    {
        shape = BeltShape::straight;
        auto* behind = findBeltLikeElement(neighbourTile(loc, oppositeOf(dir)));
        if (behind != nullptr && outputsStraightInto(*behind, dir))
        {
            return behind;
        }

        // A feeder travelling rightOf(dir) sits at the left neighbour and turns left to continue in dir;
        // a feeder travelling leftOf(dir) sits at the right neighbour and turns right. Only plain belts curve.
        const Direction turnLeftTravel = rightOf(dir);
        const Direction turnRightTravel = leftOf(dir);
        auto* turnLeftFeeder = findBeltElement(neighbourTile(loc, oppositeOf(turnLeftTravel)));
        if (turnLeftFeeder != nullptr && turnLeftFeeder->getDirection() != turnLeftTravel)
            turnLeftFeeder = nullptr;
        auto* turnRightFeeder = findBeltElement(neighbourTile(loc, oppositeOf(turnRightTravel)));
        if (turnRightFeeder != nullptr && turnRightFeeder->getDirection() != turnRightTravel)
            turnRightFeeder = nullptr;

        if (behind != nullptr && behind->getDirection() == oppositeOf(dir)
            && behind->getSubtype() == FactoryElementSubtype::belt)
        {
            // Head-on belts never feed each other; side belts may still curve in.
        }
        if (turnLeftFeeder != nullptr && turnRightFeeder != nullptr)
        {
            return nullptr; // two side feeders: both sideload onto a straight belt
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
            if (segment.next == id && segment.getNextKind() != BeltLinkKind::splitter)
            {
                segment.next = kNullRecord;
                segment.nextKind = static_cast<uint8_t>(BeltLinkKind::none);
            }
        });
    }

    static void unlinkSplitter(State& state, RecordId id)
    {
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& segment) {
            if (segment.next == id && segment.getNextKind() == BeltLinkKind::splitter)
            {
                segment.next = kNullRecord;
                segment.nextKind = static_cast<uint8_t>(BeltLinkKind::none);
            }
        });
    }

    static void bindTileToSegment(const TileCoordsXYZ& tile, RecordId segmentId, uint8_t index)
    {
        auto* element = findBeltLikeElement(tileToCoords(tile));
        if (element != nullptr && element->getSubtype() != FactoryElementSubtype::splitter)
        {
            element->setRecordId(segmentId);
            element->setFootprintIndex(index);
        }
    }

    /**
     * Recomputes what the segment's last tile feeds: the start of the belt-like thing ahead when we are its
     * feeder, a sideload into a belt we are not the feeder of, or a splitter input side.
     */
    static void computeLink(State& state, RecordId segmentId)
    {
        auto* segment = state.beltSegments.get(segmentId);
        if (segment == nullptr || segment->tiles.empty())
            return;
        segment->next = kNullRecord;
        segment->nextKind = static_cast<uint8_t>(BeltLinkKind::none);

        const auto lastLoc = tileToCoords(segment->tiles.back());
        auto* last = findBeltLikeElement(lastLoc);
        if (last == nullptr)
            return;
        const Direction dir = last->getDirection();
        const auto aheadLoc = neighbourTile(lastLoc, dir);
        auto* ahead = findBeltLikeElement(aheadLoc);
        if (ahead == nullptr || !ahead->hasRecord())
            return;

        switch (ahead->getSubtype())
        {
            case FactoryElementSubtype::belt:
            {
                if (ahead->getRecordId() == segmentId)
                    return; // a loop onto ourselves is not supported
                BeltShape aheadShape;
                auto* feeder = findBeltFeeder(aheadLoc, ahead->getDirection(), aheadShape);
                if (feeder == last && ahead->getFootprintIndex() == 0)
                {
                    segment->next = ahead->getRecordId();
                    segment->nextKind = static_cast<uint8_t>(BeltLinkKind::segment);
                    return;
                }
                if (ahead->getDirection() == oppositeOf(dir) || ahead->getDirection() == dir)
                    return; // head-on or behind-fed straight continuation that already has a feeder
                auto* aheadSegment = state.beltSegments.get(ahead->getRecordId());
                if (aheadSegment == nullptr)
                    return;
                segment->next = ahead->getRecordId();
                segment->nextKind = static_cast<uint8_t>(BeltLinkKind::sideload);
                segment->nextPos = segmentTileStart(*aheadSegment, ahead->getFootprintIndex()) + kBeltUnitsPerTile / 2;
                // Entering from the belt's left puts items on its left lane.
                segment->nextLane = dir == rightOf(ahead->getDirection()) ? kLaneLeft : kLaneRight;
                return;
            }
            case FactoryElementSubtype::undergroundBelt:
                if (ahead->getDirection() == dir && !isUndergroundExit(*ahead) && ahead->getRecordId() != segmentId)
                {
                    segment->next = ahead->getRecordId();
                    segment->nextKind = static_cast<uint8_t>(BeltLinkKind::segment);
                }
                return;
            case FactoryElementSubtype::splitter:
                if (ahead->getDirection() == dir)
                {
                    segment->next = ahead->getRecordId();
                    segment->nextKind = static_cast<uint8_t>(BeltLinkKind::splitter);
                    segment->nextLane = ahead->getFootprintIndex();
                }
                return;
            default:
                return;
        }
    }

    // Recomputes links of every segment whose last tile is at or next to loc.
    static void relinkAround(State& state, const CoordsXYZ& loc)
    {
        std::vector<RecordId> ids;
        auto consider = [&](const CoordsXYZ& at) {
            auto* element = findBeltLikeElement(at);
            if (element == nullptr || element->getSubtype() == FactoryElementSubtype::splitter || !element->hasRecord())
                return;
            auto* segment = state.beltSegments.get(element->getRecordId());
            if (segment == nullptr || segment->tiles.empty())
                return;
            if (tileToCoords(segment->tiles.back()) == at)
                ids.push_back(element->getRecordId());
        };
        consider(loc);
        for (Direction d = 0; d < 4; d++)
            consider(neighbourTile(loc, d));
        for (auto id : ids)
            computeLink(state, id);
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
        head->nextKind = tail->nextKind;
        head->nextLane = tail->nextLane;
        head->nextPos = tail->nextPos;
        // Sideloads into the tail now point at the merged segment at a shifted position.
        const int32_t shift = static_cast<int32_t>(headTiles) * kBeltUnitsPerTile;
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& other) {
            if (other.next == tailId && other.getNextKind() == BeltLinkKind::sideload)
            {
                other.next = headId;
                other.nextPos += shift;
            }
            else if (other.next == tailId && other.getNextKind() == BeltLinkKind::segment)
            {
                other.next = kNullRecord;
                other.nextKind = static_cast<uint8_t>(BeltLinkKind::none);
            }
        });
        state.beltSegments.release(tailId);
    }

    static RecordId placeBelt(State& state, const CoordsXYZ& loc, Direction dir, const FactoryPrototypeObject& proto)
    {
        const TileCoordsXYZ tile(loc);
        BeltShape shape;
        auto* feeder = findBeltFeeder(loc, dir, shape);

        RecordId segmentId = kNullRecord;
        if (feeder != nullptr && feeder->hasRecord() && feeder->getSubtype() == FactoryElementSubtype::belt)
        {
            auto* feederSegment = state.beltSegments.get(feeder->getRecordId());
            if (feederSegment != nullptr && feederSegment->extraLength == 0
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

        // Merge with the plain belt ahead when this tile is its feeder and both stay short enough.
        auto aheadLoc = neighbourTile(loc, dir);
        auto* ahead = findBeltElement(aheadLoc);
        if (ahead != nullptr && ahead->hasRecord() && ahead->getRecordId() != segmentId)
        {
            BeltShape aheadShape;
            auto* aheadFeeder = findBeltFeeder(aheadLoc, ahead->getDirection(), aheadShape);
            if (aheadFeeder != nullptr && aheadFeeder->getRecordId() == segmentId && ahead->getFootprintIndex() == 0)
            {
                auto* segment = state.beltSegments.get(segmentId);
                auto* aheadSegment = state.beltSegments.get(ahead->getRecordId());
                if (segment != nullptr && aheadSegment != nullptr && aheadSegment->extraLength == 0
                    && segment->tiles.size() + aheadSegment->tiles.size() <= kMaxSegmentTiles
                    && aheadSegment->speed == segment->speed)
                {
                    mergeSegments(state, segmentId, ahead->getRecordId());
                }
            }
        }
        computeLink(state, segmentId);
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
        const uint8_t oldNextKind = segment->nextKind;
        const uint8_t oldNextLane = segment->nextLane;
        const int32_t oldNextPos = segment->nextPos;

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

        // The right part becomes a new segment and keeps the old link; sideloads into it shift.
        RecordId rightId = kNullRecord;
        if (!rightTiles.empty())
        {
            const ObjectEntryIndex entry = segment->entry;
            const uint8_t speed = segment->speed;
            auto& right = state.beltSegments.allocateRecord(rightId);
            right.tiles = rightTiles;
            right.entry = entry;
            right.speed = speed;
            right.next = oldNext;
            right.nextKind = oldNextKind;
            right.nextLane = oldNextLane;
            right.nextPos = oldNextPos;
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
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& other) {
            if (other.next != segmentId || other.getNextKind() != BeltLinkKind::sideload)
                return;
            if (other.nextPos >= removedEnd && rightId != kNullRecord)
            {
                other.next = rightId;
                other.nextPos -= removedEnd;
            }
            else if (other.nextPos >= removedStart)
            {
                other.next = kNullRecord;
                other.nextKind = static_cast<uint8_t>(BeltLinkKind::none);
            }
        });

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
            segment->nextKind = static_cast<uint8_t>(BeltLinkKind::none);
            const int32_t leftLength = segmentLength(*segment);
            for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
            {
                rebuildLaneFromPositions(segment->lanes[lane], leftLength, leftItems[lane]);
            }
        }
        element.setRecordId(kNullRecord);
    }

    // Finds the unpaired underground entrance behind `loc` within reach, travelling `dir`.
    static FactoryElement* findUndergroundEntranceBehind(const CoordsXYZ& loc, Direction dir, int32_t reach, int32_t& gap)
    {
        auto at = loc;
        for (int32_t i = 1; i <= reach + 1; i++)
        {
            at = neighbourTile(at, oppositeOf(dir));
            auto* element = findFactoryElement(at);
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::undergroundBelt)
                continue;
            if (element->getDirection() != dir)
                continue;
            if (!element->hasRecord() && !isUndergroundExit(*element))
            {
                gap = i - 1;
                return element;
            }
            return nullptr; // another pair in the way
        }
        return nullptr;
    }

    static void placeUnderground(
        State& state, FactoryElement& element, const CoordsXYZ& loc, Direction dir, const FactoryPrototypeObject& proto)
    {
        int32_t gap = 0;
        auto* entrance = findUndergroundEntranceBehind(loc, dir, proto.getBelt().reach, gap);
        if (entrance == nullptr)
        {
            // Unpaired: an entrance waiting for its exit. No record until then.
            setUndergroundExit(element, false);
            return;
        }
        setUndergroundExit(element, true);
        RecordId segmentId;
        auto& segment = state.beltSegments.allocateRecord(segmentId);
        const auto entranceLoc = neighbourTile(loc, oppositeOf(dir));
        (void)entranceLoc;
        auto entranceTile = TileCoordsXYZ(tileToCoords(TileCoordsXYZ(loc)));
        entranceTile = TileCoordsXYZ(CoordsXYZ{ CoordsXY(loc) - CoordsDirectionDelta[dir & 3] * (gap + 1), loc.z });
        segment.tiles = { entranceTile, TileCoordsXYZ(loc) };
        segment.entry = kObjectEntryIndexNull;
        segment.speed = proto.getBelt().speed;
        segment.extraLength = static_cast<uint16_t>(gap * kBeltUnitsPerTile);
        entrance->setRecordId(segmentId);
        entrance->setFootprintIndex(0);
        element.setRecordId(segmentId);
        element.setFootprintIndex(1);
        computeLink(state, segmentId);
        relinkAround(state, tileToCoords(entranceTile));
    }

    static void removeUnderground(State& state, FactoryElement& element)
    {
        if (!element.hasRecord())
            return;
        const RecordId segmentId = element.getRecordId();
        auto* segment = state.beltSegments.get(segmentId);
        if (segment != nullptr)
        {
            // The partner becomes an unpaired entrance again (items in the tunnel are lost).
            for (auto& tile : segment->tiles)
            {
                auto* partner = findFactoryElement(tileToCoords(tile));
                if (partner != nullptr && partner != &element
                    && partner->getSubtype() == FactoryElementSubtype::undergroundBelt)
                {
                    partner->setRecordId(kNullRecord);
                    partner->setFootprintIndex(0);
                    setUndergroundExit(*partner, false);
                    MapInvalidateTileFull(tileToCoords(tile));
                }
            }
            unlinkSegment(state, segmentId);
            state.beltSegments.release(segmentId);
        }
        element.setRecordId(kNullRecord);
    }

    static CoordsXYZ splitterPartnerTile(const CoordsXYZ& loc, Direction dir, uint8_t side)
    {
        // Side 0 is the origin (left of travel); side 1 sits to its right.
        return side == 0 ? neighbourTile(loc, rightOf(dir)) : neighbourTile(loc, leftOf(dir));
    }

    bool splitterSecondTile(const CoordsXYZ& loc, Direction dir, CoordsXYZ& second)
    {
        second = splitterPartnerTile(loc, dir, 0);
        return MapIsLocationValid(second);
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

        const auto subtype = proto->getSubtype();
        element->setClearanceZ(loc.z + proto->getClearance() * kCoordsZStep);
        element->setDirection(dir & 3);
        element->setSubtype(subtype);
        element->setEntryIndex(entry);
        element->setRecordId(kNullRecord);
        element->setFootprintIndex(0);
        element->setConnectionCache(0);
        element->setFactoryFlags(FACTORY_ELEMENT_FLAG_ORIGIN);
        element->setGhost(ghost);

        FactoryElement* second = nullptr;
        if (subtype == FactoryElementSubtype::splitter)
        {
            CoordsXYZ secondLoc;
            if (!splitterSecondTile(loc, dir, secondLoc))
            {
                TileElementRemove(reinterpret_cast<TileElement*>(element));
                return nullptr;
            }
            second = TileElementInsert<FactoryElement>(secondLoc, 0b1111);
            if (second == nullptr)
            {
                element = findFactoryElement(loc, ghost);
                if (element != nullptr)
                    TileElementRemove(reinterpret_cast<TileElement*>(element));
                return nullptr;
            }
            // Inserting may have moved the element array; look the origin up again.
            element = findFactoryElement(loc, ghost);
            second->setClearanceZ(secondLoc.z + proto->getClearance() * kCoordsZStep);
            second->setDirection(dir & 3);
            second->setSubtype(subtype);
            second->setEntryIndex(entry);
            second->setRecordId(kNullRecord);
            second->setFootprintIndex(1);
            second->setConnectionCache(0);
            second->setFactoryFlags(0);
            second->setGhost(ghost);
            MapInvalidateTileFull(secondLoc);
            MapAnimations::MarkTileForInvalidation(TileCoordsXY(secondLoc));
        }

        auto& state = gameState.factory;
        if (!ghost)
        {
            switch (subtype)
            {
                case FactoryElementSubtype::belt:
                    placeBelt(state, loc, dir, *proto);
                    break;
                case FactoryElementSubtype::undergroundBelt:
                    placeUnderground(state, *element, loc, dir, *proto);
                    break;
                case FactoryElementSubtype::splitter:
                {
                    RecordId id;
                    auto& record = state.splitters.allocateRecord(id);
                    record.setLocation(TileCoordsXYZ(loc));
                    record.direction = dir & 3;
                    record.entry = entry;
                    element->setRecordId(id);
                    if (second != nullptr)
                        second->setRecordId(id);
                    break;
                }
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
                    if (!props.fluidBoxes.empty())
                    {
                        record.fluidNetworks.assign(props.fluidBoxes.size(), kNullRecord);
                        state.fluidDirty = true;
                    }
                    break;
                }
                case FactoryElementSubtype::pipe:
                {
                    RecordId id;
                    auto& record = state.pipes.allocateRecord(id);
                    record.setLocation(TileCoordsXYZ(loc));
                    record.direction = dir & 3;
                    record.entry = entry;
                    element->setRecordId(id);
                    state.fluidDirty = true;
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
            relinkAround(state, loc);
            if (second != nullptr)
                relinkAround(state, splitterPartnerTile(loc, dir, 0));
        }

        refreshBeltShapesAround(loc);
        refreshPipeConnectionsAround(loc);
        MapInvalidateTileFull(loc);
        MapAnimations::MarkTileForInvalidation(TileCoordsXY(loc));
        return element;
    }

    void removeElement(GameState_t& gameState, FactoryElement& element, const CoordsXYZ& loc)
    {
        auto& state = gameState.factory;
        const Direction dir = element.getDirection();
        const auto subtype = element.getSubtype();
        CoordsXYZ partnerLoc{};
        bool hasPartner = false;
        if (subtype == FactoryElementSubtype::splitter)
        {
            partnerLoc = splitterPartnerTile(loc, dir, element.getFootprintIndex());
            hasPartner = true;
        }

        if (!element.isGhost())
        {
            switch (subtype)
            {
                case FactoryElementSubtype::belt:
                    removeBelt(state, element);
                    break;
                case FactoryElementSubtype::undergroundBelt:
                    removeUnderground(state, element);
                    break;
                case FactoryElementSubtype::splitter:
                    if (element.hasRecord())
                    {
                        unlinkSplitter(state, element.getRecordId());
                        state.splitters.release(element.getRecordId());
                    }
                    break;
                case FactoryElementSubtype::container:
                    state.containers.release(element.getRecordId());
                    break;
                case FactoryElementSubtype::inserter:
                    state.inserters.release(element.getRecordId());
                    break;
                case FactoryElementSubtype::machine:
                    if (auto* machine = state.machines.get(element.getRecordId()); machine != nullptr)
                    {
                        if (!machine->fluidNetworks.empty())
                            state.fluidDirty = true;
                    }
                    state.machines.release(element.getRecordId());
                    state.powerDirty = true;
                    break;
                case FactoryElementSubtype::pipe:
                    state.pipes.release(element.getRecordId());
                    state.fluidDirty = true;
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
        const bool ghost = element.isGhost();
        MapInvalidateTileFull(loc);
        TileElementRemove(reinterpret_cast<TileElement*>(&element));
        if (hasPartner)
        {
            auto* partner = findFactoryElement(partnerLoc, ghost);
            if (partner != nullptr && partner->getSubtype() == FactoryElementSubtype::splitter && partner->isGhost() == ghost)
            {
                MapInvalidateTileFull(partnerLoc);
                TileElementRemove(reinterpret_cast<TileElement*>(partner));
            }
        }
        if (!ghost)
        {
            relinkAround(state, loc);
            if (hasPartner)
                relinkAround(state, partnerLoc);
        }
        refreshBeltShapesAround(loc);
        refreshPipeConnectionsAround(loc);
        if (hasPartner)
            refreshBeltShapesAround(partnerLoc);
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
                auto* belt = findBeltLikeElement(tileToCoords(segment.tiles[i]));
                if (belt == nullptr || belt->getSubtype() == FactoryElementSubtype::splitter || belt->getRecordId() != id
                    || belt->getFootprintIndex() != i)
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
        state.splitters.forEach([&](RecordId id, SplitterRecord& record) {
            auto* element = findFactoryElement(tileToCoords(record.location()));
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::splitter || element->getRecordId() != id)
                dead.push_back(id);
        });
        for (auto id : dead)
        {
            unlinkSplitter(state, id);
            state.splitters.release(id);
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
        dead.clear();
        state.pipes.forEach([&](RecordId id, PipeRecord& record) {
            auto* element = findFactoryElement(tileToCoords(record.location()));
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::pipe || element->getRecordId() != id)
                dead.push_back(id);
        });
        for (auto id : dead)
        {
            state.pipes.release(id);
            changed = true;
        }
        if (changed)
        {
            state.powerDirty = true;
            state.fluidDirty = true;
            state.topologyVersion++;
        }
    }
} // namespace OpenRCT2::Factory

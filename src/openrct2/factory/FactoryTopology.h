/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Links FactoryElements on the map to records in FactoryState.
//
// Directions follow CoordsDirectionDelta: 0 = -x, 1 = +y, 2 = +x, 3 = -y. On screen at view rotation 0,
// (d + 1) & 3 is 90 degrees clockwise ("right of travel") and (d + 3) & 3 is anticlockwise ("left").

#pragma once

#include "../world/Location.hpp"
#include "FactoryPrototypeObject.h"
#include "FactoryRecords.h"
#include "FactoryState.h"

namespace OpenRCT2
{
    struct GameState_t;
    struct FactoryElement;
} // namespace OpenRCT2

namespace OpenRCT2::Factory
{
    constexpr Direction leftOf(Direction d)
    {
        return (d + 3) & 3;
    }
    constexpr Direction rightOf(Direction d)
    {
        return (d + 1) & 3;
    }
    constexpr Direction oppositeOf(Direction d)
    {
        return (d + 2) & 3;
    }

    // Lane indices: 0 = left of travel, 1 = right of travel.
    constexpr uint8_t kLaneLeft = 0;
    constexpr uint8_t kLaneRight = 1;

    // connectionCache layout for belts: bits 0-1 hold the BeltShape; undergrounds use bit 2 for "exit".
    constexpr uint8_t kBeltShapeMask = 0b11;
    constexpr uint8_t kUndergroundExitFlag = 0b100;

    BeltShape getBeltShape(const FactoryElement& element);
    void setBeltShape(FactoryElement& element, BeltShape shape);
    bool isUndergroundExit(const FactoryElement& element);

    // The second tile a splitter facing `dir` placed at loc occupies (to its right).
    bool splitterSecondTile(const CoordsXYZ& loc, Direction dir, CoordsXYZ& second);

    /**
     * Machine footprints are squares of `size` tiles whose origin (the record's location) is the minimum corner;
     * footprint index = dy * size + dx. Every other placeable is 1x1 except splitters (two tiles, see above).
     */
    uint8_t footprintSize(const FactoryPrototypeObject* proto);
    // The origin tile of the footprint that the element at loc belongs to.
    CoordsXYZ footprintOrigin(const FactoryElement& element, const CoordsXYZ& loc);
    // The centre tile, rounded towards the origin for even sizes.
    CoordsXYZ footprintCentre(const CoordsXYZ& origin, uint8_t size);
    // The tile just beyond the centre of the footprint's edge on side d (where drills drop, fluid connects).
    CoordsXYZ footprintEdgeNeighbour(const CoordsXYZ& origin, uint8_t size, Direction d);
    // Chebyshev distance from a tile to the nearest tile of the footprint (0 inside it).
    int32_t distanceToFootprint(int32_t x, int32_t y, int32_t originX, int32_t originY, uint8_t size);
    // Which image slice footprint tile `index` uses at a view rotation: its row-major position in the rotated square.
    uint8_t footprintViewSlice(uint8_t index, uint8_t size, uint8_t rotation);

    CoordsXYZ tileToCoords(const TileCoordsXYZ& tile);
    CoordsXYZ neighbourTile(const CoordsXYZ& loc, Direction d);

    // The first factory element at this tile whose base matches loc.z. Ghosts are skipped unless asked for.
    FactoryElement* findFactoryElement(const CoordsXYZ& loc, bool includeGhost = false);
    FactoryElement* findBeltElement(const CoordsXYZ& loc, bool includeGhost = false);

    const FactoryPrototypeObject* getPrototype(ObjectEntryIndex entry);
    const FactoryPrototypeObject* getPrototype(const FactoryElement& element);

    /**
     * Which belt, if any, feeds items straight into the belt at `loc` travelling in `dir`: the tile behind
     * when its belt points the same way, otherwise a single side belt pointing into this tile (a curve).
     * Returns the feeding element and sets `shape` accordingly. Two side feeders or a straight feeder plus
     * side feeders make the side ones sideloads, which M1 does not support (they dead-end).
     */
    FactoryElement* findBeltFeeder(const CoordsXYZ& loc, Direction dir, BeltShape& shape);

    /**
     * Inserts the tile element (and, unless ghost, the record) for a placeable prototype. The caller has
     * already validated clearance and ownership. Returns nullptr when the map is out of elements or the
     * prototype is not placeable.
     */
    FactoryElement* placeElement(
        GameState_t& gameState, const CoordsXYZ& loc, Direction dir, ObjectEntryIndex entry, bool ghost);

    /**
     * Removes the element and frees its record, splitting belt segments as needed. Items on a removed
     * belt tile are lost.
     */
    void removeElement(GameState_t& gameState, FactoryElement& element, const CoordsXYZ& loc);

    // Recomputes the belt shape of the belt at loc (if any) and its four neighbours after a change.
    void refreshBeltShapesAround(const CoordsXYZ& loc);

    // After load: validates element <-> record links and drops anything inconsistent.
    void postLoad(GameState_t& gameState);
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Plain data records stored in FactoryState pools.
//
// Every record exposes `template<typename V> void visit(V& v)` which enumerates its fields in a fixed
// order. The same visitor drives park-file persistence (OrcaStream chunks), the multiplayer sync
// checksum (DataSerialiser) and game-state snapshots, so a field can never be saved but not hashed.
// Visitors provide `operator()(T&)` for scalars, `vec(std::vector<T>&, elementVisit)` for vectors and
// `isReading()`.

#pragma once

#include "../object/ObjectTypes.h"
#include "../world/Location.hpp"
#include "../world/tile_element/FactoryElement.h"

#include <array>
#include <cstdint>
#include <vector>

namespace OpenRCT2::Factory
{
    using RecordId = FactoryRecordId;
    constexpr RecordId kNullRecord = kFactoryRecordNull;

    // Belt geometry. Positions along a segment are in 1/256 tile units; items may never be closer
    // than kBeltItemSpacing. Speeds are units per tick: 12/24/36 = 15/30/45 items per second at 40 Hz.
    constexpr int32_t kBeltUnitsPerTile = 256;
    constexpr int32_t kBeltItemSpacing = 64;
    constexpr uint8_t kBeltLaneCount = 2;
    constexpr uint8_t kMaxSegmentTiles = 32;

    // Default element visitor: the element has its own visit().
    struct VisitElement
    {
        template<typename T, typename V>
        void operator()(T& element, V& v) const
        {
            element.visit(v);
        }
    };

    struct VisitTileCoords
    {
        template<typename V>
        void operator()(TileCoordsXYZ& coords, V& v) const
        {
            v(coords.x);
            v(coords.y);
            v(coords.z);
        }
    };

    /**
     * Points at a record in another pool, e.g. an inserter's pickup and drop targets. `kind` is a
     * FactoryElementSubtype stored as a byte.
     */
    struct RecordRef
    {
        uint8_t kind{ static_cast<uint8_t>(FactoryElementSubtype::count) };
        RecordId id{ kNullRecord };

        bool isNull() const
        {
            return id == kNullRecord;
        }

        FactoryElementSubtype getKind() const
        {
            return static_cast<FactoryElementSubtype>(kind);
        }

        template<typename V>
        void visit(V& v)
        {
            v(kind);
            v(id);
        }
    };

    struct ItemStack
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint16_t count{};

        bool isEmpty() const
        {
            return count == 0 || item == kObjectEntryIndexNull;
        }

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(count);
        }
    };

    /**
     * Fields shared by every record anchored to a single origin tile.
     */
    struct RecordBase
    {
        int32_t x{};
        int32_t y{};
        int32_t z{};
        uint8_t direction{};
        ObjectEntryIndex entry{ kObjectEntryIndexNull };

        TileCoordsXYZ location() const
        {
            return TileCoordsXYZ{ x, y, z };
        }

        void setLocation(const TileCoordsXYZ& loc)
        {
            x = loc.x;
            y = loc.y;
            z = loc.z;
        }

        template<typename V>
        void visitBase(V& v)
        {
            v(x);
            v(y);
            v(z);
            v(direction);
            v(entry);
        }
    };

    struct ContainerRecord : RecordBase
    {
        std::vector<ItemStack> slots;

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v.vec(slots, VisitElement{});
        }
    };

    struct BeltItem
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        // Distance in belt units from the item ahead of this one (or from the segment end for the
        // first item), never less than kBeltItemSpacing.
        uint16_t gap{};

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(gap);
        }
    };

    struct BeltLane
    {
        std::vector<BeltItem> items;

        template<typename V>
        void visit(V& v)
        {
            v.vec(items, VisitElement{});
        }
    };

    struct BeltSegmentRecord
    {
        // Tiles from the segment start to its end; each tile's own FactoryElement carries its direction.
        std::vector<TileCoordsXYZ> tiles;
        ObjectEntryIndex entry{ kObjectEntryIndexNull };
        uint8_t speed{};
        std::array<BeltLane, kBeltLaneCount> lanes{};
        // Segment fed by this one's last tile, or kNullRecord.
        RecordId next{ kNullRecord };

        template<typename V>
        void visit(V& v)
        {
            v.vec(tiles, VisitTileCoords{});
            v(entry);
            v(speed);
            for (auto& lane : lanes)
            {
                lane.visit(v);
            }
            v(next);
        }
    };

    struct InserterRecord : RecordBase
    {
        uint8_t phase{};
        uint16_t progress{};
        ItemStack hand;
        RecordRef source;
        RecordRef target;
        uint32_t topologyVersionSeen{};

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v(phase);
            v(progress);
            hand.visit(v);
            source.visit(v);
            target.visit(v);
            v(topologyVersionSeen);
        }
    };
} // namespace OpenRCT2::Factory

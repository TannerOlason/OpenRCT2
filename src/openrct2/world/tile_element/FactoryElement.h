/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned tile element that anchors factory records (belts, inserters, machines...) to the map.

#pragma once

#include "../../object/ObjectTypes.h"
#include "TileElementBase.h"

namespace OpenRCT2
{
    /**
     * Stable id of a record in one of the FactoryState pools. Ghost elements carry kFactoryRecordNull and never
     * own a record.
     */
    using FactoryRecordId = uint32_t;
    constexpr FactoryRecordId kFactoryRecordNull = 0xFFFFFFFFu;

    /**
     * Which pool the record id of a FactoryElement refers to. Stored in payload byte 5.
     */
    enum class FactoryElementSubtype : uint8_t
    {
        belt = 0,
        undergroundBelt = 1,
        splitter = 2,
        inserter = 3,
        container = 4,
        machine = 5,
        pole = 6,
        pipe = 7,
        loader = 8,
        count,
    };

    enum : uint8_t
    {
        FACTORY_ELEMENT_FLAG_ORIGIN = (1 << 0),  // This tile is the origin tile of a multi-tile footprint.
        FACTORY_ELEMENT_FLAG_WORKING = (1 << 1), // Paint hint: the record was active on its last update.
        FACTORY_ELEMENT_FLAG_DAMAGED = (1 << 2), // Paint hint: the record's health is zero.
    };

#pragma pack(push, 1)
    struct FactoryElement : TileElementBase
    {
        static constexpr TileElementType kElementType = TileElementType::factory;

    private:
        uint8_t subtype;         // 5
        uint32_t recordId;       // 6..9, little endian, see FactoryRecordId
        uint8_t footprintIndex;  // 10, index of this tile inside a multi-tile footprint (0 = origin)
        ObjectEntryIndex entry;  // 11..12, factoryPrototype object entry index
        uint8_t connectionCache; // 13, subtype specific: belt shape/tier, pipe connections, pole wires
        uint8_t factoryFlags;    // 14, FACTORY_ELEMENT_FLAG_*
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
        uint8_t pad0F;
#pragma clang diagnostic pop
    public:
        FactoryElementSubtype getSubtype() const;
        void setSubtype(FactoryElementSubtype newSubtype);

        FactoryRecordId getRecordId() const;
        void setRecordId(FactoryRecordId newRecordId);
        bool hasRecord() const;

        uint8_t getFootprintIndex() const;
        void setFootprintIndex(uint8_t newIndex);
        bool isOrigin() const;

        ObjectEntryIndex getEntryIndex() const;
        void setEntryIndex(ObjectEntryIndex newEntry);

        uint8_t getConnectionCache() const;
        void setConnectionCache(uint8_t newCache);

        uint8_t getFactoryFlags() const;
        void setFactoryFlags(uint8_t newFlags);
        bool hasFactoryFlag(uint8_t flag) const;
        void setFactoryFlag(uint8_t flag, bool on);
    };
    static_assert(sizeof(FactoryElement) == kTileElementSize);
#pragma pack(pop)
} // namespace OpenRCT2

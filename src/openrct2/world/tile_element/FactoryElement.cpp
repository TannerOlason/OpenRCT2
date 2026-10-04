/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryElement.h"

#include <cstring>

namespace OpenRCT2
{
    FactoryElementSubtype FactoryElement::getSubtype() const
    {
        return static_cast<FactoryElementSubtype>(subtype);
    }

    void FactoryElement::setSubtype(FactoryElementSubtype newSubtype)
    {
        subtype = static_cast<uint8_t>(newSubtype);
    }

    FactoryRecordId FactoryElement::getRecordId() const
    {
        // The field is unaligned inside the packed 16-byte element; copy it out rather than read in place.
        FactoryRecordId id;
        std::memcpy(&id, &recordId, sizeof(id));
        return id;
    }

    void FactoryElement::setRecordId(FactoryRecordId newRecordId)
    {
        std::memcpy(&recordId, &newRecordId, sizeof(newRecordId));
    }

    bool FactoryElement::hasRecord() const
    {
        return getRecordId() != kFactoryRecordNull;
    }

    uint8_t FactoryElement::getFootprintIndex() const
    {
        return footprintIndex;
    }

    void FactoryElement::setFootprintIndex(uint8_t newIndex)
    {
        footprintIndex = newIndex;
    }

    bool FactoryElement::isOrigin() const
    {
        return footprintIndex == 0;
    }

    ObjectEntryIndex FactoryElement::getEntryIndex() const
    {
        ObjectEntryIndex value;
        std::memcpy(&value, &entry, sizeof(value));
        return value;
    }

    void FactoryElement::setEntryIndex(ObjectEntryIndex newEntry)
    {
        std::memcpy(&entry, &newEntry, sizeof(newEntry));
    }

    uint8_t FactoryElement::getConnectionCache() const
    {
        return connectionCache;
    }

    void FactoryElement::setConnectionCache(uint8_t newCache)
    {
        connectionCache = newCache;
    }

    uint8_t FactoryElement::getFactoryFlags() const
    {
        return factoryFlags;
    }

    void FactoryElement::setFactoryFlags(uint8_t newFlags)
    {
        factoryFlags = newFlags;
    }

    bool FactoryElement::hasFactoryFlag(uint8_t flag) const
    {
        return (factoryFlags & flag) != 0;
    }

    void FactoryElement::setFactoryFlag(uint8_t flag, bool on)
    {
        if (on)
            factoryFlags |= flag;
        else
            factoryFlags &= ~flag;
    }
} // namespace OpenRCT2

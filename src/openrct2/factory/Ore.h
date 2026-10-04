/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The dense per-tile ore layer.

#pragma once

#include "../world/Location.hpp"
#include "FactoryRecords.h"

#include <cstdint>
#include <vector>

namespace OpenRCT2::Factory
{
    /**
     * One OreCell per map tile, resized with the map and saved run-length encoded in chunk 0x42. Mutations
     * keep an order-independent 64-bit hash so the sync checksum covers the layer without hashing every
     * cell each tick.
     */
    class OreLayer
    {
    private:
        int32_t _width{};
        int32_t _height{};
        std::vector<OreCell> _cells;
        uint64_t _hash{};
        uint32_t _nonEmpty{};

    public:
        void resize(const TileCoordsXY& size);
        void clear();

        int32_t width() const
        {
            return _width;
        }
        int32_t height() const
        {
            return _height;
        }
        bool inBounds(const TileCoordsXY& tile) const;

        const OreCell& get(const TileCoordsXY& tile) const;
        void set(const TileCoordsXY& tile, const OreCell& cell);

        // Removes up to `count` from the cell and returns how much was taken.
        uint32_t take(const TileCoordsXY& tile, uint32_t count);

        uint64_t hash() const
        {
            return _hash;
        }
        uint32_t nonEmptyCount() const
        {
            return _nonEmpty;
        }
        bool isEmpty() const
        {
            return _nonEmpty == 0;
        }

        const std::vector<OreCell>& cells() const
        {
            return _cells;
        }

        // Rebuilds derived counters after cells were written directly (deserialisation).
        void assign(const TileCoordsXY& size, std::vector<OreCell>&& cells);

        static uint64_t cellHash(int32_t x, int32_t y, const OreCell& cell);
    };
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Pollution: working machines emit into a coarse grid that spreads and decays.

#pragma once

#include "../world/Location.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace OpenRCT2::Factory
{
    /**
     * Pollution per cell of kCellTiles x kCellTiles tiles. Working machines add their prototype's `pollution` every
     * tick; every kSpreadTicks each cell passes a sixteenth of its value to each neighbour (lost past the map edge) and
     * loses a thirty-second (at least one unit) to decay. Integer only, so it is deterministic and part of the sync checksum;
     * saved sparsely (non-zero cells) because pollution stays local.
     */
    class PollutionLayer
    {
    public:
        static constexpr int32_t kCellTiles = 8;
        static constexpr uint32_t kSpreadTicks = 64;

        // Sizes the grid for the map, dropping everything if the size changes.
        void ensureSize(const TileCoordsXY& mapSize);
        void clear();
        bool isEmpty() const
        {
            return _total == 0;
        }
        uint64_t total() const
        {
            return _total;
        }

        uint32_t at(const TileCoordsXY& tile) const;
        void add(const TileCoordsXY& tile, uint32_t amount);
        // One spread-and-decay step.
        void spread();

        template<typename V>
        void visit(V& v)
        {
            v(_width);
            v(_height);
            // Sparse: count, then (cell index, value) pairs in ascending index order.
            uint32_t count = 0;
            if (!v.isReading())
            {
                for (auto value : _cells)
                    count += value != 0 ? 1 : 0;
            }
            v(count);
            if (v.isReading())
            {
                _cells.assign(static_cast<size_t>(std::max(0, _width)) * static_cast<size_t>(std::max(0, _height)), 0);
                _total = 0;
                for (uint32_t i = 0; i < count; i++)
                {
                    uint32_t index = 0;
                    uint32_t value = 0;
                    v(index);
                    v(value);
                    if (index < _cells.size())
                    {
                        _cells[index] = value;
                        _total += value;
                    }
                }
            }
            else
            {
                for (uint32_t i = 0; i < _cells.size(); i++)
                {
                    if (_cells[i] == 0)
                        continue;
                    uint32_t index = i;
                    uint32_t value = _cells[i];
                    v(index);
                    v(value);
                }
            }
        }

    private:
        int32_t _width{};
        int32_t _height{};
        std::vector<uint32_t> _cells;
        uint64_t _total{};

        int32_t cellIndex(const TileCoordsXY& tile) const;
    };
} // namespace OpenRCT2::Factory

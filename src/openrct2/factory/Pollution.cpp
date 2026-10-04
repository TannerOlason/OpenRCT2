/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Pollution.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    void PollutionLayer::ensureSize(const TileCoordsXY& mapSize)
    {
        const int32_t width = (mapSize.x + kCellTiles - 1) / kCellTiles;
        const int32_t height = (mapSize.y + kCellTiles - 1) / kCellTiles;
        if (width == _width && height == _height && !_cells.empty())
            return;
        _width = width;
        _height = height;
        _cells.assign(static_cast<size_t>(std::max(0, width)) * static_cast<size_t>(std::max(0, height)), 0);
        _total = 0;
    }

    void PollutionLayer::clear()
    {
        _width = 0;
        _height = 0;
        _cells.clear();
        _total = 0;
    }

    int32_t PollutionLayer::cellIndex(const TileCoordsXY& tile) const
    {
        if (tile.x < 0 || tile.y < 0)
            return -1;
        const int32_t cx = tile.x / kCellTiles;
        const int32_t cy = tile.y / kCellTiles;
        if (cx >= _width || cy >= _height)
            return -1;
        return cy * _width + cx;
    }

    uint32_t PollutionLayer::at(const TileCoordsXY& tile) const
    {
        const auto index = cellIndex(tile);
        return index < 0 ? 0 : _cells[static_cast<size_t>(index)];
    }

    void PollutionLayer::add(const TileCoordsXY& tile, uint32_t amount)
    {
        const auto index = cellIndex(tile);
        if (index < 0 || amount == 0)
            return;
        auto& cell = _cells[static_cast<size_t>(index)];
        const uint32_t added = std::min<uint32_t>(amount, 0xFFFFFFFFu - cell);
        cell += added;
        _total += added;
    }

    void PollutionLayer::spread()
    {
        if (_total == 0)
            return;
        std::vector<uint32_t> next(_cells.size(), 0);
        for (int32_t cy = 0; cy < _height; cy++)
        {
            for (int32_t cx = 0; cx < _width; cx++)
            {
                const auto index = static_cast<size_t>(cy * _width + cx);
                const uint32_t value = _cells[index];
                if (value == 0)
                    continue;
                const uint32_t share = value / 16;
                // At least one unit decays, or small values would linger forever.
                const uint32_t kept = value - 4 * share - std::max<uint32_t>(1, value / 32);
                next[index] += kept;
                const int32_t offsets[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
                for (const auto& offset : offsets)
                {
                    const int32_t nx = cx + offset[0];
                    const int32_t ny = cy + offset[1];
                    if (nx < 0 || ny < 0 || nx >= _width || ny >= _height)
                        continue; // carried off the map
                    next[static_cast<size_t>(ny * _width + nx)] += share;
                }
            }
        }
        _cells = std::move(next);
        _total = 0;
        for (auto value : _cells)
            _total += value;
    }
} // namespace OpenRCT2::Factory

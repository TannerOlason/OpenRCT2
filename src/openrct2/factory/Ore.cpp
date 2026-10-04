/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Ore.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    static const OreCell kEmptyCell{};

    static uint64_t mix64(uint64_t x)
    {
        // splitmix64 finaliser
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }

    uint64_t OreLayer::cellHash(int32_t x, int32_t y, const OreCell& cell)
    {
        if (cell.isEmpty())
            return 0;
        uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 48)
            ^ (static_cast<uint64_t>(static_cast<uint32_t>(y)) << 32) ^ (static_cast<uint64_t>(cell.ore) << 16) ^ cell.richness;
        return mix64(key) ^ mix64(static_cast<uint64_t>(cell.amount) * 0x2545F4914F6CDD1Dull + key);
    }

    void OreLayer::resize(const TileCoordsXY& size)
    {
        _width = std::max(0, size.x);
        _height = std::max(0, size.y);
        _cells.assign(static_cast<size_t>(_width) * _height, OreCell{});
        _hash = 0;
        _nonEmpty = 0;
    }

    void OreLayer::clear()
    {
        resize({ 0, 0 });
    }

    bool OreLayer::inBounds(const TileCoordsXY& tile) const
    {
        return tile.x >= 0 && tile.y >= 0 && tile.x < _width && tile.y < _height;
    }

    const OreCell& OreLayer::get(const TileCoordsXY& tile) const
    {
        if (!inBounds(tile))
            return kEmptyCell;
        return _cells[static_cast<size_t>(tile.y) * _width + tile.x];
    }

    void OreLayer::set(const TileCoordsXY& tile, const OreCell& cell)
    {
        if (!inBounds(tile))
            return;
        auto& slot = _cells[static_cast<size_t>(tile.y) * _width + tile.x];
        if (slot == cell)
            return;
        _hash ^= cellHash(tile.x, tile.y, slot);
        if (!slot.isEmpty())
            _nonEmpty--;
        slot = cell.isEmpty() ? OreCell{} : cell;
        _hash ^= cellHash(tile.x, tile.y, slot);
        if (!slot.isEmpty())
            _nonEmpty++;
    }

    uint32_t OreLayer::take(const TileCoordsXY& tile, uint32_t count)
    {
        const auto& cell = get(tile);
        if (cell.isEmpty())
            return 0;
        const uint32_t taken = std::min(count, cell.amount);
        OreCell updated = cell;
        updated.amount -= taken;
        set(tile, updated);
        return taken;
    }

    void OreLayer::assign(const TileCoordsXY& size, std::vector<OreCell>&& cells)
    {
        _width = std::max(0, size.x);
        _height = std::max(0, size.y);
        _cells = std::move(cells);
        _cells.resize(static_cast<size_t>(_width) * _height, OreCell{});
        _hash = 0;
        _nonEmpty = 0;
        for (int32_t y = 0; y < _height; y++)
        {
            for (int32_t x = 0; x < _width; x++)
            {
                auto& cell = _cells[static_cast<size_t>(y) * _width + x];
                if (cell.isEmpty())
                {
                    cell = OreCell{};
                    continue;
                }
                _hash ^= cellHash(x, y, cell);
                _nonEmpty++;
            }
        }
    }
} // namespace OpenRCT2::Factory

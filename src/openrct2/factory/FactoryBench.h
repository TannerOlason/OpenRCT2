/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The benchmark factory behind `openrct2-cli factory-bench` and its tests.

#pragma once

#include <cstdint>
#include <string>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    // Each cell is 20 x 4 tiles: drill -> 12 belts -> inserter -> furnace -> inserter -> chest on one row, a chest
    // -> inserter -> powered assembler -> inserter -> chest line with its generator and pole, and six pipes.
    constexpr int32_t kBenchCellWidth = 20;
    constexpr int32_t kBenchCellHeight = 4;
    constexpr int32_t kBenchMaxColumns = 49;
    constexpr int32_t kBenchMaxCells = kBenchMaxColumns * 249; // what fits on the largest map

    struct BenchCounts
    {
        int32_t cells{};
        int32_t mapWidth{};
        int32_t mapHeight{};
        size_t belts{};
        size_t segments{};
        size_t inserters{};
        size_t machines{};
        size_t containers{};
        size_t poles{};
        size_t pipes{};
    };

    // Map size (in tiles, including the border) a bench of `cells` needs.
    void benchMapSize(int32_t cells, int32_t& width, int32_t& height);

    /**
     * Builds the benchmark factory on the current (flat, freshly initialised) map: loads the prototypes it needs,
     * sizes the ore layer and places `cells` cells with fuel and ingredients stocked. Returns false and sets
     * `error` when a prototype is missing or the map is too small.
     */
    bool buildBenchFactory(GameState_t& gameState, int32_t cells, BenchCounts& counts, std::string& error);
} // namespace OpenRCT2::Factory

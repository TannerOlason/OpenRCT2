/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Blueprints: a copied area of factory elements as a portable text string that names
// prototypes by identifier, so it can be pasted, rotated, shared and replayed through one game action.

#pragma once

#include "../world/Location.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    struct BlueprintEntry
    {
        int32_t dx{}; // tiles from the blueprint's minimum corner (multi-tile pieces: their minimum corner)
        int32_t dy{};
        int32_t dz{};        // height steps (kCoordsZStep) above the lowest piece
        uint8_t direction{}; // map direction, as for FactoryPlaceAction
        std::string object;  // factory_prototype identifier
        std::string recipe;  // assemblers: the recipe identifier, or empty

        bool operator==(const BlueprintEntry&) const = default;
    };

    struct Blueprint
    {
        int32_t width{};  // tiles
        int32_t height{}; // tiles
        // Placement order: everything else first, then underground exits so each pairs with its entrance.
        std::vector<BlueprintEntry> entries;

        bool empty() const
        {
            return entries.empty();
        }
    };

    constexpr size_t kMaxBlueprintEntries = 1000;

    // "FTBP1;w;h;n;id_0;...;id_n-1;dx,dy,dz,dir,object,recipe;..." (object/recipe index the id list, recipe -1 = none).
    std::string serialiseBlueprint(const Blueprint& blueprint);
    std::optional<Blueprint> parseBlueprint(std::string_view text);

    // Copies the placed (non-ghost) factory elements in the inclusive tile range.
    Blueprint captureBlueprint(const GameState_t& gameState, const MapRange& range);

    // Rotates by `quarterTurns` (each turns map direction d into d + 1), keeping the minimum corner at (0, 0).
    // Footprint sizes come from the loaded prototypes.
    Blueprint rotateBlueprint(const Blueprint& blueprint, uint8_t quarterTurns);

    // The tile an entry's piece starts on when the blueprint's corner is at `origin` (z from origin.z plus dz).
    CoordsXYZ blueprintEntryLocation(const BlueprintEntry& entry, const CoordsXYZ& origin);
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Factory scenario objectives and the production statistics behind them.

#pragma once

#include "../object/ObjectTypes.h"

#include <cstdint>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Scenario
{
    struct Objective;
    enum class ObjectiveStatus : uint8_t;
} // namespace OpenRCT2::Scenario

namespace OpenRCT2::Factory
{
    struct ProducedEntry
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint32_t count{};

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(count);
        }
    };

    // All-time production per item (crafted results and mined ore), sorted by item.
    struct ProductionStats
    {
        std::vector<ProducedEntry> produced;

        uint32_t count(ObjectEntryIndex item) const;
        void add(ObjectEntryIndex item, uint32_t count);

        template<typename V>
        void visit(V& v)
        {
            v.vec(produced, [](ProducedEntry& entry, auto& vv) { entry.visit(vv); });
        }
    };

    /**
     * produceItemsBy (NumGuests = item entry, Currency = quantity) and guestsTouredFactory (NumGuests = guests):
     * succeed as soon as the target is reached, fail if it is not by the end of October of the objective's year,
     * like upstream's "guests by" objective.
     */
    Scenario::ObjectiveStatus checkObjective(const Scenario::Objective& objective, const GameState_t& gameState);

    // The item a new produce objective asks for: Iron plate when loaded, else the first loaded item.
    ObjectEntryIndex defaultObjectiveItem();
} // namespace OpenRCT2::Factory

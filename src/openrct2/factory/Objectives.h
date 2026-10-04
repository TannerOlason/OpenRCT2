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

#include <array>
#include <cstddef>
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

    constexpr uint32_t kProductionSampleTicks = 1024; // one graph sample, about 25 seconds
    constexpr size_t kProductionSamples = 48;         // about 20 minutes of history

    // Per-item counts for the last kProductionSamples sample periods; slot `head` is the period in progress.
    struct ProductionHistoryEntry
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        std::array<uint32_t, kProductionSamples> produced{};
        std::array<uint32_t, kProductionSamples> consumed{};

        bool isEmpty() const;

        template<typename V>
        void visit(V& v)
        {
            v(item);
            for (auto& value : produced)
                v(value);
            for (auto& value : consumed)
                v(value);
        }
    };

    /**
     * All-time production per item (crafted results and mined ore) and consumption (ingredients, lab packs, fuel and
     * ammunition), both sorted by item, plus the sampled history behind the production graphs.
     */
    struct ProductionStats
    {
        std::vector<ProducedEntry> produced;
        std::vector<ProducedEntry> consumed;
        std::vector<ProductionHistoryEntry> history; // sorted by item
        uint8_t head{};

        uint32_t count(ObjectEntryIndex item) const;
        uint32_t consumedCount(ObjectEntryIndex item) const;
        void add(ObjectEntryIndex item, uint32_t count);
        void consume(ObjectEntryIndex item, uint32_t count);
        // Closes the current sample period (every kProductionSampleTicks).
        void advanceSample();
        const ProductionHistoryEntry* historyOf(ObjectEntryIndex item) const;
        void clear();

        template<typename V>
        void visit(V& v)
        {
            v.vec(produced, [](ProducedEntry& entry, auto& vv) { entry.visit(vv); });
            v.vec(consumed, [](ProducedEntry& entry, auto& vv) { entry.visit(vv); });
            v.vec(history, [](ProductionHistoryEntry& entry, auto& vv) { entry.visit(vv); });
            v(head);
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

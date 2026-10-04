/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The Market: the off-map buyer for factory goods (CONTEXT.md).

#pragma once

#include "../core/Money.hpp"
#include "../object/ObjectTypes.h"

#include <cstdint>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    struct MarketEntry
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint16_t saturation{}; // 0 = fresh demand, kMarketSaturationMax = flooded

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(saturation);
        }
    };

    constexpr uint16_t kMarketSaturationMax = 1024;
    // Price never falls below 1/8 of the base price.
    constexpr uint16_t kMarketSaturationFloor = 896;

    /**
     * Each sale raises an item's saturation by its prototype's `saturation`, and the price falls in proportion;
     * once a day every entry recovers an eighth (at least one point). Income is booked as Shop sales (ADR 0011) and
     * added to `goodsSold`, the fork-side total. Sorted by item for deterministic serialisation.
     */
    struct Market
    {
        std::vector<MarketEntry> entries;
        money64 goodsSold{};        // all-time income from the Market
        int32_t lastDayIndex{ -1 }; // date (months * 32 + day) of the last daily recovery

        bool isEmpty() const
        {
            return entries.empty() && goodsSold == 0;
        }
        uint16_t saturation(ObjectEntryIndex item) const;
        // The price one more unit of `item` fetches now; 0 when it is not sellable.
        money64 price(ObjectEntryIndex item) const;
        // Sells `count` units one at a time (each lowering the next price) and returns the income. With `pay` the park
        // is paid here (export depots); otherwise the caller books it (a game action returning a negative cost).
        money64 sell(ObjectEntryIndex item, uint32_t count, bool pay);
        // Daily recovery, called from the factory update; acts once per new day.
        void update(const GameState_t& gameState);

        template<typename V>
        void visit(V& v)
        {
            v.vec(entries, [](MarketEntry& entry, auto& vv) { entry.visit(vv); });
            v(goodsSold);
            v(lastDayIndex);
        }
    };
} // namespace OpenRCT2::Factory

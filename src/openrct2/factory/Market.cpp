/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Market.h"

#include "../GameState.h"
#include "../management/Finance.h"
#include "FactoryPrototypeObject.h"
#include "FactoryTopology.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    static auto findEntry(std::vector<MarketEntry>& entries, ObjectEntryIndex item)
    {
        return std::lower_bound(entries.begin(), entries.end(), item, [](const MarketEntry& entry, ObjectEntryIndex value) {
            return entry.item < value;
        });
    }

    uint16_t Market::saturation(ObjectEntryIndex item) const
    {
        auto it = std::lower_bound(entries.begin(), entries.end(), item, [](const MarketEntry& entry, ObjectEntryIndex value) {
            return entry.item < value;
        });
        return it != entries.end() && it->item == item ? it->saturation : 0;
    }

    money64 Market::price(ObjectEntryIndex item) const
    {
        auto* proto = getPrototype(item);
        if (proto == nullptr || proto->getKind() != PrototypeKind::item || proto->getItem().marketPrice <= 0)
            return 0;
        const auto discount = std::min<uint16_t>(saturation(item), kMarketSaturationFloor);
        return proto->getItem().marketPrice * (kMarketSaturationMax - discount) / kMarketSaturationMax;
    }

    money64 Market::sell(ObjectEntryIndex item, uint32_t count, bool pay)
    {
        auto* proto = getPrototype(item);
        if (proto == nullptr || price(item) <= 0)
            return 0;
        money64 income = 0;
        for (uint32_t i = 0; i < count; i++)
        {
            income += price(item);
            auto it = findEntry(entries, item);
            if (it == entries.end() || it->item != item)
                it = entries.insert(it, MarketEntry{ item, 0 });
            it->saturation = static_cast<uint16_t>(
                std::min<uint32_t>(kMarketSaturationMax, uint32_t{ it->saturation } + proto->getItem().marketSaturation));
        }
        if (income > 0)
        {
            if (pay)
                FinancePayment(-income, ExpenditureType::shopSales);
            goodsSold += income;
        }
        return income;
    }

    void Market::update(const GameState_t& gameState)
    {
        const int32_t dayIndex = static_cast<int32_t>(gameState.date.GetMonthsElapsed()) * 32 + gameState.date.GetDay();
        if (dayIndex == lastDayIndex)
            return;
        lastDayIndex = dayIndex;
        for (auto& entry : entries)
            entry.saturation = static_cast<uint16_t>(
                entry.saturation - std::min<uint16_t>(entry.saturation, std::max(1, entry.saturation / 8)));
        std::erase_if(entries, [](const MarketEntry& entry) { return entry.saturation == 0; });
    }
} // namespace OpenRCT2::Factory

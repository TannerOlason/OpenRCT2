/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryMarketSellAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../management/Finance.h"
#include "../FactoryState.h"
#include "../FactoryStringIds.h"

#include <algorithm>

namespace OpenRCT2::GameActions
{
    FactoryMarketSellAction::FactoryMarketSellAction(ObjectEntryIndex item, uint32_t count)
        : _item(item)
        , _count(count)
    {
    }

    void FactoryMarketSellAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("item", _item);
        visitor.Visit("count", _count);
    }

    uint16_t FactoryMarketSellAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryMarketSellAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_item) << DS_TAG(_count);
    }

    Result FactoryMarketSellAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto& factory = gameState.factory;
        if (_count == 0 || factory.warehouse.count(_item) == 0)
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_NOTHING_TO_SELL);
        if (factory.market.price(_item) <= 0)
            return Result(Status::disallowed, STR_CANT_DO_THIS, STR_FT_MARKET_WONT_BUY);
        // Estimate at the current price; Execute books the exact, saturating income.
        auto res = Result();
        res.expenditure = ExpenditureType::shopSales;
        res.cost = -factory.market.price(_item) * std::min(_count, factory.warehouse.count(_item));
        return res;
    }

    Result FactoryMarketSellAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        auto& factory = gameState.factory;
        const uint32_t sold = factory.warehouse.take(_item, _count);
        res.cost = -factory.market.sell(_item, sold, false);
        return res;
    }
} // namespace OpenRCT2::GameActions

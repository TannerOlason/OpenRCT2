/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#pragma once

#include "../../actions/GameAction.hpp"
#include "FactoryCommand.h"

namespace OpenRCT2::GameActions
{
    /**
     * Sells up to `count` of an item from the Warehouse to the Market. The income is the result's negative cost,
     * booked as Shop sales (ADR 0011).
     */
    class FactoryMarketSellAction final : public GameActionBase<toGameCommand(FactoryCommand::marketSell)>
    {
    private:
        ObjectEntryIndex _item{ kObjectEntryIndexNull };
        uint32_t _count{};

    public:
        FactoryMarketSellAction() = default;
        FactoryMarketSellAction(ObjectEntryIndex item, uint32_t count);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

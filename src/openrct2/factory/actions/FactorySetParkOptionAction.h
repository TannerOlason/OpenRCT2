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
    enum class FactoryParkOption : uint8_t
    {
        constructionMode, // Factory::ConstructionMode
        shopStockMode,    // 0 infinite, 1 warehouse
        count,
    };

    // Sets one of the park-wide factory options kept in the parkExt side table (scenario designers, scripts).
    class FactorySetParkOptionAction final : public GameActionBase<toGameCommand(FactoryCommand::setParkOption)>
    {
    private:
        uint8_t _option{};
        uint8_t _value{};

    public:
        FactorySetParkOptionAction() = default;
        FactorySetParkOptionAction(FactoryParkOption option, uint8_t value);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

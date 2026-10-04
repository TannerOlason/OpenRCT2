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

namespace OpenRCT2
{
    struct FactoryElement;
}

namespace OpenRCT2::GameActions
{
    class FactoryRemoveAction final : public GameActionBase<toGameCommand(FactoryCommand::remove)>
    {
    private:
        CoordsXYZ _loc;

    public:
        FactoryRemoveAction() = default;
        explicit FactoryRemoveAction(const CoordsXYZ& loc);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;

    private:
        FactoryElement* FindElement() const;
        Result Validate(FactoryElement*& element) const;
    };
} // namespace OpenRCT2::GameActions

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
    // Sets the world a launch pad sends to (a world id, or 0xFF for the next world).
    class FactorySetLaunchTargetAction final : public GameActionBase<toGameCommand(FactoryCommand::setLaunchTarget)>
    {
    private:
        CoordsXYZ _loc;
        uint8_t _target{ 0xFF };

    public:
        FactorySetLaunchTargetAction() = default;
        FactorySetLaunchTargetAction(const CoordsXYZ& loc, uint8_t target);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

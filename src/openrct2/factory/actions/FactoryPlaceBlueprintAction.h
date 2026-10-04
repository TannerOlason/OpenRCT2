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

#include <string>

namespace OpenRCT2::GameActions
{
    /**
     * Pastes a blueprint string (Factory::serialiseBlueprint) with its minimum corner at `origin`, turned by
     * `rotation` quarter turns: every piece goes through a nested FactoryPlaceAction (pieces that do not fit are
     * skipped) and assemblers get their recipe through FactorySetRecipeAction. Fails only when nothing fits.
     */
    class FactoryPlaceBlueprintAction final : public GameActionBase<toGameCommand(FactoryCommand::placeBlueprint)>
    {
    private:
        CoordsXYZ _origin;
        uint8_t _rotation{};
        std::string _blueprint;

    public:
        FactoryPlaceBlueprintAction() = default;
        FactoryPlaceBlueprintAction(const CoordsXYZ& origin, uint8_t rotation, std::string blueprint);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;

    private:
        Result Run(GameState_t& gameState, bool apply) const;
    };
} // namespace OpenRCT2::GameActions

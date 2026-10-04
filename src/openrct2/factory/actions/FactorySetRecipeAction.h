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
     * Sets the recipe of an assembler-style machine. Items of the previous recipe left in the inputs are
     * lost (M2); kObjectEntryIndexNull clears the recipe.
     */
    class FactorySetRecipeAction final : public GameActionBase<toGameCommand(FactoryCommand::setRecipe)>
    {
    private:
        CoordsXYZ _loc;
        ObjectEntryIndex _recipe{ kObjectEntryIndexNull };

    public:
        FactorySetRecipeAction() = default;
        FactorySetRecipeAction(const CoordsXYZ& loc, ObjectEntryIndex recipe);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

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
     * Places one placeable factory prototype (belt, inserter, container, ...) on a flat tile. With
     * CommandFlag::ghost the element is a preview that owns no record.
     */
    class FactoryPlaceAction final : public GameActionBase<toGameCommand(FactoryCommand::place)>
    {
    private:
        CoordsXYZ _loc;
        Direction _direction{};
        ObjectEntryIndex _entry{ kObjectEntryIndexNull };

    public:
        FactoryPlaceAction() = default;
        FactoryPlaceAction(const CoordsXYZ& loc, Direction direction, ObjectEntryIndex entry);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;

    private:
        Result Validate(GameState_t& gameState, bool apply) const;
    };
} // namespace OpenRCT2::GameActions

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
     * Paints ore into the ore layer over a map range (scenario editor, sandbox and tests). An entry of
     * kObjectEntryIndexNull or an amount of 0 clears the cells.
     */
    class FactorySetOreAction final : public GameActionBase<toGameCommand(FactoryCommand::setOre)>
    {
    private:
        MapRange _range;
        ObjectEntryIndex _ore{ kObjectEntryIndexNull };
        uint32_t _amount{};
        uint16_t _richness{};

    public:
        FactorySetOreAction() = default;
        FactorySetOreAction(const MapRange& range, ObjectEntryIndex ore, uint32_t amount, uint16_t richness = 0);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

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
     * Sets a splitter's item filter and its input and output priorities (kSplitterPriority*). Either tile of the
     * splitter may be named. kObjectEntryIndexNull clears the filter; fluids cannot be filters.
     */
    class FactorySetFilterAction final : public GameActionBase<toGameCommand(FactoryCommand::setFilter)>
    {
    private:
        CoordsXYZ _loc;
        ObjectEntryIndex _filter{ kObjectEntryIndexNull };
        uint8_t _inputPriority{};
        uint8_t _outputPriority{};

    public:
        FactorySetFilterAction() = default;
        FactorySetFilterAction(const CoordsXYZ& loc, ObjectEntryIndex filter, uint8_t inputPriority, uint8_t outputPriority);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

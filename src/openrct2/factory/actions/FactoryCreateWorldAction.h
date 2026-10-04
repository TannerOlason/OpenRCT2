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
     * Adds a world (ADR 0015): an empty, flat, fully owned map of size x size tiles that ticks with the others and
     * shares the company's money and research.
     */
    class FactoryCreateWorldAction final : public GameActionBase<toGameCommand(FactoryCommand::createWorld)>
    {
    private:
        uint16_t _size{ 64 };
        uint8_t _preset{}; // Factory::WorldPreset

    public:
        FactoryCreateWorldAction() = default;
        FactoryCreateWorldAction(uint16_t size, uint8_t preset);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

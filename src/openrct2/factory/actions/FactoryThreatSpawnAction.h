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
     * Spawns a Threat of a threat prototype at (x, y) in world units; it heads for the nearest destructible machine.
     */
    class FactoryThreatSpawnAction final : public GameActionBase<toGameCommand(FactoryCommand::threatSpawn)>
    {
    private:
        ObjectEntryIndex _object{ kObjectEntryIndexNull };
        int32_t _x{};
        int32_t _y{};

    public:
        FactoryThreatSpawnAction() = default;
        FactoryThreatSpawnAction(ObjectEntryIndex object, int32_t x, int32_t y);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

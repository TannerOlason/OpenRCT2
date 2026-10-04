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
     * Damages a machine (id = machine record), a ride (id = RideId) or a Threat (id = threat record) by `amount`.
     * Machines at zero health are destroyed, rides break down (safety cut-out) and threats despawn. `damageType` is
     * passed through to the factory.damage hook for scripts.
     */
    class FactoryDamageAction final : public GameActionBase<toGameCommand(FactoryCommand::damage)>
    {
    private:
        uint8_t _target{};
        uint32_t _id{};
        uint16_t _amount{};
        uint8_t _damageType{};

    public:
        FactoryDamageAction() = default;
        FactoryDamageAction(uint8_t target, uint32_t id, uint16_t amount, uint8_t damageType);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    };
} // namespace OpenRCT2::GameActions

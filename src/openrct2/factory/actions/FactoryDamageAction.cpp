/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryDamageAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../ride/Ride.h"
#include "../Combat.h"
#include "../FactoryState.h"
#include "../FactoryStringIds.h"

namespace OpenRCT2::GameActions
{
    FactoryDamageAction::FactoryDamageAction(uint8_t target, uint32_t id, uint16_t amount, uint8_t damageType)
        : _target(target)
        , _id(id)
        , _amount(amount)
        , _damageType(damageType)
    {
    }

    void FactoryDamageAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("target", _target);
        visitor.Visit("id", _id);
        visitor.Visit("amount", _amount);
        visitor.Visit("damageType", _damageType);
    }

    uint16_t FactoryDamageAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryDamageAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_target) << DS_TAG(_id) << DS_TAG(_amount) << DS_TAG(_damageType);
    }

    Result FactoryDamageAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        if (_target >= static_cast<uint8_t>(Factory::DamageTarget::count) || _amount == 0)
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_NOTHING_TO_DAMAGE);
        const bool exists = _target == static_cast<uint8_t>(Factory::DamageTarget::machine)
            ? gameState.factory.machines.get(_id) != nullptr
            : _target == static_cast<uint8_t>(Factory::DamageTarget::threat)
            ? gameState.factory.threats.get(_id) != nullptr
            : GetRide(RideId::FromUnderlying(static_cast<uint16_t>(_id))) != nullptr;
        if (!exists)
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_NOTHING_TO_DAMAGE);
        return Result();
    }

    Result FactoryDamageAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        Factory::applyDamage(gameState, static_cast<Factory::DamageTarget>(_target), _id, _amount, _damageType);
        return res;
    }
} // namespace OpenRCT2::GameActions

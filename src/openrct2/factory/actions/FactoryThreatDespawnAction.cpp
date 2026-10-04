/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryThreatDespawnAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../Combat.h"
#include "../FactoryState.h"
#include "../FactoryStringIds.h"

namespace OpenRCT2::GameActions
{
    FactoryThreatDespawnAction::FactoryThreatDespawnAction(uint32_t id)
        : _id(id)
    {
    }

    void FactoryThreatDespawnAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("id", _id);
    }

    uint16_t FactoryThreatDespawnAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryThreatDespawnAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_id);
    }

    Result FactoryThreatDespawnAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        if (gameState.factory.threats.get(_id) == nullptr)
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_NOTHING_TO_DAMAGE);
        return Result();
    }

    Result FactoryThreatDespawnAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        Factory::despawnThreat(gameState, _id);
        return res;
    }
} // namespace OpenRCT2::GameActions

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryThreatSpawnAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../world/Map.h"
#include "../Combat.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"

namespace OpenRCT2::GameActions
{
    FactoryThreatSpawnAction::FactoryThreatSpawnAction(ObjectEntryIndex object, int32_t x, int32_t y)
        : _object(object)
        , _x(x)
        , _y(y)
    {
    }

    void FactoryThreatSpawnAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("object", _object);
        visitor.Visit("x", _x);
        visitor.Visit("y", _y);
    }

    uint16_t FactoryThreatSpawnAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryThreatSpawnAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_object) << DS_TAG(_x) << DS_TAG(_y);
    }

    Result FactoryThreatSpawnAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto* proto = Factory::getPrototype(_object);
        if (proto == nullptr || proto->getKind() != Factory::PrototypeKind::threat)
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_PROTOTYPE_NOT_PLACEABLE);
        if (!LocationValid({ _x, _y }))
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_OFF_EDGE_OF_MAP);
        return Result();
    }

    Result FactoryThreatSpawnAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        Factory::spawnThreat(gameState, _object, { _x, _y, 0 });
        return res;
    }
} // namespace OpenRCT2::GameActions

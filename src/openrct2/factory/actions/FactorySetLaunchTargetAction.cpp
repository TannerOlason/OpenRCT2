/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactorySetLaunchTargetAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../world/Map.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryState.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"
#include "../WorldManager.h"

namespace OpenRCT2::GameActions
{
    FactorySetLaunchTargetAction::FactorySetLaunchTargetAction(const CoordsXYZ& loc, uint8_t target)
        : _loc(loc)
        , _target(target)
    {
    }

    void FactorySetLaunchTargetAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
        visitor.Visit("target", _target);
    }

    uint16_t FactorySetLaunchTargetAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void FactorySetLaunchTargetAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc) << DS_TAG(_target);
    }

    Result FactorySetLaunchTargetAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_OFF_EDGE_OF_MAP);
        auto* element = Factory::findFactoryElement(_loc);
        auto* proto = element != nullptr ? Factory::getPrototype(*element) : nullptr;
        if (proto == nullptr || element->getSubtype() != FactoryElementSubtype::container || !proto->getContainer().launchPad)
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        if (_target != 0xFF && _target >= Factory::Worlds::count())
            return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_TOO_MANY_WORLDS);
        return Result();
    }

    Result FactorySetLaunchTargetAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        auto* element = Factory::findFactoryElement(_loc);
        if (auto* container = gameState.factory.containers.get(element->getRecordId()))
            container->targetWorld = _target;
        return res;
    }
} // namespace OpenRCT2::GameActions

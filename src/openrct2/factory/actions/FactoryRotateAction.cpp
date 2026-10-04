/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryRotateAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../management/Finance.h"
#include "../../world/Map.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"

namespace OpenRCT2::GameActions
{
    using namespace OpenRCT2::Factory;

    FactoryRotateAction::FactoryRotateAction(const CoordsXYZ& loc)
        : _loc(loc)
    {
    }

    void FactoryRotateAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
    }

    uint16_t FactoryRotateAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryRotateAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc);
    }

    Result FactoryRotateAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.position = { _loc.x + 16, _loc.y + 16, _loc.z };
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;

        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
        auto* element = findFactoryElement(_loc, GetFlags().has(CommandFlag::ghost));
        if (element == nullptr)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        if (!element->isGhost() && !MapCanBuildAt(_loc))
            return Result(Status::notOwned, STR_FT_CANT_BUILD_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);
        return res;
    }

    Result FactoryRotateAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;

        auto* element = findFactoryElement(_loc, GetFlags().has(CommandFlag::ghost));
        const bool ghost = element->isGhost();
        const auto entry = element->getEntryIndex();
        const Direction newDirection = (element->getDirection() + 1) & 3;
        removeElement(gameState, *element, _loc);
        if (placeElement(gameState, _loc, newDirection, entry, ghost) == nullptr)
            return Result(Status::noFreeElements, STR_FT_CANT_BUILD_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
        return res;
    }
} // namespace OpenRCT2::GameActions

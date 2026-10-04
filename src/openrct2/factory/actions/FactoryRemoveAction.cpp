/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryRemoveAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../management/Finance.h"
#include "../../world/Map.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"

namespace OpenRCT2::GameActions
{
    using namespace OpenRCT2::Factory;

    FactoryRemoveAction::FactoryRemoveAction(const CoordsXYZ& loc)
        : _loc(loc)
    {
    }

    void FactoryRemoveAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
    }

    uint16_t FactoryRemoveAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryRemoveAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc);
    }

    FactoryElement* FactoryRemoveAction::FindElement() const
    {
        const bool ghost = GetFlags().has(CommandFlag::ghost);
        auto* element = findFactoryElement(_loc, ghost);
        if (element != nullptr && ghost && !element->isGhost())
            return nullptr; // a ghost removal never touches real elements
        return element;
    }

    Result FactoryRemoveAction::Validate(FactoryElement*& element) const
    {
        auto res = Result();
        res.position = { _loc.x + 16, _loc.y + 16, _loc.z };
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_FT_CANT_REMOVE_THIS;

        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_FT_CANT_REMOVE_THIS, STR_OFF_EDGE_OF_MAP);

        element = FindElement();
        if (element == nullptr)
            return Result(Status::invalidParameters, STR_FT_CANT_REMOVE_THIS, STR_FT_FACTORY_ELEMENT_NOT_FOUND);

        if (!element->isGhost() && !MapCanBuildAt(_loc))
            return Result(Status::notOwned, STR_FT_CANT_REMOVE_THIS, STR_LAND_NOT_OWNED_BY_PARK);

        auto* proto = getPrototype(*element);
        if (proto != nullptr)
            res.cost = proto->getRemovalPrice();
        return res;
    }

    Result FactoryRemoveAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        FactoryElement* element = nullptr;
        return Validate(element);
    }

    Result FactoryRemoveAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        FactoryElement* element = nullptr;
        auto res = Validate(element);
        if (res.error != Status::ok)
            return res;
        removeElement(gameState, *element, _loc);
        return res;
    }
} // namespace OpenRCT2::GameActions

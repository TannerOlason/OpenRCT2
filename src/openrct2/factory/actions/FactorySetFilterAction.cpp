/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactorySetFilterAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../world/Map.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryState.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"

namespace OpenRCT2::GameActions
{
    using namespace OpenRCT2::Factory;

    FactorySetFilterAction::FactorySetFilterAction(
        const CoordsXYZ& loc, ObjectEntryIndex filter, uint8_t inputPriority, uint8_t outputPriority)
        : _loc(loc)
        , _filter(filter)
        , _inputPriority(inputPriority)
        , _outputPriority(outputPriority)
    {
    }

    void FactorySetFilterAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
        visitor.Visit("filter", _filter);
        visitor.Visit("inputPriority", _inputPriority);
        visitor.Visit("outputPriority", _outputPriority);
    }

    uint16_t FactorySetFilterAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void FactorySetFilterAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc) << DS_TAG(_filter) << DS_TAG(_inputPriority) << DS_TAG(_outputPriority);
    }

    Result FactorySetFilterAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.position = { _loc.x + 16, _loc.y + 16, _loc.z };
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
        if (_inputPriority > kSplitterPriorityRight || _outputPriority > kSplitterPriorityRight)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PROTOTYPE_NOT_PLACEABLE);
        auto* element = findFactoryElement(_loc);
        if (element == nullptr || element->getSubtype() != FactoryElementSubtype::splitter || !element->hasRecord()
            || gameState.factory.splitters.get(element->getRecordId()) == nullptr)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        if (_filter != kObjectEntryIndexNull)
        {
            auto* item = getPrototype(_filter);
            if (item == nullptr || item->getKind() != PrototypeKind::item || item->isFluid())
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PROTOTYPE_NOT_PLACEABLE);
        }
        if (!MapCanBuildAt(_loc))
            return Result(Status::notOwned, STR_FT_CANT_BUILD_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);
        return res;
    }

    Result FactorySetFilterAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        auto* element = findFactoryElement(_loc);
        auto* splitter = element != nullptr ? gameState.factory.splitters.get(element->getRecordId()) : nullptr;
        if (splitter == nullptr)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        splitter->filter = _filter;
        splitter->inputPriority = _inputPriority;
        splitter->outputPriority = _outputPriority;
        MapInvalidateTileFull(_loc);
        return res;
    }
} // namespace OpenRCT2::GameActions

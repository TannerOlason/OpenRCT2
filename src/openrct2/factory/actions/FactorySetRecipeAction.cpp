/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactorySetRecipeAction.h"

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

    FactorySetRecipeAction::FactorySetRecipeAction(const CoordsXYZ& loc, ObjectEntryIndex recipe)
        : _loc(loc)
        , _recipe(recipe)
    {
    }

    void FactorySetRecipeAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
        visitor.Visit("recipe", _recipe);
    }

    uint16_t FactorySetRecipeAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void FactorySetRecipeAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc) << DS_TAG(_recipe);
    }

    Result FactorySetRecipeAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.position = { _loc.x + 16, _loc.y + 16, _loc.z };
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
        auto* element = findFactoryElement(_loc);
        if (element == nullptr || element->getSubtype() != FactoryElementSubtype::machine || !element->hasRecord())
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        auto* machineProto = getPrototype(*element);
        auto* machine = gameState.factory.machines.get(element->getRecordId());
        if (machineProto == nullptr || machine == nullptr)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        if (_recipe != kObjectEntryIndexNull)
        {
            auto* recipe = getPrototype(_recipe);
            if (recipe == nullptr || recipe->getKind() != PrototypeKind::recipe
                || !machineProto->machineHandlesCategory(recipe->getRecipe().category))
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PROTOTYPE_NOT_PLACEABLE);
        }
        if (!MapCanBuildAt(_loc))
            return Result(Status::notOwned, STR_FT_CANT_BUILD_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);
        return res;
    }

    Result FactorySetRecipeAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        auto* element = findFactoryElement(_loc);
        auto* machine = element != nullptr ? gameState.factory.machines.get(element->getRecordId()) : nullptr;
        if (machine == nullptr)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_ELEMENT_NOT_FOUND);
        if (machine->recipe != _recipe)
        {
            machine->recipe = _recipe;
            for (auto& slot : machine->inputs)
                slot = ItemStack{};
            machine->progress = 0;
            machine->craftCost = 0;
            machine->status = static_cast<uint8_t>(MachineStatus::idle);
        }
        MapInvalidateTileFull(_loc);
        return res;
    }
} // namespace OpenRCT2::GameActions

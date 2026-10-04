/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryPlaceBlueprintAction.h"

#include "../../Context.h"
#include "../../GameState.h"
#include "../../actions/GameActionRunner.h"
#include "../../localisation/StringIds.h"
#include "../../object/ObjectManager.h"
#include "../../world/Map.h"
#include "../Blueprint.h"
#include "../FactoryStringIds.h"
#include "FactoryPlaceAction.h"
#include "FactorySetRecipeAction.h"

namespace OpenRCT2::GameActions
{
    namespace
    {
        ObjectEntryIndex entryOf(const std::string& identifier)
        {
            if (identifier.empty())
                return kObjectEntryIndexNull;
            auto& objectManager = GetContext()->GetObjectManager();
            auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(identifier));
            if (object == nullptr || object->GetObjectType() != ObjectType::factoryPrototype)
                return kObjectEntryIndexNull;
            return objectManager.GetLoadedObjectEntryIndex(object);
        }
    } // namespace

    FactoryPlaceBlueprintAction::FactoryPlaceBlueprintAction(const CoordsXYZ& origin, uint8_t rotation, std::string blueprint)
        : _origin(origin)
        , _rotation(rotation)
        , _blueprint(std::move(blueprint))
    {
    }

    void FactoryPlaceBlueprintAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_origin);
        visitor.Visit("rotation", _rotation);
        visitor.Visit("blueprint", _blueprint);
    }

    uint16_t FactoryPlaceBlueprintAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryPlaceBlueprintAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_origin) << DS_TAG(_rotation) << DS_TAG(_blueprint);
    }

    Result FactoryPlaceBlueprintAction::Run(GameState_t& gameState, bool apply) const
    {
        auto res = Result();
        res.position = _origin;
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
        auto blueprint = Factory::parseBlueprint(_blueprint);
        if (!blueprint || blueprint->empty() || !LocationValid(_origin))
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_BLUEPRINT_INVALID);
        const auto rotated = Factory::rotateBlueprint(*blueprint, _rotation);

        int32_t placed = 0;
        Result firstError;
        for (const auto& entry : rotated.entries)
        {
            const auto object = entryOf(entry.object);
            const auto loc = Factory::blueprintEntryLocation(entry, _origin);
            if (object == kObjectEntryIndexNull || !LocationValid(loc))
                continue;
            auto place = FactoryPlaceAction(loc, entry.direction, object);
            place.SetFlags(GetFlags());
            auto pieceResult = apply ? ExecuteNested(&place, gameState) : QueryNested(&place, gameState);
            if (pieceResult.error != Status::ok)
            {
                if (placed == 0 && firstError.error == Status::ok)
                    firstError = pieceResult;
                continue;
            }
            res.cost += pieceResult.cost;
            placed++;
            const auto recipe = entryOf(entry.recipe);
            if (apply && recipe != kObjectEntryIndexNull && !GetFlags().has(CommandFlag::ghost))
            {
                auto setRecipe = FactorySetRecipeAction(loc, recipe);
                setRecipe.SetFlags(GetFlags());
                ExecuteNested(&setRecipe, gameState);
            }
        }
        if (placed == 0)
        {
            if (firstError.error == Status::ok)
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_BLUEPRINT_INVALID);
            firstError.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
            return firstError;
        }
        return res;
    }

    Result FactoryPlaceBlueprintAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        return Run(gameState, false);
    }

    Result FactoryPlaceBlueprintAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        return Run(gameState, true);
    }
} // namespace OpenRCT2::GameActions

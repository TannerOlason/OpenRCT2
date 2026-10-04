/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactorySetParkOptionAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryState.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"
#include "../Materials.h"

namespace OpenRCT2::GameActions
{
    FactorySetParkOptionAction::FactorySetParkOptionAction(FactoryParkOption option, uint16_t value)
        : _option(static_cast<uint8_t>(option))
        , _value(value)
    {
    }

    void FactorySetParkOptionAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("option", _option);
        visitor.Visit("value", _value);
    }

    uint16_t FactorySetParkOptionAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void FactorySetParkOptionAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_option) << DS_TAG(_value);
    }

    Result FactorySetParkOptionAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        switch (static_cast<FactoryParkOption>(_option))
        {
            case FactoryParkOption::constructionMode:
                if (_value < static_cast<uint8_t>(Factory::ConstructionMode::count))
                    return Result();
                break;
            case FactoryParkOption::shopStockMode:
            case FactoryParkOption::affectsRating:
                if (_value <= 1)
                    return Result();
                break;
            case FactoryParkOption::objectiveItem:
            {
                auto* proto = Factory::getPrototype(static_cast<ObjectEntryIndex>(_value));
                if (proto != nullptr && proto->getKind() == Factory::PrototypeKind::item && !proto->isFluid())
                    return Result();
                break;
            }
            case FactoryParkOption::researchTarget:
                if (_value == kObjectEntryIndexNull || Factory::isTechnologyAvailable(gameState, _value))
                    return Result();
                break;
            default:
                break;
        }
        return Result(Status::invalidParameters, STR_CANT_DO_THIS, STR_FT_PROTOTYPE_NOT_PLACEABLE);
    }

    Result FactorySetParkOptionAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        auto& ext = gameState.factory.parkExt;
        switch (static_cast<FactoryParkOption>(_option))
        {
            case FactoryParkOption::constructionMode:
                ext.constructionMode = static_cast<uint8_t>(_value);
                break;
            case FactoryParkOption::shopStockMode:
                ext.shopStockMode = static_cast<uint8_t>(_value);
                break;
            case FactoryParkOption::affectsRating:
                gameState.park.flags.set(ParkFlag::factoryAffectsRating, _value != 0);
                break;
            case FactoryParkOption::researchTarget:
                gameState.factory.research.current = _value;
                break;
            default:
                gameState.scenarioOptions.objective.NumGuests = _value;
                break;
        }
        return res;
    }
} // namespace OpenRCT2::GameActions

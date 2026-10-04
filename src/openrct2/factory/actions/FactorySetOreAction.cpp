/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactorySetOreAction.h"

#include "../../GameState.h"
#include "../../OpenRCT2.h"
#include "../../localisation/StringIds.h"
#include "../../world/Map.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"

namespace OpenRCT2::GameActions
{
    using namespace OpenRCT2::Factory;

    FactorySetOreAction::FactorySetOreAction(const MapRange& range, ObjectEntryIndex ore, uint32_t amount, uint16_t richness)
        : _range(range)
        , _ore(ore)
        , _amount(amount)
        , _richness(richness)
    {
    }

    void FactorySetOreAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_range);
        visitor.Visit("ore", _ore);
        visitor.Visit("amount", _amount);
        visitor.Visit("richness", _richness);
    }

    uint16_t FactorySetOreAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void FactorySetOreAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_range) << DS_TAG(_ore) << DS_TAG(_amount) << DS_TAG(_richness);
    }

    Result FactorySetOreAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
        auto range = _range.normalise();
        if (!LocationValid(range.point1) || !LocationValid(range.point2))
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
        if (_ore != kObjectEntryIndexNull)
        {
            auto* proto = getPrototype(_ore);
            if (proto == nullptr || proto->getKind() != PrototypeKind::ore)
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PROTOTYPE_NOT_PLACEABLE);
        }
        // Ore is terrain: only the scenario editor or sandbox mode may paint it.
        if (gLegacyScene != LegacyScene::scenarioEditor && !gameState.cheats.sandboxMode)
            return Result(Status::disallowed, STR_FT_CANT_BUILD_THIS_HERE, STR_CANT_DO_THIS);
        return res;
    }

    Result FactorySetOreAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;

        auto& ore = gameState.factory.ore;
        if (ore.width() != gameState.mapSize.x || ore.height() != gameState.mapSize.y)
        {
            ore.resize(gameState.mapSize);
        }
        auto range = _range.normalise();
        const OreCell cell{ _ore, _richness, _ore == kObjectEntryIndexNull ? 0u : _amount };
        for (int32_t y = range.getY1(); y <= range.getY2(); y += kCoordsXYStep)
        {
            for (int32_t x = range.getX1(); x <= range.getX2(); x += kCoordsXYStep)
            {
                ore.set(TileCoordsXY(CoordsXY{ x, y }), cell);
                MapInvalidateTileFull({ x, y });
            }
        }
        return res;
    }
} // namespace OpenRCT2::GameActions

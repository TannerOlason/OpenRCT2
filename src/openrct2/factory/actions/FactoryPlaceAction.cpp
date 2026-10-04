/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryPlaceAction.h"

#include "../../Diagnostic.h"
#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../../management/Finance.h"
#include "../../world/ConstructionClearance.h"
#include "../../world/Map.h"
#include "../../world/QuarterTile.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../../world/tile_element/SurfaceElement.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"
#include "../Fluids.h"
#include "../Technology.h"

namespace OpenRCT2::GameActions
{
    using namespace OpenRCT2::Factory;

    FactoryPlaceAction::FactoryPlaceAction(const CoordsXYZ& loc, Direction direction, ObjectEntryIndex entry)
        : _loc(loc)
        , _direction(direction)
        , _entry(entry)
    {
    }

    void FactoryPlaceAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
        visitor.Visit("direction", _direction);
        visitor.Visit("object", _entry);
    }

    uint16_t FactoryPlaceAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryPlaceAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc) << DS_TAG(_direction) << DS_TAG(_entry);
    }

    Result FactoryPlaceAction::Validate(GameState_t& gameState, bool apply) const
    {
        auto res = Result();
        res.position = { _loc.x + 16, _loc.y + 16, _loc.z };
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;

        if (!LocationValid(_loc) || _direction > 3)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);

        auto* proto = getPrototype(_entry);
        if (proto == nullptr || !proto->isPlaceable())
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PROTOTYPE_NOT_PLACEABLE);
        if (!isPrototypeUnlocked(gameState, _entry))
            return Result(Status::disallowed, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_NOT_RESEARCHED);

        if (!MapCheckCapacityAndReorganise(_loc))
        {
            LOG_ERROR("No free map elements.");
            return Result(Status::noFreeElements, STR_FT_CANT_BUILD_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
        }

        if (!MapCanBuildAt(_loc))
            return Result(Status::notOwned, STR_FT_CANT_BUILD_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);

        // Factory elements sit on flat ground at surface height (M1 rule; supports and slopes come later).
        auto* surface = MapGetSurfaceElementAt(_loc);
        if (surface == nullptr || surface->getSlope() != 0 || surface->getBaseZ() != _loc.z)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_LEVEL_LAND_REQUIRED);
        if (surface->getWaterHeight() > 0)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_CAN_ONLY_BUILD_THIS_ON_LAND);

        if (findFactoryElement(_loc, GetFlags().has(CommandFlag::ghost)) != nullptr)
            return Result(Status::itemAlreadyPlaced, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_IN_THE_WAY);

        const int32_t clearanceZ = _loc.z + proto->getClearance() * kCoordsZStep;
        auto flags = apply ? GetFlags().with(CommandFlag::apply) : GetFlags();
        auto canBuild = MapCanConstructWithClearAt(
            { _loc, _loc.z, clearanceZ }, MapPlaceNonSceneryClearFunc, QuarterTile{ 0b1111, 0 }, flags);
        if (canBuild.error != Status::ok)
        {
            canBuild.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
            return canBuild;
        }
        res.cost = proto->getPrice() + canBuild.cost;

        if (proto->getSubtype() == FactoryElementSubtype::machine && proto->getMachine().kind == MachineKind::pump
            && !hasWaterBehind(_loc, _direction))
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PUMP_NEEDS_WATER);

        if (proto->getSubtype() == FactoryElementSubtype::splitter)
        {
            // The second tile must be just as buildable.
            CoordsXYZ second;
            if (!splitterSecondTile(_loc, _direction, second) || !LocationValid(second))
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
            if (!MapCanBuildAt(second))
                return Result(Status::notOwned, STR_FT_CANT_BUILD_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);
            auto* secondSurface = MapGetSurfaceElementAt(second);
            if (secondSurface == nullptr || secondSurface->getSlope() != 0 || secondSurface->getBaseZ() != _loc.z)
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_LEVEL_LAND_REQUIRED);
            if (findFactoryElement(second, GetFlags().has(CommandFlag::ghost)) != nullptr)
                return Result(Status::itemAlreadyPlaced, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_IN_THE_WAY);
            auto canBuildSecond = MapCanConstructWithClearAt(
                { second, second.z, clearanceZ }, MapPlaceNonSceneryClearFunc, QuarterTile{ 0b1111, 0 }, flags);
            if (canBuildSecond.error != Status::ok)
            {
                canBuildSecond.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
                return canBuildSecond;
            }
            res.cost += canBuildSecond.cost;
        }

        // Every other tile of a multi-tile machine must be just as buildable at the same height.
        const uint8_t size = footprintSize(proto);
        for (int32_t index = 1; index < size * size; index++)
        {
            const CoordsXYZ at{ _loc.x + (index % size) * kCoordsXYStep, _loc.y + (index / size) * kCoordsXYStep, _loc.z };
            if (!LocationValid(at))
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
            if (!MapCanBuildAt(at))
                return Result(Status::notOwned, STR_FT_CANT_BUILD_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);
            auto* tileSurface = MapGetSurfaceElementAt(at);
            if (tileSurface == nullptr || tileSurface->getSlope() != 0 || tileSurface->getBaseZ() != _loc.z)
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_LEVEL_LAND_REQUIRED);
            if (tileSurface->getWaterHeight() > 0)
                return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_CAN_ONLY_BUILD_THIS_ON_LAND);
            if (findFactoryElement(at, GetFlags().has(CommandFlag::ghost)) != nullptr)
                return Result(Status::itemAlreadyPlaced, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_FACTORY_IN_THE_WAY);
            if (!MapCheckCapacityAndReorganise(at))
                return Result(Status::noFreeElements, STR_FT_CANT_BUILD_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
            auto canBuildTile = MapCanConstructWithClearAt(
                { at, at.z, clearanceZ }, MapPlaceNonSceneryClearFunc, QuarterTile{ 0b1111, 0 }, flags);
            if (canBuildTile.error != Status::ok)
            {
                canBuildTile.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
                return canBuildTile;
            }
            res.cost += canBuildTile.cost;
        }
        return res;
    }

    Result FactoryPlaceAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        return Validate(gameState, false);
    }

    Result FactoryPlaceAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Validate(gameState, true);
        if (res.error != Status::ok)
            return res;

        auto* element = placeElement(gameState, _loc, _direction, _entry, GetFlags().has(CommandFlag::ghost));
        if (element == nullptr)
            return Result(Status::noFreeElements, STR_FT_CANT_BUILD_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
        return res;
    }
} // namespace OpenRCT2::GameActions

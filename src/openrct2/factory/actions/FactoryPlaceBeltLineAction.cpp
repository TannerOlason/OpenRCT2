/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryPlaceBeltLineAction.h"

#include "../../GameState.h"
#include "../../actions/GameActionRunner.h"
#include "../../localisation/StringIds.h"
#include "../../world/Map.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../FactoryPrototypeObject.h"
#include "../FactoryStringIds.h"
#include "../FactoryTopology.h"
#include "FactoryPlaceAction.h"

#include <cstdlib>

namespace OpenRCT2::GameActions
{
    using namespace OpenRCT2::Factory;

    FactoryPlaceBeltLineAction::FactoryPlaceBeltLineAction(
        const CoordsXYZ& start, const CoordsXYZ& end, Direction direction, ObjectEntryIndex entry)
        : _start(start)
        , _end(end)
        , _direction(direction)
        , _entry(entry)
    {
    }

    std::vector<CoordsXYZ> FactoryPlaceBeltLineAction::lineTiles(
        const CoordsXYZ& start, const CoordsXYZ& end, Direction fallback, Direction& dir)
    {
        std::vector<CoordsXYZ> tiles;
        const int32_t dx = end.x - start.x;
        const int32_t dy = end.y - start.y;
        int32_t count;
        if (dx == 0 && dy == 0)
        {
            dir = fallback & 3;
            count = 1;
        }
        else if (std::abs(dx) >= std::abs(dy))
        {
            dir = dx > 0 ? 2 : 0;
            count = std::abs(dx) / kCoordsXYStep + 1;
        }
        else
        {
            dir = dy > 0 ? 1 : 3;
            count = std::abs(dy) / kCoordsXYStep + 1;
        }
        count = std::min(count, kMaxTiles);
        auto at = start;
        for (int32_t i = 0; i < count; i++)
        {
            tiles.push_back(at);
            at = CoordsXYZ{ CoordsXY(at) + CoordsDirectionDelta[dir], at.z };
        }
        return tiles;
    }

    void FactoryPlaceBeltLineAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("x", _start.x);
        visitor.Visit("y", _start.y);
        visitor.Visit("z", _start.z);
        visitor.Visit("endX", _end.x);
        visitor.Visit("endY", _end.y);
        visitor.Visit("endZ", _end.z);
        visitor.Visit("direction", _direction);
        visitor.Visit("entry", _entry);
    }

    uint16_t FactoryPlaceBeltLineAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags() | Flags::AllowWhilePaused;
    }

    void FactoryPlaceBeltLineAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_start) << DS_TAG(_end) << DS_TAG(_direction) << DS_TAG(_entry);
    }

    Result FactoryPlaceBeltLineAction::Run(GameState_t& gameState, bool apply) const
    {
        auto res = Result();
        res.position = { _start.x + 16, _start.y + 16, _start.z };
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;

        if (!LocationValid(_start) || !LocationValid(_end) || _direction > 3)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_OFF_EDGE_OF_MAP);
        auto* proto = getPrototype(_entry);
        if (proto == nullptr || !proto->isPlaceable() || proto->getKind() != PrototypeKind::belt)
            return Result(Status::invalidParameters, STR_FT_CANT_BUILD_THIS_HERE, STR_FT_PROTOTYPE_NOT_PLACEABLE);

        Direction dir;
        const auto tiles = lineTiles(_start, _end, _direction, dir);
        const bool ghost = GetFlags().has(CommandFlag::ghost);
        int32_t placed = 0;
        for (const auto& tile : tiles)
        {
            if (!LocationValid(tile))
                break;
            if (findFactoryElement(tile, ghost) != nullptr)
                continue; // occupied: skip rather than fail, so re-dragging over a line is harmless
            auto place = FactoryPlaceAction(tile, dir, _entry);
            place.SetFlags(GetFlags());
            auto tileResult = apply ? ExecuteNested(&place, gameState) : QueryNested(&place, gameState);
            if (tileResult.error != Status::ok)
            {
                if (placed == 0)
                {
                    // Nothing built: report the first tile's problem.
                    tileResult.errorTitle = STR_FT_CANT_BUILD_THIS_HERE;
                    return tileResult;
                }
                break;
            }
            res.cost += tileResult.cost;
            placed++;
        }
        return res;
    }

    Result FactoryPlaceBeltLineAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        return Run(gameState, false);
    }

    Result FactoryPlaceBeltLineAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        return Run(gameState, true);
    }
} // namespace OpenRCT2::GameActions

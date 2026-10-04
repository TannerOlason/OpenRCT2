/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryCreateWorldAction.h"

#include "../../GameState.h"
#include "../../localisation/StringIds.h"
#include "../FactoryStringIds.h"
#include "../WorldManager.h"

namespace OpenRCT2::GameActions
{
    FactoryCreateWorldAction::FactoryCreateWorldAction(uint16_t size)
        : _size(size)
    {
    }

    void FactoryCreateWorldAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit("size", _size);
    }

    uint16_t FactoryCreateWorldAction::GetActionFlags() const
    {
        return GameAction::GetActionFlags();
    }

    void FactoryCreateWorldAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_size);
    }

    Result FactoryCreateWorldAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        if (_size < 16 || _size > 256)
            return Result(Status::invalidParameters, STR_FT_CANT_CREATE_WORLD, STR_FT_WORLD_SIZE_INVALID);
        if (Factory::Worlds::count() >= Factory::Worlds::kMaxWorlds)
            return Result(Status::noFreeElements, STR_FT_CANT_CREATE_WORLD, STR_FT_TOO_MANY_WORLDS);
        return Result();
    }

    Result FactoryCreateWorldAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Query(gameState, park);
        if (res.error != Status::ok)
            return res;
        if (Factory::Worlds::create({ _size, _size }) >= Factory::Worlds::kMaxWorlds)
            return Result(Status::noFreeElements, STR_FT_CANT_CREATE_WORLD, STR_FT_TOO_MANY_WORLDS);
        return res;
    }
} // namespace OpenRCT2::GameActions

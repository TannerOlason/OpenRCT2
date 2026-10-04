/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Game command ids for fork actions (ADR 0006).

#pragma once

#include "../../actions/GameCommand.h"

#include <cstdint>

namespace OpenRCT2::GameActions
{
    // Fork actions live outside the upstream GameCommand enum so upstream additions never collide.
    constexpr int32_t kFactoryCommandBase = 10000;

    enum class FactoryCommand : int32_t
    {
        place = 0,
        remove = 1,
        rotate = 2,
        setRecipe = 3,
        setFilter = 4,
        placeBeltLine = 5,
        clearArea = 6,
        placeBlueprint = 7,
        setWire = 8,
        cheat = 9,
        setOre = 10,
        setParkOption = 11,
        marketSell = 12,
        damage = 13,
        threatSpawn = 14,
        threatDespawn = 15,
        count,
    };

    constexpr GameCommand toGameCommand(FactoryCommand command)
    {
        return static_cast<GameCommand>(kFactoryCommandBase + static_cast<int32_t>(command));
    }

    constexpr bool isFactoryCommand(GameCommand command)
    {
        const auto value = static_cast<int32_t>(command);
        return value >= kFactoryCommandBase && value < kFactoryCommandBase + static_cast<int32_t>(FactoryCommand::count);
    }
} // namespace OpenRCT2::GameActions

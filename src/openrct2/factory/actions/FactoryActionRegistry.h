/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The upstream registry defers to this one for ids >= kFactoryCommandBase.

#pragma once

#include "../../actions/GameActionRegistry.h"
#include "FactoryCommand.h"

#include <optional>
#include <span>
#include <string_view>

namespace OpenRCT2::GameActions::Factory
{
    std::optional<GameActionFactory> getFactory(GameCommand command);
    const char* getName(GameCommand command);
    bool isValidId(uint32_t id);

    // Every registered fork command, for the network permission table.
    std::span<const GameCommand> allCommands();

    // Plugin-facing action names ("factoryplace") <-> commands, for context.executeAction and hooks.
    std::optional<GameCommand> commandFromScriptName(std::string_view name);
    std::string_view scriptNameFromCommand(GameCommand command);
} // namespace OpenRCT2::GameActions::Factory

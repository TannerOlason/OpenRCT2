/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryActionRegistry.h"

#include "FactoryCreateWorldAction.h"
#include "FactoryDamageAction.h"
#include "FactoryMarketSellAction.h"
#include "FactoryPlaceAction.h"
#include "FactoryPlaceBeltLineAction.h"
#include "FactoryPlaceBlueprintAction.h"
#include "FactoryRemoveAction.h"
#include "FactoryRotateAction.h"
#include "FactorySetFilterAction.h"
#include "FactorySetOreAction.h"
#include "FactorySetParkOptionAction.h"
#include "FactorySetRecipeAction.h"
#include "FactoryThreatDespawnAction.h"
#include "FactoryThreatSpawnAction.h"

#include <array>

namespace OpenRCT2::GameActions::Factory
{
    struct Entry
    {
        GameCommand command;
        const char* name;
        std::string_view scriptName;
        GameActionFactory factory;
    };

    template<typename T>
    static constexpr Entry Make(const char* name, std::string_view scriptName)
    {
        return Entry{ T::kType, name, scriptName, []() -> GameAction* { return new T(); } };
    }

    static constexpr std::array kEntries = {
        Make<FactoryPlaceAction>("FactoryPlaceAction", "factoryplace"),
        Make<FactoryRemoveAction>("FactoryRemoveAction", "factoryremove"),
        Make<FactoryPlaceBeltLineAction>("FactoryPlaceBeltLineAction", "factoryplacebeltline"),
        Make<FactoryRotateAction>("FactoryRotateAction", "factoryrotate"),
        Make<FactorySetRecipeAction>("FactorySetRecipeAction", "factorysetrecipe"),
        Make<FactorySetOreAction>("FactorySetOreAction", "factorysetore"),
        Make<FactorySetFilterAction>("FactorySetFilterAction", "factorysetfilter"),
        Make<FactorySetParkOptionAction>("FactorySetParkOptionAction", "factorysetparkoption"),
        Make<FactoryMarketSellAction>("FactoryMarketSellAction", "factorymarketsell"),
        Make<FactoryDamageAction>("FactoryDamageAction", "factorydamage"),
        Make<FactoryCreateWorldAction>("FactoryCreateWorldAction", "factorycreateworld"),
        Make<FactoryPlaceBlueprintAction>("FactoryPlaceBlueprintAction", "factoryplaceblueprint"),
        Make<FactoryThreatSpawnAction>("FactoryThreatSpawnAction", "factorythreatspawn"),
        Make<FactoryThreatDespawnAction>("FactoryThreatDespawnAction", "factorythreatdespawn"),
    };

    static constexpr std::array<GameCommand, kEntries.size()> kCommands = [] {
        std::array<GameCommand, kEntries.size()> result{};
        for (size_t i = 0; i < kEntries.size(); i++)
            result[i] = kEntries[i].command;
        return result;
    }();

    static const Entry* find(GameCommand command)
    {
        for (const auto& entry : kEntries)
        {
            if (entry.command == command)
                return &entry;
        }
        return nullptr;
    }

    std::optional<GameActionFactory> getFactory(GameCommand command)
    {
        const auto* entry = find(command);
        if (entry == nullptr)
            return std::nullopt;
        return entry->factory;
    }

    const char* getName(GameCommand command)
    {
        const auto* entry = find(command);
        return entry != nullptr ? entry->name : "FactoryUnknownAction";
    }

    bool isValidId(uint32_t id)
    {
        return find(static_cast<GameCommand>(id)) != nullptr;
    }

    std::span<const GameCommand> allCommands()
    {
        return kCommands;
    }

    std::optional<GameCommand> commandFromScriptName(std::string_view name)
    {
        for (const auto& entry : kEntries)
        {
            if (entry.scriptName == name)
                return entry.command;
        }
        return std::nullopt;
    }

    std::string_view scriptNameFromCommand(GameCommand command)
    {
        const auto* entry = find(command);
        return entry != nullptr ? entry->scriptName : std::string_view{};
    }
} // namespace OpenRCT2::GameActions::Factory

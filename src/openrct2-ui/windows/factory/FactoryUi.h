/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Small UI helpers shared by upstream windows at their Touch Points.

#pragma once

#include <openrct2-ui/UiStringIds.h>
#include <openrct2/GameState.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Materials.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/management/Finance.h>
#include <string>

namespace OpenRCT2::Ui::Factory
{
    /**
     * The cost line a construction window shows for a previewed cost, appended to `ft`: upstream's STR_COST_LABEL in
     * money mode, "Cost: X + N item" in hybrid mode and "Needs: N item" in materials mode.
     */
    inline StringId constructionCostText(money64 cost, ExpenditureType expenditure, Formatter& ft)
    {
        using namespace OpenRCT2::Factory;
        const auto& gameState = getGameState();
        const auto bill = materialsActive(gameState) ? billFromCost(cost, expenditure) : MaterialBill{};
        if (bill.empty())
        {
            ft.Add<money64>(cost);
            return STR_COST_LABEL;
        }
        static std::string itemName;
        auto* proto = getPrototype(bill.front().item);
        itemName = proto != nullptr ? proto->GetName() : std::string();
        if (gameState.factory.parkExt.constructionMode == static_cast<uint8_t>(ConstructionMode::hybrid))
            ft.Add<money64>(cost);
        ft.Add<uint32_t>(bill.front().count);
        ft.Add<const char*>(itemName.c_str());
        return gameState.factory.parkExt.constructionMode == static_cast<uint8_t>(ConstructionMode::hybrid)
            ? static_cast<StringId>(STR_FT_COST_AND_MATERIALS)
            : static_cast<StringId>(STR_FT_COST_MATERIALS);
    }
} // namespace OpenRCT2::Ui::Factory

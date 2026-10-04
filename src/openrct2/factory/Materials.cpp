/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Materials.h"

#include "../Context.h"
#include "../GameState.h"
#include "../actions/GameAction.hpp"
#include "../actions/GameActionResult.h"
#include "../localisation/Formatter.h"
#include "../localisation/StringIds.h"
#include "../management/Finance.h"
#include "../object/ObjectList.h"
#include "../object/ObjectManager.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryStringIds.h"
#include "FactoryTopology.h"
#include "actions/FactoryCommand.h"

#include <algorithm>
#include <string>

namespace OpenRCT2::Factory
{
    static auto findEntry(const std::vector<WarehouseEntry>& stock, ObjectEntryIndex item)
    {
        return std::lower_bound(stock.begin(), stock.end(), item, [](const WarehouseEntry& entry, ObjectEntryIndex value) {
            return entry.item < value;
        });
    }

    uint32_t Warehouse::count(ObjectEntryIndex item) const
    {
        auto it = findEntry(stock, item);
        return it != stock.end() && it->item == item ? it->count : 0;
    }

    void Warehouse::deposit(ObjectEntryIndex item, uint32_t amount)
    {
        if (item == kObjectEntryIndexNull || amount == 0)
            return;
        auto it = findEntry(stock, item);
        if (it != stock.end() && it->item == item)
        {
            auto& entry = stock[static_cast<size_t>(it - stock.begin())];
            entry.count = static_cast<uint32_t>(std::min<uint64_t>(uint64_t{ entry.count } + amount, 0xFFFFFFFFu));
            return;
        }
        stock.insert(it, WarehouseEntry{ item, amount });
    }

    uint32_t Warehouse::take(ObjectEntryIndex item, uint32_t amount)
    {
        auto it = findEntry(stock, item);
        if (it == stock.end() || it->item != item)
            return 0;
        const auto index = static_cast<size_t>(it - stock.begin());
        const uint32_t taken = std::min(stock[index].count, amount);
        stock[index].count -= taken;
        if (stock[index].count == 0)
            stock.erase(stock.begin() + static_cast<std::ptrdiff_t>(index));
        return taken;
    }

    bool Warehouse::canCover(const MaterialBill& bill) const
    {
        return std::all_of(bill.begin(), bill.end(), [&](const MaterialLine& line) { return count(line.item) >= line.count; });
    }

    void Warehouse::consume(const MaterialBill& bill)
    {
        for (const auto& line : bill)
            take(line.item, line.count);
    }

    void Warehouse::depositBill(const MaterialBill& bill)
    {
        for (const auto& line : bill)
            deposit(line.item, line.count);
    }

    bool materialsActive(const GameState_t& gameState)
    {
        return gameState.factory.parkExt.constructionMode != static_cast<uint8_t>(ConstructionMode::money);
    }

    // The first loaded item (ascending entry) marked as a construction material for this kind of work, else Iron plate.
    static ObjectEntryIndex billItem(ExpenditureType expenditure, money64& value)
    {
        auto& objectManager = GetContext()->GetObjectManager();
        const uint8_t bit = expenditure == ExpenditureType::rideConstruction ? 1 : 2;
        const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
        for (size_t i = 0; i < count; i++)
        {
            auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
            if (proto != nullptr && proto->getKind() == PrototypeKind::item && (proto->getItem().constructionMaterial & bit))
            {
                value = proto->getItem().materialValue > 0 ? proto->getItem().materialValue : kMoneyPerBillItem;
                return static_cast<ObjectEntryIndex>(i);
            }
        }
        value = kMoneyPerBillItem;
        auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(kBillItemIdentifier));
        return object != nullptr ? objectManager.GetLoadedObjectEntryIndex(object) : kObjectEntryIndexNull;
    }

    MaterialBill billFromCost(money64 cost, ExpenditureType expenditure)
    {
        if (cost <= 0 || (expenditure != ExpenditureType::rideConstruction && expenditure != ExpenditureType::landscaping))
            return {};
        money64 value = kMoneyPerBillItem;
        const auto item = billItem(expenditure, value);
        if (item == kObjectEntryIndexNull)
            return {};
        return { MaterialLine{ item, static_cast<uint32_t>((cost + value - 1) / value) } };
    }

    ObjectEntryIndex shopStockItem(const GameState_t& gameState, uint8_t shopItem)
    {
        if (gameState.factory.parkExt.shopStockMode == 0 || gameState.factory.isEmpty())
            return kObjectEntryIndexNull;
        auto& objectManager = GetContext()->GetObjectManager();
        const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
        for (size_t i = 0; i < count; i++)
        {
            auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
            if (proto != nullptr && proto->getKind() == PrototypeKind::item && proto->getItem().shopItem == shopItem)
                return static_cast<ObjectEntryIndex>(i);
        }
        return kObjectEntryIndexNull;
    }

    bool shopItemSoldOut(const GameState_t& gameState, uint8_t shopItem)
    {
        const auto item = shopStockItem(gameState, shopItem);
        return item != kObjectEntryIndexNull && gameState.factory.warehouse.count(item) == 0;
    }

    bool takeShopStock(GameState_t& gameState, uint8_t shopItem)
    {
        const auto item = shopStockItem(gameState, shopItem);
        return item != kObjectEntryIndexNull && gameState.factory.warehouse.take(item, 1) == 1;
    }

    // Whether this action's result is billed at all.
    static bool billable(const GameState_t& gameState, const GameActions::GameAction& action, const GameActions::Result& result)
    {
        if (!materialsActive(gameState) || result.error != GameActions::Status::ok || result.cost == 0)
            return false;
        if (GameActions::isFactoryCommand(action.GetType()))
            return false; // factory building is paid in money
        // Same exemptions as money: ghosts, previews, the editor, no-money parks.
        return FinanceCheckMoneyRequired(action.GetFlags());
    }

    void onQuery(GameState_t& gameState, const GameActions::GameAction& action, GameActions::Result& result)
    {
        if (!billable(gameState, action, result))
            return;
        const auto bill = billFromCost(result.cost, result.expenditure);
        if (bill.empty())
            return;
        if (gameState.factory.parkExt.constructionMode == static_cast<uint8_t>(ConstructionMode::materials))
            result.cost = 0;
        if (!gameState.factory.warehouse.canCover(bill))
        {
            const auto& line = bill.front();
            auto* proto = getPrototype(line.item);
            result.error = GameActions::Status::insufficientMaterials;
            result.errorTitle = STR_CANT_DO_THIS;
            result.errorMessage = STR_FT_NEEDS_MATERIALS;
            // The message is shown straight away, one at a time, so a static buffer can hold the name.
            static std::string itemName;
            itemName = proto != nullptr ? proto->GetName() : std::string();
            auto ft = Formatter(result.errorMessageArgs.data());
            ft.Add<uint32_t>(line.count);
            ft.Add<const char*>(itemName.c_str());
            ft.Add<uint32_t>(gameState.factory.warehouse.count(line.item));
        }
    }

    void onExecute(GameState_t& gameState, const GameActions::GameAction& action, GameActions::Result& result)
    {
        if (!billable(gameState, action, result))
            return;
        if (result.cost > 0)
        {
            const auto bill = billFromCost(result.cost, result.expenditure);
            gameState.factory.warehouse.consume(bill); // covered: the query just checked
        }
        else
        {
            // Refunds return the materials a cost of the same size would have taken.
            gameState.factory.warehouse.depositBill(billFromCost(-result.cost, result.expenditure));
        }
        if (gameState.factory.parkExt.constructionMode == static_cast<uint8_t>(ConstructionMode::materials))
            result.cost = 0;
    }
} // namespace OpenRCT2::Factory

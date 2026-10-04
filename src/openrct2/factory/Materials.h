/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The material economy: the Warehouse and Material Bills (CONTEXT.md).

#pragma once

#include "../core/Money.hpp"
#include "../object/ObjectTypes.h"

#include <cstdint>
#include <vector>

enum class ExpenditureType : int32_t;

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::GameActions
{
    class GameAction;
    class Result;
} // namespace OpenRCT2::GameActions

namespace OpenRCT2::Factory
{
    struct MaterialLine
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint32_t count{};
    };
    using MaterialBill = std::vector<MaterialLine>;

    struct WarehouseEntry
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint32_t count{};

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(count);
        }
    };

    /**
     * The park-wide material store: warehouse containers deposit into it, construction (and later shops and the
     * Market) draw from it. Kept sorted by item entry so it serialises deterministically.
     */
    struct Warehouse
    {
        std::vector<WarehouseEntry> stock;

        bool isEmpty() const
        {
            return stock.empty();
        }
        uint32_t count(ObjectEntryIndex item) const;
        void deposit(ObjectEntryIndex item, uint32_t count);
        // Takes up to `count`; returns how many were taken.
        uint32_t take(ObjectEntryIndex item, uint32_t count);
        bool canCover(const MaterialBill& bill) const;
        // Takes the whole bill; the caller has checked canCover.
        void consume(const MaterialBill& bill);
        void depositBill(const MaterialBill& bill);

        template<typename V>
        void visit(V& v)
        {
            v.vec(stock, [](WarehouseEntry& entry, auto& vv) { entry.visit(vv); });
        }
    };

    enum class ConstructionMode : uint8_t
    {
        money,     // upstream: construction costs money only
        hybrid,    // money and a material bill
        materials, // a material bill only
        count,
    };

    // Money per item of the bill a construction cost turns into (5.00 per Iron plate).
    constexpr money64 kMoneyPerBillItem = 50;
    // The item construction bills ask for.
    constexpr const char* kBillItemIdentifier = "factory-tour.factory_prototype.iron_plate";

    // True when construction in this park needs materials.
    bool materialsActive(const GameState_t& gameState);
    // The bill for an upstream construction cost: one bill item per kMoneyPerBillItem, rounded up. Empty for zero or
    // negative costs, when the bill item is not loaded, and for expenditures that are not construction.
    MaterialBill billFromCost(money64 cost, ExpenditureType expenditure);

    /**
     * Runner hooks (GameActionRunner.cpp, top-level actions only, never for ghosts, noSpend or the editor, never for
     * fork actions). onQuery makes the money cost zero in materials mode and fails with insufficientMaterials when
     * the warehouse cannot cover the bill; onExecute takes the bill, or for a refund (negative cost) puts the
     * equivalent materials back, and again zeroes the money in materials mode.
     */
    void onQuery(GameState_t& gameState, const GameActions::GameAction& action, GameActions::Result& result);

    /**
     * Shops in warehouse stock mode (parkExt.shopStockMode = 1): the factory item that stocks a shop item (the first
     * loaded item prototype whose `shopItem` names it), or kObjectEntryIndexNull when the item is not stocked from the
     * Warehouse (any other mode, or no factory item makes it) and shops buy it in as upstream does.
     */
    ObjectEntryIndex shopStockItem(const GameState_t& gameState, uint8_t shopItem);
    // True when a Warehouse-stocked shop item has run out: guests think "sold out" instead of buying.
    bool shopItemSoldOut(const GameState_t& gameState, uint8_t shopItem);
    // On a sale: takes one from the Warehouse and returns true (no stock cost), or false for upstream stock costs.
    bool takeShopStock(GameState_t& gameState, uint8_t shopItem);
    void onExecute(GameState_t& gameState, const GameActions::GameAction& action, GameActions::Result& result);
} // namespace OpenRCT2::Factory

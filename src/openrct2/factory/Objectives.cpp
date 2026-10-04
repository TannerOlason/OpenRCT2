/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Objectives.h"

#include "../Context.h"
#include "../Date.h"
#include "../GameState.h"
#include "../object/ObjectList.h"
#include "../object/ObjectManager.h"
#include "../scenario/ScenarioObjective.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    static auto findEntry(const std::vector<ProducedEntry>& produced, ObjectEntryIndex item)
    {
        return std::lower_bound(produced.begin(), produced.end(), item, [](const ProducedEntry& entry, ObjectEntryIndex value) {
            return entry.item < value;
        });
    }

    uint32_t ProductionStats::count(ObjectEntryIndex item) const
    {
        auto it = findEntry(produced, item);
        return it != produced.end() && it->item == item ? it->count : 0;
    }

    static void addTo(std::vector<ProducedEntry>& entries, ObjectEntryIndex item, uint32_t amount)
    {
        auto it = findEntry(entries, item);
        if (it != entries.end() && it->item == item)
        {
            auto& entry = entries[static_cast<size_t>(it - entries.begin())];
            entry.count = static_cast<uint32_t>(std::min<uint64_t>(uint64_t{ entry.count } + amount, 0xFFFFFFFFu));
            return;
        }
        entries.insert(it, ProducedEntry{ item, amount });
    }

    static ProductionHistoryEntry& historyEntry(std::vector<ProductionHistoryEntry>& history, ObjectEntryIndex item)
    {
        auto it = std::lower_bound(
            history.begin(), history.end(), item,
            [](const ProductionHistoryEntry& entry, ObjectEntryIndex v) { return entry.item < v; });
        if (it == history.end() || it->item != item)
        {
            ProductionHistoryEntry entry;
            entry.item = item;
            it = history.insert(it, entry);
        }
        return *it;
    }

    bool ProductionHistoryEntry::isEmpty() const
    {
        for (size_t i = 0; i < kProductionSamples; i++)
            if (produced[i] != 0 || consumed[i] != 0)
                return false;
        return true;
    }

    void ProductionStats::add(ObjectEntryIndex item, uint32_t amount)
    {
        if (item == kObjectEntryIndexNull || amount == 0)
            return;
        addTo(produced, item, amount);
        historyEntry(history, item).produced[head] += amount;
    }

    void ProductionStats::consume(ObjectEntryIndex item, uint32_t amount)
    {
        if (item == kObjectEntryIndexNull || amount == 0)
            return;
        addTo(consumed, item, amount);
        historyEntry(history, item).consumed[head] += amount;
    }

    uint32_t ProductionStats::consumedCount(ObjectEntryIndex item) const
    {
        auto it = findEntry(consumed, item);
        return it != consumed.end() && it->item == item ? it->count : 0;
    }

    void ProductionStats::advanceSample()
    {
        head = static_cast<uint8_t>((head + 1) % kProductionSamples);
        for (auto& entry : history)
        {
            entry.produced[head] = 0;
            entry.consumed[head] = 0;
        }
        std::erase_if(history, [](const ProductionHistoryEntry& entry) { return entry.isEmpty(); });
    }

    const ProductionHistoryEntry* ProductionStats::historyOf(ObjectEntryIndex item) const
    {
        auto it = std::lower_bound(
            history.begin(), history.end(), item,
            [](const ProductionHistoryEntry& entry, ObjectEntryIndex v) { return entry.item < v; });
        return it != history.end() && it->item == item ? &*it : nullptr;
    }

    void ProductionStats::clear()
    {
        produced.clear();
        consumed.clear();
        history.clear();
        head = 0;
    }

    ObjectEntryIndex defaultObjectiveItem()
    {
        auto& objectManager = GetContext()->GetObjectManager();
        if (auto* plate = objectManager.GetLoadedObject(ObjectEntryDescriptor(kBillItemIdentifier)); plate != nullptr)
            return objectManager.GetLoadedObjectEntryIndex(plate);
        const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
        for (size_t i = 0; i < count; i++)
        {
            auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
            if (proto != nullptr && proto->getKind() == PrototypeKind::item && !proto->isFluid())
                return static_cast<ObjectEntryIndex>(i);
        }
        return kObjectEntryIndexNull;
    }

    Scenario::ObjectiveStatus checkObjective(const Scenario::Objective& objective, const GameState_t& gameState)
    {
        const auto& state = gameState.factory;
        bool reached = false;
        if (objective.Type == Scenario::ObjectiveType::produceItemsBy)
            reached = state.production.count(objective.NumGuests)
                >= static_cast<uint64_t>(std::max<money64>(0, objective.Currency));
        else
            reached = state.parkExt.guestsToured >= objective.NumGuests;
        if (reached)
            return Scenario::ObjectiveStatus::success;
        if (static_cast<int32_t>(GetDate().GetMonthsElapsed()) >= MONTH_COUNT * objective.Year)
            return Scenario::ObjectiveStatus::failure;
        return Scenario::ObjectiveStatus::undecided;
    }
} // namespace OpenRCT2::Factory

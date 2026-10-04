/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Technology.h"

#include "../Context.h"
#include "../GameState.h"
#include "../config/Config.h"
#include "../localisation/Formatter.h"
#include "../localisation/Formatting.h"
#include "../management/NewsItem.h"
#include "../management/Research.h"
#include "../object/ObjectEntryManager.h"
#include "../object/ObjectList.h"
#include "../object/ObjectManager.h"
#include "../object/SceneryGroupEntry.h"
#include "../ride/RideEntry.h"
#include "../ride/RideManager.hpp"
#include "../windows/Intent.h"
#include "../world/Scenery.h"
#include "FactoryPrototypeObject.h"
#include "FactoryStringIds.h"
#include "FactoryTopology.h"
#include "scripting/ScFactory.h"

#include <algorithm>
#include <atomic>

namespace OpenRCT2::Factory
{
    namespace
    {
        // Derived from the loaded objects only, never from game state, so every peer builds the same index.
        struct TechnologyIndex
        {
            std::vector<ObjectEntryIndex> technologies;
            // Sorted (entry, technology) pairs: the technologies that unlock each prototype.
            std::vector<std::pair<ObjectEntryIndex, ObjectEntryIndex>> unlockedBy;
        };

        std::atomic<bool> gIndexValid{ false };
        TechnologyIndex gIndex;

        const TechnologyIndex& index()
        {
            if (gIndexValid.load())
                return gIndex;
            gIndex = {};
            const auto count = static_cast<ObjectEntryIndex>(getObjectEntryGroupCount(ObjectType::factoryPrototype));
            for (ObjectEntryIndex entry = 0; entry < count; entry++)
            {
                auto* proto = getPrototype(entry);
                if (proto == nullptr || proto->getKind() != PrototypeKind::technology)
                    continue;
                gIndex.technologies.push_back(entry);
                for (const auto& unlock : proto->getTechnology().unlocks)
                {
                    const auto unlocked = unlock.resolve();
                    if (unlocked != kObjectEntryIndexNull)
                        gIndex.unlockedBy.emplace_back(unlocked, entry);
                }
            }
            std::sort(gIndex.unlockedBy.begin(), gIndex.unlockedBy.end());
            gIndexValid.store(true);
            return gIndex;
        }

        ObjectEntryIndex resolveObject(const std::string& identifier, ObjectType type)
        {
            auto& objectManager = GetContext()->GetObjectManager();
            auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(identifier));
            if (object == nullptr || object->GetObjectType() != type)
                return kObjectEntryIndexNull;
            return objectManager.GetLoadedObjectEntryIndex(object);
        }

        void inventRideEntry(ObjectEntryIndex rideEntryIndex)
        {
            const auto* rideEntry = GetRideEntryByIndex(rideEntryIndex);
            if (rideEntry == nullptr)
                return;
            RideEntrySetInvented(rideEntryIndex);
            for (auto rideType : rideEntry->ride_type)
                if (rideType != kRideTypeNull)
                    RideTypeSetInvented(rideType);
        }

        void inventSceneryGroup(ObjectEntryIndex groupIndex)
        {
            const auto* group = ObjectEntryManager::GetObjectEntry<SceneryGroupEntry>(groupIndex);
            if (group == nullptr)
                return;
            SceneryGroupSetInvented(groupIndex);
            for (const auto& scenery : group->SceneryEntries)
                ScenerySetInvented(scenery);
        }

        void applyUnlocksOf(const FactoryPrototypeObject& technology)
        {
            for (const auto& identifier : technology.getTechnology().rideEntries)
                inventRideEntry(resolveObject(identifier, ObjectType::ride));
            for (const auto& identifier : technology.getTechnology().sceneryGroups)
                inventSceneryGroup(resolveObject(identifier, ObjectType::sceneryGroup));
        }
    } // namespace

    bool ResearchState::isResearched(ObjectEntryIndex technology) const
    {
        return std::binary_search(researched.begin(), researched.end(), technology);
    }

    uint32_t ResearchState::unitsDone(ObjectEntryIndex technology) const
    {
        auto it = std::lower_bound(progress.begin(), progress.end(), technology, [](const auto& entry, auto value) {
            return entry.technology < value;
        });
        return it != progress.end() && it->technology == technology ? it->units : 0;
    }

    void invalidateTechnologyIndex()
    {
        gIndexValid.store(false);
    }

    const std::vector<ObjectEntryIndex>& loadedTechnologies()
    {
        return index().technologies;
    }

    bool isTechnologyAvailable(const GameState_t& gameState, ObjectEntryIndex technology)
    {
        const auto& research = gameState.factory.research;
        auto* proto = getPrototype(technology);
        if (proto == nullptr || proto->getKind() != PrototypeKind::technology || research.isResearched(technology))
            return false;
        for (const auto& prerequisite : proto->getTechnology().prerequisites)
        {
            const auto entry = prerequisite.resolve();
            if (entry != kObjectEntryIndexNull && !research.isResearched(entry))
                return false;
        }
        return true;
    }

    bool isPrototypeUnlocked(const GameState_t& gameState, ObjectEntryIndex entry)
    {
        if (gameState.cheats.ignoreResearchStatus)
            return true;
        const auto& unlockedBy = index().unlockedBy;
        auto it = std::lower_bound(unlockedBy.begin(), unlockedBy.end(), std::make_pair(entry, ObjectEntryIndex{ 0 }));
        if (it == unlockedBy.end() || it->first != entry)
            return true;
        for (; it != unlockedBy.end() && it->first == entry; ++it)
            if (gameState.factory.research.isResearched(it->second))
                return true;
        return false;
    }

    void addResearchUnit(GameState_t& gameState, ObjectEntryIndex technology)
    {
        auto& research = gameState.factory.research;
        auto* proto = getPrototype(technology);
        if (proto == nullptr || proto->getKind() != PrototypeKind::technology || research.isResearched(technology))
            return;
        auto it = std::lower_bound(
            research.progress.begin(), research.progress.end(), technology,
            [](const auto& entry, auto value) { return entry.technology < value; });
        if (it == research.progress.end() || it->technology != technology)
            it = research.progress.insert(it, TechnologyProgress{ technology, 0 });
        it->units++;
        if (it->units >= proto->getTechnology().units)
            completeTechnology(gameState, technology);
    }

    void completeTechnology(GameState_t& gameState, ObjectEntryIndex technology)
    {
        auto& research = gameState.factory.research;
        auto* proto = getPrototype(technology);
        if (proto == nullptr || proto->getKind() != PrototypeKind::technology || research.isResearched(technology))
            return;
        research.researched.insert(
            std::lower_bound(research.researched.begin(), research.researched.end(), technology), technology);
        research.progress.erase(
            std::remove_if(
                research.progress.begin(), research.progress.end(),
                [technology](const auto& entry) { return entry.technology == technology; }),
            research.progress.end());
        if (research.current == technology)
            research.current = kObjectEntryIndexNull;
        applyUnlocksOf(*proto);

        if (!gSilentResearch && Config::Get().notifications.rideResearched)
        {
            const auto name = proto->GetName();
            Formatter ft;
            ft.Add<const char*>(name.c_str());
            const auto text = FormatStringIDLegacy(STR_FT_TECHNOLOGY_RESEARCHED, ft.Data());
            News::AddItemToQueue(News::ItemType::blank, text.c_str(), 0);
        }
        auto intent = Intent(INTENT_ACTION_INIT_SCENERY);
        ContextBroadcastIntent(&intent);
        invokeResearchCompleteHook(technology);
    }

    bool withholdGatedResearch(GameState_t& gameState)
    {
        const auto& technologies = loadedTechnologies();
        if (technologies.empty())
            return false;
        std::vector<ObjectEntryIndex> rides;
        std::vector<ObjectEntryIndex> groups;
        for (const auto technology : technologies)
        {
            const auto& props = getPrototype(technology)->getTechnology();
            for (const auto& identifier : props.rideEntries)
                rides.push_back(resolveObject(identifier, ObjectType::ride));
            for (const auto& identifier : props.sceneryGroups)
                groups.push_back(resolveObject(identifier, ObjectType::sceneryGroup));
        }
        auto gated = [&](const ResearchItem& item) {
            const auto& list = item.type == Research::EntryType::ride ? rides : groups;
            return std::find(list.begin(), list.end(), item.entryIndex) != list.end();
        };
        bool removed = false;
        for (auto* items : { &gameState.researchItemsInvented, &gameState.researchItemsUninvented })
        {
            const auto before = items->size();
            items->erase(std::remove_if(items->begin(), items->end(), gated), items->end());
            removed |= items->size() != before;
        }
        return removed;
    }

    void applyTechnologyUnlocks(const GameState_t& gameState)
    {
        for (const auto technology : loadedTechnologies())
            if (gameState.factory.research.isResearched(technology))
                applyUnlocksOf(*getPrototype(technology));
    }
} // namespace OpenRCT2::Factory

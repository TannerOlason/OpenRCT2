/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The fork's technology tree, researched by labs (ADR 0012).

#pragma once

#include "../object/ObjectTypes.h"

#include <cstdint>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    struct TechnologyProgress
    {
        ObjectEntryIndex technology{ kObjectEntryIndexNull };
        uint32_t units{};

        template<typename V>
        void visit(V& v)
        {
            v(technology);
            v(units);
        }
    };

    /**
     * Research state: researched technologies (sorted), units done on unfinished ones (sorted by technology) and the
     * technology labs work on. Saved in the pools chunk (version 10).
     */
    struct ResearchState
    {
        std::vector<ObjectEntryIndex> researched;
        std::vector<TechnologyProgress> progress;
        ObjectEntryIndex current{ kObjectEntryIndexNull };

        bool isEmpty() const
        {
            return researched.empty() && progress.empty() && current == kObjectEntryIndexNull;
        }
        void reset()
        {
            researched.clear();
            progress.clear();
            current = kObjectEntryIndexNull;
        }
        bool isResearched(ObjectEntryIndex technology) const;
        uint32_t unitsDone(ObjectEntryIndex technology) const;

        template<typename V>
        void visit(V& v)
        {
            v.vec(researched, [](ObjectEntryIndex& entry, auto& vv) { vv(entry); });
            v.vec(progress, [](TechnologyProgress& entry, auto& vv) { entry.visit(vv); });
            v(current);
        }
    };

    // Called whenever a factory prototype loads or unloads; the lock index is rebuilt on the next query.
    void invalidateTechnologyIndex();

    // Loaded technologies, ascending entry index.
    const std::vector<ObjectEntryIndex>& loadedTechnologies();

    // Not researched yet and every prerequisite researched.
    bool isTechnologyAvailable(const GameState_t& gameState, ObjectEntryIndex technology);

    /**
     * A factory prototype is locked while some loaded technology unlocks it and none of those is researched. The
     * "ignore research status" cheat unlocks everything.
     */
    bool isPrototypeUnlocked(const GameState_t& gameState, ObjectEntryIndex entry);

    // A lab finished a unit of `technology`; completes it once all its units are done.
    void addResearchUnit(GameState_t& gameState, ObjectEntryIndex technology);

    // Marks the technology researched, unlocks its rides and scenery and announces it.
    void completeTechnology(GameState_t& gameState, ObjectEntryIndex technology);

    /**
     * Upstream research hooks (Research.cpp). withholdGatedResearch removes ride entries and scenery groups that a
     * loaded technology unlocks from upstream's research lists and returns true if it removed any;
     * applyTechnologyUnlocks invents those of researched technologies. Both do nothing without technologies.
     */
    bool withholdGatedResearch(GameState_t& gameState);
    void applyTechnologyUnlocks(const GameState_t& gameState);
} // namespace OpenRCT2::Factory

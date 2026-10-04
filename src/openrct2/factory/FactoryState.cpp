/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryState.h"

#include "../GameState.h"
#include "../profiling/Profiling.h"

namespace OpenRCT2::Factory
{
    void State::reset()
    {
        containers.clear();
        inserters.clear();
        beltSegments.clear();
        topologyVersion = 0;
    }

    bool State::isEmpty() const
    {
        return recordCount() == 0 && topologyVersion == 0;
    }

    size_t State::recordCount() const
    {
        return containers.aliveCount() + inserters.aliveCount() + beltSegments.aliveCount();
    }

    void update(GameState_t& gameState)
    {
        PROFILED_FUNCTION();

        auto& state = gameState.factory;
        if (state.isEmpty())
        {
            return;
        }
        // M1: belt, inserter and container updates land here, in this fixed order:
        // belts move items, then inserters pick up and drop, then containers settle.
    }
} // namespace OpenRCT2::Factory

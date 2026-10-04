/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#pragma once

#include "../entity/EntityRegistry.h"

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    /**
     * The composite desync checksum: upstream's entity checksum (Guest, Staff, Vehicle, Litter) followed by
     * the factory state. Replaces every getAllEntitiesChecksum() call site in the network, replay and
     * simulate paths. With an empty factory the result equals upstream's exactly, so upstream replays and
     * Vanilla Mode servers are unaffected.
     */
    EntitiesChecksum computeSyncChecksum(GameState_t& gameState);
} // namespace OpenRCT2::Factory

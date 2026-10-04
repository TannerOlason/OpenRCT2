/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "SyncChecksum.h"

#include "../GameState.h"
#include "../core/ChecksumStream.h"
#include "../core/DataSerialiser.h"
#include "../entity/EntityList.h"
#include "../entity/Guest.h"
#include "../entity/Litter.h"
#include "../entity/Staff.h"
#include "../ride/Vehicle.h"
#include "FactorySerialisation.h"
#include "WorldManager.h"

namespace OpenRCT2::Factory
{
#ifndef DISABLE_NETWORK
    EntitiesChecksum computeSyncChecksum(GameState_t& gameState)
    {
        EntitiesChecksum checksum{};

        ChecksumStream ms(checksum.raw);
        DataSerialiser ds(true, ms);
        gameState.entities.networkSerialiseEntityTypes<Guest, Staff, Vehicle, Litter>(ds);
        if (!gameState.factory.isEmpty())
        {
            serialise(gameState.factory, ds);
        }

        // Other worlds (ADR 0015) fold in after the active one, in ascending order, each with its caches swapped in.
        if (Worlds::count() > 1)
        {
            const auto active = Worlds::active();
            for (Worlds::WorldId id = 0; id < Worlds::count(); id++)
            {
                if (id == active)
                    continue;
                Worlds::Scope inWorld(id);
                auto& other = getGameState();
                other.entities.networkSerialiseEntityTypes<Guest, Staff, Vehicle, Litter>(ds);
                if (!other.factory.isEmpty())
                    serialise(other.factory, ds);
            }
        }

        return checksum;
    }
#else
    EntitiesChecksum computeSyncChecksum(GameState_t& gameState)
    {
        return EntitiesChecksum{};
    }
#endif // DISABLE_NETWORK
} // namespace OpenRCT2::Factory

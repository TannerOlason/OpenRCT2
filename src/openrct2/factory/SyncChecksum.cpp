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

        return checksum;
    }
#else
    EntitiesChecksum computeSyncChecksum(GameState_t& gameState)
    {
        return EntitiesChecksum{};
    }
#endif // DISABLE_NETWORK
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactorySerialisation.h"

#include "../Diagnostic.h"
#include "../GameState.h"

namespace OpenRCT2::Factory
{
    void readWriteParkChunks(GameState_t& gameState, OrcaStream& os)
    {
        auto& state = gameState.factory;
        const bool reading = os.getMode() == OrcaStream::Mode::reading;

        if (!reading && state.isEmpty())
        {
            return;
        }

        bool compatible = true;
        auto found = os.readWriteChunk(ChunkType::factoryHeader, [&](OrcaStream::ChunkStream& cs) {
            uint16_t version = kFactoryHeaderVersion;
            cs.readWrite(version);
            auto records = static_cast<uint32_t>(state.recordCount());
            cs.readWrite(records);
            if (version > kFactoryHeaderVersion)
            {
                LOG_ERROR("Factory header chunk version %u is newer than supported %u", version, kFactoryHeaderVersion);
                compatible = false;
            }
        });

        if (reading && (!found || !compatible))
        {
            state.reset();
            return;
        }

        os.readWriteChunk(ChunkType::factoryPools, [&](OrcaStream::ChunkStream& cs) {
            uint16_t version = kFactoryPoolsVersion;
            cs.readWrite(version);
            if (version > kFactoryPoolsVersion)
            {
                LOG_ERROR("Factory pools chunk version %u is newer than supported %u", version, kFactoryPoolsVersion);
                compatible = false;
                return;
            }
            ChunkVisitor visitor{ cs };
            state.visit(visitor);
        });

        if (reading && !compatible)
        {
            state.reset();
        }
    }

    void serialise(State& state, DataSerialiser& ds)
    {
        SerialiserVisitor visitor{ ds };
        state.visit(visitor);
    }
} // namespace OpenRCT2::Factory

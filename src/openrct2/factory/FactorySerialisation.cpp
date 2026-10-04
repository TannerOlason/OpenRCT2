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
#include "FactoryTopology.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace OpenRCT2::Factory
{
    static void readWriteOreChunk(State& state, OrcaStream& os);
    static void readWriteParkExtChunk(State& state, OrcaStream& os);

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

        readWriteOreChunk(state, os);
        readWriteParkExtChunk(state, os);

        os.readWriteChunk(ChunkType::factoryPools, [&](OrcaStream::ChunkStream& cs) {
            uint16_t version = kFactoryPoolsVersion;
            cs.readWrite(version);
            if (version != kFactoryPoolsVersion)
            {
                // Pre-release saves are not migrated: the records change shape between milestones.
                LOG_ERROR("Factory pools chunk version %u is not the supported %u", version, kFactoryPoolsVersion);
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
        if (reading)
        {
            postLoad(gameState);
        }
    }

    static void readWriteOreChunk(State& state, OrcaStream& os)
    {
        const bool reading = os.getMode() == OrcaStream::Mode::reading;
        if (!reading && state.ore.isEmpty())
        {
            return;
        }
        bool found = os.readWriteChunk(ChunkType::factoryOre, [&](OrcaStream::ChunkStream& cs) {
            uint16_t version = kFactoryOreVersion;
            cs.readWrite(version);
            if (version > kFactoryOreVersion)
            {
                LOG_ERROR("Factory ore chunk version %u is newer than supported %u", version, kFactoryOreVersion);
                state.ore.clear();
                return;
            }
            if (reading)
            {
                int32_t width = 0;
                int32_t height = 0;
                cs.readWrite(width);
                cs.readWrite(height);
                const auto total = static_cast<size_t>(std::max(0, width)) * static_cast<size_t>(std::max(0, height));
                std::vector<OreCell> cells;
                cells.reserve(total);
                auto runs = cs.read<uint32_t>();
                for (uint32_t i = 0; i < runs && cells.size() < total; i++)
                {
                    auto length = cs.read<uint32_t>();
                    OreCell cell;
                    cs.readWrite(cell.ore);
                    cs.readWrite(cell.richness);
                    cs.readWrite(cell.amount);
                    length = static_cast<uint32_t>(std::min<size_t>(length, total - cells.size()));
                    cells.insert(cells.end(), length, cell);
                }
                state.ore.assign({ width, height }, std::move(cells));
            }
            else
            {
                int32_t width = state.ore.width();
                int32_t height = state.ore.height();
                cs.readWrite(width);
                cs.readWrite(height);
                const auto& cells = state.ore.cells();
                std::vector<std::pair<uint32_t, OreCell>> runs;
                for (const auto& cell : cells)
                {
                    if (!runs.empty() && runs.back().second == cell)
                        runs.back().first++;
                    else
                        runs.emplace_back(1u, cell);
                }
                cs.write(static_cast<uint32_t>(runs.size()));
                for (auto& run : runs)
                {
                    cs.write(run.first);
                    cs.readWrite(run.second.ore);
                    cs.readWrite(run.second.richness);
                    cs.readWrite(run.second.amount);
                }
            }
        });
        if (reading && !found)
        {
            state.ore.clear();
        }
    }

    static void readWriteParkExtChunk(State& state, OrcaStream& os)
    {
        const bool reading = os.getMode() == OrcaStream::Mode::reading;
        if (!reading && state.parkExt.isEmpty())
            return;
        bool found = os.readWriteChunk(ChunkType::parkExt, [&](OrcaStream::ChunkStream& cs) {
            uint16_t version = kParkExtVersion;
            cs.readWrite(version);
            if (version > kParkExtVersion)
            {
                LOG_ERROR("Park extension chunk version %u is newer than supported %u", version, kParkExtVersion);
                state.parkExt.reset();
                return;
            }
            ChunkVisitor visitor{ cs };
            state.parkExt.visit(visitor);
        });
        if (reading && !found)
            state.parkExt.reset();
    }

    void serialise(State& state, DataSerialiser& ds)
    {
        SerialiserVisitor visitor{ ds };
        state.visit(visitor);
        // The ore layer contributes its dimensions and running hash rather than every cell. An empty layer is not
        // saved, so a loaded park has no dimensions for it: count them only when there is ore, or a client that
        // joins a park whose ore was all mined would desync on the layer's size alone.
        int32_t width = state.ore.isEmpty() ? 0 : state.ore.width();
        int32_t height = state.ore.isEmpty() ? 0 : state.ore.height();
        uint64_t hash = state.ore.hash();
        uint32_t nonEmpty = state.ore.nonEmptyCount();
        ds << width << height << hash << nonEmpty;
        state.parkExt.visit(visitor);
    }
} // namespace OpenRCT2::Factory

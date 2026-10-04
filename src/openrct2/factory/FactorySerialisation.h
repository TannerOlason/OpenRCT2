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

#include "../core/DataSerialiser.h"
#include "../core/OrcaStream.hpp"
#include "FactoryState.h"

#include <cstdint>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    /**
     * Park-file chunk ids reserved for the fork (CONTEXT.md reserved id map). They never collide with
     * ParkFileChunkType and each chunk carries its own version, so kParkFileCurrentVersion stays untouched.
     */
    enum class ChunkType : uint32_t
    {
        factoryHeader = 0x40,
        factoryPools = 0x41,
        factoryOre = 0x42,
        company = 0x43,
        worlds = 0x44,
        parkExt = 0x45,
    };

    constexpr uint16_t kFactoryHeaderVersion = 1;
    // Pools chunk versions: 2 belt link kinds, undergrounds, splitters, poles, networks; 3 fluids; 4 splitter filters;
    // 5 ride proximity totals; 6 pollution; 7 warehouse.
    constexpr uint16_t kFactoryPoolsVersion = 7;
    constexpr uint16_t kFactoryOreVersion = 1;
    constexpr uint16_t kParkExtVersion = 2; // 2: scenario options

    /**
     * Visitor that reads or writes record fields through an OrcaStream chunk.
     */
    struct ChunkVisitor
    {
        OrcaStream::ChunkStream& cs;

        bool isReading() const
        {
            return cs.getMode() == OrcaStream::Mode::reading;
        }

        template<typename T>
        void operator()(T& value)
        {
            cs.readWrite(value);
        }

        template<typename T, typename F>
        void vec(std::vector<T>& items, F elementVisit)
        {
            cs.readWriteVector(items, [&](T& element) { elementVisit(element, *this); });
        }
    };

    /**
     * Visitor that reads or writes record fields through a DataSerialiser (sync checksum, snapshots, tests).
     */
    struct SerialiserVisitor
    {
        DataSerialiser& ds;

        bool isReading() const
        {
            return ds.isLoading();
        }

        template<typename T>
        void operator()(T& value)
        {
            ds << value;
        }

        template<typename T, typename F>
        void vec(std::vector<T>& items, F elementVisit)
        {
            auto count = static_cast<uint32_t>(items.size());
            ds << count;
            if (isReading())
            {
                items.assign(count, T{});
            }
            for (auto& element : items)
            {
                elementVisit(element, *this);
            }
        }
    };

    /**
     * Reads or writes the factory chunks. Writing is skipped entirely for an empty state so a Vanilla Mode
     * park saves byte-identically to upstream; a missing chunk on read leaves the state reset.
     */
    void readWriteParkChunks(GameState_t& gameState, OrcaStream& os);

    /**
     * Serialises the whole state through a DataSerialiser in visit order.
     */
    void serialise(State& state, DataSerialiser& ds);
} // namespace OpenRCT2::Factory

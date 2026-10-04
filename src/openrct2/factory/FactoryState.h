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

#include "FactoryPool.hpp"
#include "FactoryRecords.h"

#include <cstdint>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    /**
     * All deterministic factory simulation state. Lives inside GameState_t (ADR 0003/0004), is reset by
     * gameStateInitAll, ticked after Ride::updateAll() and persisted in fork chunks 0x40-0x42 (ADR 0007).
     * Nothing in here may be a pointer, a float or a hash-ordered container.
     */
    struct State
    {
        Pool<ContainerRecord> containers;
        Pool<InserterRecord> inserters;
        Pool<BeltSegmentRecord> beltSegments;

        // Incremented whenever an element is placed, removed or rotated; records that cache references
        // to neighbours re-resolve them when this changes.
        uint32_t topologyVersion{};

        void reset();

        // True when the park has never had factory content: the sync checksum and the saved park must
        // then be byte-identical to upstream (Vanilla Mode).
        bool isEmpty() const;

        size_t recordCount() const;

        template<typename V>
        void visit(V& v)
        {
            v(topologyVersion);
            containers.visit(v);
            inserters.visit(v);
            beltSegments.visit(v);
        }
    };

    /**
     * One simulation tick. Called from gameStateUpdateLogic between Ride::updateAll() and Park::Update so
     * the park sees this tick's production.
     */
    void update(GameState_t& gameState);

    // Container helpers shared by inserters, tests and later machines.
    bool containerTakeAny(ContainerRecord& container, ItemStack& hand);
    bool containerInsert(ContainerRecord& container, ObjectEntryIndex item, uint16_t stackSize);
} // namespace OpenRCT2::Factory

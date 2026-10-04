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
#include "Ore.h"

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
        Pool<MachineRecord> machines;
        Pool<PoleRecord> poles;
        Pool<PowerNetworkRecord> powerNetworks;
        OreLayer ore; // saved in its own chunk (0x42); its hash joins the sync checksum

        // Set when poles, generators or consumers change; networks are rebuilt by BFS on the next tick.
        bool powerDirty{};

        // Incremented whenever an element is placed, removed or rotated; records that cache references
        // to neighbours re-resolve them when this changes.
        uint32_t topologyVersion{};

        void reset();

        // True when the park has never had factory content: the sync checksum and the saved park must
        // then be byte-identical to upstream (Vanilla Mode).
        bool isEmpty() const;

        size_t recordCount() const;

        // Visits the pools (not the ore layer, which has its own chunk and hash).
        template<typename V>
        void visit(V& v)
        {
            v(topologyVersion);
            containers.visit(v);
            inserters.visit(v);
            beltSegments.visit(v);
            machines.visit(v);
            poles.visit(v);
            powerNetworks.visit(v);
            uint8_t dirty = powerDirty ? 1 : 0;
            v(dirty);
            powerDirty = dirty != 0;
        }
    };

    // Machine helpers shared by inserters, drills and tests.
    bool machineAcceptsInput(const State& state, const MachineRecord& machine, ObjectEntryIndex item);
    bool machineInsertInput(State& state, MachineRecord& machine, ObjectEntryIndex item);
    bool machineTakeOutput(MachineRecord& machine, ItemStack& hand);

    // Rebuilds power networks from the poles (wire reach) and attaches machines within a pole's supply radius.
    void rebuildPowerNetworks(State& state);

    /**
     * One simulation tick. Called from gameStateUpdateLogic between Ride::updateAll() and Park::Update so
     * the park sees this tick's production.
     */
    void update(GameState_t& gameState);

    // Container helpers shared by inserters, tests and later machines.
    bool containerTakeAny(ContainerRecord& container, ItemStack& hand);
    bool containerInsert(ContainerRecord& container, ObjectEntryIndex item, uint16_t stackSize);
} // namespace OpenRCT2::Factory

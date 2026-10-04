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
#include "ParkExt.h"
#include "Pollution.h"
#include "RideRatingsFactory.h"

#include <cstdint>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    struct MachineProperties;

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
        Pool<SplitterRecord> splitters;
        Pool<PoleRecord> poles;
        Pool<PowerNetworkRecord> powerNetworks;
        Pool<PipeRecord> pipes;
        Pool<FluidNetworkRecord> fluidNetworks;
        // Factory proximity totals of rides part-way through a rating calculation, sorted by ride id.
        std::vector<RideProximityEntry> rideProximity;
        PollutionLayer pollution;
        OreLayer ore;
        ParkExt parkExt; // saved in its own chunk (0x45); part of the sync checksum // saved in its own chunk (0x42); its hash
                         // joins the sync checksum

        // Set when poles, generators or consumers change; networks are rebuilt by BFS on the next tick.
        bool powerDirty{};
        // Set when pipes or machines with fluid boxes change; fluid networks are rebuilt on the next tick.
        bool fluidDirty{};

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
            splitters.visit(v);
            poles.visit(v);
            powerNetworks.visit(v);
            uint8_t dirty = powerDirty ? 1 : 0;
            v(dirty);
            powerDirty = dirty != 0;
            pipes.visit(v);
            fluidNetworks.visit(v);
            uint8_t fluidDirtyByte = fluidDirty ? 1 : 0;
            v(fluidDirtyByte);
            fluidDirty = fluidDirtyByte != 0;
            v.vec(rideProximity, VisitElement{});
            pollution.visit(v);
        }
    };

    // Machine helpers shared by inserters, drills and tests.
    bool machineAcceptsInput(const State& state, const MachineRecord& machine, ObjectEntryIndex item);
    bool machineInsertInput(State& state, MachineRecord& machine, ObjectEntryIndex item);
    bool machineTakeOutput(MachineRecord& machine, ItemStack& hand);

    // Rebuilds power networks from the poles (wire reach) and attaches machines within a pole's supply radius.
    void rebuildPowerNetworks(State& state);

    // Machine helpers shared with the fluid simulation.
    void setMachineStatus(MachineRecord& machine, MachineStatus status);
    bool machineBurnFuel(MachineRecord& machine, const MachineProperties& props);
    bool machineHasFuel(const MachineRecord& machine, const MachineProperties& props);

    // Wall-clock nanoseconds spent in each phase of update(), accumulated (factory-bench only).
    struct UpdatePhaseTimes
    {
        uint64_t belts{};
        uint64_t splitters{};
        uint64_t inserters{};
        uint64_t power{};
        uint64_t fluids{};
        uint64_t machines{};
    };

    /**
     * One simulation tick. Called from gameStateUpdateLogic between Ride::updateAll() and Park::Update so
     * the park sees this tick's production. With `times`, each phase's duration is added to it; timing never
     * changes the simulation.
     */
    void update(GameState_t& gameState, UpdatePhaseTimes* times = nullptr);

    // Container helpers shared by inserters, tests and later machines.
    bool containerTakeAny(ContainerRecord& container, ItemStack& hand);
    bool containerInsert(ContainerRecord& container, ObjectEntryIndex item, uint16_t stackSize);
} // namespace OpenRCT2::Factory

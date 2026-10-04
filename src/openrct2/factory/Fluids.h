/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Pipes, fluid networks and the machines that move fluid (pump, boiler, engine).
//
// A fluid network is a connected component of nodes: pipes and machine fluid boxes. Two nodes on neighbouring
// tiles connect when each faces the other (a pipe faces all four directions, a box faces its sides). The
// component holds one fluid as one volume, so there is no per-pipe flow (CONTEXT.md, Fluid Network).

#pragma once

#include "../world/Location.hpp"
#include "FactoryPrototypeObject.h"
#include "FactoryRecords.h"

namespace OpenRCT2
{
    struct FactoryElement;
}

namespace OpenRCT2::Factory
{
    struct State;

    // World directions (bit d = CoordsDirectionDelta[d]) a box faces when its machine faces `dir`.
    uint8_t fluidBoxWorldSides(const FluidBoxProperties& box, Direction dir);

    // World directions the pipe or machine at this element connects fluid through (all four for a pipe).
    uint8_t fluidFacingMask(const FactoryElement& element);

    // World directions the pipe at loc actually connects to a neighbouring pipe or fluid box.
    uint8_t pipeConnectionMask(const CoordsXYZ& loc);

    // Recomputes the cached connection masks of pipes at loc and its four neighbours after a change.
    void refreshPipeConnectionsAround(const CoordsXYZ& loc);

    // True when the tile behind a machine facing `dir` at loc holds water (where an offshore pump draws from).
    bool hasWaterBehind(const CoordsXYZ& loc, Direction dir);

    /**
     * Rebuilds every fluid network from pipes and machine boxes. Fluid already in the old networks is first
     * shared out over their nodes by capacity, then summed into the new components, so splitting and joining
     * networks conserves fluid. A component that ends up with two fluids keeps the larger and drops the other.
     */
    void rebuildFluidNetworks(State& state);

    // Start of tick: rebuild if dirty, then turn last tick's demand into this tick's satisfaction.
    void updateFluidNetworks(State& state);

    // The network a machine's box belongs to, or nullptr.
    FluidNetworkRecord* machineFluidNetwork(State& state, const MachineRecord& machine, size_t box);

    // Index of the first box with this role, or -1.
    int32_t findFluidBox(const MachineProperties& props, FluidBoxRole role);

    void updatePump(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto);
    void updateBoiler(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto);
    void updateSteamEngine(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto);
} // namespace OpenRCT2::Factory

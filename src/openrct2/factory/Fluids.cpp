/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Fluids.h"

#include "../world/Map.h"
#include "../world/tile_element/FactoryElement.h"
#include "../world/tile_element/SurfaceElement.h"
#include "FactoryState.h"
#include "FactoryTopology.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace OpenRCT2::Factory
{
    uint8_t fluidBoxWorldSides(const FluidBoxProperties& box, Direction dir)
    {
        uint8_t world = 0;
        for (uint8_t k = 0; k < 4; k++)
        {
            if (box.sides & (1 << k))
                world |= static_cast<uint8_t>(1 << ((k + dir) & 3));
        }
        return world;
    }

    uint8_t fluidFacingMask(const FactoryElement& element)
    {
        switch (element.getSubtype())
        {
            case FactoryElementSubtype::pipe:
                return 0b1111;
            case FactoryElementSubtype::machine:
            {
                auto* proto = getPrototype(element);
                if (proto == nullptr)
                    return 0;
                uint8_t mask = 0;
                for (const auto& box : proto->getMachine().fluidBoxes)
                    mask |= fluidBoxWorldSides(box, element.getDirection());
                return mask;
            }
            default:
                return 0;
        }
    }

    uint8_t pipeConnectionMask(const CoordsXYZ& loc)
    {
        uint8_t mask = 0;
        for (Direction d = 0; d < 4; d++)
        {
            const auto neighbourLoc = neighbourTile(loc, d);
            auto* neighbour = findFactoryElement(neighbourLoc);
            if (neighbour == nullptr || !(fluidFacingMask(*neighbour) & (1 << oppositeOf(d))))
                continue;
            // A multi-tile machine connects only through the centre of each edge.
            const auto origin = footprintOrigin(*neighbour, neighbourLoc);
            const uint8_t size = footprintSize(getPrototype(*neighbour));
            if (footprintEdgeNeighbour(origin, size, oppositeOf(d)) == loc)
                mask |= static_cast<uint8_t>(1 << d);
        }
        return mask;
    }

    static void refreshPipeConnections(const CoordsXYZ& loc)
    {
        auto* element = findFactoryElement(loc, true);
        if (element == nullptr || element->getSubtype() != FactoryElementSubtype::pipe)
            return;
        const uint8_t mask = pipeConnectionMask(loc);
        if (element->getConnectionCache() != mask)
        {
            element->setConnectionCache(mask);
            MapInvalidateTileFull(loc);
        }
    }

    void refreshPipeConnectionsAround(const CoordsXYZ& loc)
    {
        refreshPipeConnections(loc);
        for (Direction d = 0; d < 4; d++)
            refreshPipeConnections(neighbourTile(loc, d));
    }

    bool hasWaterBehind(const CoordsXYZ& loc, Direction dir)
    {
        const auto behind = neighbourTile(loc, oppositeOf(dir));
        if (!MapIsLocationValid(behind))
            return false;
        auto* surface = MapGetSurfaceElementAt(behind);
        return surface != nullptr && surface->getWaterHeight() > surface->getBaseZ();
    }

    int32_t findFluidBox(const MachineProperties& props, FluidBoxRole role)
    {
        for (size_t i = 0; i < props.fluidBoxes.size(); i++)
        {
            if (props.fluidBoxes[i].role == role)
                return static_cast<int32_t>(i);
        }
        return -1;
    }

    FluidNetworkRecord* machineFluidNetwork(State& state, const MachineRecord& machine, size_t box)
    {
        if (box >= machine.fluidNetworks.size() || machine.fluidNetworks[box] == kNullRecord)
            return nullptr;
        return state.fluidNetworks.get(machine.fluidNetworks[box]);
    }

    namespace
    {
        struct FluidNode
        {
            TileCoordsXYZ tile;  // footprint origin (a pipe's own tile)
            uint8_t size{ 1 };   // footprint size; a node connects through its edge centres
            uint8_t sides{};     // world directions this node faces
            uint32_t capacity{}; // what it adds to its network
            RecordId oldNetwork{ kNullRecord };
            ObjectEntryIndex fluid{ kObjectEntryIndexNull };
            uint32_t content{}; // its share of the old network's fluid
            RecordId network{ kNullRecord };
            RecordId pipe{ kNullRecord }; // pipe record, or kNullRecord for a machine box
            RecordId machine{ kNullRecord };
            uint8_t box{};
        };
    } // namespace

    void rebuildFluidNetworks(State& state)
    {
        // Nodes in a fixed order: pipes by id, then machine boxes by machine id and box index.
        std::vector<FluidNode> nodes;
        std::vector<int32_t> pipeNode(state.pipes.slotCount(), -1);
        std::vector<int32_t> machineFirstBox(state.machines.slotCount(), -1);
        state.pipes.forEach([&](RecordId id, PipeRecord& pipe) {
            auto* proto = getPrototype(pipe.entry);
            FluidNode node;
            node.tile = pipe.location();
            node.sides = 0b1111;
            node.capacity = proto != nullptr ? proto->getPipe().capacity : 1000;
            node.oldNetwork = pipe.network;
            node.pipe = id;
            pipeNode[id] = static_cast<int32_t>(nodes.size());
            nodes.push_back(node);
        });
        state.machines.forEach([&](RecordId id, MachineRecord& machine) {
            auto* proto = getPrototype(machine.entry);
            if (proto == nullptr)
            {
                machine.fluidNetworks.clear();
                return;
            }
            const auto& boxes = proto->getMachine().fluidBoxes;
            machine.fluidNetworks.resize(boxes.size(), kNullRecord);
            if (boxes.empty())
                return;
            machineFirstBox[id] = static_cast<int32_t>(nodes.size());
            for (size_t b = 0; b < boxes.size(); b++)
            {
                FluidNode node;
                node.tile = machine.location();
                node.size = footprintSize(proto);
                node.sides = fluidBoxWorldSides(boxes[b], machine.direction);
                node.capacity = boxes[b].capacity;
                node.oldNetwork = machine.fluidNetworks[b];
                node.machine = id;
                node.box = static_cast<uint8_t>(b);
                nodes.push_back(node);
            }
        });

        // Share each old network's fluid over its surviving nodes by capacity; the remainder goes to the first.
        const size_t oldCount = state.fluidNetworks.slotCount();
        std::vector<uint64_t> oldCapacity(oldCount, 0);
        std::vector<int32_t> oldFirst(oldCount, -1);
        for (size_t i = 0; i < nodes.size(); i++)
        {
            const auto old = nodes[i].oldNetwork;
            if (old == kNullRecord || old >= oldCount || state.fluidNetworks.get(old) == nullptr)
            {
                nodes[i].oldNetwork = kNullRecord;
                continue;
            }
            oldCapacity[old] += nodes[i].capacity;
            if (oldFirst[old] < 0)
                oldFirst[old] = static_cast<int32_t>(i);
        }
        std::vector<uint32_t> oldAssigned(oldCount, 0);
        for (auto& node : nodes)
        {
            if (node.oldNetwork == kNullRecord)
                continue;
            const auto* old = state.fluidNetworks.get(node.oldNetwork);
            if (old == nullptr)
                continue;
            node.fluid = old->fluid;
            node.content = static_cast<uint32_t>(
                (static_cast<uint64_t>(old->amount) * node.capacity) / std::max<uint64_t>(1, oldCapacity[node.oldNetwork]));
            oldAssigned[node.oldNetwork] += node.content;
        }
        for (size_t old = 0; old < oldCount; old++)
        {
            const auto* network = state.fluidNetworks.get(static_cast<RecordId>(old));
            if (network != nullptr && oldFirst[old] >= 0 && network->amount > oldAssigned[old])
                nodes[oldFirst[old]].content += network->amount - oldAssigned[old];
        }

        // Nodes at a tile: the pipe there, or every box of the machine there.
        auto nodesAt = [&](const CoordsXYZ& loc, std::vector<int32_t>& out) {
            out.clear();
            auto* element = findFactoryElement(loc);
            if (element == nullptr || !element->hasRecord())
                return;
            const auto id = element->getRecordId();
            if (element->getSubtype() == FactoryElementSubtype::pipe)
            {
                if (id < pipeNode.size() && pipeNode[id] >= 0)
                    out.push_back(pipeNode[id]);
            }
            else if (element->getSubtype() == FactoryElementSubtype::machine)
            {
                if (id >= machineFirstBox.size() || machineFirstBox[id] < 0)
                    return;
                for (auto i = static_cast<size_t>(machineFirstBox[id]); i < nodes.size() && nodes[i].machine == id; i++)
                    out.push_back(static_cast<int32_t>(i));
            }
        };

        state.fluidNetworks.clear();
        std::vector<int32_t> stack;
        std::vector<int32_t> around;
        for (size_t start = 0; start < nodes.size(); start++)
        {
            if (nodes[start].network != kNullRecord)
                continue;
            RecordId networkId;
            auto& network = state.fluidNetworks.allocateRecord(networkId);
            std::vector<std::pair<ObjectEntryIndex, uint64_t>> fluids;
            uint64_t capacity = 0;
            uint16_t pipeCount = 0;
            uint16_t boxCount = 0;
            nodes[start].network = networkId;
            stack.assign(1, static_cast<int32_t>(start));
            while (!stack.empty())
            {
                auto& node = nodes[stack.back()];
                stack.pop_back();
                capacity += node.capacity;
                if (node.pipe != kNullRecord)
                    pipeCount++;
                else
                    boxCount++;
                if (node.content > 0 && node.fluid != kObjectEntryIndexNull)
                {
                    auto it = std::find_if(fluids.begin(), fluids.end(), [&](auto& f) { return f.first == node.fluid; });
                    if (it == fluids.end())
                        fluids.emplace_back(node.fluid, node.content);
                    else
                        it->second += node.content;
                }
                const auto origin = tileToCoords(node.tile);
                for (Direction d = 0; d < 4; d++)
                {
                    if (!(node.sides & (1 << d)))
                        continue;
                    // Through the edge centre on side d; the other node must face back from exactly that tile.
                    const auto beyond = footprintEdgeNeighbour(origin, node.size, d);
                    const auto edge = neighbourTile(beyond, oppositeOf(d));
                    nodesAt(beyond, around);
                    for (auto other : around)
                    {
                        auto& otherNode = nodes[other];
                        if (otherNode.network != kNullRecord || !(otherNode.sides & (1 << oppositeOf(d))))
                            continue;
                        if (footprintEdgeNeighbour(tileToCoords(otherNode.tile), otherNode.size, oppositeOf(d)) != edge)
                            continue;
                        otherNode.network = networkId;
                        stack.push_back(other);
                    }
                }
            }
            network.capacity = static_cast<uint32_t>(std::min<uint64_t>(capacity, 0xFFFFFFFFu));
            network.pipeCount = pipeCount;
            network.boxCount = boxCount;
            // The fluid with the most volume wins; ties go to the lower entry index.
            std::sort(fluids.begin(), fluids.end(), [](auto& a, auto& b) {
                return a.second != b.second ? a.second > b.second : a.first < b.first;
            });
            if (!fluids.empty())
            {
                network.fluid = fluids.front().first;
                network.amount = static_cast<uint32_t>(std::min<uint64_t>(fluids.front().second, network.capacity));
            }
        }

        for (auto& node : nodes)
        {
            if (node.pipe != kNullRecord)
            {
                if (auto* pipe = state.pipes.get(node.pipe))
                    pipe->network = node.network;
            }
            else if (auto* machine = state.machines.get(node.machine))
            {
                machine->fluidNetworks[node.box] = node.network;
            }
        }
        state.fluidDirty = false;
    }

    void updateFluidNetworks(State& state)
    {
        if (state.fluidDirty)
            rebuildFluidNetworks(state);
        state.fluidNetworks.forEach([&](RecordId, FluidNetworkRecord& network) {
            if (network.demand == 0 || network.amount >= network.demand)
                network.satisfactionQ16 = kSatisfactionFull;
            else
                network.satisfactionQ16 = static_cast<uint32_t>((static_cast<uint64_t>(network.amount) << 16) / network.demand);
            network.lastDemand = network.demand;
            network.demand = 0;
        });
    }

    // Registers a request and returns this tick's share of it, never more than the network holds.
    static uint32_t drawFluid(FluidNetworkRecord& network, uint32_t want)
    {
        network.demand += want;
        auto take = static_cast<uint32_t>((static_cast<uint64_t>(want) * network.satisfactionQ16) >> 16);
        take = std::min(take, network.amount);
        network.amount -= take;
        if (network.amount == 0)
            network.fluid = kObjectEntryIndexNull;
        return take;
    }

    static bool acceptsFluid(const FluidNetworkRecord& network, ObjectEntryIndex fluid)
    {
        return network.fluid == kObjectEntryIndexNull || network.fluid == fluid;
    }

    void updatePump(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        const auto& props = proto.getMachine();
        const auto outBox = findFluidBox(props, FluidBoxRole::output);
        auto* out = outBox >= 0 ? machineFluidNetwork(state, machine, static_cast<size_t>(outBox)) : nullptr;
        const auto fluid = props.outputFluid.resolve();
        if (out == nullptr || fluid == kObjectEntryIndexNull)
        {
            setMachineStatus(machine, MachineStatus::noRecipe);
            return;
        }
        if (!hasWaterBehind(tileToCoords(machine.location()), machine.direction))
        {
            setMachineStatus(machine, MachineStatus::noInput);
            return;
        }
        const uint32_t added = acceptsFluid(*out, fluid) ? std::min(props.fluidRate, out->space()) : 0;
        if (added == 0)
        {
            setMachineStatus(machine, MachineStatus::outputFull);
            return;
        }
        out->fluid = fluid;
        out->amount += added;
        setMachineStatus(machine, MachineStatus::working);
    }

    void updateBoiler(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        const auto& props = proto.getMachine();
        const auto inBox = findFluidBox(props, FluidBoxRole::input);
        const auto outBox = findFluidBox(props, FluidBoxRole::output);
        auto* in = inBox >= 0 ? machineFluidNetwork(state, machine, static_cast<size_t>(inBox)) : nullptr;
        auto* out = outBox >= 0 ? machineFluidNetwork(state, machine, static_cast<size_t>(outBox)) : nullptr;
        const auto inFluid = props.inputFluid.resolve();
        const auto outFluid = props.outputFluid.resolve();
        if (in == nullptr || out == nullptr || in == out || inFluid == kObjectEntryIndexNull
            || outFluid == kObjectEntryIndexNull)
        {
            setMachineStatus(machine, MachineStatus::noRecipe);
            return;
        }
        const uint32_t want = acceptsFluid(*out, outFluid) ? std::min(props.fluidRate, out->space()) : 0;
        if (want == 0)
        {
            setMachineStatus(machine, MachineStatus::outputFull);
            return;
        }
        if (!machineHasFuel(machine, props))
        {
            setMachineStatus(machine, MachineStatus::noFuel);
            return;
        }
        if (in->fluid != inFluid)
        {
            setMachineStatus(machine, MachineStatus::noInput);
            return;
        }
        const uint32_t take = drawFluid(*in, want);
        if (take == 0)
        {
            setMachineStatus(machine, MachineStatus::noInput);
            return;
        }
        machineBurnFuel(state, machine, props);
        out->fluid = outFluid;
        out->amount += take;
        setMachineStatus(machine, MachineStatus::working);
    }

    void updateSteamEngine(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        const auto& props = proto.getMachine();
        auto* power = machine.powerNetwork != kNullRecord ? state.powerNetworks.get(machine.powerNetwork) : nullptr;
        if (power == nullptr)
        {
            setMachineStatus(machine, MachineStatus::idle);
            return;
        }
        const auto inBox = findFluidBox(props, FluidBoxRole::input);
        auto* in = inBox >= 0 ? machineFluidNetwork(state, machine, static_cast<size_t>(inBox)) : nullptr;
        const auto fuel = props.inputFluid.resolve();
        const uint32_t rate = std::max<uint32_t>(1, props.fluidRate);
        const uint32_t available = in != nullptr && in->fluid == fuel ? in->amount : 0;
        // Offer what the fluid on hand can back for a full tick.
        const auto offered = static_cast<uint32_t>(
            (static_cast<uint64_t>(props.powerOutput) * std::min(rate, available)) / rate);
        if (offered == 0)
        {
            setMachineStatus(machine, MachineStatus::noInput);
            return;
        }
        power->supply += offered;
        if (power->consumerCount == 0 || power->lastDemand == 0)
        {
            setMachineStatus(machine, MachineStatus::idle);
            return;
        }
        // Burn fluid in proportion to how loaded the network was last tick (rounded up so a load always costs).
        const uint32_t loadQ16 = power->lastSupply == 0
            ? kSatisfactionFull
            : static_cast<uint32_t>(
                  std::min<uint64_t>(kSatisfactionFull, (static_cast<uint64_t>(power->lastDemand) << 16) / power->lastSupply));
        const auto want = static_cast<uint32_t>((static_cast<uint64_t>(rate) * loadQ16 + 0xFFFF) >> 16);
        drawFluid(*in, want);
        setMachineStatus(machine, MachineStatus::working);
    }
} // namespace OpenRCT2::Factory

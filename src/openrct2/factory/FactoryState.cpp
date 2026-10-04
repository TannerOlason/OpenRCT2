/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryState.h"

#include "../Context.h"
#include "../GameState.h"
#include "../object/ObjectList.h"
#include "../object/ObjectManager.h"
#include "../profiling/Profiling.h"
#include "../world/tile_element/FactoryElement.h"
#include "Belts.h"
#include "FactoryPrototypeObject.h"
#include "FactoryTopology.h"
#include "Fluids.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <vector>

namespace OpenRCT2::Factory
{
    void State::reset()
    {
        containers.clear();
        inserters.clear();
        beltSegments.clear();
        machines.clear();
        splitters.clear();
        poles.clear();
        powerNetworks.clear();
        pipes.clear();
        fluidNetworks.clear();
        rideProximity.clear();
        pollution.clear();
        ore.clear();
        parkExt.reset();
        powerDirty = false;
        fluidDirty = false;
        topologyVersion = 0;
    }

    bool State::isEmpty() const
    {
        return recordCount() == 0 && topologyVersion == 0 && ore.isEmpty() && parkExt.isEmpty();
    }

    size_t State::recordCount() const
    {
        return containers.aliveCount() + inserters.aliveCount() + beltSegments.aliveCount() + machines.aliveCount()
            + splitters.aliveCount() + poles.aliveCount() + pipes.aliveCount();
    }

    static void updateBelts(State& state)
    {
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& segment) {
            LaneTarget targets[kBeltLaneCount];
            switch (segment.getNextKind())
            {
                case BeltLinkKind::segment:
                {
                    auto* next = state.beltSegments.get(segment.next);
                    if (next != nullptr)
                    {
                        const int32_t nextLength = segmentLength(*next);
                        for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
                            targets[lane] = LaneTarget{ &next->lanes[lane], nextLength, -1 };
                    }
                    break;
                }
                case BeltLinkKind::sideload:
                {
                    auto* next = state.beltSegments.get(segment.next);
                    if (next != nullptr)
                    {
                        // Both of our lanes merge onto the near lane of the target at the entry point.
                        const uint8_t lane = segment.nextLane < kBeltLaneCount ? segment.nextLane : 0;
                        for (uint8_t ourLane = 0; ourLane < kBeltLaneCount; ourLane++)
                            targets[ourLane] = LaneTarget{ &next->lanes[lane], segmentLength(*next), segment.nextPos };
                    }
                    break;
                }
                default:
                    break; // dead end or splitter input: items park at the end and the splitter takes them
            }
            tickSegment(segment, targets);
        });
    }

    static void resolveSplitterOutputs(State& state, RecordId splitterId, SplitterRecord& splitter)
    {
        if (splitter.topologyVersionSeen == state.topologyVersion)
            return;
        splitter.topologyVersionSeen = state.topologyVersion;
        const auto origin = tileToCoords(splitter.location());
        for (uint8_t side = 0; side < 2; side++)
        {
            splitter.outputs[side] = kNullRecord;
            const auto sideLoc = side == 0 ? origin : neighbourTile(origin, rightOf(splitter.direction));
            auto* ahead = findFactoryElement(neighbourTile(sideLoc, splitter.direction));
            if (ahead == nullptr || !ahead->hasRecord() || ahead->getFootprintIndex() != 0
                || ahead->getDirection() != splitter.direction)
                continue;
            if (ahead->getSubtype() == FactoryElementSubtype::belt
                || (ahead->getSubtype() == FactoryElementSubtype::undergroundBelt && !isUndergroundExit(*ahead)))
            {
                splitter.outputs[side] = ahead->getRecordId();
            }
        }
        (void)splitterId;
    }

    // The output sides an item may take, in the order to try them; returns how many (0-2).
    static uint8_t splitterSidesFor(const SplitterRecord& splitter, ObjectEntryIndex item, uint8_t sides[2])
    {
        const uint8_t preferred = splitter.outputPriority == kSplitterPriorityRight ? 1 : 0;
        if (splitter.filter != kObjectEntryIndexNull)
        {
            sides[0] = item == splitter.filter ? preferred : static_cast<uint8_t>(preferred ^ 1);
            return 1;
        }
        if (splitter.outputPriority != kSplitterPriorityNone)
        {
            sides[0] = preferred;
            sides[1] = static_cast<uint8_t>(preferred ^ 1);
            return 2;
        }
        sides[0] = splitter.nextOutput & 1;
        sides[1] = static_cast<uint8_t>(sides[0] ^ 1);
        return 2;
    }

    static void splitterMoveFrom(State& state, SplitterRecord& splitter, BeltSegmentRecord& input)
    {
        for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
        {
            for (int guard = 0; guard < 4; guard++)
            {
                auto& inLane = input.lanes[lane];
                if (inLane.items.empty() || inLane.items.front().gap != 0)
                    break;
                uint8_t sides[2];
                const uint8_t sideCount = splitterSidesFor(splitter, inLane.items.front().item, sides);
                bool moved = false;
                for (uint8_t attempt = 0; attempt < sideCount && !moved; attempt++)
                {
                    const uint8_t side = sides[attempt];
                    auto* out = splitter.outputs[side] != kNullRecord ? state.beltSegments.get(splitter.outputs[side])
                                                                      : nullptr;
                    if (out == nullptr)
                        continue;
                    const int32_t outLength = segmentLength(*out);
                    if (laneRearPosition(out->lanes[lane], outLength) < kBeltItemSpacing)
                        continue;
                    auto item = laneTakeFrontAtEnd(inLane);
                    if (!item.has_value())
                        break;
                    laneInsertAt(out->lanes[lane], outLength, 0, *item);
                    splitter.nextOutput = (side + 1) & 1;
                    moved = true;
                }
                if (!moved)
                    break;
            }
        }
    }

    static void updateSplitters(State& state)
    {
        state.splitters.forEach([&](RecordId splitterId, SplitterRecord& splitter) {
            resolveSplitterOutputs(state, splitterId, splitter);
            // Inputs: segments linked to this splitter (nextLane is the input side). Items parked at their ends go
            // to the outputs; the priority input, if any, is served first.
            const int32_t firstSide = splitter.inputPriority == kSplitterPriorityLeft ? 0
                : splitter.inputPriority == kSplitterPriorityRight                    ? 1
                                                                                      : -1;
            for (int32_t pass = 0; pass < (firstSide < 0 ? 1 : 2); pass++)
            {
                state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& input) {
                    if (input.getNextKind() != BeltLinkKind::splitter || input.next != splitterId)
                        return;
                    if (firstSide >= 0 && (input.nextLane == firstSide) != (pass == 0))
                        return;
                    splitterMoveFrom(state, splitter, input);
                });
            }
        });
    }

    static RecordRef resolveNeighbour(const InserterRecord& inserter, Direction d)
    {
        auto loc = neighbourTile(tileToCoords(inserter.location()), d);
        auto* element = findFactoryElement(loc);
        if (element == nullptr || !element->hasRecord())
            return RecordRef{};
        auto kind = element->getSubtype();
        if (kind == FactoryElementSubtype::undergroundBelt)
            kind = FactoryElementSubtype::belt; // the pair is a belt segment; tile index selects the end
        if (kind == FactoryElementSubtype::splitter)
            return RecordRef{};
        return RecordRef{ static_cast<uint8_t>(kind), element->getRecordId(), element->getFootprintIndex() };
    }

    static void resolveInserterRefs(State& state, InserterRecord& inserter)
    {
        if (inserter.topologyVersionSeen == state.topologyVersion)
            return;
        inserter.source = resolveNeighbour(inserter, oppositeOf(inserter.direction));
        inserter.target = resolveNeighbour(inserter, inserter.direction);
        inserter.topologyVersionSeen = state.topologyVersion;
    }

    bool containerTakeAny(ContainerRecord& container, ItemStack& hand)
    {
        for (auto& slot : container.slots)
        {
            if (!slot.isEmpty())
            {
                hand.item = slot.item;
                hand.count = 1;
                slot.count--;
                if (slot.count == 0)
                    slot.item = kObjectEntryIndexNull;
                return true;
            }
        }
        return false;
    }

    bool containerInsert(ContainerRecord& container, ObjectEntryIndex item, uint16_t stackSize)
    {
        for (auto& slot : container.slots)
        {
            if (slot.item == item && slot.count < stackSize)
            {
                slot.count++;
                return true;
            }
        }
        for (auto& slot : container.slots)
        {
            if (slot.isEmpty())
            {
                slot.item = item;
                slot.count = 1;
                return true;
            }
        }
        return false;
    }

    static int32_t beltTileStart(const BeltSegmentRecord& segment, const RecordRef& ref)
    {
        return segmentTileStart(segment, ref.aux);
    }

    static uint16_t stackSizeOf(ObjectEntryIndex item)
    {
        auto* proto = getPrototype(item);
        return proto != nullptr ? proto->getItem().stackSize : 1;
    }

    static bool stackAdd(std::vector<ItemStack>& slots, ObjectEntryIndex item, uint16_t count, uint16_t stackSize)
    {
        for (auto& slot : slots)
        {
            if (slot.item == item && slot.count + count <= stackSize)
            {
                slot.count = static_cast<uint16_t>(slot.count + count);
                return true;
            }
        }
        for (auto& slot : slots)
        {
            if (slot.isEmpty() && count <= stackSize)
            {
                slot.item = item;
                slot.count = count;
                return true;
            }
        }
        return false;
    }

    static bool stackCanAdd(const std::vector<ItemStack>& slots, ObjectEntryIndex item, uint16_t count, uint16_t stackSize)
    {
        for (const auto& slot : slots)
        {
            if ((slot.item == item && slot.count + count <= stackSize) || (slot.isEmpty() && count <= stackSize))
                return true;
        }
        return false;
    }

    static uint16_t stackCount(const std::vector<ItemStack>& slots, ObjectEntryIndex item)
    {
        uint32_t total = 0;
        for (const auto& slot : slots)
            if (slot.item == item)
                total += slot.count;
        return static_cast<uint16_t>(std::min<uint32_t>(total, 0xFFFF));
    }

    static bool stackRemove(std::vector<ItemStack>& slots, ObjectEntryIndex item, uint16_t count)
    {
        if (stackCount(slots, item) < count)
            return false;
        for (auto& slot : slots)
        {
            if (slot.item != item || count == 0)
                continue;
            const uint16_t taken = std::min(slot.count, count);
            slot.count = static_cast<uint16_t>(slot.count - taken);
            count = static_cast<uint16_t>(count - taken);
            if (slot.count == 0)
                slot.item = kObjectEntryIndexNull;
        }
        return true;
    }

    // Recipes a machine could run on `item`: the first loaded recipe of a handled category listing it as an
    // ingredient. Furnaces pick their recipe this way; assemblers only accept ingredients of their set recipe.
    static ObjectEntryIndex findRecipeForInput(const FactoryPrototypeObject& machineProto, ObjectEntryIndex item)
    {
        auto& objectManager = GetContext()->GetObjectManager();
        const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
        for (size_t i = 0; i < count; i++)
        {
            auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
            if (proto == nullptr || proto->getKind() != PrototypeKind::recipe)
                continue;
            const auto& recipe = proto->getRecipe();
            if (!machineProto.machineHandlesCategory(recipe.category))
                continue;
            for (const auto& ingredient : recipe.ingredients)
            {
                if (ingredient.item.resolve() == item)
                    return static_cast<ObjectEntryIndex>(i);
            }
        }
        return kObjectEntryIndexNull;
    }

    static bool isFuel(ObjectEntryIndex item)
    {
        auto* proto = getPrototype(item);
        return proto != nullptr && proto->getKind() == PrototypeKind::item && proto->getItem().fuelTicks > 0;
    }

    bool machineAcceptsInput(const State& state, const MachineRecord& machine, ObjectEntryIndex item)
    {
        auto* machineProto = getPrototype(machine.entry);
        if (machineProto == nullptr || item == kObjectEntryIndexNull)
            return false;
        const auto& props = machineProto->getMachine();
        if (props.energy == EnergySource::burner && isFuel(item)
            && (machine.fuel.isEmpty() || (machine.fuel.item == item && machine.fuel.count < stackSizeOf(item))))
            return true;
        switch (machine.getKind())
        {
            case MachineKind::drill:
                return false;
            case MachineKind::furnace:
            {
                if (machine.inputs.empty())
                    return false;
                // One input kind at a time.
                for (const auto& slot : machine.inputs)
                    if (!slot.isEmpty() && slot.item != item)
                        return false;
                if (findRecipeForInput(*machineProto, item) == kObjectEntryIndexNull)
                    return false;
                return stackCanAdd(machine.inputs, item, 1, std::min<uint16_t>(stackSizeOf(item), 50));
            }
            default:
            {
                auto* recipe = getPrototype(machine.recipe);
                if (recipe == nullptr || recipe->getKind() != PrototypeKind::recipe)
                    return false;
                for (const auto& ingredient : recipe->getRecipe().ingredients)
                {
                    if (ingredient.item.resolve() == item)
                    {
                        // Keep at most two crafts' worth buffered.
                        const uint16_t limit = static_cast<uint16_t>(std::max<uint32_t>(ingredient.count * 2u, 1u));
                        return stackCount(machine.inputs, item) < limit
                            && stackCanAdd(machine.inputs, item, 1, std::max<uint16_t>(limit, 1));
                    }
                }
                return false;
            }
        }
    }

    bool machineInsertInput(State& state, MachineRecord& machine, ObjectEntryIndex item)
    {
        if (!machineAcceptsInput(state, machine, item))
            return false;
        auto* machineProto = getPrototype(machine.entry);
        if (machineProto->getMachine().energy == EnergySource::burner && isFuel(item))
        {
            // Fuel goes to the fuel slot unless the machine's recipe also wants it as an ingredient.
            bool wantsAsIngredient = false;
            if (machine.getKind() != MachineKind::furnace && machine.getKind() != MachineKind::drill)
            {
                auto* recipe = getPrototype(machine.recipe);
                if (recipe != nullptr)
                    for (const auto& ingredient : recipe->getRecipe().ingredients)
                        if (ingredient.item.resolve() == item)
                            wantsAsIngredient = true;
            }
            if (!wantsAsIngredient || machine.fuel.isEmpty())
            {
                if (machine.fuel.isEmpty())
                {
                    machine.fuel = { item, 1 };
                    return true;
                }
                if (machine.fuel.item == item && machine.fuel.count < stackSizeOf(item))
                {
                    machine.fuel.count++;
                    return true;
                }
                if (!wantsAsIngredient)
                    return false;
            }
        }
        const uint16_t limit = machine.getKind() == MachineKind::furnace ? std::min<uint16_t>(stackSizeOf(item), 50)
                                                                         : stackSizeOf(item);
        return stackAdd(machine.inputs, item, 1, limit);
    }

    bool machineTakeOutput(MachineRecord& machine, ItemStack& hand)
    {
        for (auto& slot : machine.outputs)
        {
            if (!slot.isEmpty())
            {
                hand.item = slot.item;
                hand.count = 1;
                slot.count--;
                if (slot.count == 0)
                    slot.item = kObjectEntryIndexNull;
                return true;
            }
        }
        return false;
    }

    static bool inserterPickUp(State& state, InserterRecord& inserter)
    {
        auto& ref = inserter.source;
        switch (ref.getKind())
        {
            case FactoryElementSubtype::container:
            {
                auto* container = state.containers.get(ref.id);
                return container != nullptr && containerTakeAny(*container, inserter.hand);
            }
            case FactoryElementSubtype::belt:
            {
                auto* segment = state.beltSegments.get(ref.id);
                if (segment == nullptr)
                    return false;
                const int32_t length = segmentLength(*segment);
                const int32_t from = beltTileStart(*segment, ref);
                const int32_t to = from + kBeltUnitsPerTile - 1;
                for (auto& lane : segment->lanes)
                {
                    auto item = laneTakeInRange(lane, length, from, to);
                    if (item.has_value())
                    {
                        inserter.hand.item = *item;
                        inserter.hand.count = 1;
                        return true;
                    }
                }
                return false;
            }
            case FactoryElementSubtype::machine:
            {
                auto* machine = state.machines.get(ref.id);
                return machine != nullptr && machineTakeOutput(*machine, inserter.hand);
            }
            default:
                return false;
        }
    }

    // Items go on the lane farthest from the inserter; when the belt runs along the drop direction, the
    // right lane.
    static uint8_t dropLaneFor(Direction dropDirection, Direction beltDirection)
    {
        if (dropDirection == leftOf(beltDirection))
            return kLaneLeft;
        return kLaneRight;
    }

    static bool inserterDrop(State& state, InserterRecord& inserter)
    {
        auto& ref = inserter.target;
        switch (ref.getKind())
        {
            case FactoryElementSubtype::container:
            {
                auto* container = state.containers.get(ref.id);
                if (container == nullptr)
                    return false;
                auto* itemProto = getPrototype(inserter.hand.item);
                const uint16_t stackSize = itemProto != nullptr ? itemProto->getItem().stackSize : 1;
                return containerInsert(*container, inserter.hand.item, stackSize);
            }
            case FactoryElementSubtype::belt:
            {
                auto* segment = state.beltSegments.get(ref.id);
                if (segment == nullptr || ref.aux >= segment->tiles.size())
                    return false;
                const int32_t length = segmentLength(*segment);
                const int32_t pos = beltTileStart(*segment, ref) + kBeltUnitsPerTile / 2;
                auto* beltElement = findBeltElement(tileToCoords(segment->tiles[ref.aux]));
                const Direction beltDirection = beltElement != nullptr ? beltElement->getDirection() : inserter.direction;
                const uint8_t lane = dropLaneFor(inserter.direction, beltDirection);
                return laneInsertAt(segment->lanes[lane], length, pos, inserter.hand.item);
            }
            case FactoryElementSubtype::machine:
            {
                auto* machine = state.machines.get(ref.id);
                return machine != nullptr && machineInsertInput(state, *machine, inserter.hand.item);
            }
            default:
                return false;
        }
    }

    static void updateInserter(State& state, InserterRecord& inserter)
    {
        resolveInserterRefs(state, inserter);
        auto* proto = getPrototype(inserter.entry);
        const uint16_t swingTicks = proto != nullptr ? proto->getInserter().swingTicks : 24;

        switch (inserter.phase)
        {
            case kInserterPhaseWaitingForItem:
                if (!inserter.source.isNull() && inserterPickUp(state, inserter))
                {
                    inserter.phase = kInserterPhaseSwingingToDrop;
                    inserter.progress = 0;
                }
                break;
            case kInserterPhaseSwingingToDrop:
                if (++inserter.progress >= swingTicks)
                {
                    inserter.phase = kInserterPhaseWaitingToDrop;
                    inserter.progress = swingTicks;
                }
                break;
            case kInserterPhaseWaitingToDrop:
                if (!inserter.target.isNull() && inserterDrop(state, inserter))
                {
                    inserter.hand = ItemStack{};
                    inserter.phase = kInserterPhaseReturning;
                    inserter.progress = 0;
                }
                break;
            case kInserterPhaseReturning:
                if (++inserter.progress >= swingTicks)
                {
                    inserter.phase = kInserterPhaseWaitingForItem;
                    inserter.progress = 0;
                }
                break;
            default:
                inserter.phase = kInserterPhaseWaitingForItem;
                inserter.progress = 0;
                break;
        }
    }

    static void updateInserters(State& state)
    {
        state.inserters.forEach([&](RecordId, InserterRecord& inserter) { updateInserter(state, inserter); });
    }

    void setMachineStatus(MachineRecord& machine, MachineStatus status)
    {
        machine.status = static_cast<uint8_t>(status);
    }

    bool machineHasFuel(const MachineRecord& machine, const MachineProperties& props)
    {
        return props.energy != EnergySource::burner || machine.fuelEnergy > 0 || !machine.fuel.isEmpty();
    }

    // Burner machines consume one tick of fuel per working tick; returns false (and sets noFuel) when empty.
    bool machineBurnFuel(MachineRecord& machine, const MachineProperties& props)
    {
        if (props.energy != EnergySource::burner)
            return true;
        if (machine.fuelEnergy == 0)
        {
            if (machine.fuel.isEmpty())
            {
                setMachineStatus(machine, MachineStatus::noFuel);
                return false;
            }
            auto* fuelProto = getPrototype(machine.fuel.item);
            machine.fuelEnergy = fuelProto != nullptr ? fuelProto->getItem().fuelTicks : 0;
            machine.fuel.count--;
            if (machine.fuel.count == 0)
                machine.fuel.item = kObjectEntryIndexNull;
            if (machine.fuelEnergy == 0)
            {
                setMachineStatus(machine, MachineStatus::noFuel);
                return false;
            }
        }
        machine.fuelEnergy--;
        return true;
    }

    /**
     * This tick's energy: electric machines register their demand and get their network's satisfaction (Q16), burner
     * machines burn a tick of fuel. Returns false, with the status set, when the machine cannot work this tick.
     */
    static bool takeEnergy(State& state, MachineRecord& machine, const MachineProperties& props, uint32_t& satisfactionQ16)
    {
        satisfactionQ16 = kSatisfactionFull;
        if (props.energy == EnergySource::electric)
        {
            auto* network = machine.powerNetwork != kNullRecord ? state.powerNetworks.get(machine.powerNetwork) : nullptr;
            if (network == nullptr)
            {
                setMachineStatus(machine, MachineStatus::noPower);
                return false;
            }
            network->demand += props.powerUsage;
            satisfactionQ16 = network->satisfactionQ16;
            if (satisfactionQ16 == 0)
            {
                setMachineStatus(machine, MachineStatus::noPower);
                return false;
            }
        }
        return machineBurnFuel(machine, props);
    }

    // Tries to push one output item onto whatever is on the tile ahead of the front edge's centre (belt, container
    // or machine).
    static void drillDropAhead(State& state, MachineRecord& machine)
    {
        const auto loc = footprintEdgeNeighbour(
            tileToCoords(machine.location()), footprintSize(getPrototype(machine.entry)), machine.direction);
        auto* element = findFactoryElement(loc);
        if (element == nullptr || !element->hasRecord())
            return;
        for (auto& slot : machine.outputs)
        {
            if (slot.isEmpty())
                continue;
            bool moved = false;
            switch (element->getSubtype())
            {
                case FactoryElementSubtype::belt:
                {
                    auto* segment = state.beltSegments.get(element->getRecordId());
                    if (segment == nullptr)
                        break;
                    const int32_t pos = segmentTileStart(*segment, element->getFootprintIndex()) + kBeltUnitsPerTile / 2;
                    const uint8_t lane = dropLaneFor(machine.direction, element->getDirection());
                    moved = laneInsertAt(segment->lanes[lane], segmentLength(*segment), pos, slot.item);
                    break;
                }
                case FactoryElementSubtype::container:
                {
                    auto* container = state.containers.get(element->getRecordId());
                    moved = container != nullptr && containerInsert(*container, slot.item, stackSizeOf(slot.item));
                    break;
                }
                case FactoryElementSubtype::machine:
                {
                    auto* target = state.machines.get(element->getRecordId());
                    moved = target != nullptr && target != &machine && machineInsertInput(state, *target, slot.item);
                    break;
                }
                default:
                    break;
            }
            if (moved)
            {
                slot.count--;
                if (slot.count == 0)
                    slot.item = kObjectEntryIndexNull;
            }
            return;
        }
    }

    static void updateDrill(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        const auto& props = proto.getMachine();
        drillDropAhead(state, machine);

        // Find the next cell with ore in the mining area (a square of radius miningRadius around the centre).
        const int32_t radius = props.miningRadius;
        const int32_t side = radius * 2 + 1;
        const int32_t cells = side * side;
        const int32_t half = (props.size - 1) / 2;
        const auto origin = TileCoordsXY(machine.x + half - radius, machine.y + half - radius);
        OreCell cell{};
        TileCoordsXY cellTile;
        bool found = false;
        for (int32_t i = 0; i < cells; i++)
        {
            const int32_t index = (machine.miningCursor + i) % cells;
            cellTile = TileCoordsXY(origin.x + index % side, origin.y + index / side);
            cell = state.ore.get(cellTile);
            if (!cell.isEmpty())
            {
                machine.miningCursor = static_cast<uint16_t>(index);
                found = true;
                break;
            }
        }
        if (!found)
        {
            setMachineStatus(machine, MachineStatus::noOre);
            machine.progress = 0;
            return;
        }
        auto* oreProto = getPrototype(cell.ore);
        const ObjectEntryIndex product = oreProto != nullptr ? oreProto->getOre().item.resolve() : kObjectEntryIndexNull;
        if (product == kObjectEntryIndexNull)
        {
            setMachineStatus(machine, MachineStatus::noRecipe);
            return;
        }
        if (!stackCanAdd(machine.outputs, product, 1, stackSizeOf(product)))
        {
            setMachineStatus(machine, MachineStatus::outputFull);
            return;
        }
        uint32_t satisfactionQ16;
        if (!takeEnergy(state, machine, props, satisfactionQ16))
            return;

        setMachineStatus(machine, MachineStatus::working);
        machine.craftCost = static_cast<uint32_t>(props.miningTimeTicks) * kWorkUnitsPerTick;
        machine.progress += static_cast<uint32_t>((static_cast<uint64_t>(props.speedQ8) * satisfactionQ16) >> 16);
        if (machine.progress >= machine.craftCost)
        {
            machine.progress -= machine.craftCost;
            if (state.ore.take(cellTile, 1) == 1)
            {
                stackAdd(machine.outputs, product, 1, stackSizeOf(product));
                // Move the cursor on so neighbouring cells deplete evenly.
                machine.miningCursor = static_cast<uint16_t>((machine.miningCursor + 1) % cells);
            }
        }
    }

    static bool startCraft(MachineRecord& machine, const FactoryPrototypeObject& recipeProto)
    {
        const auto& recipe = recipeProto.getRecipe();
        for (const auto& ingredient : recipe.ingredients)
        {
            if (stackCount(machine.inputs, ingredient.item.resolve()) < ingredient.count)
                return false;
        }
        for (const auto& result : recipe.results)
        {
            const auto item = result.item.resolve();
            if (item == kObjectEntryIndexNull || !stackCanAdd(machine.outputs, item, result.count, stackSizeOf(item)))
            {
                setMachineStatus(machine, MachineStatus::outputFull);
                return false;
            }
        }
        for (const auto& ingredient : recipe.ingredients)
            stackRemove(machine.inputs, ingredient.item.resolve(), ingredient.count);
        machine.craftCost = static_cast<uint32_t>(recipe.timeTicks) * kWorkUnitsPerTick;
        machine.progress = 0;
        return true;
    }

    static void finishCraft(MachineRecord& machine, const FactoryPrototypeObject& recipeProto)
    {
        for (const auto& result : recipeProto.getRecipe().results)
        {
            const auto item = result.item.resolve();
            stackAdd(machine.outputs, item, result.count, stackSizeOf(item));
        }
        machine.craftCost = 0;
        machine.progress = 0;
    }

    static void updateCrafter(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        const auto& props = proto.getMachine();

        if (machine.getKind() == MachineKind::furnace)
        {
            // Auto recipe from the input item.
            ObjectEntryIndex input = kObjectEntryIndexNull;
            for (const auto& slot : machine.inputs)
                if (!slot.isEmpty())
                    input = slot.item;
            if (machine.craftCost == 0)
                machine.recipe = input != kObjectEntryIndexNull ? findRecipeForInput(proto, input) : kObjectEntryIndexNull;
        }

        auto* recipeProto = getPrototype(machine.recipe);
        if (recipeProto == nullptr || recipeProto->getKind() != PrototypeKind::recipe)
        {
            setMachineStatus(
                machine, machine.getKind() == MachineKind::furnace ? MachineStatus::noInput : MachineStatus::noRecipe);
            return;
        }

        if (machine.craftCost == 0 && !startCraft(machine, *recipeProto))
        {
            if (machine.getStatus() != MachineStatus::outputFull)
                setMachineStatus(machine, MachineStatus::noInput);
            return;
        }

        uint32_t satisfactionQ16;
        if (!takeEnergy(state, machine, props, satisfactionQ16))
            return;

        setMachineStatus(machine, MachineStatus::working);
        machine.progress += static_cast<uint32_t>((static_cast<uint64_t>(props.speedQ8) * satisfactionQ16) >> 16);
        if (machine.progress >= machine.craftCost)
        {
            finishCraft(machine, *recipeProto);
        }
    }

    static int32_t tileDistance(const RecordBase& a, const RecordBase& b)
    {
        return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
    }

    namespace
    {
        /**
         * Poles bucketed by 16x16 tile cells over their bounding box, so neighbour queries touch only nearby
         * poles. Buckets hold ids in ascending order (filled while iterating the pool), keeping every query
         * deterministic.
         */
        class PoleGrid
        {
        public:
            static constexpr int32_t kCell = 16;

            explicit PoleGrid(State& state)
            {
                bool any = false;
                state.poles.forEach([&](RecordId, PoleRecord& pole) {
                    _minX = any ? std::min(_minX, pole.x) : pole.x;
                    _minY = any ? std::min(_minY, pole.y) : pole.y;
                    _maxX = any ? std::max(_maxX, pole.x) : pole.x;
                    _maxY = any ? std::max(_maxY, pole.y) : pole.y;
                    any = true;
                });
                if (!any)
                    return;
                _cols = (_maxX - _minX) / kCell + 1;
                _rows = (_maxY - _minY) / kCell + 1;
                _buckets.resize(static_cast<size_t>(_cols) * static_cast<size_t>(_rows));
                state.poles.forEach([&](RecordId id, PoleRecord& pole) { _buckets[index(pole.x, pole.y)].push_back(id); });
            }

            // Calls f(id) for every pole within `radius` tiles (Chebyshev) of (x, y), bucket by bucket.
            template<typename F>
            void forEachNear(int32_t x, int32_t y, int32_t radius, F f) const
            {
                if (_buckets.empty())
                    return;
                const int32_t c0 = std::max(0, (x - radius - _minX) / kCell);
                const int32_t c1 = std::min(_cols - 1, (x + radius - _minX) / kCell);
                const int32_t r0 = std::max(0, (y - radius - _minY) / kCell);
                const int32_t r1 = std::min(_rows - 1, (y + radius - _minY) / kCell);
                if (x + radius < _minX || y + radius < _minY)
                    return;
                for (int32_t r = r0; r <= r1; r++)
                    for (int32_t c = c0; c <= c1; c++)
                        for (auto id : _buckets[static_cast<size_t>(r) * _cols + c])
                            f(id);
            }

        private:
            size_t index(int32_t x, int32_t y) const
            {
                return static_cast<size_t>((y - _minY) / kCell) * _cols + static_cast<size_t>((x - _minX) / kCell);
            }

            int32_t _minX{}, _minY{}, _maxX{}, _maxY{};
            int32_t _cols{}, _rows{};
            std::vector<std::vector<RecordId>> _buckets;
        };
    } // namespace

    void rebuildPowerNetworks(State& state)
    {
        state.powerNetworks.clear();
        std::vector<RecordId> poleIds;
        int32_t maxReach = 0;
        int32_t maxRadius = 0;
        state.poles.forEach([&](RecordId id, PoleRecord& pole) {
            pole.network = kNullRecord;
            poleIds.push_back(id);
            if (auto* proto = getPrototype(pole.entry))
            {
                maxReach = std::max<int32_t>(maxReach, proto->getPole().wireReach);
                maxRadius = std::max<int32_t>(maxRadius, proto->getPole().supplyRadius);
            }
        });
        const PoleGrid grid(state);

        // Connected components over "within wire reach of each other" (the longer reach wins).
        for (auto startId : poleIds)
        {
            auto* start = state.poles.get(startId);
            if (start == nullptr || start->network != kNullRecord)
                continue;
            RecordId networkId;
            auto& network = state.powerNetworks.allocateRecord(networkId);
            std::vector<RecordId> stack{ startId };
            start->network = networkId;
            while (!stack.empty())
            {
                auto currentId = stack.back();
                stack.pop_back();
                auto* current = state.poles.get(currentId);
                if (current == nullptr)
                    continue;
                auto* currentProto = getPrototype(current->entry);
                const int32_t currentReach = currentProto != nullptr ? currentProto->getPole().wireReach : 0;
                network.poleCount++;
                grid.forEachNear(current->x, current->y, maxReach, [&](RecordId otherId) {
                    auto* other = state.poles.get(otherId);
                    if (other == nullptr || other->network != kNullRecord || other->z != current->z)
                        return;
                    auto* otherProto = getPrototype(other->entry);
                    const int32_t otherReach = otherProto != nullptr ? otherProto->getPole().wireReach : 0;
                    if (tileDistance(*current, *other) <= std::max(currentReach, otherReach))
                    {
                        other->network = networkId;
                        stack.push_back(otherId);
                    }
                });
            }
        }

        // Attach machines to the first pole (lowest id) whose supply area covers them.
        state.machines.forEach([&](RecordId, MachineRecord& machine) {
            machine.powerNetwork = kNullRecord;
            auto* proto = getPrototype(machine.entry);
            if (proto == nullptr)
                return;
            const auto& props = proto->getMachine();
            const bool isGenerator = proto->isGenerator();
            if (!isGenerator && props.energy != EnergySource::electric)
                return;
            RecordId best = kNullRecord;
            RecordId bestNetwork = kNullRecord;
            const uint8_t size = std::max<uint8_t>(1, props.size);
            grid.forEachNear(machine.x + (size - 1) / 2, machine.y + (size - 1) / 2, maxRadius + size, [&](RecordId poleId) {
                if (best != kNullRecord && poleId > best)
                    return;
                auto* pole = state.poles.get(poleId);
                if (pole == nullptr || pole->network == kNullRecord || pole->z != machine.z)
                    return;
                auto* poleProto = getPrototype(pole->entry);
                const int32_t radius = poleProto != nullptr ? poleProto->getPole().supplyRadius : 0;
                if (distanceToFootprint(pole->x, pole->y, machine.x, machine.y, size) <= radius)
                {
                    best = poleId;
                    bestNetwork = pole->network;
                }
            });
            if (best == kNullRecord)
                return;
            machine.powerNetwork = bestNetwork;
            if (auto* network = state.powerNetworks.get(machine.powerNetwork))
            {
                if (isGenerator)
                    network->generatorCount++;
                else
                    network->consumerCount++;
            }
        });
        state.powerDirty = false;
    }

    // Generators burn fuel while the network draws power and publish their output for this tick.
    static void updateGenerator(State& state, MachineRecord& machine, const FactoryPrototypeObject& proto)
    {
        const auto& props = proto.getMachine();
        auto* network = machine.powerNetwork != kNullRecord ? state.powerNetworks.get(machine.powerNetwork) : nullptr;
        if (network == nullptr)
        {
            setMachineStatus(machine, MachineStatus::idle);
            return;
        }
        if (network->consumerCount == 0 || network->lastDemand == 0)
        {
            // Nothing is drawing: stay available without burning fuel.
            setMachineStatus(machine, MachineStatus::idle);
            network->supply += props.powerOutput;
            return;
        }
        if (!machineBurnFuel(machine, props))
            return;
        setMachineStatus(machine, MachineStatus::working);
        network->supply += props.powerOutput;
    }

    static void updatePower(State& state)
    {
        if (state.powerDirty)
            rebuildPowerNetworks(state);

        // Last tick's totals decide this tick's satisfaction, then the totals restart.
        state.powerNetworks.forEach([&](RecordId, PowerNetworkRecord& network) {
            if (network.demand == 0)
                network.satisfactionQ16 = kSatisfactionFull;
            else
                network.satisfactionQ16 = static_cast<uint32_t>(
                    std::min<uint64_t>(kSatisfactionFull, (static_cast<uint64_t>(network.supply) << 16) / network.demand));
            network.lastDemand = network.demand;
            network.lastSupply = network.supply;
            network.supply = 0;
            network.demand = 0;
        });
    }

    static void updateMachines(State& state)
    {
        state.machines.forEach([&](RecordId, MachineRecord& machine) {
            auto* proto = getPrototype(machine.entry);
            if (proto == nullptr)
                return;
            // Last tick's work pollutes now; centre of the footprint.
            if (machine.isWorking() && proto->getMachine().pollution > 0)
            {
                const int32_t half = (proto->getMachine().size - 1) / 2;
                state.pollution.add({ machine.x + half, machine.y + half }, proto->getMachine().pollution);
            }
            switch (machine.getKind())
            {
                case MachineKind::drill:
                    updateDrill(state, machine, *proto);
                    break;
                case MachineKind::furnace:
                case MachineKind::assembler:
                    updateCrafter(state, machine, *proto);
                    break;
                case MachineKind::engine:
                    if (proto->getMachine().energy == EnergySource::fluid)
                        updateSteamEngine(state, machine, *proto);
                    else
                        updateGenerator(state, machine, *proto);
                    break;
                case MachineKind::pump:
                    updatePump(state, machine, *proto);
                    break;
                case MachineKind::boiler:
                    updateBoiler(state, machine, *proto);
                    break;
                default:
                    setMachineStatus(machine, MachineStatus::idle);
                    break;
            }
        });
    }

    void update(GameState_t& gameState, UpdatePhaseTimes* times)
    {
        PROFILED_FUNCTION();

        auto& state = gameState.factory;
        if (state.isEmpty())
        {
            return;
        }
        state.pollution.ensureSize(gameState.mapSize);
        if (gameState.currentTicks % kParkExtPruneTicks == 0)
            pruneParkExt(gameState);
        if (gameState.currentTicks % PollutionLayer::kSpreadTicks == 0)
            state.pollution.spread();
        // Fixed order: belts move, then inserters pick up and drop, then machines work. Containers have no
        // per-tick behaviour.
        if (times == nullptr)
        {
            updateBelts(state);
            updateSplitters(state);
            updateInserters(state);
            updatePower(state);
            updateFluidNetworks(state);
            updateMachines(state);
            return;
        }
        using Clock = std::chrono::steady_clock;
        auto mark = Clock::now();
        auto lap = [&](uint64_t& total) {
            const auto now = Clock::now();
            total += static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - mark).count());
            mark = now;
        };
        updateBelts(state);
        lap(times->belts);
        updateSplitters(state);
        lap(times->splitters);
        updateInserters(state);
        lap(times->inserters);
        updatePower(state);
        lap(times->power);
        updateFluidNetworks(state);
        lap(times->fluids);
        updateMachines(state);
        lap(times->machines);
    }
} // namespace OpenRCT2::Factory

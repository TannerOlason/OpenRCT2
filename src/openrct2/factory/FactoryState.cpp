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

#include <algorithm>

namespace OpenRCT2::Factory
{
    void State::reset()
    {
        containers.clear();
        inserters.clear();
        beltSegments.clear();
        machines.clear();
        ore.clear();
        topologyVersion = 0;
    }

    bool State::isEmpty() const
    {
        return recordCount() == 0 && topologyVersion == 0 && ore.isEmpty();
    }

    size_t State::recordCount() const
    {
        return containers.aliveCount() + inserters.aliveCount() + beltSegments.aliveCount() + machines.aliveCount();
    }

    static void updateBelts(State& state)
    {
        state.beltSegments.forEach([&](RecordId, BeltSegmentRecord& segment) {
            auto* next = segment.next != kNullRecord ? state.beltSegments.get(segment.next) : nullptr;
            tickSegment(segment, next);
        });
    }

    static RecordRef resolveNeighbour(const InserterRecord& inserter, Direction d)
    {
        auto loc = neighbourTile(tileToCoords(inserter.location()), d);
        auto* element = findFactoryElement(loc);
        if (element == nullptr || !element->hasRecord())
            return RecordRef{};
        return RecordRef{ static_cast<uint8_t>(element->getSubtype()), element->getRecordId(), element->getFootprintIndex() };
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

    static int32_t beltTileStart(const RecordRef& ref)
    {
        return ref.aux * kBeltUnitsPerTile;
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
                const int32_t from = beltTileStart(ref);
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
                const int32_t pos = beltTileStart(ref) + kBeltUnitsPerTile / 2;
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

    static void setStatus(MachineRecord& machine, MachineStatus status)
    {
        machine.status = static_cast<uint8_t>(status);
    }

    // Burner machines consume one tick of fuel per working tick; returns false (and sets noFuel) when empty.
    static bool burnFuel(MachineRecord& machine, const MachineProperties& props)
    {
        if (props.energy != EnergySource::burner)
            return true;
        if (machine.fuelEnergy == 0)
        {
            if (machine.fuel.isEmpty())
            {
                setStatus(machine, MachineStatus::noFuel);
                return false;
            }
            auto* fuelProto = getPrototype(machine.fuel.item);
            machine.fuelEnergy = fuelProto != nullptr ? fuelProto->getItem().fuelTicks : 0;
            machine.fuel.count--;
            if (machine.fuel.count == 0)
                machine.fuel.item = kObjectEntryIndexNull;
            if (machine.fuelEnergy == 0)
            {
                setStatus(machine, MachineStatus::noFuel);
                return false;
            }
        }
        machine.fuelEnergy--;
        return true;
    }

    // Tries to push one output item onto whatever is on the tile ahead (belt, container or machine).
    static void drillDropAhead(State& state, MachineRecord& machine)
    {
        auto loc = neighbourTile(tileToCoords(machine.location()), machine.direction);
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
                    const int32_t pos = element->getFootprintIndex() * kBeltUnitsPerTile + kBeltUnitsPerTile / 2;
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

        // Find the next cell with ore in the mining area (a square of radius miningRadius).
        const int32_t radius = props.miningRadius;
        const int32_t side = radius * 2 + 1;
        const int32_t cells = side * side;
        const auto origin = TileCoordsXY(machine.x - radius, machine.y - radius);
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
            setStatus(machine, MachineStatus::noOre);
            machine.progress = 0;
            return;
        }
        auto* oreProto = getPrototype(cell.ore);
        const ObjectEntryIndex product = oreProto != nullptr ? oreProto->getOre().item.resolve() : kObjectEntryIndexNull;
        if (product == kObjectEntryIndexNull)
        {
            setStatus(machine, MachineStatus::noRecipe);
            return;
        }
        if (!stackCanAdd(machine.outputs, product, 1, stackSizeOf(product)))
        {
            setStatus(machine, MachineStatus::outputFull);
            return;
        }
        if (!burnFuel(machine, props))
            return;

        setStatus(machine, MachineStatus::working);
        machine.craftCost = static_cast<uint32_t>(props.miningTimeTicks) * kWorkUnitsPerTick;
        machine.progress += props.speedQ8;
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
                setStatus(machine, MachineStatus::outputFull);
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
            setStatus(machine, machine.getKind() == MachineKind::furnace ? MachineStatus::noInput : MachineStatus::noRecipe);
            return;
        }

        if (machine.craftCost == 0 && !startCraft(machine, *recipeProto))
        {
            if (machine.getStatus() != MachineStatus::outputFull)
                setStatus(machine, MachineStatus::noInput);
            return;
        }

        if (props.energy == EnergySource::electric)
        {
            // Power networks arrive with the next slice; until then electric machines run at full satisfaction
            // only when no network exists at all, so the content pack stays testable.
            if (machine.powerNetwork != kNullRecord)
            {
                setStatus(machine, MachineStatus::noPower);
                return;
            }
        }
        if (!burnFuel(machine, props))
            return;

        setStatus(machine, MachineStatus::working);
        machine.progress += props.speedQ8;
        if (machine.progress >= machine.craftCost)
        {
            finishCraft(machine, *recipeProto);
        }
    }

    static void updateMachines(State& state)
    {
        state.machines.forEach([&](RecordId, MachineRecord& machine) {
            auto* proto = getPrototype(machine.entry);
            if (proto == nullptr)
                return;
            switch (machine.getKind())
            {
                case MachineKind::drill:
                    updateDrill(state, machine, *proto);
                    break;
                case MachineKind::furnace:
                case MachineKind::assembler:
                    updateCrafter(state, machine, *proto);
                    break;
                default:
                    setStatus(machine, MachineStatus::idle);
                    break;
            }
        });
    }

    void update(GameState_t& gameState)
    {
        PROFILED_FUNCTION();

        auto& state = gameState.factory;
        if (state.isEmpty())
        {
            return;
        }
        // Fixed order: belts move, then inserters pick up and drop, then machines work. Containers have no
        // per-tick behaviour.
        updateBelts(state);
        updateInserters(state);
        updateMachines(state);
    }
} // namespace OpenRCT2::Factory

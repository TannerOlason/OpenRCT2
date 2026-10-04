/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Plain data records stored in FactoryState pools.
//
// Every record exposes `template<typename V> void visit(V& v)` which enumerates its fields in a fixed
// order. The same visitor drives park-file persistence (OrcaStream chunks), the multiplayer sync
// checksum (DataSerialiser) and game-state snapshots, so a field can never be saved but not hashed.
// Visitors provide `operator()(T&)` for scalars, `vec(std::vector<T>&, elementVisit)` for vectors and
// `isReading()`.

#pragma once

#include "../object/ObjectTypes.h"
#include "../world/Location.hpp"
#include "../world/tile_element/FactoryElement.h"

#include <array>
#include <cstdint>
#include <vector>

namespace OpenRCT2::Factory
{
    using RecordId = FactoryRecordId;
    constexpr RecordId kNullRecord = kFactoryRecordNull;

    // Belt geometry. Positions along a segment are in 1/256 tile units; items may never be closer
    // than kBeltItemSpacing. Speeds are units per tick: 12/24/36 = 15/30/45 items per second at 40 Hz.
    constexpr int32_t kBeltUnitsPerTile = 256;
    constexpr int32_t kBeltItemSpacing = 64;
    constexpr uint8_t kBeltLaneCount = 2;
    constexpr uint8_t kMaxSegmentTiles = 32;

    // Default element visitor: the element has its own visit().
    struct VisitElement
    {
        template<typename T, typename V>
        void operator()(T& element, V& v) const
        {
            element.visit(v);
        }
    };

    struct VisitTileCoords
    {
        template<typename V>
        void operator()(TileCoordsXYZ& coords, V& v) const
        {
            v(coords.x);
            v(coords.y);
            v(coords.z);
        }
    };

    /**
     * Points at a record in another pool, e.g. an inserter's pickup and drop targets. `kind` is a
     * FactoryElementSubtype stored as a byte.
     */
    struct RecordRef
    {
        uint8_t kind{ static_cast<uint8_t>(FactoryElementSubtype::count) };
        RecordId id{ kNullRecord };
        uint8_t aux{}; // belt: tile index inside the segment

        bool isNull() const
        {
            return id == kNullRecord;
        }

        FactoryElementSubtype getKind() const
        {
            return static_cast<FactoryElementSubtype>(kind);
        }

        template<typename V>
        void visit(V& v)
        {
            v(kind);
            v(id);
            v(aux);
        }
    };

    constexpr uint8_t kInserterPhaseWaitingForItem = 0;
    constexpr uint8_t kInserterPhaseSwingingToDrop = 1;
    constexpr uint8_t kInserterPhaseWaitingToDrop = 2;
    constexpr uint8_t kInserterPhaseReturning = 3;

    struct ItemStack
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint16_t count{};

        bool isEmpty() const
        {
            return count == 0 || item == kObjectEntryIndexNull;
        }

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(count);
        }
    };

    /**
     * Fields shared by every record anchored to a single origin tile.
     */
    struct RecordBase
    {
        int32_t x{};
        int32_t y{};
        int32_t z{};
        uint8_t direction{};
        ObjectEntryIndex entry{ kObjectEntryIndexNull };

        TileCoordsXYZ location() const
        {
            return TileCoordsXYZ{ x, y, z };
        }

        void setLocation(const TileCoordsXYZ& loc)
        {
            x = loc.x;
            y = loc.y;
            z = loc.z;
        }

        template<typename V>
        void visitBase(V& v)
        {
            v(x);
            v(y);
            v(z);
            v(direction);
            v(entry);
        }
    };

    struct ContainerRecord : RecordBase
    {
        std::vector<ItemStack> slots;

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v.vec(slots, VisitElement{});
        }
    };

    struct BeltItem
    {
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        // Distance in belt units from the item ahead of this one (or from the segment end for the
        // first item), never less than kBeltItemSpacing.
        uint16_t gap{};

        template<typename V>
        void visit(V& v)
        {
            v(item);
            v(gap);
        }
    };

    struct BeltLane
    {
        std::vector<BeltItem> items;

        template<typename V>
        void visit(V& v)
        {
            v.vec(items, VisitElement{});
        }
    };

    // What a belt segment's last tile feeds.
    enum class BeltLinkKind : uint8_t
    {
        none = 0,     // dead end
        segment = 1,  // start of `next`
        sideload = 2, // the middle of `next` at nextPos on lane nextLane
        splitter = 3, // input side nextLane of splitter `next`
    };

    struct BeltSegmentRecord
    {
        // Tiles from the segment start to its end; each tile's own FactoryElement carries its direction.
        std::vector<TileCoordsXYZ> tiles;
        ObjectEntryIndex entry{ kObjectEntryIndexNull };
        uint8_t speed{};
        std::array<BeltLane, kBeltLaneCount> lanes{};
        // Link from the last tile: see BeltLinkKind.
        RecordId next{ kNullRecord };
        uint8_t nextKind{ static_cast<uint8_t>(BeltLinkKind::none) };
        uint8_t nextLane{};
        int32_t nextPos{};
        // Underground pairs are two-tile segments with this many hidden belt units between the tiles.
        uint16_t extraLength{};

        BeltLinkKind getNextKind() const
        {
            return static_cast<BeltLinkKind>(nextKind);
        }

        template<typename V>
        void visit(V& v)
        {
            v.vec(tiles, VisitTileCoords{});
            v(entry);
            v(speed);
            for (auto& lane : lanes)
            {
                lane.visit(v);
            }
            v(next);
            v(nextKind);
            v(nextLane);
            v(nextPos);
            v(extraLength);
        }
    };

    // Splitter priorities: which side is preferred (side 0 = left of travel, the origin tile).
    constexpr uint8_t kSplitterPriorityNone = 0;
    constexpr uint8_t kSplitterPriorityLeft = 1;
    constexpr uint8_t kSplitterPriorityRight = 2;

    /**
     * A 1x2 splitter: the origin tile is input/output side 0 (left of travel), the second tile side 1.
     * Items arriving on either input side go alternately to the output segments ahead of each side. With an
     * output priority the preferred side is tried first; with a filter, the filter item goes only to the
     * priority side (left when none is set) and every other item only to the other side. With an input
     * priority the preferred input is served first each tick.
     */
    struct SplitterRecord : RecordBase
    {
        std::array<RecordId, 2> outputs{ kNullRecord, kNullRecord }; // segments starting ahead of each side
        uint8_t nextOutput{};
        uint32_t topologyVersionSeen{};
        ObjectEntryIndex filter{ kObjectEntryIndexNull };
        uint8_t inputPriority{ kSplitterPriorityNone };
        uint8_t outputPriority{ kSplitterPriorityNone };

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v(outputs[0]);
            v(outputs[1]);
            v(nextOutput);
            v(topologyVersionSeen);
            v(filter);
            v(inputPriority);
            v(outputPriority);
        }
    };

    enum class MachineKind : uint8_t
    {
        drill,
        furnace,
        assembler,
        boiler,
        engine,
        pump,
        lab,
        turret,
        exportDepot,
        count,
    };

    enum class EnergySource : uint8_t
    {
        none,
        burner,
        electric,
        fluid, // steam engines: power from the fluid in an input fluid box
    };

    enum class MachineStatus : uint8_t
    {
        idle,
        working,
        noInput,
        outputFull,
        noFuel,
        noPower,
        noOre,
        noRecipe,
    };

    // Work units: a recipe taking T ticks at speed 1.0 costs T * kWorkUnitsPerTick; a machine adds speedQ8
    // (256 = 1.0) scaled by its power satisfaction every tick.
    constexpr uint32_t kWorkUnitsPerTick = 256;

    struct MachineRecord : RecordBase
    {
        uint8_t kind{};                                   // MachineKind
        ObjectEntryIndex recipe{ kObjectEntryIndexNull }; // assemblers: chosen; furnaces: current auto recipe
        std::vector<ItemStack> inputs;
        std::vector<ItemStack> outputs;
        ItemStack fuel;          // burner machines
        uint32_t fuelEnergy{};   // ticks of work left in the item being burnt
        uint32_t progress{};     // work units done on the current craft
        uint32_t craftCost{};    // work units the current craft needs, 0 when idle
        uint8_t status{};        // MachineStatus
        uint16_t miningCursor{}; // drills: next cell of the mining area to scan
        RecordId powerNetwork{ kNullRecord };
        uint32_t topologyVersionSeen{};
        std::vector<RecordId> fluidNetworks; // one per fluid box of the prototype, kNullRecord until rebuilt

        MachineKind getKind() const
        {
            return static_cast<MachineKind>(kind);
        }
        MachineStatus getStatus() const
        {
            return static_cast<MachineStatus>(status);
        }
        bool isWorking() const
        {
            return getStatus() == MachineStatus::working;
        }

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v(kind);
            v(recipe);
            v.vec(inputs, VisitElement{});
            v.vec(outputs, VisitElement{});
            fuel.visit(v);
            v(fuelEnergy);
            v(progress);
            v(craftCost);
            v(status);
            v(miningCursor);
            v(powerNetwork);
            v(topologyVersionSeen);
            v.vec(fluidNetworks, [](RecordId& id, auto& vv) { vv(id); });
        }
    };

    struct PoleRecord : RecordBase
    {
        RecordId network{ kNullRecord };

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v(network);
        }
    };

    /**
     * One connected component of poles. Supply and demand are totals of the previous tick; satisfaction
     * (Q16, 65536 = fully powered) scales every consumer's progress this tick.
     */
    struct PowerNetworkRecord
    {
        uint32_t supply{};
        uint32_t demand{};
        uint32_t lastDemand{}; // demand of the previous tick, what generators react to
        uint32_t lastSupply{}; // supply offered on the previous tick; lastDemand / lastSupply is the load
        uint32_t satisfactionQ16{ 65536 };
        uint16_t poleCount{};
        uint16_t generatorCount{};
        uint16_t consumerCount{};

        template<typename V>
        void visit(V& v)
        {
            v(supply);
            v(demand);
            v(lastDemand);
            v(lastSupply);
            v(satisfactionQ16);
            v(poleCount);
            v(generatorCount);
            v(consumerCount);
        }
    };

    constexpr uint32_t kSatisfactionFull = 65536;

    struct PipeRecord : RecordBase
    {
        RecordId network{ kNullRecord };

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v(network);
        }
    };

    /**
     * One connected component of pipes and machine fluid boxes. The whole component holds a single fluid as one
     * volume; there is no per-pipe flow. Consumers register what they want in `demand` and draw this tick's
     * share, scaled by satisfaction (Q16) computed from last tick's demand, so scarce fluid is shared in
     * proportion rather than by id order.
     */
    struct FluidNetworkRecord
    {
        ObjectEntryIndex fluid{ kObjectEntryIndexNull }; // kObjectEntryIndexNull while empty
        uint32_t amount{};
        uint32_t capacity{};
        uint32_t demand{};
        uint32_t lastDemand{};
        uint32_t satisfactionQ16{ kSatisfactionFull };
        uint16_t pipeCount{};
        uint16_t boxCount{};

        uint32_t space() const
        {
            return capacity > amount ? capacity - amount : 0;
        }

        template<typename V>
        void visit(V& v)
        {
            v(fluid);
            v(amount);
            v(capacity);
            v(demand);
            v(lastDemand);
            v(satisfactionQ16);
            v(pipeCount);
            v(boxCount);
        }
    };

    /**
     * One tile of the ore layer. 8 bytes; a map is at most 1001 x 1001 tiles.
     */
    struct OreCell
    {
        ObjectEntryIndex ore{ kObjectEntryIndexNull };
        uint16_t richness{};
        uint32_t amount{};

        bool isEmpty() const
        {
            return amount == 0 || ore == kObjectEntryIndexNull;
        }
        bool operator==(const OreCell& other) const
        {
            return ore == other.ore && richness == other.richness && amount == other.amount;
        }
    };

    struct InserterRecord : RecordBase
    {
        uint8_t phase{};
        uint16_t progress{};
        ItemStack hand;
        RecordRef source;
        RecordRef target;
        uint32_t topologyVersionSeen{};

        template<typename V>
        void visit(V& v)
        {
            visitBase(v);
            v(phase);
            v(progress);
            hand.visit(v);
            source.visit(v);
            target.visit(v);
            v(topologyVersionSeen);
        }
    };
} // namespace OpenRCT2::Factory

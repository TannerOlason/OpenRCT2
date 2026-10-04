/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The single object type for all factory content (ADR 0005).

#pragma once

#include "../core/Money.hpp"
#include "../drawing/ImageIndexType.h"
#include "../object/Object.h"
#include "../world/tile_element/FactoryElement.h"
#include "FactoryRecords.h"

#include <string>
#include <string_view>
#include <vector>

namespace OpenRCT2::Factory
{
    /**
     * `properties.kind` of a factory_prototype object. Items, recipes, technologies and ores are data only;
     * the other kinds are placeable and own a FactoryElement subtype.
     */
    enum class PrototypeKind : uint8_t
    {
        item,
        recipe,
        belt,
        undergroundBelt,
        splitter,
        inserter,
        container,
        machine,
        pole,
        pipe,
        generator,
        ore,
        technology,
        count,
    };

    std::string_view prototypeKindName(PrototypeKind kind);

    // Object selection sub-tabs on the factory page: 0 all, 1 items, fluids and ores, 2 recipes, 3 logistics,
    // 4 machines, 5 technologies.
    bool isInSelectionGroup(uint8_t kind, size_t group);
    PrototypeKind parsePrototypeKind(std::string_view name);
    FactoryElementSubtype subtypeForKind(PrototypeKind kind);

    /**
     * Image layouts (index = base image + offset). Shapes and directions are in map space; the painter adds
     * the view rotation.
     *   item:      [0] icon (UI, about 24x24), [1] belt sprite (small, drawn on belts)
     *   belt:      [shape * 4 * frames + direction * frames + frame], shapes: 0 straight, 1 turn left (items
     *              enter travelling in (d + 1) & 3), 2 turn right (enter travelling in (d + 3) & 3)
     *   inserter:  [direction * frames + frame], frame 0 = arm over the pickup tile, frames-1 = over the drop tile
     *   container: [direction] when rotations == 4, otherwise [0]
     *   machine:   [(direction * frames + frame) * size * size + slice] (direction is 0 when rotations == 1);
     *              frame 0 = idle; slice = viewRow * size + viewColumn, the tile's position in the view-rotated
     *              footprint (see footprintViewSlice), so a 1x1 machine has one slice; a multi-tile machine may add
     *              one whole-machine preview image after the last slice for the build palette
     *   ore:       [0] ground overlay (64x32 diamond), [1] icon
     *   pole:      [0] the pole
     *   underground: [direction] entrance, [4 + direction] exit
     *   splitter:  [side * 4 + direction], side 0 = origin tile (left of travel)
     *   pipe:      [mask], 16 images; bit d of mask = connected towards view direction d
     *   technology: [0] icon (optional)
     */
    enum class BeltShape : uint8_t
    {
        straight = 0,
        turnLeft = 1,
        turnRight = 2,
        count,
    };

    struct ItemProperties
    {
        uint16_t stackSize = 100;
        uint32_t fuelTicks = 0;        // ticks of burner work one item provides; 0 = not a fuel
        bool fluid = false;            // fluids live in fluid networks and machine fluid boxes, never on belts
        money64 marketPrice = 0;       // what the Market pays for one, before saturation; 0 = not sellable
        uint16_t marketSaturation = 8; // how much each sale floods the Market (out of 1024)
        uint8_t shopItem = 255;        // ShopItem shops sell this as, in warehouse stock mode (255 = none)
    };

    /**
     * A reference to another prototype by object identifier, resolved to a loaded entry index on demand
     * (object lists are only known once a park is loaded).
     */
    struct PrototypeRef
    {
        std::string identifier;
        mutable ObjectEntryIndex cached = kObjectEntryIndexNull;

        bool isSet() const
        {
            return !identifier.empty();
        }
        ObjectEntryIndex resolve() const;
    };

    struct ItemAmount
    {
        PrototypeRef item;
        uint16_t count = 1;
    };

    struct OreProperties
    {
        PrototypeRef item;            // item mined from this ore
        uint32_t defaultAmount = 500; // per cell when painted without an explicit amount
    };

    struct RecipeProperties
    {
        std::vector<ItemAmount> ingredients;
        std::vector<ItemAmount> results;
        uint16_t timeTicks = 40; // at speed 1.0
        std::string category;    // "smelting", "crafting", ...
    };

    struct PoleProperties
    {
        uint8_t wireReach = 7;    // tiles between poles that connect
        uint8_t supplyRadius = 2; // tiles around the pole that machines draw power from
    };

    // Sides of a machine relative to its facing: bit k is the side facing (direction + k) & 3.
    constexpr uint8_t kSideFront = 1 << 0;
    constexpr uint8_t kSideRight = 1 << 1;
    constexpr uint8_t kSideBack = 1 << 2;
    constexpr uint8_t kSideLeft = 1 << 3;

    enum class FluidBoxRole : uint8_t
    {
        input,
        output,
    };

    /**
     * A machine's connection to fluid networks. A box with several sides is a pass-through: everything connected
     * on those sides joins one network. Each box adds `capacity` to the network it belongs to.
     */
    struct FluidBoxProperties
    {
        FluidBoxRole role = FluidBoxRole::input;
        uint8_t sides = kSideBack;
        uint32_t capacity = 1000;
    };

    struct PipeProperties
    {
        uint32_t capacity = 1000; // fluid units each pipe adds to its network
    };

    struct MachineProperties
    {
        MachineKind kind = MachineKind::assembler;
        EnergySource energy = EnergySource::electric;
        uint16_t speedQ8 = 256;   // 256 = 1.0
        uint32_t powerUsage = 0;  // electric machines, arbitrary power units per tick
        uint32_t powerOutput = 0; // generators (kind engine): power units per tick at full load
        uint8_t inputSlots = 1;
        uint8_t outputSlots = 1;
        std::vector<std::string> recipeCategories;
        uint8_t miningRadius = 1;      // drills: tiles around the machine scanned for ore
        uint16_t miningTimeTicks = 80; // drills: ticks per ore item at speed 1.0
        uint8_t frames = 1;            // working animation frames per direction
        uint8_t rotations = 4;         // 1 or 4
        uint8_t size = 1;              // square footprint, size x size tiles from the origin (minimum) corner
        uint16_t pollution = 0;        // added to the pollution layer every working tick
        uint8_t noise = 0;             // loudness while working, heard by guests within a few tiles
        bool photogenic = true;        // guests stop to watch it while it works
        std::vector<FluidBoxProperties> fluidBoxes;
        PrototypeRef inputFluid;  // boilers and steam engines: what the input box must hold
        PrototypeRef outputFluid; // pumps and boilers: what the output box receives
        uint32_t fluidRate = 0;   // fluid units per tick moved (pump), converted (boiler) or burnt (engine)
    };

    struct BeltProperties
    {
        uint8_t speed = 12; // belt units per tick (12/24/36 = 15/30/45 items per second)
        uint8_t frames = 1; // animation frames per shape and direction
        uint8_t reach = 4;  // underground belts: tiles the pair may span (gap between entrance and exit)
    };

    struct InserterProperties
    {
        uint8_t frames = 1;       // arm frames from pickup to drop
        uint16_t swingTicks = 24; // ticks for the arm to travel from pickup to drop (and back)
        uint8_t reach = 1;        // tiles between the inserter and its pickup/drop tiles
    };

    struct ContainerProperties
    {
        uint16_t slots = 16;
        uint8_t rotations = 1;    // 1 or 4 images
        bool warehouse = false;   // items put in go to the park-wide Warehouse instead of its slots
        bool exportDepot = false; // items put in are sold to the Market at once (unsellable items are refused)
    };

    /**
     * A technology in the fork's research tree (ADR 0012). Labs research it unit by unit: each unit consumes `packs`
     * and takes `unitTicks` of lab work at speed 1.0. Researching it unlocks factory prototypes, ride entries and
     * scenery groups that are otherwise withheld while the technology is loaded.
     */
    struct TechnologyProperties
    {
        std::vector<PrototypeRef> prerequisites;
        std::vector<ItemAmount> packs;
        uint16_t units = 10;
        uint16_t unitTicks = 600;
        std::vector<PrototypeRef> unlocks;      // factory prototypes: recipes, machines, belts, ...
        std::vector<std::string> rideEntries;   // ride object identifiers
        std::vector<std::string> sceneryGroups; // scenery group object identifiers
    };

    class FactoryPrototypeObject final : public Object
    {
    private:
        PrototypeKind _kind = PrototypeKind::count;
        ItemProperties _item{};
        BeltProperties _belt{};
        InserterProperties _inserter{};
        ContainerProperties _container{};
        OreProperties _ore{};
        RecipeProperties _recipe{};
        MachineProperties _machine{};
        PoleProperties _pole{};
        PipeProperties _pipe{};
        TechnologyProperties _technology{};
        money64 _price = 0;
        money64 _removalPrice = 0;
        uint8_t _clearance = 8; // height of the placed element in z units (kCoordsZStep multiples)
        ImageIndex _baseImageId = kImageIndexUndefined;
        uint32_t _numImages = 0;

    public:
        static constexpr ObjectType kObjectType = ObjectType::factoryPrototype;

        void ReadJson(IReadObjectContext* context, json_t& root) override;
        void Load() override;
        void Unload() override;
        void DrawPreview(Drawing::RenderTarget& rt, int32_t width, int32_t height) const override;
        void SetRepositoryItem(ObjectRepositoryItem* item) const override;

        PrototypeKind getKind() const
        {
            return _kind;
        }
        bool isPlaceable() const;
        FactoryElementSubtype getSubtype() const;

        money64 getPrice() const
        {
            return _price;
        }
        money64 getRemovalPrice() const
        {
            return _removalPrice;
        }
        uint8_t getClearance() const
        {
            return _clearance;
        }

        const ItemProperties& getItem() const
        {
            return _item;
        }
        const BeltProperties& getBelt() const
        {
            return _belt;
        }
        const InserterProperties& getInserter() const
        {
            return _inserter;
        }
        const ContainerProperties& getContainer() const
        {
            return _container;
        }
        const OreProperties& getOre() const
        {
            return _ore;
        }
        const RecipeProperties& getRecipe() const
        {
            return _recipe;
        }
        const MachineProperties& getMachine() const
        {
            return _machine;
        }
        const PoleProperties& getPole() const
        {
            return _pole;
        }
        const PipeProperties& getPipe() const
        {
            return _pipe;
        }
        const TechnologyProperties& getTechnology() const
        {
            return _technology;
        }
        bool isFluid() const
        {
            return _kind == PrototypeKind::item && _item.fluid;
        }
        bool isGenerator() const
        {
            return (_kind == PrototypeKind::machine || _kind == PrototypeKind::generator)
                && _machine.kind == MachineKind::engine;
        }
        bool machineHandlesCategory(std::string_view category) const;

        bool hasImages() const
        {
            return _baseImageId != kImageIndexUndefined && _numImages > 0;
        }
        uint32_t getNumLoadedImages() const
        {
            return _numImages;
        }

        // Each returns kImageIndexUndefined when the object has no image at that slot.
        ImageIndex getItemIconImage() const;
        ImageIndex getItemBeltImage() const;
        ImageIndex getBeltImage(BeltShape shape, uint8_t direction, uint8_t frame) const;
        ImageIndex getInserterImage(uint8_t direction, uint8_t frame) const;
        ImageIndex getContainerImage(uint8_t direction) const;
        ImageIndex getMachineImage(uint8_t direction, uint8_t frame, uint8_t slice = 0) const;
        ImageIndex getOreOverlayImage() const;
        ImageIndex getOreIconImage() const;
        ImageIndex getPoleImage() const;
        ImageIndex getUndergroundImage(bool exit, uint8_t direction) const;
        ImageIndex getSplitterImage(uint8_t side, uint8_t direction) const;
        ImageIndex getPipeImage(uint8_t viewMask) const;
        ImageIndex getTechnologyIcon() const;

    private:
        ImageIndex imageAt(uint32_t offset) const;
    };
} // namespace OpenRCT2::Factory

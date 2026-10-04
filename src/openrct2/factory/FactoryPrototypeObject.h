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

#include <string_view>

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
    };

    struct BeltProperties
    {
        uint8_t speed = 12; // belt units per tick (12/24/36 = 15/30/45 items per second)
        uint8_t frames = 1; // animation frames per shape and direction
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
        uint8_t rotations = 1; // 1 or 4 images
    };

    class FactoryPrototypeObject final : public Object
    {
    private:
        PrototypeKind _kind = PrototypeKind::count;
        ItemProperties _item{};
        BeltProperties _belt{};
        InserterProperties _inserter{};
        ContainerProperties _container{};
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

    private:
        ImageIndex imageAt(uint32_t offset) const;
    };
} // namespace OpenRCT2::Factory

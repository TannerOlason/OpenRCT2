/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryPrototypeObject.h"

#include "../core/EnumMap.hpp"
#include "../core/EnumUtils.hpp"
#include "../core/Guard.hpp"
#include "../core/Json.hpp"
#include "../drawing/Drawing.h"
#include "../drawing/ImageId.hpp"
#include "../interface/ScreenCoords.hpp"

#include <algorithm>

namespace OpenRCT2::Factory
{
    static const EnumMap<PrototypeKind> kPrototypeKindMap(
        {
            { "item", PrototypeKind::item },
            { "recipe", PrototypeKind::recipe },
            { "belt", PrototypeKind::belt },
            { "underground_belt", PrototypeKind::undergroundBelt },
            { "splitter", PrototypeKind::splitter },
            { "inserter", PrototypeKind::inserter },
            { "container", PrototypeKind::container },
            { "machine", PrototypeKind::machine },
            { "pole", PrototypeKind::pole },
            { "pipe", PrototypeKind::pipe },
            { "generator", PrototypeKind::generator },
            { "ore", PrototypeKind::ore },
            { "technology", PrototypeKind::technology },
        });

    std::string_view prototypeKindName(PrototypeKind kind)
    {
        auto it = kPrototypeKindMap.find(kind);
        return it != kPrototypeKindMap.end() ? it->first : std::string_view("unknown");
    }

    PrototypeKind parsePrototypeKind(std::string_view name)
    {
        auto it = kPrototypeKindMap.find(name);
        return it != kPrototypeKindMap.end() ? it->second : PrototypeKind::count;
    }

    FactoryElementSubtype subtypeForKind(PrototypeKind kind)
    {
        switch (kind)
        {
            case PrototypeKind::belt:
                return FactoryElementSubtype::belt;
            case PrototypeKind::undergroundBelt:
                return FactoryElementSubtype::undergroundBelt;
            case PrototypeKind::splitter:
                return FactoryElementSubtype::splitter;
            case PrototypeKind::inserter:
                return FactoryElementSubtype::inserter;
            case PrototypeKind::container:
                return FactoryElementSubtype::container;
            case PrototypeKind::machine:
            case PrototypeKind::generator:
                return FactoryElementSubtype::machine;
            case PrototypeKind::pole:
                return FactoryElementSubtype::pole;
            case PrototypeKind::pipe:
                return FactoryElementSubtype::pipe;
            default:
                return FactoryElementSubtype::count;
        }
    }

    void FactoryPrototypeObject::ReadJson(IReadObjectContext* context, json_t& root)
    {
        Guard::Assert(root.is_object(), "FactoryPrototypeObject::ReadJson expects parameter root to be an object");

        PopulateTablesFromJson(context, root);

        auto properties = root["properties"];
        if (!properties.is_object())
        {
            context->LogError(ObjectError::invalidProperty, "factory_prototype needs a properties object");
            return;
        }

        _kind = parsePrototypeKind(Json::GetString(properties["kind"]));
        if (_kind == PrototypeKind::count)
        {
            context->LogError(ObjectError::invalidProperty, "factory_prototype has an unknown properties.kind");
            return;
        }

        _price = Json::GetNumber<money64>(properties["price"], 0);
        _removalPrice = Json::GetNumber<money64>(properties["removalPrice"], 0);
        _clearance = Json::GetNumber<uint8_t>(properties["clearance"], 8);

        switch (_kind)
        {
            case PrototypeKind::item:
                _item.stackSize = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["stackSize"], 100));
                break;
            case PrototypeKind::belt:
                _belt.speed = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["speed"], 12), 1, 64);
                _belt.frames = std::max<uint8_t>(1, Json::GetNumber<uint8_t>(properties["frames"], 1));
                break;
            case PrototypeKind::inserter:
                _inserter.frames = std::max<uint8_t>(1, Json::GetNumber<uint8_t>(properties["frames"], 1));
                _inserter.swingTicks = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["swingTicks"], 24));
                _inserter.reach = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["reach"], 1), 1, 2);
                break;
            case PrototypeKind::container:
                _container.slots = std::clamp<uint16_t>(Json::GetNumber<uint16_t>(properties["slots"], 16), 1, 256);
                _container.rotations = Json::GetNumber<uint8_t>(properties["rotations"], 1) == 4 ? 4 : 1;
                break;
            default:
                // Remaining kinds are parsed when their simulation lands (M2/M5).
                break;
        }
    }

    void FactoryPrototypeObject::Load()
    {
        _numImages = GetImageTable().GetCount();
        if (_numImages > 0)
        {
            _baseImageId = LoadImages();
        }
    }

    void FactoryPrototypeObject::Unload()
    {
        UnloadImages();
        _baseImageId = kImageIndexUndefined;
        _numImages = 0;
    }

    bool FactoryPrototypeObject::isPlaceable() const
    {
        return subtypeForKind(_kind) != FactoryElementSubtype::count;
    }

    FactoryElementSubtype FactoryPrototypeObject::getSubtype() const
    {
        return subtypeForKind(_kind);
    }

    ImageIndex FactoryPrototypeObject::imageAt(uint32_t offset) const
    {
        if (!hasImages() || offset >= _numImages)
        {
            return kImageIndexUndefined;
        }
        return _baseImageId + offset;
    }

    ImageIndex FactoryPrototypeObject::getItemIconImage() const
    {
        return imageAt(0);
    }

    ImageIndex FactoryPrototypeObject::getItemBeltImage() const
    {
        auto image = imageAt(1);
        return image != kImageIndexUndefined ? image : imageAt(0);
    }

    ImageIndex FactoryPrototypeObject::getBeltImage(BeltShape shape, uint8_t direction, uint8_t frame) const
    {
        const uint32_t frames = _belt.frames;
        return imageAt((EnumValue(shape) * 4 + (direction & 3)) * frames + (frame % frames));
    }

    ImageIndex FactoryPrototypeObject::getInserterImage(uint8_t direction, uint8_t frame) const
    {
        const uint32_t frames = _inserter.frames;
        return imageAt((direction & 3) * frames + std::min<uint32_t>(frame, frames - 1));
    }

    ImageIndex FactoryPrototypeObject::getContainerImage(uint8_t direction) const
    {
        return imageAt(_container.rotations == 4 ? (direction & 3) : 0);
    }

    void FactoryPrototypeObject::DrawPreview(Drawing::RenderTarget& rt, int32_t width, int32_t height) const
    {
        ImageIndex image = kImageIndexUndefined;
        switch (_kind)
        {
            case PrototypeKind::item:
                image = getItemIconImage();
                break;
            case PrototypeKind::belt:
                image = getBeltImage(BeltShape::straight, 0, 0);
                break;
            case PrototypeKind::inserter:
                image = getInserterImage(0, 0);
                break;
            case PrototypeKind::container:
                image = getContainerImage(0);
                break;
            default:
                image = imageAt(0);
                break;
        }
        if (image == kImageIndexUndefined)
        {
            return;
        }
        auto centre = ScreenCoordsXY{ width / 2, height / 2 };
        GfxDrawSprite(rt, ImageId(image), centre);
    }
} // namespace OpenRCT2::Factory

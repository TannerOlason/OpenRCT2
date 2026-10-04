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

#include "../Context.h"
#include "../core/EnumMap.hpp"
#include "../core/EnumUtils.hpp"
#include "../core/Guard.hpp"
#include "../core/Json.hpp"
#include "../drawing/Drawing.h"
#include "../drawing/ImageId.hpp"
#include "../interface/ScreenCoords.hpp"
#include "../object/ObjectManager.h"
#include "../object/ObjectRepository.h"
#include "../object/RideObject.h"
#include "Technology.h"

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

    static const EnumMap<MachineKind> kMachineKindMap(
        {
            { "drill", MachineKind::drill },
            { "furnace", MachineKind::furnace },
            { "assembler", MachineKind::assembler },
            { "boiler", MachineKind::boiler },
            { "engine", MachineKind::engine },
            { "pump", MachineKind::pump },
            { "lab", MachineKind::lab },
            { "turret", MachineKind::turret },
            { "export_depot", MachineKind::exportDepot },
        });

    static const EnumMap<EnergySource> kEnergySourceMap(
        {
            { "none", EnergySource::none },
            { "burner", EnergySource::burner },
            { "electric", EnergySource::electric },
            { "fluid", EnergySource::fluid },
        });

    static uint8_t readSides(json_t& array)
    {
        static const EnumMap<uint8_t> kSideMap(
            {
                { "front", kSideFront },
                { "right", kSideRight },
                { "back", kSideBack },
                { "left", kSideLeft },
            });
        uint8_t sides = 0;
        if (!array.is_array())
            return sides;
        for (auto& side : array)
        {
            if (!side.is_string())
                continue;
            auto it = kSideMap.find(side.get<std::string>());
            if (it != kSideMap.end())
                sides |= it->second;
        }
        return sides;
    }

    ObjectEntryIndex PrototypeRef::resolve() const
    {
        if (identifier.empty())
            return kObjectEntryIndexNull;
        auto& objectManager = GetContext()->GetObjectManager();
        if (cached != kObjectEntryIndexNull)
        {
            auto* object = objectManager.GetLoadedObject<FactoryPrototypeObject>(cached);
            if (object != nullptr && object->GetIdentifier() == identifier)
                return cached;
        }
        auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(identifier));
        cached = object != nullptr ? objectManager.GetLoadedObjectEntryIndex(object) : kObjectEntryIndexNull;
        return cached;
    }

    static std::vector<ItemAmount> readItemAmounts(json_t& array)
    {
        std::vector<ItemAmount> result;
        if (!array.is_array())
            return result;
        for (auto& entry : array)
        {
            if (!entry.is_object())
                continue;
            ItemAmount amount;
            amount.item.identifier = Json::GetString(entry["item"]);
            amount.count = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(entry["count"], 1));
            if (!amount.item.identifier.empty())
                result.push_back(std::move(amount));
        }
        return result;
    }

    bool FactoryPrototypeObject::machineHandlesCategory(std::string_view category) const
    {
        for (const auto& handled : _machine.recipeCategories)
        {
            if (handled == category)
                return true;
        }
        return false;
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
                _item.fuelTicks = Json::GetNumber<uint32_t>(properties["fuelTicks"], 0);
                _item.fluid = Json::GetBoolean(properties["fluid"], false);
                _item.marketPrice = Json::GetNumber<money64>(properties["marketPrice"], 0);
                _item.marketSaturation = Json::GetNumber<uint16_t>(properties["marketSaturation"], 8);
                _item.shopItem = static_cast<uint8_t>(RideObject::ParseShopItem(Json::GetString(properties["shopItem"])));
                break;
            case PrototypeKind::ore:
                _ore.item.identifier = Json::GetString(properties["item"]);
                _ore.defaultAmount = std::max<uint32_t>(1, Json::GetNumber<uint32_t>(properties["defaultAmount"], 500));
                break;
            case PrototypeKind::recipe:
                _recipe.ingredients = readItemAmounts(properties["ingredients"]);
                _recipe.results = readItemAmounts(properties["results"]);
                _recipe.timeTicks = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["timeTicks"], 40));
                _recipe.category = Json::GetString(properties["category"], "crafting");
                break;
            case PrototypeKind::machine:
            case PrototypeKind::generator:
            {
                auto kindIt = kMachineKindMap.find(Json::GetString(properties["machineKind"], "assembler"));
                _machine.kind = kindIt != kMachineKindMap.end() ? kindIt->second : MachineKind::assembler;
                auto energyIt = kEnergySourceMap.find(Json::GetString(properties["energy"], "electric"));
                _machine.energy = energyIt != kEnergySourceMap.end() ? energyIt->second : EnergySource::electric;
                _machine.speedQ8 = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["speedQ8"], 256));
                _machine.powerUsage = Json::GetNumber<uint32_t>(properties["powerUsage"], 0);
                _machine.powerOutput = Json::GetNumber<uint32_t>(properties["powerOutput"], 0);
                _machine.inputSlots = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["inputSlots"], 1), 0, 16);
                _machine.outputSlots = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["outputSlots"], 1), 0, 16);
                _machine.miningRadius = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["miningRadius"], 1), 0, 4);
                _machine.miningTimeTicks = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["miningTimeTicks"], 80));
                _machine.frames = std::max<uint8_t>(1, Json::GetNumber<uint8_t>(properties["frames"], 1));
                _machine.rotations = Json::GetNumber<uint8_t>(properties["rotations"], 4) == 1 ? 1 : 4;
                _machine.size = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["size"], 1), 1, 5);
                _machine.pollution = Json::GetNumber<uint16_t>(properties["pollution"], 0);
                _machine.noise = Json::GetNumber<uint8_t>(properties["noise"], 0);
                _machine.photogenic = Json::GetBoolean(properties["photogenic"], true);
                _machine.inputFluid.identifier = Json::GetString(properties["inputFluid"]);
                _machine.outputFluid.identifier = Json::GetString(properties["outputFluid"]);
                _machine.fluidRate = Json::GetNumber<uint32_t>(properties["fluidRate"], 0);
                auto boxes = properties["fluidBoxes"];
                if (boxes.is_array())
                {
                    for (auto& box : boxes)
                    {
                        if (!box.is_object() || _machine.fluidBoxes.size() >= 4)
                            continue;
                        FluidBoxProperties props;
                        props.role = Json::GetString(box["role"], "input") == "output" ? FluidBoxRole::output
                                                                                       : FluidBoxRole::input;
                        props.sides = readSides(box["sides"]);
                        props.capacity = std::max<uint32_t>(1, Json::GetNumber<uint32_t>(box["capacity"], 1000));
                        if (props.sides != 0)
                            _machine.fluidBoxes.push_back(props);
                    }
                }
                auto categories = properties["recipeCategories"];
                if (categories.is_array())
                {
                    for (auto& category : categories)
                    {
                        if (category.is_string())
                            _machine.recipeCategories.push_back(category.get<std::string>());
                    }
                }
                break;
            }
            case PrototypeKind::belt:
            case PrototypeKind::undergroundBelt:
            case PrototypeKind::splitter:
                _belt.speed = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["speed"], 12), 1, 64);
                _belt.frames = std::max<uint8_t>(1, Json::GetNumber<uint8_t>(properties["frames"], 1));
                _belt.reach = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["reach"], 4), 1, 30);
                break;
            case PrototypeKind::inserter:
                _inserter.frames = std::max<uint8_t>(1, Json::GetNumber<uint8_t>(properties["frames"], 1));
                _inserter.swingTicks = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["swingTicks"], 24));
                _inserter.reach = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["reach"], 1), 1, 2);
                break;
            case PrototypeKind::container:
                _container.slots = std::clamp<uint16_t>(Json::GetNumber<uint16_t>(properties["slots"], 16), 1, 256);
                _container.rotations = Json::GetNumber<uint8_t>(properties["rotations"], 1) == 4 ? 4 : 1;
                _container.warehouse = Json::GetBoolean(properties["warehouse"], false);
                _container.exportDepot = Json::GetBoolean(properties["exportDepot"], false);
                break;
            case PrototypeKind::pole:
                _pole.wireReach = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["wireReach"], 7), 1, 30);
                _pole.supplyRadius = std::clamp<uint8_t>(Json::GetNumber<uint8_t>(properties["supplyRadius"], 2), 0, 15);
                break;
            case PrototypeKind::pipe:
                _pipe.capacity = std::max<uint32_t>(1, Json::GetNumber<uint32_t>(properties["capacity"], 1000));
                break;
            case PrototypeKind::technology:
            {
                auto readStrings = [](json_t& array) {
                    std::vector<std::string> result;
                    if (array.is_array())
                        for (auto& value : array)
                            if (value.is_string() && !value.get<std::string>().empty())
                                result.push_back(value.get<std::string>());
                    return result;
                };
                auto toRefs = [](std::vector<std::string>&& identifiers) {
                    std::vector<PrototypeRef> refs;
                    for (auto& identifier : identifiers)
                        refs.push_back(PrototypeRef{ std::move(identifier) });
                    return refs;
                };
                _technology.prerequisites = toRefs(readStrings(properties["prerequisites"]));
                _technology.packs = readItemAmounts(properties["packs"]);
                _technology.units = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["units"], 10));
                _technology.unitTicks = std::max<uint16_t>(1, Json::GetNumber<uint16_t>(properties["unitTicks"], 600));
                _technology.unlocks = toRefs(readStrings(properties["unlocks"]));
                _technology.rideEntries = readStrings(properties["rideEntries"]);
                _technology.sceneryGroups = readStrings(properties["sceneryGroups"]);
                break;
            }
            default:
                // Remaining kinds are parsed when their simulation lands (M2/M5).
                break;
        }
    }

    bool isInSelectionGroup(uint8_t kind, size_t group)
    {
        switch (static_cast<PrototypeKind>(kind))
        {
            case PrototypeKind::item:
            case PrototypeKind::ore:
                return group == 0 || group == 1;
            case PrototypeKind::recipe:
                return group == 0 || group == 2;
            case PrototypeKind::machine:
            case PrototypeKind::generator:
                return group == 0 || group == 4;
            case PrototypeKind::technology:
                return group == 0 || group == 5;
            default:
                return group == 0 || group == 3;
        }
    }

    void FactoryPrototypeObject::SetRepositoryItem(ObjectRepositoryItem* item) const
    {
        item->FactoryPrototypeInfo.Kind = static_cast<uint8_t>(_kind);
    }

    void FactoryPrototypeObject::Load()
    {
        invalidateTechnologyIndex();
        _numImages = GetImageTable().GetCount();
        if (_numImages > 0)
        {
            _baseImageId = LoadImages();
        }
    }

    void FactoryPrototypeObject::Unload()
    {
        invalidateTechnologyIndex();
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

    ImageIndex FactoryPrototypeObject::getMachineImage(uint8_t direction, uint8_t frame, uint8_t slice) const
    {
        const uint32_t frames = _machine.frames;
        const uint32_t dir = _machine.rotations == 4 ? (direction & 3) : 0;
        const uint32_t slices = static_cast<uint32_t>(_machine.size) * _machine.size;
        return imageAt((dir * frames + (frame % frames)) * slices + (slice % slices));
    }

    ImageIndex FactoryPrototypeObject::getOreOverlayImage() const
    {
        return imageAt(0);
    }

    ImageIndex FactoryPrototypeObject::getPoleImage() const
    {
        return imageAt(0);
    }

    ImageIndex FactoryPrototypeObject::getUndergroundImage(bool exit, uint8_t direction) const
    {
        return imageAt((exit ? 4u : 0u) + (direction & 3));
    }

    ImageIndex FactoryPrototypeObject::getSplitterImage(uint8_t side, uint8_t direction) const
    {
        return imageAt((side & 1) * 4u + (direction & 3));
    }

    ImageIndex FactoryPrototypeObject::getTechnologyIcon() const
    {
        return _kind == PrototypeKind::technology ? imageAt(0) : kImageIndexUndefined;
    }

    ImageIndex FactoryPrototypeObject::getPipeImage(uint8_t viewMask) const
    {
        return imageAt(viewMask & 0xF);
    }

    ImageIndex FactoryPrototypeObject::getOreIconImage() const
    {
        auto image = imageAt(1);
        return image != kImageIndexUndefined ? image : imageAt(0);
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
            case PrototypeKind::machine:
            case PrototypeKind::generator:
            {
                const uint32_t slices = static_cast<uint32_t>(_machine.size) * _machine.size;
                const uint32_t previewIndex = static_cast<uint32_t>(_machine.rotations) * _machine.frames * slices;
                image = slices > 1 && previewIndex < _numImages ? imageAt(previewIndex) : getMachineImage(0, 0);
                break;
            }
            case PrototypeKind::ore:
                image = getOreIconImage();
                break;
            case PrototypeKind::pole:
                image = getPoleImage();
                break;
            case PrototypeKind::undergroundBelt:
                image = getUndergroundImage(false, 0);
                break;
            case PrototypeKind::splitter:
                image = getSplitterImage(0, 0);
                break;
            case PrototypeKind::pipe:
                image = getPipeImage(0b0101);
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

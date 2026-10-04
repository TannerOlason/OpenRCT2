/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Blueprint.h"

#include "../Context.h"
#include "../GameState.h"
#include "../object/ObjectManager.h"
#include "../world/Map.h"
#include "../world/TileElementsView.h"
#include "../world/tile_element/FactoryElement.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryTopology.h"

#include <algorithm>
#include <charconv>
#include <map>

namespace OpenRCT2::Factory
{
    namespace
    {
        constexpr std::string_view kMagic = "FTBP1";

        uint8_t entryFootprint(const BlueprintEntry& entry)
        {
            auto& objectManager = GetContext()->GetObjectManager();
            auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(entry.object));
            if (object == nullptr || object->GetObjectType() != ObjectType::factoryPrototype)
                return 1;
            return footprintSize(static_cast<const FactoryPrototypeObject*>(object));
        }

        std::vector<std::string_view> split(std::string_view text, char separator)
        {
            std::vector<std::string_view> parts;
            size_t start = 0;
            while (start <= text.size())
            {
                const auto end = text.find(separator, start);
                parts.push_back(text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start));
                if (end == std::string_view::npos)
                    break;
                start = end + 1;
            }
            return parts;
        }

        std::optional<int32_t> toInt(std::string_view text)
        {
            int32_t value{};
            auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
            if (ec != std::errc() || ptr != text.data() + text.size())
                return std::nullopt;
            return value;
        }

        bool isValidIdentifier(std::string_view id)
        {
            return !id.empty() && id.size() <= 128
                && std::all_of(id.begin(), id.end(), [](char c) { return c > ' ' && c != ';' && c != ','; });
        }
    } // namespace

    std::string serialiseBlueprint(const Blueprint& blueprint)
    {
        // Identifier table in first-use order.
        std::vector<std::string> ids;
        auto indexOf = [&](const std::string& id) -> int32_t {
            if (id.empty())
                return -1;
            auto it = std::find(ids.begin(), ids.end(), id);
            if (it != ids.end())
                return static_cast<int32_t>(it - ids.begin());
            ids.push_back(id);
            return static_cast<int32_t>(ids.size() - 1);
        };
        std::string body;
        for (const auto& entry : blueprint.entries)
        {
            const auto object = indexOf(entry.object);
            const auto recipe = indexOf(entry.recipe);
            body += ';' + std::to_string(entry.dx) + ',' + std::to_string(entry.dy) + ',' + std::to_string(entry.dz) + ','
                + std::to_string(entry.direction) + ',' + std::to_string(object) + ',' + std::to_string(recipe);
        }
        std::string text = std::string(kMagic) + ';' + std::to_string(blueprint.width) + ';' + std::to_string(blueprint.height)
            + ';' + std::to_string(ids.size());
        for (const auto& id : ids)
            text += ';' + id;
        return text + body;
    }

    std::optional<Blueprint> parseBlueprint(std::string_view text)
    {
        while (!text.empty() && (text.front() == ' ' || text.front() == '\n' || text.front() == '\r' || text.front() == '\t'))
            text.remove_prefix(1);
        while (!text.empty() && (text.back() == ' ' || text.back() == '\n' || text.back() == '\r' || text.back() == '\t'))
            text.remove_suffix(1);
        const auto parts = split(text, ';');
        if (parts.size() < 4 || parts[0] != kMagic)
            return std::nullopt;
        Blueprint blueprint;
        const auto width = toInt(parts[1]);
        const auto height = toInt(parts[2]);
        const auto idCount = toInt(parts[3]);
        if (!width || !height || !idCount || *width < 1 || *height < 1 || *width > 256 || *height > 256 || *idCount < 0
            || parts.size() < 4 + static_cast<size_t>(*idCount))
            return std::nullopt;
        blueprint.width = *width;
        blueprint.height = *height;
        std::vector<std::string> ids;
        for (int32_t i = 0; i < *idCount; i++)
        {
            const auto id = parts[4 + i];
            if (!isValidIdentifier(id))
                return std::nullopt;
            ids.emplace_back(id);
        }
        for (size_t i = 4 + *idCount; i < parts.size(); i++)
        {
            const auto fields = split(parts[i], ',');
            if (fields.size() != 6 || blueprint.entries.size() >= kMaxBlueprintEntries)
                return std::nullopt;
            const auto dx = toInt(fields[0]), dy = toInt(fields[1]), dz = toInt(fields[2]);
            const auto direction = toInt(fields[3]), object = toInt(fields[4]), recipe = toInt(fields[5]);
            if (!dx || !dy || !dz || !direction || !object || !recipe || *dx < 0 || *dy < 0 || *dx >= *width || *dy >= *height
                || *dz < 0 || *dz > 255 || *direction < 0 || *direction > 3 || *object < 0 || *object >= *idCount
                || *recipe < -1 || *recipe >= *idCount)
                return std::nullopt;
            BlueprintEntry entry;
            entry.dx = *dx;
            entry.dy = *dy;
            entry.dz = *dz;
            entry.direction = static_cast<uint8_t>(*direction);
            entry.object = ids[*object];
            if (*recipe >= 0)
                entry.recipe = ids[*recipe];
            blueprint.entries.push_back(std::move(entry));
        }
        return blueprint;
    }

    Blueprint captureBlueprint(const GameState_t& gameState, const MapRange& rangeIn)
    {
        const auto range = rangeIn.normalise();
        const auto& state = gameState.factory;
        struct Captured
        {
            int32_t x, y, z;
            BlueprintEntry entry;
            bool late; // underground exit
            uint8_t size;
        };
        std::vector<Captured> captured;
        for (int32_t y = range.getY1(); y <= range.getY2(); y += kCoordsXYStep)
        {
            for (int32_t x = range.getX1(); x <= range.getX2(); x += kCoordsXYStep)
            {
                const CoordsXY tile{ x, y };
                if (!MapIsLocationValid(tile))
                    continue;
                for (const auto* element : TileElementsView<FactoryElement>(tile))
                {
                    if (element->isGhost() || captured.size() >= kMaxBlueprintEntries)
                        continue;
                    auto* proto = getPrototype(*element);
                    if (proto == nullptr || !proto->isPlaceable())
                        continue;
                    const auto subtype = element->getSubtype();
                    // Only the piece's own tile: multi-tile machines at footprint 0, splitters at their record tile.
                    if (subtype == FactoryElementSubtype::machine && element->getFootprintIndex() != 0)
                        continue;
                    if (subtype == FactoryElementSubtype::splitter)
                    {
                        auto* splitter = state.splitters.get(element->getRecordId());
                        if (splitter == nullptr || splitter->x * kCoordsXYStep != x || splitter->y * kCoordsXYStep != y)
                            continue;
                    }
                    Captured c{ x / kCoordsXYStep, y / kCoordsXYStep, element->getBaseZ() / kCoordsZStep, {}, false, 1 };
                    c.entry.direction = element->getDirection();
                    c.entry.object = proto->GetIdentifier();
                    c.size = footprintSize(proto);
                    if (subtype == FactoryElementSubtype::machine)
                    {
                        auto* machine = state.machines.get(element->getRecordId());
                        if (machine != nullptr && machine->getKind() == MachineKind::assembler)
                            if (auto* recipe = getPrototype(machine->recipe))
                                c.entry.recipe = recipe->GetIdentifier();
                    }
                    c.late = subtype == FactoryElementSubtype::undergroundBelt && isUndergroundExit(*element);
                    captured.push_back(std::move(c));
                }
            }
        }
        Blueprint blueprint;
        if (captured.empty())
            return blueprint;
        int32_t minX = INT32_MAX, minY = INT32_MAX, minZ = INT32_MAX, maxX = INT32_MIN, maxY = INT32_MIN;
        for (const auto& c : captured)
        {
            minX = std::min(minX, c.x);
            minY = std::min(minY, c.y);
            minZ = std::min(minZ, c.z);
            maxX = std::max(maxX, c.x + c.size - 1);
            maxY = std::max(maxY, c.y + c.size - 1);
        }
        blueprint.width = maxX - minX + 1;
        blueprint.height = maxY - minY + 1;
        std::stable_partition(captured.begin(), captured.end(), [](const Captured& c) { return !c.late; });
        for (auto& c : captured)
        {
            c.entry.dx = c.x - minX;
            c.entry.dy = c.y - minY;
            c.entry.dz = c.z - minZ;
            blueprint.entries.push_back(std::move(c.entry));
        }
        return blueprint;
    }

    Blueprint rotateBlueprint(const Blueprint& blueprint, uint8_t quarterTurns)
    {
        Blueprint result = blueprint;
        for (uint8_t turn = 0; turn < (quarterTurns & 3); turn++)
        {
            // Direction d becomes d + 1: the map vector (x, y) turns into (y, -x); shift so the corner stays at 0.
            const int32_t width = result.width;
            for (auto& entry : result.entries)
            {
                const int32_t size = entryFootprint(entry);
                const int32_t x = entry.dx;
                const int32_t y = entry.dy;
                entry.dx = y;
                entry.dy = width - size - x;
                entry.direction = static_cast<uint8_t>((entry.direction + 1) & 3);
            }
            std::swap(result.width, result.height);
        }
        return result;
    }

    CoordsXYZ blueprintEntryLocation(const BlueprintEntry& entry, const CoordsXYZ& origin)
    {
        return { origin.x + entry.dx * kCoordsXYStep, origin.y + entry.dy * kCoordsXYStep, origin.z + entry.dz * kCoordsZStep };
    }
} // namespace OpenRCT2::Factory

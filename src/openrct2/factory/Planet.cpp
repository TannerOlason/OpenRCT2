/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Planet.h"

#include "../Context.h"
#include "../GameState.h"
#include "../object/Object.h"
#include "../object/ObjectManager.h"
#include "../world/Map.h"
#include "../world/Weather.h"
#include "../world/tile_element/SurfaceElement.h"
#include "FactoryState.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    namespace
    {
        struct PresetData
        {
            const char* terrain;
            uint16_t beltSpeedPercent;
            uint16_t machineSpeedPercent;
            uint8_t weather; // Weather::Type + 1, 0 = climate
            const char* ore; // ore painted in clusters, or nullptr
        };

        constexpr PresetData kPresets[] = {
            { "rct2.terrain_surface.grass", 100, 100, 0, nullptr },
            { "rct2.terrain_surface.sand", 100, 100, static_cast<uint8_t>(Weather::Type::sunny) + 1, nullptr },
            { "rct2.terrain_surface.ice", 100, 80, static_cast<uint8_t>(Weather::Type::snow) + 1, nullptr },
            // The weird dimension: fast belts, sluggish machines, endless storms and void crystal.
            { "rct2.terrain_surface.martian", 150, 75, static_cast<uint8_t>(Weather::Type::thunder) + 1,
              "factory-tour.factory_prototype.void_crystal_patch" },
        };

        ObjectEntryIndex loadedIndex(const char* identifier)
        {
            auto& objectManager = GetContext()->GetObjectManager();
            auto* object = objectManager.GetLoadedObject(ObjectEntryDescriptor(identifier));
            return object != nullptr ? objectManager.GetLoadedObjectEntryIndex(object) : kObjectEntryIndexNull;
        }

        Weather::State forcedState(uint8_t weather)
        {
            const auto type = static_cast<Weather::Type>(weather - 1);
            Weather::State state{ type, 20, Weather::EffectType::none, 0, Weather::Level::none };
            switch (type)
            {
                case Weather::Type::sunny:
                    state.temperature = 32;
                    break;
                case Weather::Type::partiallyCloudy:
                case Weather::Type::cloudy:
                    state.weatherGloom = 1;
                    break;
                case Weather::Type::rain:
                    state = { type, 14, Weather::EffectType::rain, 1, Weather::Level::light };
                    break;
                case Weather::Type::heavyRain:
                    state = { type, 12, Weather::EffectType::rain, 2, Weather::Level::heavy };
                    break;
                case Weather::Type::thunder:
                    state = { type, 18, Weather::EffectType::storm, 2, Weather::Level::heavy };
                    break;
                case Weather::Type::snow:
                    state = { type, -4, Weather::EffectType::snow, 1, Weather::Level::light };
                    break;
                case Weather::Type::heavySnow:
                case Weather::Type::blizzard:
                    state = { type, -12, Weather::EffectType::blizzard, 2, Weather::Level::heavy };
                    break;
                default:
                    break;
            }
            return state;
        }
    } // namespace

    void applyWorldPreset(GameState_t& gameState, WorldPreset preset)
    {
        const auto index = std::min<uint8_t>(static_cast<uint8_t>(preset), static_cast<uint8_t>(WorldPreset::count) - 1);
        const auto& data = kPresets[index];
        auto& planet = gameState.factory.parkExt.planet;
        planet.preset = index;
        planet.beltSpeedPercent = data.beltSpeedPercent;
        planet.machineSpeedPercent = data.machineSpeedPercent;
        planet.weather = data.weather;

        const auto terrain = loadedIndex(data.terrain);
        const auto size = gameState.mapSize;
        for (int32_t y = 0; y < size.y; y++)
        {
            for (int32_t x = 0; x < size.x; x++)
            {
                auto* surface = MapGetSurfaceElementAt(TileCoordsXY{ x, y });
                if (surface == nullptr)
                    continue;
                if (terrain != kObjectEntryIndexNull)
                    surface->setSurfaceObjectIndex(terrain);
                // A lake in the north corner, for offshore pumps.
                if (x >= 2 && y >= 2 && x < 2 + size.x / 6 && y < 2 + size.y / 6)
                    surface->setWaterHeight(surface->getBaseZ() + 2 * kCoordsZStep);
            }
        }

        if (data.ore != nullptr)
        {
            const auto ore = loadedIndex(data.ore);
            if (ore != kObjectEntryIndexNull)
            {
                auto& layer = gameState.factory.ore;
                layer.resize(size);
                // Three fixed clusters, so every peer paints the same cells.
                const TileCoordsXY centres[] = { { size.x / 2, size.y / 2 },
                                                 { size.x / 4, size.y * 3 / 4 },
                                                 { size.x * 3 / 4, size.y / 3 } };
                for (const auto& centre : centres)
                    for (int32_t dy = -2; dy <= 2; dy++)
                        for (int32_t dx = -2; dx <= 2; dx++)
                            if (std::abs(dx) + std::abs(dy) <= 3)
                                layer.set({ centre.x + dx, centre.y + dy }, { ore, 0, 400 });
            }
        }
        if (planet.weather != 0)
        {
            gameState.weatherCurrent = forcedState(planet.weather);
            gameState.weatherNext = gameState.weatherCurrent;
        }
    }

    void updatePlanet(GameState_t& gameState)
    {
        const auto& planet = gameState.factory.parkExt.planet;
        if (planet.weather == 0)
            return;
        gameState.weatherCurrent = forcedState(planet.weather);
        gameState.weatherNext = gameState.weatherCurrent;
    }

    int32_t scaledBeltSpeed(const PlanetParams& planet, int32_t speed)
    {
        const auto percent = planet.beltSpeedPercent;
        if (percent == 100)
            return speed;
        return std::max<int32_t>(1, speed * percent / 100);
    }

    uint32_t scaledMachineSpeed(const PlanetParams& planet, uint32_t speedQ8)
    {
        const auto percent = planet.machineSpeedPercent;
        if (percent == 100)
            return speedQ8;
        return std::max<uint32_t>(1, speedQ8 * percent / 100);
    }
} // namespace OpenRCT2::Factory

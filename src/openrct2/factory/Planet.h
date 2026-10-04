/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Planet Params: per-world rules (belt and machine speed, forced weather) and the
// presets new worlds are created from (terrain, water, ore, rules).

#pragma once

#include <cstdint>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    enum class WorldPreset : uint8_t
    {
        plain,
        desert,
        iceMoon,
        weird,
        count,
    };

    // Saved in the world's own parkExt chunk (version 5); never company state.
    struct PlanetParams
    {
        uint8_t preset{}; // WorldPreset the world was created from
        uint16_t beltSpeedPercent{ 100 };
        uint16_t machineSpeedPercent{ 100 };
        uint8_t weather{}; // 0 = the climate decides, else Weather::Type + 1, held every tick

        bool isDefault() const
        {
            return preset == 0 && beltSpeedPercent == 100 && machineSpeedPercent == 100 && weather == 0;
        }

        template<typename V>
        void visit(V& v)
        {
            v(preset);
            v(beltSpeedPercent);
            v(machineSpeedPercent);
            v(weather);
        }
    };

    // Applies a preset to the active (freshly created) world: terrain, a lake, ore and rules.
    void applyWorldPreset(GameState_t& gameState, WorldPreset preset);

    // Each tick: holds the forced weather, if any.
    void updatePlanet(GameState_t& gameState);

    // Speeds scaled by the world's rules.
    int32_t scaledBeltSpeed(const PlanetParams& planet, int32_t speed);
    uint32_t scaledMachineSpeed(const PlanetParams& planet, uint32_t speedQ8);
} // namespace OpenRCT2::Factory

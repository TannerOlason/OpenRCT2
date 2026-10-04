/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Machine sounds: occasional one-shot 3D sounds from noisy working machines. Audio
// only; nothing here touches game state.

#pragma once

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    // Called every tick next to VehicleSoundsUpdate.
    void updateMachineSounds(const GameState_t& gameState);
} // namespace OpenRCT2::Factory

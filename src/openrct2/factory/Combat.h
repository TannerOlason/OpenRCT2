/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The combat stub (ADR 0013): machine and ride health, damage, Threats that walk
// straight at machines, and turrets. Deliberately minimal; scripts build anything smarter on top.

#pragma once

#include "../object/ObjectTypes.h"
#include "../world/Location.hpp"
#include "FactoryRecords.h"

#include <cstdint>
#include <optional>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    class FactoryPrototypeObject;

    enum class DamageTarget : uint8_t
    {
        machine, // id = machine record
        ride,    // id = RideId
        threat,  // id = threat record
        count,
    };

    // Rides take this much damage before breaking down (a safety cut-out); the damage then resets.
    constexpr uint16_t kRideHealth = 1000;

    struct DamageResult
    {
        bool hit{};        // the target exists and can be damaged
        uint16_t health{}; // what is left (rides: kRideHealth minus damage)
        bool destroyed{};  // this hit destroyed the machine or threat, or broke the ride down
    };

    DamageResult applyDamage(GameState_t& gameState, DamageTarget target, uint32_t id, uint16_t amount, uint8_t damageType);

    // Adds a threat of prototype `entry` at `pos` (world units); it picks the nearest destructible machine as target.
    std::optional<RecordId> spawnThreat(GameState_t& gameState, ObjectEntryIndex entry, const CoordsXYZ& pos);
    void despawnThreat(GameState_t& gameState, RecordId id);

    // Simulation phases, called from Factory::update.
    void updateThreats(GameState_t& gameState);
    void updateTurret(GameState_t& gameState, MachineRecord& machine, const FactoryPrototypeObject& proto);

    // Turret ammo: the prototype's ammo item, up to one stack.
    bool turretAcceptsAmmo(const MachineRecord& machine, const FactoryPrototypeObject& proto, ObjectEntryIndex item);

    // Machine health for display: {health, maxHealth}; maxHealth 0 = indestructible.
    std::pair<uint16_t, uint16_t> machineHealth(const MachineRecord& machine);
} // namespace OpenRCT2::Factory

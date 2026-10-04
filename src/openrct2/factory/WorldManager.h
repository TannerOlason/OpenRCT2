/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Several worlds (ADR 0015): extra GameState_t instances that are swapped in one at a
// time and ticked in lockstep. Company state (money, research, date, objectives, market) follows the active world.

#pragma once

#include "../world/Location.hpp"

#include <cstdint>
#include <string>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::GameActions
{
    class GameAction;
}

namespace OpenRCT2
{
    class OrcaStream;
}

namespace OpenRCT2::Factory::Worlds
{
    using WorldId = uint8_t;
    constexpr WorldId kPrimaryWorld = 0;
    constexpr WorldId kMaxWorlds = 8;

    size_t count();
    WorldId active();
    WorldId viewed();

    // The state of any world, active or not (inactive worlds may only be read or written as plain data: their tile
    // index, map animations and ride-use history are stashed until they are activated).
    GameState_t& state(WorldId id);

    // Swaps `id` in: per-world caches are stashed and restored, company state moves across. No-op if already active.
    void activate(WorldId id);

    // Activates a world for the duration of a scope.
    class Scope
    {
        WorldId _previous;

    public:
        explicit Scope(WorldId id);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    };

    // Adds an empty, fully owned, flat world of the given size; returns its id (or kMaxWorlds when full).
    WorldId create(const TileCoordsXY& mapSize);

    // Drops every world but the active one, which becomes world 0 (new game, park load).
    void adoptActiveAsPrimary();

    // gameStateUpdateLogic hooks. tickAll runs one tick of every world in ascending order (each through a nested
    // gameStateUpdateLogic) and returns true; inside that loop it returns false and the caller ticks normally.
    bool tickAll();
    // False while ticking a secondary world: once-per-tick work (network, replay, date, research, action queue, hooks)
    // runs in world 0's pass only.
    bool isPrimaryPass();
    // True when the active world is the one on screen (audio, provisional ghosts).
    bool isViewedActive();

    // The world shown on screen; switching closes windows that refer to the old world.
    void setViewed(WorldId id);

    // Saving: world 0 is always the top-level park; the others are nested park files in fork chunk 0x44 (worlds).
    // ParkFileExporter saves saveTarget(): world 0, or the world being written while nested.
    WorldId saveTarget();
    void writeWorldsChunk(OrcaStream& os);
    // Reading keeps the nested parks until the outer import is complete (finishImport, at the end of
    // ParkFile::Import), then imports each into a new world.
    void readWorldsChunk(OrcaStream& os);
    void finishImport();

    // Game actions carry their world in CommandFlags bits 16-23 (0 = world 0, so upstream actions are unchanged).
    WorldId actionWorld(const GameActions::GameAction& action);
    // Called for actions issued locally: an action without a world targets the active world.
    void stampActionWorld(const GameActions::GameAction& action);
} // namespace OpenRCT2::Factory::Worlds

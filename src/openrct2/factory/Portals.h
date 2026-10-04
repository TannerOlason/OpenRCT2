/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Portal Terminals (ADR 0016): guests who finish a ride on one step out of the
// matching terminal in the next world, keeping their mood, needs and money.

#pragma once

#include "../object/ObjectTypes.h"

#include <cstdint>
#include <string>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
    struct Guest;
    struct Ride;
} // namespace OpenRCT2

namespace OpenRCT2::Factory
{
    // A guest on the way to another world: the world-neutral part of a guest (ids into the old world are dropped).
    struct GuestTransfer
    {
        uint8_t toWorld{};
        uint16_t terminalIndex{}; // which portal terminal (in ascending ride id order) the guest used
        uint8_t happiness{};
        uint8_t happinessTarget{};
        uint8_t energy{};
        uint8_t energyTarget{};
        uint8_t hunger{};
        uint8_t thirst{};
        uint8_t toilet{};
        uint8_t nausea{};
        uint8_t nauseaTarget{};
        int64_t cashInPocket{};
        int64_t cashSpent{};
        std::string name;

        template<typename V>
        void visit(V& v)
        {
            v(toWorld);
            v(terminalIndex);
            v(happiness);
            v(happinessTarget);
            v(energy);
            v(energyTarget);
            v(hunger);
            v(thirst);
            v(toilet);
            v(nausea);
            v(nauseaTarget);
            v(cashInPocket);
            v(cashSpent);
            v(name);
        }
    };

    struct GuestDeparture
    {
        uint8_t world{};
        uint16_t guest{}; // EntityId in that world

        template<typename V>
        void visit(V& v)
        {
            v(world);
            v(guest);
        }
    };

    // Company state (moves with the active world).
    struct PortalState
    {
        std::vector<GuestTransfer> arrivals;
        std::vector<GuestDeparture> departures;

        bool isEmpty() const
        {
            return arrivals.empty() && departures.empty();
        }
        template<typename V>
        void visit(V& v)
        {
            v.vec(arrivals, [](GuestTransfer& entry, auto& vv) { entry.visit(vv); });
            v.vec(departures, [](GuestDeparture& entry, auto& vv) { entry.visit(vv); });
        }
    };

    bool isPortalTerminal(const Ride& ride);

    // Guest::onExitRide hook: queues the guest's transfer to the next world when there is a terminal to arrive at.
    void onGuestExitRide(GameState_t& gameState, Guest& guest, const Ride& ride);

    // After the active world's peeps have moved: departing guests leave it and arrivals step out of its terminals.
    void updatePortals(GameState_t& gameState);
} // namespace OpenRCT2::Factory

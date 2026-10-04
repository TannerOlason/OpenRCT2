/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Portals.h"

#include "../GameState.h"
#include "../entity/EntityRegistry.h"
#include "../entity/Guest.h"
#include "../ride/Ride.h"
#include "../ride/RideManager.hpp"
#include "../world/Footpath.h"
#include "../world/Map.h"
#include "../world/tile_element/PathElement.h"
#include "FactoryState.h"
#include "WorldManager.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    namespace
    {
        // Portal terminals of a world in ascending ride id order.
        std::vector<const Ride*> terminalsOf(GameState_t& gameState)
        {
            std::vector<const Ride*> terminals;
            for (const auto& ride : gameState.rides)
                if (!ride.id.IsNull() && ride.type == RIDE_TYPE_PORTAL_TERMINAL)
                    terminals.push_back(&ride);
            return terminals;
        }

        // A footpath tile beside the terminal's first exit, where arriving guests appear.
        std::optional<CoordsXYZ> arrivalTile(const Ride& terminal)
        {
            for (const auto& station : terminal.getStations())
            {
                if (station.exit.isNull())
                    continue;
                const auto exit = station.exit.toCoordsXYZD();
                for (Direction d = 0; d < 4; d++)
                {
                    const CoordsXY beside = CoordsXY(exit) + CoordsDirectionDelta[d];
                    if (!MapIsLocationValid(beside))
                        continue;
                    for (int32_t dz : { 0, 16, -16 })
                    {
                        auto* path = MapGetPathElementAt(TileCoordsXYZ{ CoordsXYZ{ beside, exit.z + dz } });
                        if (path != nullptr && !path->isQueue())
                            return CoordsXYZ{ beside.x + 16, beside.y + 16, path->getBaseZ() };
                    }
                }
            }
            return std::nullopt;
        }
    } // namespace

    bool isPortalTerminal(const Ride& ride)
    {
        return ride.type == RIDE_TYPE_PORTAL_TERMINAL;
    }

    void onGuestExitRide(GameState_t& gameState, Guest& guest, const Ride& ride)
    {
        if (!isPortalTerminal(ride) || Worlds::count() <= 1)
            return;
        const auto here = Worlds::active();
        const auto toWorld = static_cast<uint8_t>((here + 1) % Worlds::count());
        // The terminal's index among this world's terminals picks the terminal in the next world (wrapping).
        const auto terminals = terminalsOf(gameState);
        const auto it = std::find(terminals.begin(), terminals.end(), &ride);
        if (it == terminals.end())
            return;
        auto& destination = Worlds::state(toWorld);
        const auto targets = terminalsOf(destination);
        if (targets.empty())
            return;
        auto& portals = gameState.factory.portals;
        for (const auto& departure : portals.departures)
            if (departure.world == here && departure.guest == guest.id.ToUnderlying())
                return; // already leaving

        GuestTransfer transfer;
        transfer.toWorld = toWorld;
        transfer.terminalIndex = static_cast<uint16_t>(it - terminals.begin());
        transfer.happiness = guest.happiness;
        transfer.happinessTarget = guest.happinessTarget;
        transfer.energy = guest.energy;
        transfer.energyTarget = guest.energyTarget;
        transfer.hunger = guest.hunger;
        transfer.thirst = guest.thirst;
        transfer.toilet = guest.toilet;
        transfer.nausea = guest.nausea;
        transfer.nauseaTarget = guest.nauseaTarget;
        transfer.cashInPocket = guest.cashInPocket;
        transfer.cashSpent = guest.cashSpent;
        transfer.name = guest.getName(); // the guest keeps their name in the next world
        portals.arrivals.push_back(std::move(transfer));
        portals.departures.push_back(GuestDeparture{ here, guest.id.ToUnderlying() });
    }

    void updatePortals(GameState_t& gameState)
    {
        auto& portals = gameState.factory.portals;
        if (portals.isEmpty())
            return;
        const auto here = Worlds::active();

        // Departures: the guests leave this world (the count of guests in the park follows).
        std::erase_if(portals.departures, [&](const GuestDeparture& departure) {
            if (departure.world != here)
                return false;
            if (auto* guest = gameState.entities.tryGetEntity<Guest>(EntityId::FromUnderlying(departure.guest)))
                guest->remove();
            return true;
        });

        // Arrivals: step out beside the matching terminal's exit.
        const auto terminals = terminalsOf(gameState);
        std::erase_if(portals.arrivals, [&](const GuestTransfer& transfer) {
            if (transfer.toWorld != here)
                return false;
            if (terminals.empty())
                return true; // the terminal was demolished on the way: the guest goes home
            const auto* terminal = terminals[transfer.terminalIndex % terminals.size()];
            const auto tile = arrivalTile(*terminal);
            if (!tile)
                return true;
            auto* guest = Guest::generate(*tile);
            if (guest == nullptr)
                return false; // no free entity: try again next tick
            guest->outsideOfPark = false;
            IncrementGuestsInPark();
            guest->setState(PeepState::walking);
            guest->happiness = transfer.happiness;
            guest->happinessTarget = transfer.happinessTarget;
            guest->energy = transfer.energy;
            guest->energyTarget = transfer.energyTarget;
            guest->hunger = transfer.hunger;
            guest->thirst = transfer.thirst;
            guest->toilet = transfer.toilet;
            guest->nausea = transfer.nausea;
            guest->nauseaTarget = transfer.nauseaTarget;
            guest->cashInPocket = transfer.cashInPocket;
            guest->cashSpent = transfer.cashSpent;
            if (!transfer.name.empty())
                guest->setName(transfer.name);
            return true;
        });
    }
} // namespace OpenRCT2::Factory

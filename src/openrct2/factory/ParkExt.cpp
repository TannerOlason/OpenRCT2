/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "ParkExt.h"

#include "../GameState.h"
#include "../entity/Guest.h"
#include "../ride/Ride.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    static auto lowerBound(const std::vector<GuestExt>& guests, uint16_t id)
    {
        return std::lower_bound(
            guests.begin(), guests.end(), id, [](const GuestExt& guest, uint16_t value) { return guest.id < value; });
    }

    uint8_t ParkExt::guestFlags(uint16_t id) const
    {
        auto it = lowerBound(guests, id);
        return it != guests.end() && it->id == id ? it->flags : 0;
    }

    void ParkExt::addGuestFlags(uint16_t id, uint8_t flags)
    {
        auto it = lowerBound(guests, id);
        if (it != guests.end() && it->id == id)
        {
            guests[static_cast<size_t>(it - guests.begin())].flags |= flags;
            return;
        }
        guests.insert(it, GuestExt{ id, flags });
    }

    size_t ParkExt::countGuestsWith(uint8_t flags) const
    {
        return static_cast<size_t>(
            std::count_if(guests.begin(), guests.end(), [&](const GuestExt& guest) { return (guest.flags & flags) == flags; }));
    }

    uint16_t ParkExt::rideDamage(uint16_t id) const
    {
        auto it = std::lower_bound(rides.begin(), rides.end(), id, [](const RideExt& ride, uint16_t v) { return ride.id < v; });
        return it != rides.end() && it->id == id ? it->damage : 0;
    }

    void ParkExt::setRideDamage(uint16_t id, uint16_t damage)
    {
        auto it = std::lower_bound(rides.begin(), rides.end(), id, [](const RideExt& ride, uint16_t v) { return ride.id < v; });
        const bool found = it != rides.end() && it->id == id;
        if (damage == 0)
        {
            if (found)
                rides.erase(it);
            return;
        }
        if (found)
            it->damage = damage;
        else
            rides.insert(it, RideExt{ id, damage });
    }

    void pruneParkExt(GameState_t& gameState)
    {
        auto& guests = gameState.factory.parkExt.guests;
        std::erase_if(guests, [&](const GuestExt& guest) {
            return gameState.entities.tryGetEntity<Guest>(EntityId::FromUnderlying(guest.id)) == nullptr;
        });
        std::erase_if(gameState.factory.parkExt.rides, [](const RideExt& ride) {
            return GetRide(RideId::FromUnderlying(ride.id)) == nullptr;
        });
    }
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Park-side fork fields kept as side tables (ADR 0008), saved in fork chunk 0x45.

#pragma once

#include <cstdint>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    // Guest flags.
    constexpr uint8_t kGuestTouredFactory = 1 << 0; // rode a Factory Tour or stopped to watch working machinery

    struct GuestExt
    {
        uint16_t id{}; // EntityId
        uint8_t flags{};

        template<typename V>
        void visit(V& v)
        {
            v(id);
            v(flags);
        }
    };

    struct RideExt
    {
        uint16_t id{};     // RideId
        uint16_t damage{}; // towards Factory::kRideHealth

        template<typename V>
        void visit(V& v)
        {
            v(id);
            v(damage);
        }
    };

    /**
     * Fork fields that belong to upstream objects (guests now; rides and scenario options later), keyed by their
     * upstream id and kept sorted so iteration and serialisation are deterministic. Entries for guests that left are
     * pruned periodically.
     */
    struct ParkExt
    {
        std::vector<GuestExt> guests;
        std::vector<RideExt> rides; // sorted by ride id; only rides with damage
        // Scenario options (Factory::ConstructionMode, ShopStockMode); 0 is upstream behaviour.
        uint8_t constructionMode{};
        uint8_t shopStockMode{};
        // Guests ever marked kGuestTouredFactory (the guest entries are pruned when they leave).
        uint32_t guestsToured{};

        bool isEmpty() const
        {
            return guests.empty() && rides.empty() && constructionMode == 0 && shopStockMode == 0 && guestsToured == 0;
        }
        void reset()
        {
            guests.clear();
            rides.clear();
            constructionMode = 0;
            shopStockMode = 0;
            guestsToured = 0;
        }
        uint8_t guestFlags(uint16_t id) const;
        void addGuestFlags(uint16_t id, uint8_t flags);
        size_t countGuestsWith(uint8_t flags) const;
        uint16_t rideDamage(uint16_t id) const;
        void setRideDamage(uint16_t id, uint16_t damage);

        template<typename V>
        void visit(V& v)
        {
            v.vec(guests, [](GuestExt& guest, auto& vv) { guest.visit(vv); });
            v(constructionMode);
            v(shopStockMode);
            v(guestsToured);
            v.vec(rides, [](RideExt& ride, auto& vv) { ride.visit(vv); });
        }
    };

    // Drops entries whose entity is no longer a guest. Called every kParkExtPruneTicks from the factory update.
    constexpr uint32_t kParkExtPruneTicks = 256;
    void pruneParkExt(GameState_t& gameState);
} // namespace OpenRCT2::Factory

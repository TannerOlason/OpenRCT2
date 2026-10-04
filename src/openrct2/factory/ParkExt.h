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

    /**
     * Fork fields that belong to upstream objects (guests now; rides and scenario options later), keyed by their
     * upstream id and kept sorted so iteration and serialisation are deterministic. Entries for guests that left are
     * pruned periodically.
     */
    struct ParkExt
    {
        std::vector<GuestExt> guests;

        bool isEmpty() const
        {
            return guests.empty();
        }
        void reset()
        {
            guests.clear();
        }
        uint8_t guestFlags(uint16_t id) const;
        void addGuestFlags(uint16_t id, uint8_t flags);
        size_t countGuestsWith(uint8_t flags) const;

        template<typename V>
        void visit(V& v)
        {
            v.vec(guests, [](GuestExt& guest, auto& vv) { guest.visit(vv); });
        }
    };

    // Drops entries whose entity is no longer a guest. Called every kParkExtPruneTicks from the factory update.
    constexpr uint32_t kParkExtPruneTicks = 256;
    void pruneParkExt(GameState_t& gameState);
} // namespace OpenRCT2::Factory

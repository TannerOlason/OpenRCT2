/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. How factories near a ride's track feed its ratings (bonusFactoryProximity).
//
// While the rating state machine walks a ride's track, each piece scans the 5x5 tiles around it for factory elements.
// The totals live in FactoryState::rideProximity keyed by ride id (rating can pause mid-walk across ticks, saves and
// network joins, so they are game state), and are turned into a ratings bonus and dropped when the ride is calculated.
// Only rides whose ride type has the modifier are tracked, so rides elsewhere never touch factory state.

#pragma once

#include "../world/Location.hpp"

#include <cstdint>

namespace OpenRCT2
{
    struct GameState_t;
    struct Ride;
    struct RatingsModifier;
} // namespace OpenRCT2

namespace OpenRCT2::Factory
{
    struct RideProximityStats
    {
        uint16_t pieces{};       // track pieces scanned
        uint16_t factoryTiles{}; // factory elements found around them (a tile near two pieces counts twice)
        uint16_t machines{};     // machine elements among them
        uint16_t working{};      // ... whose machine was working
        uint32_t kinds{};        // bit per element subtype (0-15) and per machine kind (16+)

        template<typename V>
        void visit(V& v)
        {
            v(pieces);
            v(factoryTiles);
            v(machines);
            v(working);
            v(kinds);
        }
    };

    struct RideProximityEntry
    {
        uint16_t ride{};
        RideProximityStats stats;

        template<typename V>
        void visit(V& v)
        {
            v(ride);
            stats.visit(v);
        }
    };

    struct RatingsBonus
    {
        int32_t excitement{};
        int32_t intensity{};
        int32_t nausea{};
    };

    // Up to `modifier.excitement` for density (six factory elements per piece saturates it), a quarter of that again
    // spread over variety (eight kinds) and activity (working machines); intensity and nausea scale with density.
    RatingsBonus factoryProximityScore(const RideProximityStats& stats, const RatingsModifier& modifier);

    bool rideHasFactoryProximity(const Ride& ride);
    // Rating hooks called from RideRatings.cpp; no-ops unless the ride's type has the modifier.
    void rideRatingsBegin(GameState_t& gameState, const Ride& ride);
    void rideRatingsScorePiece(GameState_t& gameState, const Ride& ride, const CoordsXYZ& trackLoc);
    RatingsBonus rideRatingsTakeBonus(GameState_t& gameState, const Ride& ride, const RatingsModifier& modifier);
    // The stats scanned so far for a ride (tests, UI), or nullptr.
    const RideProximityStats* rideProximityStats(const GameState_t& gameState, uint16_t ride);
} // namespace OpenRCT2::Factory

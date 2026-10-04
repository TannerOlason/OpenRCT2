/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. What guests think of the factory around them.

#pragma once

#include "../world/Location.hpp"

#include <cstdint>

namespace OpenRCT2
{
    struct GameState_t;
    struct FactoryElement;
    struct Ride;
    enum class PeepThoughtType : uint8_t;
} // namespace OpenRCT2

namespace OpenRCT2::Factory
{
    // A guest's cell at or above this smells the factory (see PollutionLayer for units).
    constexpr uint32_t kSmellPollution = 4000;
    // Summed noise / (1 + distance) of working machines within three tiles at or above this is too loud.
    constexpr uint32_t kTooNoisy = 40;
    // Within four tiles: at least this many machines, half of them working, of two kinds or more, impress.
    constexpr uint32_t kImpressiveMachines = 4;

    struct GuestFactoryVerdict
    {
        PeepThoughtType thought;
        int16_t happiness{}; // added to the guest's happiness target
        uint8_t nausea{};    // added to the guest's nausea target
        bool hasThought{};
    };

    /**
     * Called where a walking guest assesses their surroundings. Pollution beats noise beats an impressive factory;
     * no thought means the guest falls back to upstream's scenery, fountain and music thoughts. Parks without a factory
     * always get no thought, so they behave exactly as upstream.
     */
    GuestFactoryVerdict assessGuestSurroundings(const GameState_t& gameState, const CoordsXYZ& guestLoc);

    // Seat bit a watching guest gets when the thing watched is factory machinery (bit 0 is upstream's "new ride").
    constexpr uint8_t kWatchingFactorySeatBit = 0x04;
    // True for a working machine whose prototype is photogenic: guests may stop beside it to watch.
    bool isWatchableMachine(const GameState_t& gameState, const FactoryElement& element);
    // Records that a guest toured the factory (watched machines, or rode a Factory Tour).
    void markGuestToured(GameState_t& gameState, uint16_t guestId);
    bool isFactoryTourRide(const Ride& ride);
} // namespace OpenRCT2::Factory

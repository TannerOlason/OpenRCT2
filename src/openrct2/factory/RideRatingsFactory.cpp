/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "RideRatingsFactory.h"

#include "../GameState.h"
#include "../ride/Ride.h"
#include "../ride/RideData.h"
#include "../world/Map.h"
#include "../world/TileElementsView.h"
#include "../world/tile_element/FactoryElement.h"
#include "FactoryState.h"

#include <algorithm>
#include <bit>

namespace OpenRCT2::Factory
{
    RatingsBonus factoryProximityScore(const RideProximityStats& stats, const RatingsModifier& modifier)
    {
        RatingsBonus bonus;
        if (stats.pieces == 0 || stats.factoryTiles == 0)
            return bonus;
        const int64_t densityQ16 = std::min<int64_t>(
            65536, (int64_t{ stats.factoryTiles } << 16) / (int64_t{ stats.pieces } * 6));
        const int32_t variety = std::min(std::popcount(stats.kinds), 8);
        const int64_t activityQ16 = stats.machines > 0 ? (int64_t{ stats.working } << 16) / stats.machines : 0;
        bonus.excitement = static_cast<int32_t>((modifier.excitement * densityQ16) >> 16) + modifier.excitement * variety / 64
            + static_cast<int32_t>((modifier.excitement * activityQ16) >> 19);
        bonus.intensity = static_cast<int32_t>((modifier.intensity * densityQ16) >> 16);
        bonus.nausea = static_cast<int32_t>((modifier.nausea * densityQ16) >> 16);
        return bonus;
    }

    bool rideHasFactoryProximity(const Ride& ride)
    {
        for (const auto& modifier : ride.getRideTypeDescriptor().RatingsData.Modifiers)
        {
            if (modifier.type == RatingsModifierType::bonusFactoryProximity)
                return true;
        }
        return false;
    }

    static std::vector<RideProximityEntry>::iterator findEntry(State& state, uint16_t ride)
    {
        return std::lower_bound(
            state.rideProximity.begin(), state.rideProximity.end(), ride,
            [](const RideProximityEntry& entry, uint16_t id) { return entry.ride < id; });
    }

    void rideRatingsBegin(GameState_t& gameState, const Ride& ride)
    {
        if (!rideHasFactoryProximity(ride))
            return;
        auto& state = gameState.factory;
        // Rides that were demolished, or never finished a calculation, leave entries behind: drop them.
        std::erase_if(state.rideProximity, [](const RideProximityEntry& entry) {
            return GetRide(RideId::FromUnderlying(entry.ride)) == nullptr;
        });
        const auto id = ride.id.ToUnderlying();
        auto it = findEntry(state, id);
        if (it != state.rideProximity.end() && it->ride == id)
            it->stats = RideProximityStats{};
        else if (!state.isEmpty())
            state.rideProximity.insert(it, RideProximityEntry{ id, {} }); // kept sorted by ride id
    }

    void rideRatingsScorePiece(GameState_t& gameState, const Ride& ride, const CoordsXYZ& trackLoc)
    {
        auto& state = gameState.factory;
        if (state.rideProximity.empty())
            return;
        const auto id = ride.id.ToUnderlying();
        auto it = findEntry(state, id);
        if (it == state.rideProximity.end() || it->ride != id)
            return;
        auto& stats = it->stats;
        stats.pieces = static_cast<uint16_t>(std::min<int32_t>(stats.pieces + 1, 0xFFFF));
        for (int32_t dy = -2; dy <= 2; dy++)
        {
            for (int32_t dx = -2; dx <= 2; dx++)
            {
                const CoordsXY tile{ trackLoc.x + dx * kCoordsXYStep, trackLoc.y + dy * kCoordsXYStep };
                if (!MapIsLocationValid(tile))
                    continue;
                for (auto* element : TileElementsView<FactoryElement>(tile))
                {
                    // Machinery within about three storeys of the track counts.
                    if (element->isGhost() || std::abs(element->getBaseZ() - trackLoc.z) > 24 * kCoordsZStep)
                        continue;
                    stats.factoryTiles = static_cast<uint16_t>(std::min<int32_t>(stats.factoryTiles + 1, 0xFFFF));
                    stats.kinds |= 1u << (static_cast<uint32_t>(element->getSubtype()) & 15);
                    if (element->getSubtype() != FactoryElementSubtype::machine || !element->hasRecord())
                        continue;
                    const auto* machine = state.machines.get(element->getRecordId());
                    if (machine == nullptr)
                        continue;
                    stats.machines = static_cast<uint16_t>(std::min<int32_t>(stats.machines + 1, 0xFFFF));
                    stats.kinds |= 1u << (16 + (machine->kind & 15));
                    if (machine->isWorking())
                        stats.working = static_cast<uint16_t>(std::min<int32_t>(stats.working + 1, 0xFFFF));
                }
            }
        }
    }

    RatingsBonus rideRatingsTakeBonus(GameState_t& gameState, const Ride& ride, const RatingsModifier& modifier)
    {
        auto& state = gameState.factory;
        const auto id = ride.id.ToUnderlying();
        auto it = findEntry(state, id);
        if (it == state.rideProximity.end() || it->ride != id)
            return {};
        const auto bonus = factoryProximityScore(it->stats, modifier);
        state.rideProximity.erase(it);
        return bonus;
    }

    const RideProximityStats* rideProximityStats(const GameState_t& gameState, uint16_t ride)
    {
        const auto& entries = gameState.factory.rideProximity;
        auto it = std::lower_bound(
            entries.begin(), entries.end(), ride, [](const RideProximityEntry& entry, uint16_t id) { return entry.ride < id; });
        return it != entries.end() && it->ride == ride ? &it->stats : nullptr;
    }
} // namespace OpenRCT2::Factory

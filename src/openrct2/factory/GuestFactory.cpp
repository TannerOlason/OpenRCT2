/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "GuestFactory.h"

#include "../GameState.h"
#include "../entity/EntityList.h"
#include "../entity/Guest.h"
#include "../ride/Ride.h"
#include "../world/Map.h"
#include "../world/ParkData.h"
#include "../world/TileElementsView.h"
#include "../world/tile_element/FactoryElement.h"
#include "FactoryState.h"
#include "FactoryTopology.h"

#include <algorithm>
#include <bit>
#include <cstdlib>

namespace OpenRCT2::Factory
{
    GuestFactoryVerdict assessGuestSurroundings(const GameState_t& gameState, const CoordsXYZ& guestLoc)
    {
        GuestFactoryVerdict verdict{ PeepThoughtType::none };
        const auto& state = gameState.factory;
        if (state.isEmpty())
            return verdict;

        const TileCoordsXY guestTile{ CoordsXY(guestLoc) };
        if (state.pollution.at(guestTile) >= kSmellPollution)
        {
            verdict = { PeepThoughtType::factorySmell, -20, 12, true };
            return verdict;
        }

        uint32_t noise = 0;
        uint32_t machines = 0;
        uint32_t working = 0;
        uint32_t kinds = 0;
        for (int32_t dy = -4; dy <= 4; dy++)
        {
            for (int32_t dx = -4; dx <= 4; dx++)
            {
                const CoordsXY tile{ guestLoc.x + dx * kCoordsXYStep, guestLoc.y + dy * kCoordsXYStep };
                if (!MapIsLocationValid(tile))
                    continue;
                for (const auto* element : TileElementsView<FactoryElement>(tile))
                {
                    if (element->isGhost() || element->getSubtype() != FactoryElementSubtype::machine || !element->hasRecord()
                        || std::abs(element->getBaseZ() - guestLoc.z) > 48)
                        continue;
                    // Count each machine once, at its origin tile.
                    if (!(element->getFactoryFlags() & FACTORY_ELEMENT_FLAG_ORIGIN))
                        continue;
                    const auto* machine = state.machines.get(element->getRecordId());
                    const auto* proto = getPrototype(*element);
                    if (machine == nullptr || proto == nullptr)
                        continue;
                    machines++;
                    kinds |= 1u << (machine->kind & 31);
                    if (!machine->isWorking())
                        continue;
                    working++;
                    const int32_t distance = std::max(std::abs(dx), std::abs(dy));
                    if (distance <= 3)
                        noise += proto->getMachine().noise / static_cast<uint32_t>(1 + distance);
                }
            }
        }
        if (noise >= kTooNoisy)
            verdict = { PeepThoughtType::factoryNoise, -10, 0, true };
        else if (machines >= kImpressiveMachines && working * 2 >= machines && std::popcount(kinds) >= 2)
            verdict = { PeepThoughtType::factoryImpressive, 30, 0, true };
        return verdict;
    }

    bool isWatchableMachine(const GameState_t& gameState, const FactoryElement& element)
    {
        if (element.isGhost() || element.getSubtype() != FactoryElementSubtype::machine || !element.hasRecord())
            return false;
        const auto* proto = getPrototype(element);
        const auto* machine = gameState.factory.machines.get(element.getRecordId());
        return proto != nullptr && machine != nullptr && proto->getMachine().photogenic && machine->isWorking();
    }

    void markGuestToured(GameState_t& gameState, uint16_t guestId)
    {
        auto& ext = gameState.factory.parkExt;
        if (!(ext.guestFlags(guestId) & kGuestTouredFactory))
            ext.guestsToured++;
        ext.addGuestFlags(guestId, kGuestTouredFactory);
    }

    bool isFactoryTourRide(const Ride& ride)
    {
        return ride.type == RIDE_TYPE_FACTORY_TOUR;
    }

    int32_t parkRatingAdjustment(const Park::ParkData& park, const GameState_t& gameState)
    {
        if (!park.flags.has(ParkFlag::factoryAffectsRating))
            return 0;
        const auto& state = gameState.factory;
        int32_t result = 0;

        uint64_t pollution = 0;
        uint32_t guests = 0;
        for (auto* guest : EntityList<Guest>())
        {
            if (guest->outsideOfPark)
                continue;
            guests++;
            pollution += std::min(state.pollution.at(TileCoordsXY{ guest->getLocation() }), kRatingPollutionFull);
        }
        if (guests > 0)
            result -= static_cast<int32_t>(pollution * 150 / (uint64_t{ guests } * kRatingPollutionFull));

        uint32_t machines = 0;
        uint32_t working = 0;
        state.machines.forEach([&](RecordId, const MachineRecord& machine) {
            machines++;
            working += machine.isWorking() ? 1 : 0;
        });
        if (machines > 0)
            result += static_cast<int32_t>(working * 50 / machines) - 25;
        return result;
    }
} // namespace OpenRCT2::Factory

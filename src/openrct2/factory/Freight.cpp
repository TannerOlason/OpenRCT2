/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Freight.h"

#include "../GameState.h"
#include "../entity/EntityRegistry.h"
#include "../ride/Ride.h"
#include "../ride/RideManager.hpp"
#include "../ride/Vehicle.h"
#include "../world/Map.h"
#include "../world/tile_element/TrackElement.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryTopology.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    namespace
    {
        auto lowerBound(const std::vector<FreightCargo>& cargo, uint16_t vehicle)
        {
            return std::lower_bound(
                cargo.begin(), cargo.end(), vehicle, [](const FreightCargo& entry, uint16_t v) { return entry.vehicle < v; });
        }

        // The freight container (loader or unloader) on `tile` at about the track's height, if any.
        ContainerRecord* freightContainerAt(State& state, const CoordsXYZ& tile, bool& loader)
        {
            if (!MapIsLocationValid(tile))
                return nullptr;
            auto* element = findFactoryElement(tile);
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::container
                || std::abs(element->getBaseZ() - tile.z) > 16)
                return nullptr;
            auto* proto = getPrototype(*element);
            if (proto == nullptr)
                return nullptr;
            const auto& props = proto->getContainer();
            if (!props.freightLoader && !props.freightUnloader)
                return nullptr;
            loader = props.freightLoader;
            return state.containers.get(element->getRecordId());
        }

        uint16_t stackSizeOf(ObjectEntryIndex item)
        {
            auto* proto = getPrototype(item);
            return proto != nullptr ? proto->getItem().stackSize : 1;
        }

        void tradeWithCar(State& state, uint16_t carId, ContainerRecord& container, bool loader)
        {
            auto& cargo = state.freight.get(carId);
            if (loader)
            {
                // One item a tick into the car: any item while it is empty, then only more of the same.
                for (auto& slot : container.slots)
                {
                    if (slot.isEmpty() || cargo.count >= kFreightCarCapacity)
                        continue;
                    if (cargo.count > 0 && cargo.item != slot.item)
                        continue;
                    cargo.item = slot.item;
                    cargo.count++;
                    if (--slot.count == 0)
                        slot.item = kObjectEntryIndexNull;
                    return;
                }
                return;
            }
            if (cargo.count == 0)
                return;
            if (containerInsert(container, cargo.item, stackSizeOf(cargo.item)))
            {
                if (--cargo.count == 0)
                    cargo.item = kObjectEntryIndexNull;
            }
        }
    } // namespace

    const FreightCargo* FreightState::find(uint16_t vehicle) const
    {
        auto it = lowerBound(cargo, vehicle);
        return it != cargo.end() && it->vehicle == vehicle ? &*it : nullptr;
    }

    FreightCargo& FreightState::get(uint16_t vehicle)
    {
        auto it = lowerBound(cargo, vehicle);
        if (it == cargo.end() || it->vehicle != vehicle)
            it = cargo.insert(it, FreightCargo{ vehicle, kObjectEntryIndexNull, 0 });
        return cargo[static_cast<size_t>(it - cargo.begin())];
    }

    void FreightState::prune(GameState_t& gameState)
    {
        std::erase_if(cargo, [&](const FreightCargo& entry) {
            if (entry.count == 0)
                return true;
            auto* vehicle = gameState.entities.tryGetEntity<Vehicle>(EntityId::FromUnderlying(entry.vehicle));
            auto* ride = vehicle != nullptr ? GetRide(vehicle->ride) : nullptr;
            return ride == nullptr || ride->type != RIDE_TYPE_FREIGHT_RAILWAY;
        });
    }

    void updateFreight(GameState_t& gameState)
    {
        auto& state = gameState.factory;
        if (gameState.currentTicks % 256 == 0)
            state.freight.prune(gameState);
        for (auto& ride : RideManager(gameState))
        {
            if (ride.type != RIDE_TYPE_FREIGHT_RAILWAY || ride.status == RideStatus::closed)
                continue;
            // A train trades while it stands still; each of its cars on station track trades with the containers
            // beside it. (Upstream's trainAtStation only tracks passenger boarding.)
            for (uint8_t train = 0; train < ride.numTrains && train < std::size(ride.vehicles); train++)
            {
                auto* car = gameState.entities.tryGetEntity<Vehicle>(ride.vehicles[train]);
                if (car == nullptr || car->velocity != 0)
                    continue;
                for (; car != nullptr; car = gameState.entities.tryGetEntity<Vehicle>(car->next_vehicle_on_train))
                {
                    auto* track = MapGetTrackElementAt(car->TrackLocation);
                    if (track == nullptr || !track->isStation())
                        continue;
                    const auto direction = track->getDirection();
                    for (const auto side :
                         { static_cast<Direction>((direction + 1) & 3), static_cast<Direction>((direction + 3) & 3) })
                    {
                        const CoordsXYZ beside{ CoordsXY(car->TrackLocation) + CoordsDirectionDelta[side],
                                                car->TrackLocation.z };
                        bool loader = false;
                        if (auto* container = freightContainerAt(state, beside, loader))
                            tradeWithCar(state, car->id.ToUnderlying(), *container, loader);
                    }
                }
            }
        }
    }
} // namespace OpenRCT2::Factory

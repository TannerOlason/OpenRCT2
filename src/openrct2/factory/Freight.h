/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Freight: cargo carried by freight railway cars, loaded and unloaded by freight
// containers beside the station track where a car stands (ADR 0014).

#pragma once

#include "../object/ObjectTypes.h"

#include <cstdint>
#include <vector>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    constexpr uint16_t kFreightCarCapacity = 200; // items of one kind per car

    struct FreightCargo
    {
        uint16_t vehicle{}; // car EntityId
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint16_t count{};

        template<typename V>
        void visit(V& v)
        {
            v(vehicle);
            v(item);
            v(count);
        }
    };

    // Cargo per car, sorted by vehicle id; cars that leave the park (or the freight railway) are pruned.
    struct FreightState
    {
        std::vector<FreightCargo> cargo;

        const FreightCargo* find(uint16_t vehicle) const;
        FreightCargo& get(uint16_t vehicle);
        void prune(GameState_t& gameState);

        template<typename V>
        void visit(V& v)
        {
            v.vec(cargo, [](FreightCargo& entry, auto& vv) { entry.visit(vv); });
        }
    };

    // One tick: every freight train standing at a station trades with the freight containers beside its cars.
    void updateFreight(GameState_t& gameState);
} // namespace OpenRCT2::Factory

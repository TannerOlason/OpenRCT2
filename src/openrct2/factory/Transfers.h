/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Item transfer between worlds: launch pads send what they hold to another world,
// where it lands in landing pads (ADR 0015).

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
    constexpr uint32_t kLaunchTicks = 200;  // launch pads launch their contents this often
    constexpr uint32_t kTransitTicks = 400; // ticks between launch and landing
    constexpr uint8_t kLaunchToNextWorld = 0xFF;

    struct Transfer
    {
        uint8_t toWorld{};
        ObjectEntryIndex item{ kObjectEntryIndexNull };
        uint16_t count{};
        uint32_t arrivalTick{};

        template<typename V>
        void visit(V& v)
        {
            v(toWorld);
            v(item);
            v(count);
            v(arrivalTick);
        }
    };

    // Shipments in flight, in launch order. Company state: it moves with the active world.
    struct TransferState
    {
        std::vector<Transfer> queue;

        template<typename V>
        void visit(V& v)
        {
            v.vec(queue, [](Transfer& transfer, auto& vv) { transfer.visit(vv); });
        }
    };

    // The world a launch pad sends to (its target, or the next world when kLaunchToNextWorld).
    uint8_t launchTargetOf(uint8_t target, uint8_t fromWorld, size_t worldCount);

    // One tick in the active world: launch pads launch every kLaunchTicks; arrivals for this world land.
    void updateTransfers(GameState_t& gameState);
} // namespace OpenRCT2::Factory

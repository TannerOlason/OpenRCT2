/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Transfers.h"

#include "../GameState.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryTopology.h"
#include "WorldManager.h"

#include <algorithm>

namespace OpenRCT2::Factory
{
    uint8_t launchTargetOf(uint8_t target, uint8_t fromWorld, size_t worldCount)
    {
        if (worldCount <= 1)
            return fromWorld;
        if (target == kLaunchToNextWorld || target >= worldCount)
            return static_cast<uint8_t>((fromWorld + 1) % worldCount);
        return target;
    }

    void updateTransfers(GameState_t& gameState)
    {
        auto& state = gameState.factory;
        const auto here = Worlds::active();
        const auto now = gameState.currentTicks;

        if (now % kLaunchTicks == 0 && Worlds::count() > 1)
        {
            state.containers.forEach([&](RecordId, ContainerRecord& container) {
                auto* proto = getPrototype(container.entry);
                if (proto == nullptr || !proto->getContainer().launchPad)
                    return;
                const auto target = launchTargetOf(container.targetWorld, here, Worlds::count());
                if (target == here)
                    return;
                // A launch may cost an item (a rocket part), consumed rather than shipped.
                const auto& props = proto->getContainer();
                const auto fuel = props.launchItem.resolve();
                if (fuel != kObjectEntryIndexNull)
                {
                    uint32_t held = 0;
                    for (const auto& slot : container.slots)
                        if (slot.item == fuel)
                            held += slot.count;
                    if (held < props.launchItemCount)
                        return;
                    uint32_t toTake = props.launchItemCount;
                    for (auto& slot : container.slots)
                    {
                        if (slot.item != fuel || toTake == 0)
                            continue;
                        const auto take = std::min<uint32_t>(toTake, slot.count);
                        slot.count = static_cast<uint16_t>(slot.count - take);
                        toTake -= take;
                        if (slot.count == 0)
                            slot.item = kObjectEntryIndexNull;
                    }
                }
                for (auto& slot : container.slots)
                {
                    if (slot.isEmpty())
                        continue;
                    state.transfers.queue.push_back(Transfer{ target, slot.item, slot.count, now + kTransitTicks });
                    slot = {};
                }
            });
        }

        auto& queue = state.transfers.queue;
        for (auto& transfer : queue)
        {
            if (transfer.toWorld != here || transfer.arrivalTick > now || transfer.count == 0)
                continue;
            auto* itemProto = getPrototype(transfer.item);
            const uint16_t stackSize = itemProto != nullptr ? itemProto->getItem().stackSize : 1;
            state.containers.forEach([&](RecordId, ContainerRecord& container) {
                auto* proto = getPrototype(container.entry);
                if (transfer.count == 0 || proto == nullptr || !proto->getContainer().landingPad)
                    return;
                while (transfer.count > 0 && containerInsert(container, transfer.item, stackSize))
                    transfer.count--;
            });
        }
        std::erase_if(queue, [](const Transfer& transfer) { return transfer.count == 0; });
    }
} // namespace OpenRCT2::Factory

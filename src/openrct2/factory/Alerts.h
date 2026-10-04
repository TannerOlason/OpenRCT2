/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Factory alerts: news items when machines run out of power, fuel, ore or ammunition,
// back up, or are destroyed.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace OpenRCT2
{
    struct GameState_t;
}

namespace OpenRCT2::Factory
{
    struct MachineRecord;

    enum class AlertKind : uint8_t
    {
        noPower,
        noFuel,
        noOre,
        noAmmo,
        outputFull,
        count,
    };

    constexpr uint32_t kAlertCheckTicks = 1024; // how often machine statuses are counted (about 25 seconds)
    constexpr uint32_t kAlertQuietTicks = 4096; // minimum gap between two alerts of one kind

    /**
     * Per kind: machines in that state at the last check and the tick of the last alert. An alert is posted when
     * the count grows and the kind has been quiet long enough. Saved in the pools chunk (version 12).
     */
    struct AlertState
    {
        std::array<uint16_t, static_cast<std::size_t>(AlertKind::count)> lastCount{};
        std::array<uint32_t, static_cast<std::size_t>(AlertKind::count)> lastAlertTick{};

        bool isEmpty() const
        {
            for (std::size_t i = 0; i < lastCount.size(); i++)
                if (lastCount[i] != 0 || lastAlertTick[i] != 0)
                    return false;
            return true;
        }

        template<typename V>
        void visit(V& v)
        {
            for (auto& count : lastCount)
                v(count);
            for (auto& tick : lastAlertTick)
                v(tick);
        }
    };

    void updateAlerts(GameState_t& gameState);
    void alertMachineDestroyed(const MachineRecord& machine);
} // namespace OpenRCT2::Factory

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Alerts.h"

#include "../GameState.h"
#include "../localisation/Formatter.h"
#include "../localisation/Formatting.h"
#include "../management/NewsItem.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryStringIds.h"
#include "FactoryTopology.h"

namespace OpenRCT2::Factory
{
    namespace
    {
        constexpr StringId kAlertStrings[] = {
            STR_FT_ALERT_NO_POWER, STR_FT_ALERT_NO_FUEL, STR_FT_ALERT_NO_ORE, STR_FT_ALERT_NO_AMMO, STR_FT_ALERT_OUTPUT_FULL,
        };

        std::optional<AlertKind> alertKindOf(MachineStatus status)
        {
            switch (status)
            {
                case MachineStatus::noPower:
                    return AlertKind::noPower;
                case MachineStatus::noFuel:
                    return AlertKind::noFuel;
                case MachineStatus::noOre:
                    return AlertKind::noOre;
                case MachineStatus::noAmmo:
                    return AlertKind::noAmmo;
                case MachineStatus::outputFull:
                    return AlertKind::outputFull;
                default:
                    return std::nullopt;
            }
        }

        // News subject for a blank item: the machine's footprint centre in world units, packed as x | y << 16.
        uint32_t newsLocation(const MachineRecord& machine)
        {
            const int32_t size = footprintSize(getPrototype(machine.entry));
            const auto x = static_cast<uint32_t>(machine.x * kCoordsXYStep + size * 16) & 0xFFFF;
            const auto y = static_cast<uint32_t>(machine.y * kCoordsXYStep + size * 16) & 0xFFFF;
            return x | (y << 16);
        }

        void post(StringId format, const Formatter& ft, uint32_t location)
        {
            const auto text = FormatStringIDLegacy(format, ft.Data());
            News::AddItemToQueue(News::ItemType::blank, text.c_str(), location);
        }
    } // namespace

    void updateAlerts(GameState_t& gameState)
    {
        auto& state = gameState.factory;
        constexpr size_t kKinds = static_cast<size_t>(AlertKind::count);
        std::array<uint16_t, kKinds> counts{};
        std::array<uint32_t, kKinds> firstLocation{};
        state.machines.forEach([&](RecordId, const MachineRecord& machine) {
            const auto kind = alertKindOf(machine.getStatus());
            if (!kind)
                return;
            auto& count = counts[static_cast<size_t>(*kind)];
            if (count == 0)
                firstLocation[static_cast<size_t>(*kind)] = newsLocation(machine);
            if (count < UINT16_MAX)
                count++;
        });
        const uint32_t now = gameState.currentTicks;
        for (size_t i = 0; i < kKinds; i++)
        {
            const bool quiet = state.alerts.lastAlertTick[i] == 0 || now - state.alerts.lastAlertTick[i] >= kAlertQuietTicks;
            if (counts[i] > state.alerts.lastCount[i] && quiet)
            {
                Formatter ft;
                ft.Add<int32_t>(counts[i]);
                post(kAlertStrings[i], ft, firstLocation[i]);
                state.alerts.lastAlertTick[i] = std::max<uint32_t>(now, 1);
            }
            state.alerts.lastCount[i] = counts[i];
        }
    }

    void alertMachineDestroyed(const MachineRecord& machine)
    {
        auto* proto = getPrototype(machine.entry);
        const auto name = proto != nullptr ? proto->GetName() : std::string();
        Formatter ft;
        ft.Add<const char*>(name.c_str());
        post(STR_FT_ALERT_DESTROYED, ft, newsLocation(machine));
    }
} // namespace OpenRCT2::Factory

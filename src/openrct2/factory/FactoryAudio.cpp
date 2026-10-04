/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "FactoryAudio.h"

#include "../GameState.h"
#include "../OpenRCT2.h"
#include "../audio/Audio.h"
#include "FactoryPrototypeObject.h"
#include "FactoryState.h"
#include "FactoryTopology.h"

namespace OpenRCT2::Factory
{
    namespace
    {
        constexpr uint32_t kSoundPeriodTicks = 160; // each noisy machine clanks about every four seconds
        constexpr uint8_t kNoisyThreshold = 30;     // prototype `noise` at which a machine makes sounds
        constexpr int32_t kMaxSoundsPerTick = 2;
    } // namespace

    void updateMachineSounds(const GameState_t& gameState)
    {
        if (gOpenRCT2Headless || !Audio::IsAvailable())
            return;
        const auto& state = gameState.factory;
        if (state.machines.aliveCount() == 0)
            return;
        int32_t played = 0;
        state.machines.forEach([&](RecordId id, const MachineRecord& machine) {
            if (played >= kMaxSoundsPerTick || !machine.isWorking()
                || (gameState.currentTicks + id * 37) % kSoundPeriodTicks != 0)
                return;
            auto* proto = getPrototype(machine.entry);
            if (proto == nullptr || proto->getMachine().noise < kNoisyThreshold)
                return;
            const int32_t size = footprintSize(proto);
            const CoordsXYZ loc{ machine.x * kCoordsXYStep + size * 16, machine.y * kCoordsXYStep + size * 16,
                                 machine.z * kCoordsZStep };
            Audio::Play3D(Audio::SoundId::mechanicFix, loc);
            played++;
        });
    }
} // namespace OpenRCT2::Factory

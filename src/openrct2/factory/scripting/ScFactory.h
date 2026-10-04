/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The `factory` script global and the factory hooks.

#pragma once

#include "../../object/ObjectTypes.h"

#include <cstdint>

struct JSContext;

namespace OpenRCT2::Factory
{
    struct MachineRecord;
    enum class MachineStatus : uint8_t;

    // ScriptEngine touch points: register the classes once, add the `factory` global to every new context.
    void registerScriptClasses(JSContext* ctx);
    void unregisterScriptClasses();
    void initialiseScriptContext(JSContext* ctx);

    // Hooks, each a no-op without subscribers (and without scripting).
    void invokeMachineStatusHook(const MachineRecord& machine, MachineStatus previous);
    void invokeResearchCompleteHook(ObjectEntryIndex technology);
} // namespace OpenRCT2::Factory

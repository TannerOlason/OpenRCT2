/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. String ids in the fork block 20481-40959 (0x5001-0x9FFF).
//
// Upstream allocates object strings in 0x2000-0x5000 and parses language files with STR_%5d after the fork
// patch in LanguagePack.cpp. Text lives at the end of data/language/en-GB.txt in the FACTORY-TOUR block; other
// languages fall back to en-GB. Content text (item names, descriptions) never goes here: it lives in object
// "strings" tables.

#pragma once

#include "../localisation/StringIdType.h"

namespace OpenRCT2
{
    enum : StringId
    {
        STR_FT_OBJECT_SELECTION_FACTORY_PROTOTYPES = 20481,
        STR_FT_FACTORY = 20482,
        STR_FT_BUILD_FACTORY_TIP = 20483,
        STR_FT_ACTION_FACTORY = 20484, // permission name
        STR_FT_CANT_BUILD_THIS_HERE = 20485,
        STR_FT_CANT_REMOVE_THIS = 20486,
        STR_FT_PROTOTYPE_NOT_PLACEABLE = 20487,
        STR_FT_FACTORY_ELEMENT_NOT_FOUND = 20488,
        STR_FT_FACTORY_IN_THE_WAY = 20489,
        STR_FT_ROTATE_TIP = 20490,
        STR_FT_SHOW_ORE = 20491,
        STR_FT_RECIPE = 20492,
        STR_FT_NO_RECIPE = 20493,
        STR_FT_FUEL = 20494,
        STR_FT_INPUTS = 20495,
        STR_FT_OUTPUTS = 20496,
        STR_FT_CONTENTS = 20497,
        STR_FT_STATUS_IDLE = 20498,
        STR_FT_STATUS_WORKING = 20499,
        STR_FT_STATUS_NO_INPUT = 20500,
        STR_FT_STATUS_OUTPUT_FULL = 20501,
        STR_FT_STATUS_NO_FUEL = 20502,
        STR_FT_STATUS_NO_POWER = 20503,
        STR_FT_STATUS_NO_ORE = 20504,
        STR_FT_STATUS_NO_RECIPE = 20505,
        STR_FT_SELECT_RECIPE_TIP = 20506,
        STR_FT_PUMP_NEEDS_WATER = 20507,
        STR_FT_FLUID = 20508,
        STR_FT_FLUID_AMOUNT = 20509,
        STR_FT_FLUID_EMPTY = 20510,
        STR_FT_FILTER = 20511,
        STR_FT_NO_FILTER = 20512,
        STR_FT_INPUT_PRIORITY = 20513,
        STR_FT_OUTPUT_PRIORITY = 20514,
        STR_FT_PRIORITY_NONE = 20515,
        STR_FT_PRIORITY_LEFT = 20516,
        STR_FT_PRIORITY_RIGHT = 20517,
        STR_FT_SELECT_FILTER_TIP = 20518,
        STR_FT_SELECT_PRIORITY_TIP = 20519,
        STR_FT_POWER_OVERVIEW = 20520,
        STR_FT_POWER_NETWORK = 20521,
        STR_FT_POWER_NO_NETWORKS = 20522,
        STR_FT_POWER_COUNTS = 20523,
        STR_FT_POWER_SUPPLY = 20524,
        STR_FT_POWER_DEMAND = 20525,
        STR_FT_POWER_SATISFACTION = 20526,
        STR_FT_PREVIOUS_NETWORK_TIP = 20527,
        STR_FT_NEXT_NETWORK_TIP = 20528,
        STR_FT_POWER_TIP = 20529,
        STR_FT_RIDE_NAME_FACTORY_TOUR = 20530,
        STR_FT_RIDE_DESCRIPTION_FACTORY_TOUR = 20531,
        STR_FT_THOUGHT_FACTORY_IMPRESSIVE = 20532,
        STR_FT_THOUGHT_FACTORY_SMELL = 20533,
        STR_FT_THOUGHT_FACTORY_NOISE = 20534,
        STR_FT_THOUGHT_FACTORY_WATCHING = 20535,
        STR_FT_THOUGHT_FACTORY_MADE_HERE = 20536,
        STR_FT_THOUGHT_SOLD_OUT = 20537,
        STR_FT_THOUGHT_FACTORY_DANGER = 20538,
        STR_FT_NEEDS_MATERIALS = 20539,
        STR_FT_WAREHOUSE = 20540,
    };
} // namespace OpenRCT2

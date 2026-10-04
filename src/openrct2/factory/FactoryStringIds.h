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
    };
} // namespace OpenRCT2

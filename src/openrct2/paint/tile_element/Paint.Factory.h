/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#pragma once

#include <cstdint>

struct PaintSession;

namespace OpenRCT2
{
    struct FactoryElement;
}

struct CoordsXY;

void PaintFactory(PaintSession& session, uint8_t direction, int32_t height, const OpenRCT2::FactoryElement& factoryElement);

// Draws the ore layer cell under a surface tile (ViewportFlag::factoryOre).
void PaintFactoryOreOverlay(PaintSession& session, const CoordsXY& tile, int32_t height);

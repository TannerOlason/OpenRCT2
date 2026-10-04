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
    struct SurfaceElement;

    namespace Factory
    {
        // What ViewportFlag::factoryOre shows on the ground (a client view setting, never simulation state).
        enum class Overlay : uint8_t
        {
            ore,
            pollution,
        };
        extern Overlay gOverlay;
    } // namespace Factory
} // namespace OpenRCT2

struct CoordsXY;

void PaintFactory(PaintSession& session, uint8_t direction, int32_t height, const OpenRCT2::FactoryElement& factoryElement);

// Draws the factory overlay (ore cells or pollution, Factory::gOverlay) on a surface tile (ViewportFlag::factoryOre).
void PaintFactoryOverlay(
    PaintSession& session, const OpenRCT2::SurfaceElement& surface, const CoordsXY& tile, int32_t height, uint8_t rotation);

// Threats standing on `tile` (called from the entity paint pass, like sprites).
void PaintFactoryThreats(PaintSession& session, const CoordsXY& tile);

/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#include "Paint.Factory.h"

#include "../../profiling/Profiling.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../Paint.h"

using namespace OpenRCT2;

/**
 * Paints a FactoryElement. M0 stub: factory records do not exist yet, so nothing is drawn. M1 draws belts
 * (shape, direction, animation frame), their items as child images, inserter arms and container boxes from the
 * prototype's object images.
 */
void PaintFactory(PaintSession& session, uint8_t direction, int32_t height, const FactoryElement& factoryElement)
{
    PROFILED_FUNCTION();
    (void)session;
    (void)direction;
    (void)height;
    (void)factoryElement;
}

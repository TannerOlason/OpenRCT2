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

#include "../../actions/GameAction.hpp"
#include "FactoryCommand.h"

#include <vector>

namespace OpenRCT2::GameActions
{
    /**
     * Places a straight run of belts from `start` towards `end` (same row or column; the longer axis wins when
     * the two differ). Belts face along the run, or `direction` when start and end coincide. Tiles that already
     * hold a factory element are skipped, so dragging over an existing line is harmless. Each tile goes through
     * a nested FactoryPlaceAction; a tile that cannot be built stops the run there.
     */
    class FactoryPlaceBeltLineAction final : public GameActionBase<toGameCommand(FactoryCommand::placeBeltLine)>
    {
    private:
        CoordsXYZ _start;
        CoordsXYZ _end;
        Direction _direction{};
        ObjectEntryIndex _entry{ kObjectEntryIndexNull };

    public:
        static constexpr int32_t kMaxTiles = 64;

        FactoryPlaceBeltLineAction() = default;
        FactoryPlaceBeltLineAction(const CoordsXYZ& start, const CoordsXYZ& end, Direction direction, ObjectEntryIndex entry);

        // The tiles the run covers, in placement order, with the direction the belts will face.
        static std::vector<CoordsXYZ> lineTiles(
            const CoordsXYZ& start, const CoordsXYZ& end, Direction fallback, Direction& dir);

        void AcceptParameters(GameActionParameterVisitor& visitor) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;

    private:
        Result Run(GameState_t& gameState, bool apply) const;
    };
} // namespace OpenRCT2::GameActions

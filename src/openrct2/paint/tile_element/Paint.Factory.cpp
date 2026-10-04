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

#include "../../GameState.h"
#include "../../drawing/ImageId.hpp"
#include "../../factory/Belts.h"
#include "../../factory/FactoryPrototypeObject.h"
#include "../../factory/FactoryState.h"
#include "../../factory/FactoryTopology.h"
#include "../../interface/Viewport.h"
#include "../../profiling/Profiling.h"
#include "../../world/Map.h"
#include "../../world/tile_element/FactoryElement.h"
#include "../Paint.h"
#include "Paint.TileElement.h"

using namespace OpenRCT2;
using namespace OpenRCT2::Drawing;
using namespace OpenRCT2::Factory;

namespace
{
    constexpr int32_t kLaneOffset = 7; // map units either side of the belt centre line
    constexpr int32_t kItemZ = 2;      // items float just above the belt surface

    /**
     * Map-local offset (0..32 in both axes) of a point along a belt tile's centre line. `t` runs from 0 at
     * the entry edge to 256 at the exit edge; lanes sit either side of the line.
     */
    CoordsXY beltPointLocal(BeltShape shape, Direction dir, int32_t t, uint8_t lane)
    {
        const CoordsXY centre{ kCoordsXYHalfTile, kCoordsXYHalfTile };
        Direction travel = dir;
        int32_t along = t; // 0..256 along the current straight piece
        if (shape != BeltShape::straight)
        {
            const Direction incoming = shape == BeltShape::turnLeft ? rightOf(dir) : leftOf(dir);
            if (t < kBeltUnitsPerTile / 2)
            {
                travel = incoming;
                along = t * 2; // first half: from the entry edge to the centre
                const auto delta = CoordsDirectionDelta[travel];
                const auto entry = centre - CoordsXY{ delta.x / 2, delta.y / 2 };
                const CoordsXY side = CoordsDirectionDelta[lane == kLaneLeft ? leftOf(travel) : rightOf(travel)];
                return entry + CoordsXY{ delta.x * along / 512, delta.y * along / 512 }
                + CoordsXY{ side.x * kLaneOffset / kCoordsXYStep, side.y * kLaneOffset / kCoordsXYStep };
            }
            along = (t - kBeltUnitsPerTile / 2) * 2; // second half: from the centre to the exit edge
            const auto delta = CoordsDirectionDelta[travel];
            const CoordsXY side = CoordsDirectionDelta[lane == kLaneLeft ? leftOf(travel) : rightOf(travel)];
            return centre + CoordsXY{ delta.x * along / 512, delta.y * along / 512 }
            + CoordsXY{ side.x * kLaneOffset / kCoordsXYStep, side.y * kLaneOffset / kCoordsXYStep };
        }
        const auto delta = CoordsDirectionDelta[travel];
        const auto entry = centre - CoordsXY{ delta.x / 2, delta.y / 2 };
        const CoordsXY side = CoordsDirectionDelta[lane == kLaneLeft ? leftOf(travel) : rightOf(travel)];
        return entry + CoordsXY{ delta.x * along / kBeltUnitsPerTile, delta.y * along / kBeltUnitsPerTile }
        + CoordsXY{ side.x * kLaneOffset / kCoordsXYStep, side.y * kLaneOffset / kCoordsXYStep };
    }

    // Paint offsets are given in the view frame; rotate a map-local tile point about the tile centre.
    CoordsXY toViewFrame(const PaintSession& session, const CoordsXY& local)
    {
        const CoordsXY centre{ kCoordsXYHalfTile, kCoordsXYHalfTile };
        return (local - centre).rotate(session.CurrentRotation) + centre;
    }

    void paintBeltItems(PaintSession& session, int32_t height, const FactoryElement& element)
    {
        if (session.rt.zoom_level > ZoomLevel{ 1 })
            return;
        auto& state = getGameState().factory;
        auto* segment = state.beltSegments.get(element.getRecordId());
        if (segment == nullptr)
            return;
        const int32_t length = segmentLength(*segment);
        const int32_t tileStart = segmentTileStart(*segment, element.getFootprintIndex());
        const auto shape = getBeltShape(element);
        const Direction dir = element.getDirection();

        for (uint8_t lane = 0; lane < kBeltLaneCount; lane++)
        {
            laneForEachInRange(
                segment->lanes[lane], length, tileStart, tileStart + kBeltUnitsPerTile - 1, [&](LaneItemView view) {
                    auto* itemProto = getPrototype(view.item);
                    if (itemProto == nullptr)
                        return;
                    auto image = itemProto->getItemBeltImage();
                    if (image == kImageIndexUndefined)
                        return;
                    const auto local = beltPointLocal(shape, dir, view.position - tileStart, lane);
                    const auto offset = toViewFrame(session, local);
                    const CoordsXYZ pos{ offset.x, offset.y, height + kItemZ };
                    PaintAddImageAsChild(session, ImageId(image), pos, { pos, { 1, 1, 1 } });
                });
        }
    }

    uint8_t inserterFrame(const InserterRecord& inserter, const InserterProperties& props)
    {
        const int32_t last = props.frames - 1;
        if (last <= 0)
            return 0;
        switch (inserter.phase)
        {
            case kInserterPhaseSwingingToDrop:
                return static_cast<uint8_t>(inserter.progress * last / std::max<int32_t>(1, props.swingTicks));
            case kInserterPhaseWaitingToDrop:
                return static_cast<uint8_t>(last);
            case kInserterPhaseReturning:
                return static_cast<uint8_t>(
                    (props.swingTicks - inserter.progress) * last / std::max<int32_t>(1, props.swingTicks));
            default:
                return 0;
        }
    }
} // namespace

void PaintFactory(PaintSession& session, uint8_t direction, int32_t height, const FactoryElement& factoryElement)
{
    PROFILED_FUNCTION();

    auto* proto = getPrototype(factoryElement);
    if (proto == nullptr)
        return;

    session.InteractionType = ViewportInteractionItem::factory;
    ImageId imageTemplate;
    if (factoryElement.isGhost())
    {
        session.InteractionType = ViewportInteractionItem::none;
        imageTemplate = ImageId().WithRemap(FilterPaletteID::paletteGhost);
    }
    else if (session.SelectedElement == reinterpret_cast<const TileElement*>(&factoryElement))
    {
        imageTemplate = ImageId().WithRemap(FilterPaletteID::paletteGhost);
    }

    const auto& state = getGameState().factory;
    const int32_t clearance = factoryElement.getClearanceZ() - factoryElement.getBaseZ();
    const BoundBoxXYZ fullTile{ { 0, 0, height }, { 32, 32, std::max(1, clearance - 1) } };
    const BoundBoxXYZ centreBox{ { 8, 8, height }, { 16, 16, std::max(1, clearance - 1) } };

    switch (factoryElement.getSubtype())
    {
        case FactoryElementSubtype::belt:
        {
            const auto& belt = proto->getBelt();
            const uint32_t ticks = getGameState().currentTicks;
            const uint8_t frame = static_cast<uint8_t>((ticks * belt.speed / 16) % belt.frames);
            auto image = proto->getBeltImage(getBeltShape(factoryElement), direction, frame);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, fullTile);
                if (!factoryElement.isGhost() && factoryElement.hasRecord())
                {
                    paintBeltItems(session, height, factoryElement);
                }
            }
            break;
        }
        case FactoryElementSubtype::undergroundBelt:
        {
            auto image = proto->getUndergroundImage(isUndergroundExit(factoryElement), direction);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, fullTile);
                if (!factoryElement.isGhost() && factoryElement.hasRecord())
                {
                    paintBeltItems(session, height, factoryElement);
                }
            }
            break;
        }
        case FactoryElementSubtype::splitter:
        {
            auto image = proto->getSplitterImage(factoryElement.getFootprintIndex(), direction);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, fullTile);
            }
            break;
        }
        case FactoryElementSubtype::inserter:
        {
            uint8_t frame = 0;
            if (factoryElement.hasRecord())
            {
                auto* record = state.inserters.get(factoryElement.getRecordId());
                if (record != nullptr)
                    frame = inserterFrame(*record, proto->getInserter());
            }
            auto image = proto->getInserterImage(direction, frame);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, centreBox);
            }
            break;
        }
        case FactoryElementSubtype::container:
        {
            auto image = proto->getContainerImage(direction);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, centreBox);
            }
            break;
        }
        case FactoryElementSubtype::machine:
        {
            uint8_t frame = 0;
            if (factoryElement.hasRecord())
            {
                auto* record = state.machines.get(factoryElement.getRecordId());
                const auto& props = proto->getMachine();
                // Destroyed machines are drawn as dark wrecks until removed.
                if (record != nullptr && record->isDestroyed() && !factoryElement.isGhost())
                    imageTemplate = ImageId().WithRemap(FilterPaletteID::paletteDarken3);
                if (record != nullptr && record->isWorking() && props.frames > 1)
                {
                    // Cycle the working frames at roughly 10 frames per second.
                    frame = static_cast<uint8_t>(1 + ((getGameState().currentTicks / 4) % (props.frames - 1)));
                }
            }
            // Multi-tile machines draw one slice per tile, chosen by the tile's place in the view-rotated square.
            const uint8_t size = std::max<uint8_t>(1, proto->getMachine().size);
            const uint8_t slice = footprintViewSlice(factoryElement.getFootprintIndex(), size, session.CurrentRotation & 3);
            auto image = proto->getMachineImage(direction, frame, slice);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, fullTile);
            }
            break;
        }
        case FactoryElementSubtype::pole:
        {
            auto image = proto->getPoleImage();
            if (image != kImageIndexUndefined)
            {
                const BoundBoxXYZ poleBox{ { 12, 12, height }, { 8, 8, std::max(1, clearance - 1) } };
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, poleBox);
            }
            break;
        }
        case FactoryElementSubtype::pipe:
        {
            // The cached mask is in map directions; rotate it into the view.
            const uint8_t worldMask = factoryElement.getConnectionCache() & 0xF;
            const uint8_t rotation = session.CurrentRotation & 3;
            const uint8_t viewMask = static_cast<uint8_t>(((worldMask << rotation) | (worldMask >> (4 - rotation))) & 0xF);
            auto image = proto->getPipeImage(viewMask);
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, fullTile);
            }
            break;
        }
        default:
        {
            auto image = proto->hasImages() ? proto->GetBaseImageId() : kImageIndexUndefined;
            if (image != kImageIndexUndefined)
            {
                PaintAddImageAsParent(session, imageTemplate.WithIndex(image), { 0, 0, height }, fullTile);
            }
            break;
        }
    }

    PaintUtilSetGeneralSupportHeight(session, static_cast<int16_t>(factoryElement.getClearanceZ()));
    PaintUtilSetSegmentSupportHeight(session, kSegmentsAll, 0xFFFF, 0);
}

void PaintFactoryOreOverlay(PaintSession& session, const CoordsXY& tile, int32_t height)
{
    const auto& ore = getGameState().factory.ore;
    const auto& cell = ore.get(TileCoordsXY(tile));
    if (cell.isEmpty())
        return;
    auto* proto = getPrototype(cell.ore);
    if (proto == nullptr)
        return;
    auto image = proto->getOreOverlayImage();
    if (image == kImageIndexUndefined)
        return;
    // Drawn as a child of the surface so it never sorts above things standing on the tile.
    PaintAddImageAsChild(session, ImageId(image), { 0, 0, height + 1 }, { { 0, 0, height + 1 }, { 32, 32, 1 } });
}

void PaintFactoryThreats(PaintSession& session, const CoordsXY& tile)
{
    const auto& threats = getGameState().factory.threats;
    if (threats.aliveCount() == 0)
        return;
    const auto tileX = tile.x / kCoordsXYStep;
    const auto tileY = tile.y / kCoordsXYStep;
    threats.forEach([&](RecordId, const ThreatRecord& threat) {
        if (threat.x / kCoordsXYStep != tileX || threat.y / kCoordsXYStep != tileY)
            return;
        auto* proto = getPrototype(threat.entry);
        if (proto == nullptr)
            return;
        const auto& props = proto->getThreat();
        const uint8_t viewDirection = static_cast<uint8_t>((threat.direction + session.CurrentRotation) & 3);
        const uint8_t frame = static_cast<uint8_t>((threat.animation / 6) % props.frames);
        const auto image = proto->getThreatImage(viewDirection, frame);
        if (image == kImageIndexUndefined)
            return;
        session.CurrentlyDrawnEntity = nullptr;
        session.SpritePosition = { threat.x, threat.y };
        session.InteractionType = ViewportInteractionItem::none;
        PaintAddImageAsParent(session, ImageId(image), { 0, 0, threat.z }, { { 0, 0, threat.z }, { 1, 1, 12 } });
    });
}

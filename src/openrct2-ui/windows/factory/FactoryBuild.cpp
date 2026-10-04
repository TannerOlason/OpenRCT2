/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. The factory build window: a palette of placeable prototypes, a rotate
// button, a ghost preview under the cursor and click-to-place through FactoryPlaceAction.

#include <algorithm>
#include <openrct2-ui/interface/ViewportInteraction.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Technology.h>
#include <openrct2/factory/actions/FactoryPlaceAction.h>
#include <openrct2/factory/actions/FactoryPlaceBeltLineAction.h>
#include <openrct2/factory/actions/FactoryRemoveAction.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/interface/WidgetIndexGlobals.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/object/ObjectList.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/ParkData.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;

    static constexpr StringId kWindowTitle = STR_FT_FACTORY;
    static constexpr ScreenSize kWindowSize = { 302, 200 };
    static constexpr int32_t kButtonWidth = 66;
    static constexpr int32_t kButtonHeight = 66;

    enum WindowFactoryBuildWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_LIST,
        WIDX_ROTATE,
        WIDX_POWER,
        WIDX_OPTIONS,
        WIDX_RESEARCH,
        WIDX_PRODUCTION,
    };
    VALIDATE_GLOBAL_WIDX(WC_FACTORY_BUILD, WIDX_ROTATE);

    // clang-format off
    static constexpr auto kWindowFactoryBuildWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({  2,  17}, {272, 150}, WidgetType::scroll,  WindowColour::secondary, SCROLL_VERTICAL                                 ),
        makeWidget({276,  17}, { 24,  24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_ROTATE_ARROW), STR_FT_ROTATE_TIP     ),
        makeWidget({276,  43}, { 24,  24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_GRAPH),        STR_FT_POWER_TIP      ),
        makeWidget({276,  69}, { 24,  24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_TAB_GEARS_0),  STR_FT_OPTIONS_TIP    ),
        makeWidget({276,  95}, { 24,  24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_TAB_FINANCES_RESEARCH_0), STR_FT_RESEARCH_TIP),
        makeWidget({276, 121}, { 24,  24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_TAB_GRAPH_0),             STR_FT_PRODUCTION_TIP)
    );
    // clang-format on

    class FactoryBuildWindow final : public Window
    {
    private:
        std::vector<ObjectEntryIndex> _entries;
        ObjectEntryIndex _selected = kObjectEntryIndexNull;
        ObjectEntryIndex _hover = kObjectEntryIndexNull;
        uint8_t _rotation = 0; // view-relative, converted to a map direction at placement
        money64 _cost = kMoney64Undefined;
        bool _errorOccurred = false;

        bool _ghostPlaced = false;
        CoordsXYZ _ghostLoc{};
        Direction _ghostDirection{};
        ObjectEntryIndex _ghostEntry = kObjectEntryIndexNull;

        // Click-and-drag placement of other kinds: the last origin placed this press, so a drag never re-places on
        // top of it (which would only report "in the way").
        std::optional<CoordsXYZ> _lastPlaced;

        // Belt drags: tool down fixes the start, dragging previews a ghost run, tool up builds it.
        bool _dragging = false;
        CoordsXYZ _dragStart{};
        CoordsXYZ _dragEnd{};
        std::vector<CoordsXYZ> _ghostLine;

    public:
        void onOpen() override
        {
            setWidgets(kWindowFactoryBuildWidgets);
            WindowInitScrollWidgets(*this);
            WindowPushOthersRight(*this);
            ShowGridlines();
            RefreshEntries();

            ToolCancel();
            ToolSet(*this, WIDX_BACKGROUND, Tool::crosshair);
            gInputFlags.set(InputFlag::allowRightMouseRemoval);
            _errorOccurred = false;
        }

        void onClose() override
        {
            RemoveGhost();
            RemoveGhostLine();
            gMapSelectFlags.unset(MapSelectFlag::enable);
            auto* windowMgr = GetWindowManager();
            windowMgr->InvalidateByClass(WindowClass::topToolbar);
            HideGridlines();
        }

        void onUpdate() override
        {
            if (!isToolActive(WindowClass::factoryBuild, WIDX_BACKGROUND))
            {
                close();
                return;
            }
            // Research can unlock prototypes at any time.
            if (currentFrame % 32 == 0)
            {
                RefreshEntries();
                invalidate();
            }
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_ROTATE:
                    _rotation = (_rotation + 1) & 3;
                    RemoveGhost();
                    invalidate();
                    break;
                case WIDX_POWER:
                    FactoryPowerOpen(kNullRecord);
                    break;
                case WIDX_OPTIONS:
                    FactoryOptionsOpen();
                    break;
                case WIDX_RESEARCH:
                    FactoryResearchOpen();
                    break;
                case WIDX_PRODUCTION:
                    FactoryProductionOpen();
                    break;
            }
        }

        void onToolUpdate(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (widgetIndex != WIDX_BACKGROUND || _dragging)
                return;
            UpdateGhost(screenCoords);
        }

        void onToolDown(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (widgetIndex != WIDX_BACKGROUND)
                return;
            if (SelectedIsBelt())
            {
                BeginDrag(screenCoords);
                return;
            }
            PlaceAtCursor(screenCoords);
        }

        void onToolDrag(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (widgetIndex != WIDX_BACKGROUND)
                return;
            if (_dragging)
            {
                auto tile = CursorTile(screenCoords);
                if (!tile.has_value())
                    return;
                // Keep the run on the start tile's level so every tile shares one z.
                const CoordsXYZ end{ CoordsXY(*tile), _dragStart.z };
                if (end == _dragEnd)
                    return;
                _dragEnd = end;
                UpdateGhostLine();
                return;
            }
            if (SelectedIsBelt())
            {
                // The press landed off the map; start the run where the drag first reaches it.
                BeginDrag(screenCoords);
                return;
            }
            PlaceAtCursor(screenCoords);
        }

        void onToolUp(WidgetIndex widgetIndex, const ScreenCoordsXY&) override
        {
            if (_dragging)
            {
                _dragging = false;
                RemoveGhostLine();
                PlaceLine();
            }
            _errorOccurred = false;
            _lastPlaced.reset();
        }

        void onToolAbort(WidgetIndex widgetIndex) override
        {
            _dragging = false;
            RemoveGhost();
            RemoveGhostLine();
            gMapSelectFlags.unset(MapSelectFlag::enable);
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            const auto columns = GetNumColumns();
            const auto rows = (static_cast<int32_t>(_entries.size()) + columns - 1) / columns;
            return { 0, rows * kButtonHeight };
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            auto entry = EntryAt(screenCoords);
            if (entry == kObjectEntryIndexNull)
                return;
            _selected = entry;
            RemoveGhost();
            Audio::Play(Audio::SoundId::click1, 0, windowPos.x + (width / 2));
            _cost = kMoney64Undefined;
            invalidate();
        }

        void onScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            auto entry = EntryAt(screenCoords);
            if (entry != _hover)
            {
                _hover = entry;
                invalidate();
            }
        }

        void onScrollDraw(int32_t scrollIndex, Drawing::RenderTarget& rt) override
        {
            GfxClear(rt, getColourMap(colours[1].colour).midLight);
            const auto columns = GetNumColumns();
            ScreenCoordsXY topLeft{ 0, 0 };
            for (size_t i = 0; i < _entries.size(); i++)
            {
                const auto entry = _entries[i];
                if (entry == _selected)
                {
                    Drawing::Rectangle::fillInset(
                        rt, { topLeft, topLeft + ScreenCoordsXY{ kButtonWidth - 1, kButtonHeight - 1 } }, colours[1],
                        Drawing::Rectangle::BorderStyle::inset, Drawing::Rectangle::FillBrightness::dark);
                }
                else if (entry == _hover)
                {
                    Drawing::Rectangle::fillInset(
                        rt, { topLeft, topLeft + ScreenCoordsXY{ kButtonWidth - 1, kButtonHeight - 1 } }, colours[1],
                        Drawing::Rectangle::BorderStyle::outset, Drawing::Rectangle::FillBrightness::dark);
                }

                Drawing::RenderTarget clipped;
                if (ClipRenderTarget(clipped, rt, topLeft + ScreenCoordsXY{ 1, 1 }, kButtonWidth - 2, kButtonHeight - 2))
                {
                    auto* proto = getPrototype(entry);
                    if (proto != nullptr)
                    {
                        proto->DrawPreview(clipped, kButtonWidth - 2, kButtonHeight - 2);
                    }
                }

                topLeft.x += kButtonWidth;
                if (topLeft.x >= columns * kButtonWidth)
                {
                    topLeft.y += kButtonHeight;
                    topLeft.x = 0;
                }
            }
        }

        void onPrepareDraw() override
        {
            setWidgetDisabled(WIDX_ROTATE, _selected == kObjectEntryIndexNull);
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            WindowDrawWidgets(*this, rt);

            auto screenCoords = windowPos + ScreenCoordsXY{ widgets[WIDX_LIST].left + 2, widgets[WIDX_LIST].bottom + 4 };
            auto* proto = getPrototype(_selected);
            if (proto != nullptr)
            {
                auto ft = Formatter();
                const auto name = proto->GetName();
                ft.Add<StringId>(STR_STRING);
                ft.Add<const char*>(name.c_str());
                drawText(rt, screenCoords, STR_BLACK_STRING, ft, {});
                screenCoords.y += 12;
            }
            if (_cost != kMoney64Undefined && !getGameState().park.flags.has(ParkFlag::noMoney))
            {
                auto ft = Formatter();
                ft.Add<money64>(_cost);
                drawText(rt, screenCoords, STR_COST_LABEL, ft, {});
            }
        }

    private:
        int32_t GetNumColumns() const
        {
            const auto& listWidget = widgets[WIDX_LIST];
            const auto contentWidth = listWidget.width() - 1 - kScrollBarWidth;
            return std::max(1, contentWidth / kButtonWidth);
        }

        ObjectEntryIndex EntryAt(const ScreenCoordsXY& screenCoords) const
        {
            const auto columns = GetNumColumns();
            const auto col = screenCoords.x / kButtonWidth;
            const auto row = screenCoords.y / kButtonHeight;
            if (col < 0 || col >= columns || row < 0)
                return kObjectEntryIndexNull;
            const auto index = static_cast<size_t>(row * columns + col);
            return index < _entries.size() ? _entries[index] : kObjectEntryIndexNull;
        }

        void RefreshEntries()
        {
            _entries.clear();
            auto& objectManager = GetContext()->GetObjectManager();
            const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
            for (size_t i = 0; i < count; i++)
            {
                auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
                if (proto != nullptr && proto->isPlaceable()
                    && isPrototypeUnlocked(getGameState(), static_cast<ObjectEntryIndex>(i)))
                {
                    _entries.push_back(static_cast<ObjectEntryIndex>(i));
                }
            }
            if (std::find(_entries.begin(), _entries.end(), _selected) == _entries.end())
                _selected = kObjectEntryIndexNull;
            if (_selected == kObjectEntryIndexNull && !_entries.empty())
            {
                _selected = _entries.front();
            }
        }

        Direction PlacementDirection() const
        {
            return (_rotation - GetCurrentRotation()) & 3;
        }

        // Multi-tile machines are centred on the cursor: their origin (minimum corner) sits half a footprint back.
        std::optional<CoordsXYZ> PlacementOrigin(const ScreenCoordsXY& screenCoords) const
        {
            auto tile = CursorTile(screenCoords);
            const int32_t half = (footprintSize(getPrototype(_selected)) - 1) / 2;
            if (!tile.has_value() || half == 0)
                return tile;
            const CoordsXY origin{ tile->x - half * kCoordsXYStep, tile->y - half * kCoordsXYStep };
            auto* surface = MapIsLocationValid(origin) ? MapGetSurfaceElementAt(origin) : nullptr;
            if (surface == nullptr)
                return std::nullopt;
            return CoordsXYZ{ origin, surface->getBaseZ() };
        }

        std::optional<CoordsXYZ> CursorTile(const ScreenCoordsXY& screenCoords) const
        {
            auto info = GetMapCoordinatesFromPos(screenCoords, { ViewportInteractionItem::terrain });
            if (info.interactionType == ViewportInteractionItem::none)
                return std::nullopt;
            auto tile = info.Loc.toTileStart();
            auto* surface = MapGetSurfaceElementAt(tile);
            if (surface == nullptr)
                return std::nullopt;
            return CoordsXYZ{ tile, surface->getBaseZ() };
        }

        void RemoveGhost()
        {
            if (!_ghostPlaced)
                return;
            auto action = GameActions::FactoryRemoveAction(_ghostLoc);
            action.SetFlags(
                { GameActions::CommandFlag::ghost, GameActions::CommandFlag::allowDuringPaused,
                  GameActions::CommandFlag::noSpend });
            GameActions::Execute(&action, getGameState());
            _ghostPlaced = false;
        }

        void UpdateGhost(const ScreenCoordsXY& screenCoords)
        {
            auto tile = PlacementOrigin(screenCoords);
            if (!tile.has_value() || _selected == kObjectEntryIndexNull)
            {
                RemoveGhost();
                gMapSelectFlags.unset(MapSelectFlag::enable);
                SetCost(kMoney64Undefined);
                return;
            }

            const auto direction = PlacementDirection();
            if (_ghostPlaced && _ghostLoc == *tile && _ghostDirection == direction && _ghostEntry == _selected)
                return;

            RemoveGhost();
            gMapSelectFlags.set(MapSelectFlag::enable);
            gMapSelectType = MapSelectType::full;
            const int32_t span = (footprintSize(getPrototype(_selected)) - 1) * kCoordsXYStep;
            setMapSelectRange(MapRange{ CoordsXY(*tile), CoordsXY{ tile->x + span, tile->y + span } });

            auto action = GameActions::FactoryPlaceAction(*tile, direction, _selected);
            action.SetFlags(
                { GameActions::CommandFlag::ghost, GameActions::CommandFlag::allowDuringPaused,
                  GameActions::CommandFlag::noSpend });
            auto res = GameActions::Execute(&action, getGameState());
            money64 cost = kMoney64Undefined;
            if (res.error == GameActions::Status::ok)
            {
                _ghostPlaced = true;
                _ghostLoc = *tile;
                _ghostDirection = direction;
                _ghostEntry = _selected;
                cost = res.cost;
            }
            if (cost != _cost)
            {
                _cost = cost;
                invalidate();
            }
        }

        bool SelectedIsBelt() const
        {
            auto* proto = getPrototype(_selected);
            return proto != nullptr && proto->getKind() == PrototypeKind::belt;
        }

        static GameActions::CommandFlags GhostFlags()
        {
            return { GameActions::CommandFlag::ghost, GameActions::CommandFlag::allowDuringPaused,
                     GameActions::CommandFlag::noSpend };
        }

        void BeginDrag(const ScreenCoordsXY& screenCoords)
        {
            auto tile = CursorTile(screenCoords);
            if (!tile.has_value())
                return;
            RemoveGhost();
            _dragging = true;
            _dragStart = *tile;
            _dragEnd = *tile;
            UpdateGhostLine();
        }

        void RemoveGhostAt(const CoordsXYZ& loc)
        {
            auto action = GameActions::FactoryRemoveAction(loc);
            action.SetFlags(GhostFlags());
            GameActions::Execute(&action, getGameState());
        }

        void RemoveGhostLine()
        {
            for (const auto& loc : _ghostLine)
                RemoveGhostAt(loc);
            _ghostLine.clear();
        }

        void SetCost(money64 cost)
        {
            if (cost != _cost)
            {
                _cost = cost;
                invalidate();
            }
        }

        void UpdateGhostLine()
        {
            Direction dir;
            const auto tiles = GameActions::FactoryPlaceBeltLineAction::lineTiles(
                _dragStart, _dragEnd, PlacementDirection(), dir);
            gMapSelectFlags.set(MapSelectFlag::enable);
            gMapSelectType = MapSelectType::full;
            setMapSelectRange(MapRange{ CoordsXY(tiles.front()), CoordsXY(tiles.back()) });

            // The run always starts at _dragStart, so the old and new runs share a prefix. Keep ghosts the new
            // run still covers facing the same way; the ghost run below skips occupied tiles and fills the rest.
            std::vector<CoordsXYZ> kept;
            for (const auto& loc : _ghostLine)
            {
                auto* element = findFactoryElement(loc, true);
                if (element != nullptr && element->isGhost() && element->getDirection() == dir
                    && std::find(tiles.begin(), tiles.end(), loc) != tiles.end())
                    kept.push_back(loc);
                else
                    RemoveGhostAt(loc);
            }
            _ghostLine = std::move(kept);

            // Ghosts only where the real run would build, so the preview matches the result tile for tile.
            auto action = GameActions::FactoryPlaceBeltLineAction(_dragStart, _dragEnd, PlacementDirection(), _selected);
            action.SetFlags(GhostFlags());
            GameActions::Execute(&action, getGameState());
            for (const auto& tile : tiles)
            {
                auto* element = findFactoryElement(tile, true);
                if (element != nullptr && element->isGhost()
                    && std::find(_ghostLine.begin(), _ghostLine.end(), tile) == _ghostLine.end())
                    _ghostLine.push_back(tile);
            }

            // Ghosts never block construction, so querying the real run prices every tile, kept or new.
            auto query = GameActions::FactoryPlaceBeltLineAction(_dragStart, _dragEnd, PlacementDirection(), _selected);
            auto res = GameActions::Query(&query, getGameState());
            SetCost(res.error == GameActions::Status::ok ? res.cost : kMoney64Undefined);
        }

        void PlaceLine()
        {
            if (_selected == kObjectEntryIndexNull)
                return;
            auto action = GameActions::FactoryPlaceBeltLineAction(_dragStart, _dragEnd, PlacementDirection(), _selected);
            action.SetCallback([](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error == GameActions::Status::ok && result->cost != 0)
                {
                    Audio::Play3D(Audio::SoundId::placeItem, result->position);
                }
            });
            GameActions::Execute(&action, getGameState());
            // The run's price no longer describes anything under the cursor.
            SetCost(kMoney64Undefined);
        }

        void PlaceAtCursor(const ScreenCoordsXY& screenCoords)
        {
            if (_errorOccurred || _selected == kObjectEntryIndexNull)
                return;
            auto tile = PlacementOrigin(screenCoords);
            if (!tile.has_value())
                return;
            if (_lastPlaced.has_value())
            {
                const int32_t size = footprintSize(getPrototype(_selected));
                const int32_t dx = std::abs(tile->x - _lastPlaced->x) / kCoordsXYStep;
                const int32_t dy = std::abs(tile->y - _lastPlaced->y) / kCoordsXYStep;
                if (dx < size && dy < size)
                    return; // still over what this press just built
            }
            _lastPlaced = *tile;

            RemoveGhost();
            auto action = GameActions::FactoryPlaceAction(*tile, PlacementDirection(), _selected);
            action.SetCallback([this](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error == GameActions::Status::ok)
                {
                    if (result->cost != 0)
                    {
                        Audio::Play3D(Audio::SoundId::placeItem, result->position);
                    }
                }
                else
                {
                    _errorOccurred = true;
                }
            });
            GameActions::Execute(&action, getGameState());
        }
    };

    WindowBase* FactoryBuildOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<FactoryBuildWindow>(WindowClass::factoryBuild, kWindowSize, {});
    }

    void ToggleFactoryBuildWindow()
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr->FindByClass(WindowClass::factoryBuild) == nullptr)
        {
            ContextOpenWindow(WindowClass::factoryBuild);
        }
        else
        {
            ToolCancel();
            windowMgr->CloseByClass(WindowClass::factoryBuild);
        }
    }
} // namespace OpenRCT2::Ui::Windows

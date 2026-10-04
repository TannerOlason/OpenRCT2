/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Blueprints: drag over an area to copy it, then paste it (with a ghost preview) or
// export it as text for the clipboard. Pasting runs FactoryPlaceBlueprintAction.

#include <SDL.h>
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
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/Blueprint.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/actions/FactoryPlaceBlueprintAction.h>
#include <openrct2/factory/actions/FactoryRemoveAction.h>
#include <openrct2/interface/Viewport.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapSelection.h>
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;

    static constexpr ScreenSize kWindowSize = { 230, 98 };

    enum WindowFactoryBlueprintWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_COPY,
        WIDX_PASTE,
        WIDX_ROTATE,
        WIDX_EXPORT,
        WIDX_IMPORT,
    };

    // clang-format off
    static constexpr auto kWindowFactoryBlueprintWidgets = makeWidgets(
        makeWindowShim(STR_FT_BLUEPRINTS, kWindowSize),
        makeWidget({  6, 18}, {106, 14}, WidgetType::button, WindowColour::secondary, STR_FT_BLUEPRINT_COPY,   STR_FT_BLUEPRINT_COPY_TIP),
        makeWidget({118, 18}, {106, 14}, WidgetType::button, WindowColour::secondary, STR_FT_BLUEPRINT_PASTE,  STR_FT_BLUEPRINT_PASTE_TIP),
        makeWidget({  6, 36}, { 70, 14}, WidgetType::button, WindowColour::secondary, STR_FT_BLUEPRINT_ROTATE),
        makeWidget({ 80, 36}, { 70, 14}, WidgetType::button, WindowColour::secondary, STR_FT_BLUEPRINT_EXPORT, STR_FT_BLUEPRINT_EXPORT_TIP),
        makeWidget({154, 36}, { 70, 14}, WidgetType::button, WindowColour::secondary, STR_FT_BLUEPRINT_IMPORT, STR_FT_BLUEPRINT_IMPORT_TIP)
    );
    // clang-format on

    // The client's clipboard (blueprint text) survives closing the window.
    static std::string gBlueprintClipboard;

    class FactoryBlueprintWindow final : public Window
    {
    private:
        enum class Mode : uint8_t
        {
            idle,
            copy,
            paste,
        };
        Mode _mode = Mode::idle;
        bool _selecting = false;
        CoordsXY _selectStart{};
        uint8_t _rotation = 0;
        std::optional<CoordsXYZ> _ghostOrigin;
        std::vector<CoordsXYZ> _ghostTiles;
        money64 _cost = kMoney64Undefined;
        StringId _message = kStringIdNone;

    public:
        void onOpen() override
        {
            setWidgets(kWindowFactoryBlueprintWidgets);
            StartMode(gBlueprintClipboard.empty() ? Mode::copy : Mode::paste);
        }

        void onClose() override
        {
            RemoveGhosts();
            gMapSelectFlags.unset(MapSelectFlag::enable);
            if (isToolActive(WindowClass::factoryBlueprint, WIDX_BACKGROUND))
                ToolCancel();
        }

        void onUpdate() override
        {
            if (_mode != Mode::idle && !isToolActive(WindowClass::factoryBlueprint, WIDX_BACKGROUND))
            {
                RemoveGhosts();
                _mode = Mode::idle;
            }
            invalidate();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_COPY:
                    StartMode(Mode::copy);
                    break;
                case WIDX_PASTE:
                    StartMode(Mode::paste);
                    break;
                case WIDX_ROTATE:
                    _rotation = (_rotation + 1) & 3;
                    RemoveGhosts();
                    break;
                case WIDX_EXPORT:
                    if (!gBlueprintClipboard.empty())
                    {
                        GetContext()->GetUiContext().SetClipboardText(gBlueprintClipboard.c_str());
                        _message = STR_FT_BLUEPRINT_EXPORTED;
                    }
                    break;
                case WIDX_IMPORT:
                {
                    char* text = SDL_GetClipboardText();
                    const std::string clipboard = text != nullptr ? text : "";
                    SDL_free(text);
                    if (parseBlueprint(clipboard).has_value())
                    {
                        gBlueprintClipboard = clipboard;
                        _rotation = 0;
                        StartMode(Mode::paste);
                        _message = STR_FT_BLUEPRINT_IMPORTED;
                    }
                    else
                    {
                        _message = STR_FT_BLUEPRINT_NOT_IN_CLIPBOARD;
                    }
                    break;
                }
            }
        }

        void onPrepareDraw() override
        {
            setWidgetDisabled(WIDX_PASTE, gBlueprintClipboard.empty());
            setWidgetDisabled(WIDX_EXPORT, gBlueprintClipboard.empty());
            setWidgetPressed(WIDX_COPY, _mode == Mode::copy);
            setWidgetPressed(WIDX_PASTE, _mode == Mode::paste);
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            auto pos = windowPos + ScreenCoordsXY{ 6, 56 };
            const auto blueprint = parseBlueprint(gBlueprintClipboard);
            if (blueprint)
            {
                auto ft = Formatter();
                ft.Add<int32_t>(static_cast<int32_t>(blueprint->entries.size()));
                ft.Add<int32_t>(_rotation % 2 == 0 ? blueprint->width : blueprint->height);
                ft.Add<int32_t>(_rotation % 2 == 0 ? blueprint->height : blueprint->width);
                drawText(rt, pos, STR_FT_BLUEPRINT_SIZE, ft);
            }
            else
            {
                drawText(rt, pos, STR_FT_BLUEPRINT_EMPTY);
            }
            pos.y += 12;
            if (_mode == Mode::paste && _cost != kMoney64Undefined)
            {
                auto ft = Formatter();
                ft.Add<money64>(_cost);
                drawText(rt, pos, STR_FT_BLUEPRINT_COST, ft);
            }
            else if (_message != kStringIdNone)
            {
                drawText(rt, pos, _message);
            }
            else
            {
                drawText(rt, pos, _mode == Mode::copy ? STR_FT_BLUEPRINT_HINT_COPY : STR_FT_BLUEPRINT_HINT_PASTE);
            }
        }

        void onToolUpdate(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (_mode == Mode::copy && !_selecting)
            {
                auto tile = CursorTile(screenCoords);
                if (tile)
                    ShowSelection(*tile, *tile);
                return;
            }
            if (_mode == Mode::paste)
                UpdateGhosts(screenCoords);
        }

        void onToolDown(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            auto tile = CursorTile(screenCoords);
            if (!tile)
                return;
            if (_mode == Mode::copy)
            {
                _selecting = true;
                _selectStart = *tile;
                ShowSelection(_selectStart, *tile);
                return;
            }
            if (_mode == Mode::paste)
                Paste(screenCoords);
        }

        void onToolDrag(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (_mode != Mode::copy || !_selecting)
                return;
            auto tile = CursorTile(screenCoords);
            if (tile)
                ShowSelection(_selectStart, *tile);
        }

        void onToolUp(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (_mode != Mode::copy || !_selecting)
                return;
            _selecting = false;
            auto tile = CursorTile(screenCoords);
            const CoordsXY end = tile ? *tile : _selectStart;
            const auto blueprint = captureBlueprint(getGameState(), MapRange{ _selectStart, end });
            gMapSelectFlags.unset(MapSelectFlag::enable);
            if (blueprint.empty())
            {
                _message = STR_FT_BLUEPRINT_NOTHING_COPIED;
                return;
            }
            gBlueprintClipboard = serialiseBlueprint(blueprint);
            _rotation = 0;
            _message = kStringIdNone;
            StartMode(Mode::paste);
        }

        void onToolAbort(WidgetIndex widgetIndex) override
        {
            RemoveGhosts();
            _selecting = false;
            gMapSelectFlags.unset(MapSelectFlag::enable);
            _mode = Mode::idle;
        }

    private:
        void StartMode(Mode mode)
        {
            // One tool serves both modes: switching only changes what it does (re-setting the tool while the input
            // system is mid-click leaves it waiting for a drag that never ends).
            RemoveGhosts();
            _mode = mode;
            _selecting = false;
            _cost = kMoney64Undefined;
            if (!isToolActive(WindowClass::factoryBlueprint, WIDX_BACKGROUND))
                ToolSet(*this, WIDX_BACKGROUND, Tool::crosshair);
            invalidate();
        }

        static std::optional<CoordsXY> CursorTile(const ScreenCoordsXY& screenCoords)
        {
            // Plain projection onto the ground, like the land tools: works over pieces and empty land alike.
            auto loc = ScreenGetMapXY(screenCoords, nullptr);
            if (!loc || !MapIsLocationValid(*loc))
                return std::nullopt;
            return loc->toTileStart();
        }

        static void ShowSelection(const CoordsXY& a, const CoordsXY& b)
        {
            gMapSelectFlags.set(MapSelectFlag::enable);
            gMapSelectType = MapSelectType::full;
            setMapSelectRange(MapRange{ a, b });
        }

        static GameActions::CommandFlags GhostFlags()
        {
            return { GameActions::CommandFlag::ghost, GameActions::CommandFlag::allowDuringPaused,
                     GameActions::CommandFlag::noSpend };
        }

        void RemoveGhosts()
        {
            for (const auto& loc : _ghostTiles)
            {
                auto action = GameActions::FactoryRemoveAction(loc);
                action.SetFlags(GhostFlags());
                GameActions::Execute(&action, getGameState());
            }
            _ghostTiles.clear();
            _ghostOrigin.reset();
        }

        std::optional<CoordsXYZ> PasteOrigin(const ScreenCoordsXY& screenCoords) const
        {
            auto tile = CursorTile(screenCoords);
            if (!tile)
                return std::nullopt;
            auto* surface = MapGetSurfaceElementAt(*tile);
            if (surface == nullptr)
                return std::nullopt;
            return CoordsXYZ{ *tile, surface->getBaseZ() };
        }

        void UpdateGhosts(const ScreenCoordsXY& screenCoords)
        {
            auto origin = PasteOrigin(screenCoords);
            if (origin == _ghostOrigin)
                return;
            RemoveGhosts();
            auto blueprint = parseBlueprint(gBlueprintClipboard);
            if (!origin || !blueprint)
                return;
            const auto rotated = rotateBlueprint(*blueprint, _rotation);
            ShowSelection(
                *origin, { origin->x + (rotated.width - 1) * kCoordsXYStep, origin->y + (rotated.height - 1) * kCoordsXYStep });
            auto action = GameActions::FactoryPlaceBlueprintAction(*origin, _rotation, gBlueprintClipboard);
            action.SetFlags(GhostFlags());
            const auto result = GameActions::Execute(&action, getGameState());
            _cost = result.error == GameActions::Status::ok ? result.cost : kMoney64Undefined;
            for (const auto& entry : rotated.entries)
                _ghostTiles.push_back(blueprintEntryLocation(entry, *origin));
            _ghostOrigin = origin;
        }

        void Paste(const ScreenCoordsXY& screenCoords)
        {
            auto origin = PasteOrigin(screenCoords);
            if (!origin)
                return;
            RemoveGhosts();
            auto action = GameActions::FactoryPlaceBlueprintAction(*origin, _rotation, gBlueprintClipboard);
            action.SetCallback([](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error == GameActions::Status::ok)
                    Audio::Play3D(Audio::SoundId::placeItem, result->position);
            });
            GameActions::Execute(&action, getGameState());
        }
    };

    WindowBase* FactoryBlueprintOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<FactoryBlueprintWindow>(WindowClass::factoryBlueprint, kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows

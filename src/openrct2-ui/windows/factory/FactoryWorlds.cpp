/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Worlds: every world of the company, which one is on screen, and creating new ones
// (ADR 0015). Viewing is local; creating goes through FactoryCreateWorldAction.

#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/GameState.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/WorldManager.h>
#include <openrct2/factory/actions/FactoryCreateWorldAction.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/ui/WindowManager.h>

namespace OpenRCT2::Ui::Windows
{
    namespace Worlds = OpenRCT2::Factory::Worlds;

    static constexpr ScreenSize kWindowSize = { 260, 170 };
    static constexpr int32_t kRowHeight = 24;

    enum WindowFactoryWorldsWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_LIST,
        WIDX_SIZE_DOWN,
        WIDX_SIZE_UP,
        WIDX_CREATE,
        WIDX_PRESET,
    };

    // clang-format off
    static constexpr auto kWindowFactoryWorldsWidgets = makeWidgets(
        makeWindowShim(STR_FT_WORLDS, kWindowSize),
        makeWidget({  4,  18}, {252,  94}, WidgetType::scroll, WindowColour::secondary, SCROLL_VERTICAL, STR_FT_WORLD_LIST_TIP),
        makeWidget({180, 133}, { 16,  14}, WidgetType::button, WindowColour::secondary, STR_NUMERIC_DOWN),
        makeWidget({198, 133}, { 16,  14}, WidgetType::button, WindowColour::secondary, STR_NUMERIC_UP),
        makeWidget({  4, 151}, {252,  14}, WidgetType::button, WindowColour::secondary, STR_FT_WORLD_CREATE, STR_FT_WORLD_CREATE_TIP),
        makeWidget({218, 116}, { 38,  14}, WidgetType::button, WindowColour::secondary, STR_FT_PRESET_NEXT, STR_FT_PRESET_NEXT_TIP)
    );
    static constexpr StringId kPresetNames[] = {
        STR_FT_PRESET_PLAIN, STR_FT_PRESET_DESERT, STR_FT_PRESET_ICE_MOON, STR_FT_PRESET_WEIRD,
    };
    // clang-format on

    class FactoryWorldsWindow final : public Window
    {
    private:
        uint16_t _newSize = 64;
        uint8_t _preset = 0;

    public:
        void onOpen() override
        {
            setWidgets(kWindowFactoryWorldsWidgets);
            WindowInitScrollWidgets(*this);
        }

        void onUpdate() override
        {
            invalidate();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_SIZE_DOWN:
                    _newSize = static_cast<uint16_t>(std::max(32, _newSize - 32));
                    break;
                case WIDX_SIZE_UP:
                    _newSize = static_cast<uint16_t>(std::min(256, _newSize + 32));
                    break;
                case WIDX_PRESET:
                    _preset = static_cast<uint8_t>((_preset + 1) % std::size(kPresetNames));
                    break;
                case WIDX_CREATE:
                {
                    auto action = GameActions::FactoryCreateWorldAction(_newSize, _preset);
                    GameActions::Execute(&action, getGameState());
                    break;
                }
            }
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            return { 0, static_cast<int32_t>(Worlds::count()) * kRowHeight };
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            const auto row = screenCoords.y / kRowHeight;
            if (row >= 0 && static_cast<size_t>(row) < Worlds::count())
                Worlds::setViewed(static_cast<Worlds::WorldId>(row));
        }

        void onScrollDraw(int32_t scrollIndex, Drawing::RenderTarget& rt) override
        {
            GfxClear(rt, getColourMap(colours[1].colour).midLight);
            for (Worlds::WorldId id = 0; id < Worlds::count(); id++)
            {
                const int32_t y = id * kRowHeight;
                if (id == Worlds::viewed())
                    Drawing::Rectangle::fill(
                        rt, { { 0, y }, { width, y + kRowHeight - 1 } }, getColourMap(colours[1].colour).lighter);
                const auto& state = Worlds::state(id);
                auto ft = Formatter();
                ft.Add<uint16_t>(id + 1);
                drawText(rt, { 4, y + 1 }, STR_FT_WORLD_NAME, ft);
                drawText(rt, { 140, y + 1 }, kPresetNames[std::min<size_t>(state.factory.parkExt.planet.preset, 3)]);
                if (id == Worlds::viewed())
                    drawText(rt, { 80, y + 1 }, STR_FT_WORLD_VIEWING);
                ft = Formatter();
                ft.Add<uint16_t>(state.mapSize.x);
                ft.Add<uint16_t>(state.mapSize.y);
                ft.Add<int32_t>(static_cast<int32_t>(state.factory.machines.aliveCount()));
                drawText(rt, { 12, y + 12 }, STR_FT_WORLD_DETAILS, ft);
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            auto ft = Formatter();
            ft.Add<uint16_t>(_newSize);
            ft.Add<uint16_t>(_newSize);
            drawText(rt, windowPos + ScreenCoordsXY{ 6, 134 }, STR_FT_WORLD_SIZE, ft);
            auto kind = Formatter();
            kind.Add<StringId>(kPresetNames[_preset]);
            drawText(rt, windowPos + ScreenCoordsXY{ 6, 117 }, STR_FT_WORLD_KIND, kind);
        }
    };

    WindowBase* FactoryWorldsOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<FactoryWorldsWindow>(WindowClass::factoryWorlds, kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows

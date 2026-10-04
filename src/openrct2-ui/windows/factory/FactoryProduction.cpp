/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Production statistics: per-item rates and a graph of the last ~20 minutes of
// production (green) and consumption (red), sampled by the simulation every kProductionSampleTicks.

#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/ui/WindowManager.h>
#include <string>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;

    static constexpr ScreenSize kWindowSize = { 440, 230 };
    static constexpr int32_t kRowHeight = 12;
    static constexpr int32_t kListWidth = 190;
    static constexpr ScreenRect kGraph = { { 204, 46 }, { 434, 210 } };

    enum WindowFactoryProductionWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_LIST,
    };

    // clang-format off
    static constexpr auto kWindowFactoryProductionWidgets = makeWidgets(
        makeWindowShim(STR_FT_PRODUCTION, kWindowSize),
        makeWidget({4, 18}, {kListWidth, 206}, WidgetType::scroll, WindowColour::secondary, SCROLL_VERTICAL, STR_FT_PRODUCTION_LIST_TIP)
    );
    // clang-format on

    // Items per minute from one sample period's count.
    static uint32_t perMinute(uint32_t count)
    {
        return static_cast<uint32_t>(uint64_t{ count } * 60 * kGameUpdateFPS / kProductionSampleTicks);
    }

    // The last completed sample (the one before `head`).
    static size_t lastSample(const ProductionStats& stats)
    {
        return (stats.head + kProductionSamples - 1) % kProductionSamples;
    }

    class FactoryProductionWindow final : public Window
    {
    private:
        ObjectEntryIndex _selected = kObjectEntryIndexNull;

    public:
        void onOpen() override
        {
            setWidgets(kWindowFactoryProductionWidgets);
            WindowInitScrollWidgets(*this);
        }

        void onUpdate() override
        {
            invalidate();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            if (widgetIndex == WIDX_CLOSE)
                close();
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            const auto rows = static_cast<int32_t>(getGameState().factory.production.history.size());
            return { 0, std::max(1, rows) * kRowHeight };
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            const auto& history = getGameState().factory.production.history;
            const auto row = static_cast<size_t>(screenCoords.y / kRowHeight);
            if (row < history.size())
                _selected = history[row].item;
        }

        void onScrollDraw(int32_t scrollIndex, Drawing::RenderTarget& rt) override
        {
            GfxClear(rt, getColourMap(colours[1].colour).midLight);
            const auto& stats = getGameState().factory.production;
            const auto sample = lastSample(stats);
            int32_t y = 0;
            for (const auto& entry : stats.history)
            {
                if (entry.item == _selected)
                    Drawing::Rectangle::fill(
                        rt, { { 0, y }, { kListWidth, y + kRowHeight - 1 } }, getColourMap(colours[1].colour).lighter);
                auto* proto = getPrototype(entry.item);
                static std::string name;
                name = proto != nullptr ? proto->GetName() : std::string();
                auto ft = Formatter();
                ft.Add<const char*>(name.c_str());
                drawTextEllipsised(rt, { 2, y }, 104, STR_FT_STRING_BLACK, ft);
                ft = Formatter();
                ft.Add<int32_t>(perMinute(entry.produced[sample]));
                ft.Add<int32_t>(perMinute(entry.consumed[sample]));
                drawText(rt, { 108, y }, STR_FT_PRODUCTION_RATE, ft);
                y += kRowHeight;
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            const auto& stats = getGameState().factory.production;
            const auto* entry = stats.historyOf(_selected);
            const auto origin = windowPos + kGraph.point1;
            if (entry == nullptr)
            {
                drawText(rt, windowPos + ScreenCoordsXY{ kGraph.point1.x, 20 }, STR_FT_PRODUCTION_PICK);
                return;
            }
            auto* proto = getPrototype(_selected);
            static std::string name;
            name = proto != nullptr ? proto->GetName() : std::string();
            auto ft = Formatter();
            ft.Add<const char*>(name.c_str());
            ft.Add<int32_t>(stats.count(_selected));
            ft.Add<int32_t>(stats.consumedCount(_selected));
            drawText(rt, windowPos + ScreenCoordsXY{ kGraph.point1.x, 20 }, STR_FT_PRODUCTION_TOTALS, ft);

            // Bars for the completed samples, oldest on the left: production up from the middle, consumption down.
            const int32_t graphWidth = kGraph.getWidth();
            const int32_t graphHeight = kGraph.getHeight();
            const int32_t middle = graphHeight / 2;
            Drawing::Rectangle::fillInset(
                rt, { origin, origin + ScreenCoordsXY{ graphWidth, graphHeight } }, colours[1],
                Drawing::Rectangle::BorderStyle::inset, Drawing::Rectangle::FillBrightness::dark);
            uint32_t peak = 1;
            for (size_t i = 0; i < kProductionSamples; i++)
                peak = std::max({ peak, entry->produced[i], entry->consumed[i] });
            const int32_t barWidth = std::max(1, (graphWidth - 4) / static_cast<int32_t>(kProductionSamples - 1));
            for (size_t n = 0; n + 1 < kProductionSamples; n++)
            {
                const size_t i = (stats.head + 1 + n) % kProductionSamples; // oldest first, the open period last
                const int32_t x = origin.x + 2 + static_cast<int32_t>(n) * barWidth;
                const int32_t up = static_cast<int32_t>(uint64_t{ entry->produced[i] } * (middle - 2) / peak);
                const int32_t down = static_cast<int32_t>(uint64_t{ entry->consumed[i] } * (middle - 2) / peak);
                if (up > 0)
                    Drawing::Rectangle::fill(
                        rt, { { x, origin.y + middle - up }, { x + barWidth - 2, origin.y + middle - 1 } },
                        getColourMap(Drawing::Colour::brightGreen).midLight);
                if (down > 0)
                    Drawing::Rectangle::fill(
                        rt, { { x, origin.y + middle + 1 }, { x + barWidth - 2, origin.y + middle + down } },
                        getColourMap(Drawing::Colour::brightRed).midLight);
            }
            Drawing::Rectangle::fill(
                rt, { { origin.x + 1, origin.y + middle }, { origin.x + graphWidth - 1, origin.y + middle } },
                getColourMap(colours[1].colour).lightest);
            ft = Formatter();
            ft.Add<int32_t>(perMinute(peak));
            drawText(rt, origin + ScreenCoordsXY{ 4, 2 }, STR_FT_PRODUCTION_PEAK, ft);
        }
    };

    WindowBase* FactoryProductionOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<FactoryProductionWindow>(WindowClass::factoryProduction, kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows

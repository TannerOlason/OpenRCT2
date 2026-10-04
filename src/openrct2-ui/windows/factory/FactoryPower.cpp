/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Power overview: one electric network at a time with its pole, generator and
// consumer counts, last tick's supply, demand and satisfaction, and a history graph sampled while open.

#include <algorithm>
#include <array>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/GameState.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Line.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/ui/WindowManager.h>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;

    static constexpr StringId kWindowTitle = STR_FT_POWER_OVERVIEW;
    static constexpr ScreenSize kWindowSize = { 260, 196 };
    static constexpr int32_t kHistoryLength = 120; // samples
    static constexpr int32_t kSampleTicks = 8;     // one sample every 8 ticks: 24 seconds of history
    static constexpr int32_t kGraphTop = 92;
    static constexpr int32_t kGraphHeight = 92;

    enum WindowFactoryPowerWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_PREVIOUS,
        WIDX_NEXT,
    };

    // clang-format off
    static constexpr auto kWindowFactoryPowerWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({206, 17}, { 24, 24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_PREVIOUS), STR_FT_PREVIOUS_NETWORK_TIP),
        makeWidget({232, 17}, { 24, 24}, WidgetType::flatBtn, WindowColour::secondary, ImageId(SPR_NEXT),     STR_FT_NEXT_NETWORK_TIP    )
    );
    // clang-format on

    class FactoryPowerWindow final : public Window
    {
    private:
        struct Sample
        {
            uint32_t supply{};
            uint32_t demand{};
        };

        RecordId _network = kNullRecord;
        std::vector<Sample> _history;
        int32_t _sampleCountdown = 0;

    public:
        void selectNetwork(RecordId network)
        {
            if (network != _network)
            {
                _network = network;
                _history.clear();
                _sampleCountdown = 0;
            }
            invalidate();
        }

        void onOpen() override
        {
            setWidgets(kWindowFactoryPowerWidgets);
            WindowInitScrollWidgets(*this);
        }

        void onUpdate() override
        {
            const auto& networks = getGameState().factory.powerNetworks;
            if (networks.get(_network) == nullptr)
                selectNetwork(FirstNetwork());
            if (--_sampleCountdown <= 0)
            {
                _sampleCountdown = kSampleTicks;
                if (const auto* network = networks.get(_network))
                {
                    _history.push_back({ network->lastSupply, network->lastDemand });
                    if (_history.size() > static_cast<size_t>(kHistoryLength))
                        _history.erase(_history.begin());
                }
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
                case WIDX_PREVIOUS:
                case WIDX_NEXT:
                    selectNetwork(Step(widgetIndex == WIDX_NEXT ? 1 : -1));
                    break;
            }
        }

        void onPrepareDraw() override
        {
            const bool several = getGameState().factory.powerNetworks.aliveCount() > 1;
            setWidgetDisabled(WIDX_PREVIOUS, !several);
            setWidgetDisabled(WIDX_NEXT, !several);
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            const auto& networks = getGameState().factory.powerNetworks;
            auto pos = windowPos + ScreenCoordsXY{ 6, 20 };
            const auto* network = networks.get(_network);
            if (network == nullptr)
            {
                drawText(rt, pos, STR_FT_POWER_NO_NETWORKS);
                return;
            }

            {
                uint16_t index = 0;
                uint16_t count = 0;
                networks.forEach([&](RecordId id, const PowerNetworkRecord&) {
                    count++;
                    if (id == _network)
                        index = count;
                });
                auto ft = Formatter();
                ft.Add<uint16_t>(index);
                ft.Add<uint16_t>(count);
                drawText(rt, pos, STR_FT_POWER_NETWORK, ft);
            }
            pos.y += 12;
            {
                auto ft = Formatter();
                ft.Add<uint16_t>(network->poleCount);
                ft.Add<uint16_t>(network->generatorCount);
                ft.Add<uint16_t>(network->consumerCount);
                drawText(rt, pos, STR_FT_POWER_COUNTS, ft);
            }
            pos.y += 12;
            {
                auto ft = Formatter();
                ft.Add<uint32_t>(network->lastSupply);
                drawText(rt, pos, STR_FT_POWER_SUPPLY, ft);
            }
            pos.y += 12;
            {
                auto ft = Formatter();
                ft.Add<uint32_t>(network->lastDemand);
                drawText(rt, pos, STR_FT_POWER_DEMAND, ft);
            }
            pos.y += 12;
            {
                auto ft = Formatter();
                ft.Add<uint16_t>(static_cast<uint16_t>((static_cast<uint64_t>(network->satisfactionQ16) * 100) >> 16));
                drawText(rt, pos, STR_FT_POWER_SATISFACTION, ft);
            }
            DrawGraph(rt);
        }

    private:
        static RecordId FirstNetwork()
        {
            RecordId first = kNullRecord;
            getGameState().factory.powerNetworks.forEach([&](RecordId id, const PowerNetworkRecord&) {
                if (first == kNullRecord)
                    first = id;
            });
            return first;
        }

        RecordId Step(int32_t delta) const
        {
            std::vector<RecordId> ids;
            getGameState().factory.powerNetworks.forEach([&](RecordId id, const PowerNetworkRecord&) { ids.push_back(id); });
            if (ids.empty())
                return kNullRecord;
            auto it = std::find(ids.begin(), ids.end(), _network);
            const auto current = it == ids.end() ? 0 : static_cast<int32_t>(it - ids.begin());
            const auto count = static_cast<int32_t>(ids.size());
            return ids[static_cast<size_t>(((current + delta) % count + count) % count)];
        }

        // Supply (green) and demand (yellow) over the sampled history, scaled to the largest value seen.
        void DrawGraph(Drawing::RenderTarget& rt) const
        {
            const ScreenCoordsXY topLeft = windowPos + ScreenCoordsXY{ 6, kGraphTop };
            const ScreenCoordsXY bottomRight = windowPos + ScreenCoordsXY{ kWindowSize.width - 7, kGraphTop + kGraphHeight };
            Drawing::Rectangle::fillInset(
                rt, { topLeft, bottomRight }, colours[1], Drawing::Rectangle::BorderStyle::inset,
                Drawing::Rectangle::FillBrightness::dark);
            if (_history.size() < 2)
                return;
            uint32_t peak = 1;
            for (const auto& sample : _history)
                peak = std::max({ peak, sample.supply, sample.demand });
            peak += peak / 8; // headroom so a flat line at the peak stays off the frame
            const int32_t graphWidth = bottomRight.x - topLeft.x - 4;
            const int32_t graphHeight = bottomRight.y - topLeft.y - 4;
            auto point = [&](size_t i, uint32_t value) {
                const int32_t x = topLeft.x + 2 + static_cast<int32_t>(i) * graphWidth / (kHistoryLength - 1);
                const int32_t y = bottomRight.y - 2 - static_cast<int32_t>(static_cast<uint64_t>(value) * graphHeight / peak);
                return ScreenCoordsXY{ x, y };
            };
            const auto supplyColour = getColourMap(Drawing::Colour::brightGreen).light;
            const auto demandColour = getColourMap(Drawing::Colour::yellow).light;
            for (size_t i = 1; i < _history.size(); i++)
            {
                GfxDrawLine(rt, { point(i - 1, _history[i - 1].supply), point(i, _history[i].supply) }, supplyColour);
                GfxDrawLine(rt, { point(i - 1, _history[i - 1].demand), point(i, _history[i].demand) }, demandColour);
            }
        }
    };

    WindowBase* FactoryPowerOpen(uint32_t network)
    {
        auto* windowMgr = GetWindowManager();
        auto* w = static_cast<FactoryPowerWindow*>(windowMgr->FindByClass(WindowClass::factoryPower));
        if (w == nullptr)
            w = windowMgr->Create<FactoryPowerWindow>(WindowClass::factoryPower, kWindowSize, {});
        if (w != nullptr)
        {
            w->selectNetwork(network);
            windowMgr->BringToFront(*w);
        }
        return w;
    }
} // namespace OpenRCT2::Ui::Windows

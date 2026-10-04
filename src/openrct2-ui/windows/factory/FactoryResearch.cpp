/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Factory research: the technology tree, what labs are researching and how far along
// they are. Choosing a technology goes through FactorySetParkOptionAction (researchTarget).

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
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Technology.h>
#include <openrct2/factory/actions/FactorySetParkOptionAction.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/ui/WindowManager.h>
#include <string>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;

    static constexpr ScreenSize kWindowSize = { 320, 250 };
    static constexpr int32_t kRowHeight = 12;
    static constexpr int32_t kListTop = 76;

    enum WindowFactoryResearchWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_LIST,
    };

    // clang-format off
    static constexpr auto kWindowFactoryResearchWidgets = makeWidgets(
        makeWindowShim(STR_FT_FACTORY_RESEARCH, kWindowSize),
        makeWidget({4, kListTop}, {312, 170}, WidgetType::scroll, WindowColour::secondary, SCROLL_VERTICAL, STR_FT_RESEARCH_LIST_TIP)
    );
    // clang-format on

    class FactoryResearchWindow final : public Window
    {
    public:
        void onOpen() override
        {
            setWidgets(kWindowFactoryResearchWidgets);
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
            return { 0, std::max<int32_t>(1, static_cast<int32_t>(loadedTechnologies().size())) * kRowHeight };
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            const auto& technologies = loadedTechnologies();
            const auto row = static_cast<size_t>(screenCoords.y / kRowHeight);
            if (row >= technologies.size())
                return;
            const auto technology = technologies[row];
            auto& gameState = getGameState();
            const auto target = gameState.factory.research.current == technology ? kObjectEntryIndexNull : technology;
            if (target != kObjectEntryIndexNull && !isTechnologyAvailable(gameState, target))
                return;
            auto action = GameActions::FactorySetParkOptionAction(GameActions::FactoryParkOption::researchTarget, target);
            GameActions::Execute(&action, gameState);
        }

        void onScrollDraw(int32_t scrollIndex, Drawing::RenderTarget& rt) override
        {
            GfxClear(rt, getColourMap(colours[1].colour).midLight);
            const auto& gameState = getGameState();
            const auto& research = gameState.factory.research;
            int32_t y = 0;
            for (const auto technology : loadedTechnologies())
            {
                auto* proto = getPrototype(technology);
                if (proto == nullptr)
                    continue;
                if (technology == research.current)
                    Drawing::Rectangle::fill(
                        rt, { { 0, y }, { width, y + kRowHeight - 1 } }, getColourMap(colours[1].colour).lighter);
                StringId status = STR_FT_TECH_LOCKED;
                if (research.isResearched(technology))
                    status = STR_FT_TECH_RESEARCHED;
                else if (technology == research.current)
                    status = STR_FT_TECH_RESEARCHING;
                else if (isTechnologyAvailable(gameState, technology))
                    status = STR_FT_TECH_AVAILABLE;
                static std::string name;
                name = proto->GetName();
                auto ft = Formatter();
                ft.Add<const char*>(name.c_str());
                drawTextEllipsised(rt, { 2, y }, 190, STR_FT_STRING_BLACK, ft);
                drawText(rt, { 196, y }, status);
                y += kRowHeight;
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            const auto& research = getGameState().factory.research;
            auto screenCoords = windowPos + ScreenCoordsXY{ 6, 20 };
            auto* proto = getPrototype(research.current);
            if (proto == nullptr || proto->getKind() != PrototypeKind::technology)
            {
                drawText(rt, screenCoords, loadedTechnologies().empty() ? STR_FT_NO_TECHNOLOGIES : STR_FT_RESEARCH_NOTHING);
                return;
            }
            const auto& tech = proto->getTechnology();
            static std::string name;
            name = proto->GetName();
            auto ft = Formatter();
            ft.Add<const char*>(name.c_str());
            drawText(rt, screenCoords, STR_FT_RESEARCHING_LABEL, ft);

            // Progress bar: units done out of the technology's units.
            const auto done = std::min<uint32_t>(research.unitsDone(research.current), tech.units);
            const int32_t barLeft = 6;
            const int32_t barWidth = 308;
            const auto barTop = windowPos.y + 34;
            Drawing::Rectangle::fillInset(
                rt, { { windowPos.x + barLeft, barTop }, { windowPos.x + barLeft + barWidth - 1, barTop + 9 } }, colours[1],
                Drawing::Rectangle::BorderStyle::inset, Drawing::Rectangle::FillBrightness::dark);
            const int32_t filled = static_cast<int32_t>((barWidth - 2) * done / std::max<uint32_t>(1, tech.units));
            if (filled > 0)
                Drawing::Rectangle::fill(
                    rt, { { windowPos.x + barLeft + 1, barTop + 1 }, { windowPos.x + barLeft + filled, barTop + 8 } },
                    getColourMap(colours[1].colour).midLight);

            ft = Formatter();
            ft.Add<uint32_t>(done);
            ft.Add<uint32_t>(tech.units);
            drawText(rt, windowPos + ScreenCoordsXY{ 6, 46 }, STR_FT_RESEARCH_UNITS, ft);

            // Packs each unit takes.
            static std::string packs;
            packs.clear();
            for (const auto& pack : tech.packs)
            {
                auto* item = getPrototype(pack.item.resolve());
                if (!packs.empty())
                    packs += ", ";
                packs += std::to_string(pack.count) + " x " + (item != nullptr ? item->GetName() : pack.item.identifier);
            }
            ft = Formatter();
            ft.Add<const char*>(packs.c_str());
            drawText(rt, windowPos + ScreenCoordsXY{ 6, 58 }, STR_FT_RESEARCH_PACKS, ft);
        }
    };

    WindowBase* FactoryResearchOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<FactoryResearchWindow>(WindowClass::factoryResearch, kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows

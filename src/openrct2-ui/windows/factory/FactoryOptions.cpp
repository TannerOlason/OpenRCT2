/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Factory options: how the park's factory and economy meet (construction mode, shop
// stock, rating) and the item a "produce" objective counts. Every change goes through FactorySetParkOptionAction.

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/actions/FactorySetParkOptionAction.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/object/ObjectList.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/scenario/ScenarioObjective.h>
#include <openrct2/ui/WindowManager.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;
    using GameActions::FactoryParkOption;

    static constexpr ScreenSize kWindowSize = { 280, 118 };

    enum WindowFactoryOptionsWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_CONSTRUCTION,
        WIDX_CONSTRUCTION_BUTTON,
        WIDX_SHOP_STOCK,
        WIDX_SHOP_STOCK_BUTTON,
        WIDX_RATING,
        WIDX_OBJECTIVE_ITEM,
        WIDX_OBJECTIVE_ITEM_BUTTON,
    };

    // clang-format off
    static constexpr auto kWindowFactoryOptionsWidgets = makeWidgets(
        makeWindowShim(STR_FT_FACTORY_OPTIONS, kWindowSize),
        makeWidget({130, 20}, {144, 12}, WidgetType::dropdownMenu, WindowColour::secondary),
        makeWidget({262, 21}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH),
        makeWidget({130, 38}, {144, 12}, WidgetType::dropdownMenu, WindowColour::secondary),
        makeWidget({262, 39}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH),
        makeWidget({  6, 56}, {268, 12}, WidgetType::checkbox,     WindowColour::secondary, STR_FT_OPTION_AFFECTS_RATING),
        makeWidget({130, 92}, {144, 12}, WidgetType::dropdownMenu, WindowColour::secondary),
        makeWidget({262, 93}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH)
    );
    // clang-format on

    static constexpr StringId kConstructionModeNames[] = {
        STR_FT_CONSTRUCTION_MONEY,
        STR_FT_CONSTRUCTION_HYBRID,
        STR_FT_CONSTRUCTION_MATERIALS,
    };
    static constexpr StringId kShopStockNames[] = {
        STR_FT_SHOP_STOCK_INFINITE,
        STR_FT_SHOP_STOCK_WAREHOUSE,
    };

    class FactoryOptionsWindow final : public Window
    {
    private:
        std::vector<ObjectEntryIndex> _items;

    public:
        void onOpen() override
        {
            setWidgets(kWindowFactoryOptionsWidgets);
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
            else if (widgetIndex == WIDX_RATING)
                SetOption(
                    FactoryParkOption::affectsRating, getGameState().park.flags.has(ParkFlag::factoryAffectsRating) ? 0 : 1);
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CONSTRUCTION_BUTTON:
                    ShowDropdown(widgetIndex, kConstructionModeNames, getGameState().factory.parkExt.constructionMode);
                    break;
                case WIDX_SHOP_STOCK_BUTTON:
                    ShowDropdown(widgetIndex, kShopStockNames, getGameState().factory.parkExt.shopStockMode);
                    break;
                case WIDX_OBJECTIVE_ITEM_BUTTON:
                    ShowItemDropdown();
                    break;
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t index) override
        {
            if (index < 0)
                return;
            switch (widgetIndex)
            {
                case WIDX_CONSTRUCTION_BUTTON:
                    SetOption(FactoryParkOption::constructionMode, static_cast<uint16_t>(index));
                    break;
                case WIDX_SHOP_STOCK_BUTTON:
                    SetOption(FactoryParkOption::shopStockMode, static_cast<uint16_t>(index));
                    break;
                case WIDX_OBJECTIVE_ITEM_BUTTON:
                    if (static_cast<size_t>(index) < _items.size())
                        SetOption(FactoryParkOption::objectiveItem, _items[index]);
                    break;
            }
        }

        void onPrepareDraw() override
        {
            const auto& gameState = getGameState();
            const auto& ext = gameState.factory.parkExt;
            widgets[WIDX_CONSTRUCTION].setString(kConstructionModeNames[std::min<uint8_t>(ext.constructionMode, 2)]);
            widgets[WIDX_SHOP_STOCK].setString(kShopStockNames[std::min<uint8_t>(ext.shopStockMode, 1)]);
            setWidgetPressed(WIDX_RATING, gameState.park.flags.has(ParkFlag::factoryAffectsRating));
            const bool produce = gameState.scenarioOptions.objective.Type == Scenario::ObjectiveType::produceItemsBy;
            widgets[WIDX_OBJECTIVE_ITEM].setVisible(produce);
            widgets[WIDX_OBJECTIVE_ITEM_BUTTON].setVisible(produce);
            if (produce)
            {
                static std::string itemName;
                auto* proto = getPrototype(gameState.scenarioOptions.objective.NumGuests);
                itemName = proto != nullptr ? proto->GetName() : std::string();
                widgets[WIDX_OBJECTIVE_ITEM].setString(itemName.c_str());
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);
            drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_CONSTRUCTION].top + 1 }, STR_FT_OPTION_CONSTRUCTION);
            drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_SHOP_STOCK].top + 1 }, STR_FT_OPTION_SHOP_STOCK);
            if (widgets[WIDX_OBJECTIVE_ITEM].isVisible())
                drawText(
                    rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_OBJECTIVE_ITEM].top + 1 }, STR_FT_OPTION_OBJECTIVE_ITEM);
            else
                drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_OBJECTIVE_ITEM].top + 1 }, STR_FT_OPTION_NO_OBJECTIVE);
        }

    private:
        static void SetOption(FactoryParkOption option, uint16_t value)
        {
            auto action = GameActions::FactorySetParkOptionAction(option, value);
            GameActions::Execute(&action, getGameState());
        }

        template<size_t N>
        void ShowDropdown(WidgetIndex buttonIndex, const StringId (&names)[N], uint8_t current)
        {
            for (size_t i = 0; i < N; i++)
                gDropdown.items[i] = Dropdown::MenuLabel(names[i]);
            Widget* widget = &widgets[buttonIndex - 1];
            WindowDropdownShowTextCustomWidth(
                { widget->left + windowPos.x, widget->top + windowPos.y }, widget->height(), colours[1], 0, {}, N,
                widget->width() - 1 + 3);
            if (current < N)
                gDropdown.items[current].setChecked(true);
        }

        void ShowItemDropdown()
        {
            _items.clear();
            auto& objectManager = GetContext()->GetObjectManager();
            const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
            for (size_t i = 0; i < count && _items.size() < 200; i++)
            {
                auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
                if (proto != nullptr && proto->getKind() == PrototypeKind::item && !proto->isFluid())
                    _items.push_back(static_cast<ObjectEntryIndex>(i));
            }
            const auto current = getGameState().scenarioOptions.objective.NumGuests;
            for (size_t i = 0; i < _items.size(); i++)
            {
                auto* proto = getPrototype(_items[i]);
                gDropdown.items[i] = Dropdown::MenuLabel(proto != nullptr ? proto->GetName() : std::string());
            }
            Widget* widget = &widgets[WIDX_OBJECTIVE_ITEM];
            WindowDropdownShowTextCustomWidth(
                { widget->left + windowPos.x, widget->top + windowPos.y }, widget->height(), colours[1], 0, {}, _items.size(),
                widget->width() - 1 + 3);
            for (size_t i = 0; i < _items.size(); i++)
                if (_items[i] == current)
                    gDropdown.items[i].setChecked(true);
        }
    };

    WindowBase* FactoryOptionsOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<FactoryOptionsWindow>(WindowClass::factoryOptions, kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows

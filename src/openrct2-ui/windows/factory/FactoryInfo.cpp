/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Info window for a machine, chest, pipe or splitter: status, recipe, slots, fluids,
// progress, and a splitter's filter and priorities.

#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/factory/Combat.h>
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Fluids.h>
#include <openrct2/factory/actions/FactoryMarketSellAction.h>
#include <openrct2/factory/actions/FactorySetFilterAction.h>
#include <openrct2/factory/actions/FactorySetRecipeAction.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/object/ObjectList.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/tile_element/FactoryElement.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Factory;

    static constexpr StringId kWindowTitle = STR_FT_FACTORY;
    static constexpr ScreenSize kWindowSize = { 230, 170 };
    static constexpr int32_t kSlotSize = 26;
    static constexpr int32_t kSlotsPerRow = 8;

    enum WindowFactoryInfoWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_RECIPE_DROPDOWN,
        WIDX_RECIPE_DROPDOWN_BUTTON,
        WIDX_FILTER_DROPDOWN,
        WIDX_FILTER_DROPDOWN_BUTTON,
        WIDX_INPUT_PRIORITY_DROPDOWN,
        WIDX_INPUT_PRIORITY_DROPDOWN_BUTTON,
        WIDX_OUTPUT_PRIORITY_DROPDOWN,
        WIDX_OUTPUT_PRIORITY_DROPDOWN_BUTTON,
        WIDX_WAREHOUSE_SCROLL,
    };

    // clang-format off
    static constexpr auto kWindowFactoryInfoWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({ 60, 44}, {164, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                     ),
        makeWidget({212, 45}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH, STR_FT_SELECT_RECIPE_TIP),
        makeWidget({ 96, 32}, {128, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                     ),
        makeWidget({212, 33}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH, STR_FT_SELECT_FILTER_TIP),
        makeWidget({ 96, 48}, {128, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                     ),
        makeWidget({212, 49}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH, STR_FT_SELECT_PRIORITY_TIP),
        makeWidget({ 96, 64}, {128, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                     ),
        makeWidget({212, 65}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH, STR_FT_SELECT_PRIORITY_TIP),
        makeWidget({  4, 44}, {222, 108}, WidgetType::scroll,      WindowColour::secondary, SCROLL_VERTICAL,    STR_FT_SELL_STACK_TIP)
    );
    // clang-format on

    class FactoryInfoWindow final : public Window
    {
    private:
        CoordsXYZ _loc{};
        std::vector<ObjectEntryIndex> _recipeChoices;
        std::vector<ObjectEntryIndex> _filterChoices;

    public:
        void initialise(const CoordsXYZ& loc)
        {
            _loc = loc;
            RefreshRecipes();
        }

        void onOpen() override
        {
            setWidgets(kWindowFactoryInfoWidgets);
            WindowInitScrollWidgets(*this);
        }

        void onUpdate() override
        {
            // Slots and progress change every tick.
            if (FindElement() == nullptr)
            {
                close();
                return;
            }
            invalidate();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            if (widgetIndex == WIDX_CLOSE)
                close();
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            const auto rows = (static_cast<int32_t>(WarehouseStacks().size()) + kSlotsPerRow - 1) / kSlotsPerRow;
            return { 0, std::max(1, rows) * kSlotSize };
        }

        void onScrollDraw(int32_t scrollIndex, Drawing::RenderTarget& rt) override
        {
            GfxClear(rt, getColourMap(colours[1].colour).midLight);
            DrawSlots(rt, { 1, 1 }, WarehouseStacks());
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            // Clicking a stack sells all of it to the Market.
            const auto& stock = getGameState().factory.warehouse.stock;
            const int32_t column = screenCoords.x / kSlotSize;
            const auto index = static_cast<size_t>((screenCoords.y / kSlotSize) * kSlotsPerRow + column);
            if (column >= kSlotsPerRow || index >= stock.size())
                return;
            auto action = GameActions::FactoryMarketSellAction(stock[index].item, stock[index].count);
            GameActions::Execute(&action, getGameState());
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            if (widgetIndex == WIDX_FILTER_DROPDOWN_BUTTON || widgetIndex == WIDX_INPUT_PRIORITY_DROPDOWN_BUTTON
                || widgetIndex == WIDX_OUTPUT_PRIORITY_DROPDOWN_BUTTON)
            {
                ShowSplitterDropdown(widgetIndex);
                return;
            }
            if (widgetIndex != WIDX_RECIPE_DROPDOWN_BUTTON)
                return;
            RefreshRecipes();
            auto* machine = FindMachine();
            if (machine == nullptr || _recipeChoices.empty())
                return;

            gDropdown.items[0] = Dropdown::MenuLabel(STR_FT_NO_RECIPE);
            size_t count = 1;
            for (auto recipe : _recipeChoices)
            {
                auto* proto = getPrototype(recipe);
                gDropdown.items[count++] = Dropdown::MenuLabel(proto != nullptr ? proto->GetName() : std::string());
            }
            Widget* widget = &widgets[WIDX_RECIPE_DROPDOWN];
            WindowDropdownShowTextCustomWidth(
                { widget->left + windowPos.x, widget->top + windowPos.y }, widget->height(), colours[1], 0, {}, count,
                widget->width() - 1 + 3);
            if (machine->recipe == kObjectEntryIndexNull)
                gDropdown.items[0].setChecked(true);
            for (size_t i = 0; i < _recipeChoices.size(); i++)
                if (_recipeChoices[i] == machine->recipe)
                    gDropdown.items[i + 1].setChecked(true);
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex) override
        {
            if (dropdownIndex < 0)
                return;
            if (auto* splitter = FindSplitter(); splitter != nullptr)
            {
                auto filter = splitter->filter;
                auto inputPriority = splitter->inputPriority;
                auto outputPriority = splitter->outputPriority;
                if (widgetIndex == WIDX_FILTER_DROPDOWN_BUTTON)
                    filter = dropdownIndex >= 1 && static_cast<size_t>(dropdownIndex - 1) < _filterChoices.size()
                        ? _filterChoices[dropdownIndex - 1]
                        : kObjectEntryIndexNull;
                else if (widgetIndex == WIDX_INPUT_PRIORITY_DROPDOWN_BUTTON)
                    inputPriority = static_cast<uint8_t>(std::min(dropdownIndex, 2));
                else if (widgetIndex == WIDX_OUTPUT_PRIORITY_DROPDOWN_BUTTON)
                    outputPriority = static_cast<uint8_t>(std::min(dropdownIndex, 2));
                else
                    return;
                auto action = GameActions::FactorySetFilterAction(_loc, filter, inputPriority, outputPriority);
                GameActions::Execute(&action, getGameState());
                return;
            }
            if (widgetIndex != WIDX_RECIPE_DROPDOWN_BUTTON)
                return;
            ObjectEntryIndex recipe = kObjectEntryIndexNull;
            if (dropdownIndex >= 1 && static_cast<size_t>(dropdownIndex - 1) < _recipeChoices.size())
                recipe = _recipeChoices[dropdownIndex - 1];
            auto action = GameActions::FactorySetRecipeAction(_loc, recipe);
            GameActions::Execute(&action, getGameState());
        }

        void onPrepareDraw() override
        {
            auto* machine = FindMachine();
            auto* proto = machine != nullptr ? getPrototype(machine->entry) : nullptr;
            const bool choosesRecipe = proto != nullptr && machine->getKind() == MachineKind::assembler;
            widgets[WIDX_RECIPE_DROPDOWN].setVisible(choosesRecipe);
            widgets[WIDX_RECIPE_DROPDOWN_BUTTON].setVisible(choosesRecipe);
            if (choosesRecipe && machine->recipe != kObjectEntryIndexNull)
            {
                auto* recipeProto = getPrototype(machine->recipe);
                static std::string recipeName;
                recipeName = recipeProto != nullptr ? recipeProto->GetName() : std::string();
                widgets[WIDX_RECIPE_DROPDOWN].setString(recipeName.c_str());
            }
            else
            {
                widgets[WIDX_RECIPE_DROPDOWN].setString(STR_FT_NO_RECIPE);
            }

            widgets[WIDX_WAREHOUSE_SCROLL].setVisible(IsWarehouseDepot());

            auto* splitter = FindSplitter();
            for (auto widx :
                 { WIDX_FILTER_DROPDOWN, WIDX_FILTER_DROPDOWN_BUTTON, WIDX_INPUT_PRIORITY_DROPDOWN,
                   WIDX_INPUT_PRIORITY_DROPDOWN_BUTTON, WIDX_OUTPUT_PRIORITY_DROPDOWN, WIDX_OUTPUT_PRIORITY_DROPDOWN_BUTTON })
                widgets[widx].setVisible(splitter != nullptr);
            if (splitter != nullptr)
            {
                auto* filterProto = getPrototype(splitter->filter);
                static std::string filterName;
                filterName = filterProto != nullptr ? filterProto->GetName() : std::string();
                if (filterProto != nullptr)
                    widgets[WIDX_FILTER_DROPDOWN].setString(filterName.c_str());
                else
                    widgets[WIDX_FILTER_DROPDOWN].setString(STR_FT_NO_FILTER);
                widgets[WIDX_INPUT_PRIORITY_DROPDOWN].setString(PriorityString(splitter->inputPriority));
                widgets[WIDX_OUTPUT_PRIORITY_DROPDOWN].setString(PriorityString(splitter->outputPriority));
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            drawWidgets(rt);

            auto* element = FindElement();
            if (element == nullptr)
                return;
            auto* proto = getPrototype(*element);
            auto& state = getGameState().factory;

            auto pos = windowPos + ScreenCoordsXY{ 6, 20 };
            if (proto != nullptr)
            {
                static std::string name;
                name = proto->GetName();
                auto ft = Formatter();
                ft.Add<StringId>(STR_STRING);
                ft.Add<const char*>(name.c_str());
                drawText(rt, pos, STR_BLACK_STRING, ft);
            }
            pos.y += 12;

            if (element->getSubtype() == FactoryElementSubtype::container)
            {
                auto* container = state.containers.get(element->getRecordId());
                if (container == nullptr)
                    return;
                if (proto != nullptr && proto->getContainer().warehouse)
                {
                    // A depot shows the park-wide stock it feeds (in the scroll) and what the Market has paid.
                    drawText(rt, pos, STR_FT_WAREHOUSE);
                    auto ft = Formatter();
                    ft.Add<money64>(state.market.goodsSold);
                    drawText(
                        rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_WAREHOUSE_SCROLL].bottom + 3 }, STR_FT_MARKET_PRICE,
                        ft);
                    return;
                }
                drawText(rt, pos, STR_FT_CONTENTS);
                pos.y += 12;
                DrawSlots(rt, pos, container->slots);
                return;
            }

            if (element->getSubtype() == FactoryElementSubtype::splitter)
            {
                drawText(rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_FILTER_DROPDOWN].top + 1 }, STR_FT_FILTER);
                drawText(
                    rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_INPUT_PRIORITY_DROPDOWN].top + 1 }, STR_FT_INPUT_PRIORITY);
                drawText(
                    rt, windowPos + ScreenCoordsXY{ 6, widgets[WIDX_OUTPUT_PRIORITY_DROPDOWN].top + 1 },
                    STR_FT_OUTPUT_PRIORITY);
                return;
            }

            if (element->getSubtype() == FactoryElementSubtype::pipe)
            {
                auto* pipe = state.pipes.get(element->getRecordId());
                if (pipe != nullptr)
                    DrawFluidNetwork(rt, pos, state.fluidNetworks.get(pipe->network));
                return;
            }

            auto* machine = state.machines.get(element->getRecordId());
            if (machine == nullptr)
                return;
            drawText(rt, pos, StatusString(machine->getStatus()));
            if (const auto [health, maxHealth] = machineHealth(*machine); maxHealth > 0)
            {
                auto ft = Formatter();
                ft.Add<int32_t>(health);
                ft.Add<int32_t>(maxHealth);
                drawText(rt, pos + ScreenCoordsXY{ 112, 0 }, STR_FT_HEALTH, ft);
            }
            pos.y += 12;
            if (proto != nullptr && machine->getKind() == MachineKind::assembler)
            {
                drawText(rt, pos, STR_FT_RECIPE);
                pos.y += 14;
            }

            if (proto != nullptr && proto->getMachine().energy == EnergySource::burner)
            {
                drawText(rt, pos, STR_FT_FUEL);
                std::vector<ItemStack> fuel{ machine->fuel };
                DrawSlots(rt, pos + ScreenCoordsXY{ 54, -2 }, fuel);
                pos.y += kSlotSize + 2;
            }
            if (!machine->inputs.empty())
            {
                drawText(rt, pos, STR_FT_INPUTS);
                DrawSlots(rt, pos + ScreenCoordsXY{ 54, -2 }, machine->inputs);
                pos.y += kSlotSize + 2;
            }
            if (!machine->outputs.empty())
            {
                drawText(rt, pos, STR_FT_OUTPUTS);
                DrawSlots(rt, pos + ScreenCoordsXY{ 54, -2 }, machine->outputs);
                pos.y += kSlotSize + 2;
            }
            for (size_t box = 0; box < machine->fluidNetworks.size(); box++)
            {
                DrawFluidNetwork(rt, pos, machineFluidNetwork(state, *machine, box));
                pos.y += 12;
            }

            // Progress bar.
            if (machine->craftCost > 0)
            {
                const int32_t barWidth = kWindowSize.width - 12;
                const int32_t filled = static_cast<int32_t>(
                    static_cast<uint64_t>(std::min(machine->progress, machine->craftCost)) * barWidth / machine->craftCost);
                const ScreenRect frame{ pos, pos + ScreenCoordsXY{ barWidth, 8 } };
                Drawing::Rectangle::fillInset(
                    rt, frame, colours[1], Drawing::Rectangle::BorderStyle::inset, Drawing::Rectangle::FillBrightness::dark);
                if (filled > 2)
                {
                    Drawing::Rectangle::fill(
                        rt, { pos + ScreenCoordsXY{ 1, 1 }, pos + ScreenCoordsXY{ filled - 1, 7 } },
                        getColourMap(colours[1].colour).midLight);
                }
            }
        }

    private:
        FactoryElement* FindElement() const
        {
            return findFactoryElement(_loc);
        }

        MachineRecord* FindMachine() const
        {
            auto* element = FindElement();
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::machine || !element->hasRecord())
                return nullptr;
            return getGameState().factory.machines.get(element->getRecordId());
        }

        bool IsWarehouseDepot() const
        {
            auto* element = FindElement();
            auto* proto = element != nullptr ? getPrototype(*element) : nullptr;
            return proto != nullptr && element->getSubtype() == FactoryElementSubtype::container
                && proto->getContainer().warehouse;
        }

        std::vector<ItemStack> WarehouseStacks() const
        {
            std::vector<ItemStack> stock;
            for (const auto& entry : getGameState().factory.warehouse.stock)
                stock.push_back({ entry.item, static_cast<uint16_t>(std::min<uint32_t>(entry.count, 0xFFFF)) });
            return stock;
        }

        SplitterRecord* FindSplitter() const
        {
            auto* element = FindElement();
            if (element == nullptr || element->getSubtype() != FactoryElementSubtype::splitter || !element->hasRecord())
                return nullptr;
            return getGameState().factory.splitters.get(element->getRecordId());
        }

        static StringId PriorityString(uint8_t priority)
        {
            switch (priority)
            {
                case kSplitterPriorityLeft:
                    return STR_FT_PRIORITY_LEFT;
                case kSplitterPriorityRight:
                    return STR_FT_PRIORITY_RIGHT;
                default:
                    return STR_FT_PRIORITY_NONE;
            }
        }

        void ShowSplitterDropdown(WidgetIndex buttonIndex)
        {
            auto* splitter = FindSplitter();
            if (splitter == nullptr)
                return;
            Widget* widget = &widgets[buttonIndex - 1];
            size_t count = 0;
            size_t checked = 0;
            if (buttonIndex == WIDX_FILTER_DROPDOWN_BUTTON)
            {
                // Every placeable-on-belt item: loaded item prototypes that are not fluids.
                _filterChoices.clear();
                auto& objectManager = GetContext()->GetObjectManager();
                const auto total = getObjectEntryGroupCount(ObjectType::factoryPrototype);
                for (size_t i = 0; i < total && _filterChoices.size() < static_cast<size_t>(Dropdown::kItemsMaxSize - 1); i++)
                {
                    auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
                    if (proto != nullptr && proto->getKind() == PrototypeKind::item && !proto->isFluid())
                        _filterChoices.push_back(static_cast<ObjectEntryIndex>(i));
                }
                gDropdown.items[count++] = Dropdown::MenuLabel(STR_FT_NO_FILTER);
                for (auto item : _filterChoices)
                {
                    if (item == splitter->filter)
                        checked = count;
                    auto* proto = getPrototype(item);
                    gDropdown.items[count++] = Dropdown::MenuLabel(proto != nullptr ? proto->GetName() : std::string());
                }
            }
            else
            {
                const uint8_t current = buttonIndex == WIDX_INPUT_PRIORITY_DROPDOWN_BUTTON ? splitter->inputPriority
                                                                                           : splitter->outputPriority;
                for (uint8_t priority = kSplitterPriorityNone; priority <= kSplitterPriorityRight; priority++)
                {
                    if (priority == current)
                        checked = count;
                    gDropdown.items[count++] = Dropdown::MenuLabel(PriorityString(priority));
                }
            }
            WindowDropdownShowTextCustomWidth(
                { widget->left + windowPos.x, widget->top + windowPos.y }, widget->height(), colours[1], 0, {}, count,
                widget->width() - 1 + 3);
            gDropdown.items[checked].setChecked(true);
        }

        void RefreshRecipes()
        {
            _recipeChoices.clear();
            auto* machine = FindMachine();
            auto* machineProto = machine != nullptr ? getPrototype(machine->entry) : nullptr;
            if (machineProto == nullptr)
                return;
            auto& objectManager = GetContext()->GetObjectManager();
            const auto count = getObjectEntryGroupCount(ObjectType::factoryPrototype);
            for (size_t i = 0; i < count && _recipeChoices.size() < 200; i++)
            {
                auto* proto = objectManager.GetLoadedObject<FactoryPrototypeObject>(i);
                if (proto != nullptr && proto->getKind() == PrototypeKind::recipe
                    && machineProto->machineHandlesCategory(proto->getRecipe().category)
                    && isPrototypeUnlocked(getGameState(), static_cast<ObjectEntryIndex>(i)))
                {
                    _recipeChoices.push_back(static_cast<ObjectEntryIndex>(i));
                }
            }
        }

        static StringId StatusString(MachineStatus status)
        {
            switch (status)
            {
                case MachineStatus::working:
                    return STR_FT_STATUS_WORKING;
                case MachineStatus::noInput:
                    return STR_FT_STATUS_NO_INPUT;
                case MachineStatus::outputFull:
                    return STR_FT_STATUS_OUTPUT_FULL;
                case MachineStatus::noFuel:
                    return STR_FT_STATUS_NO_FUEL;
                case MachineStatus::noPower:
                    return STR_FT_STATUS_NO_POWER;
                case MachineStatus::noOre:
                    return STR_FT_STATUS_NO_ORE;
                case MachineStatus::noRecipe:
                    return STR_FT_STATUS_NO_RECIPE;
                case MachineStatus::destroyed:
                    return STR_FT_STATUS_DESTROYED;
                case MachineStatus::noAmmo:
                    return STR_FT_STATUS_NO_AMMO;
                default:
                    return STR_FT_STATUS_IDLE;
            }
        }

        // "Fluid  Water: 1,200 / 3,000", or the capacity when the network is empty.
        void DrawFluidNetwork(Drawing::RenderTarget& rt, ScreenCoordsXY pos, const FluidNetworkRecord* network)
        {
            if (network == nullptr)
                return;
            drawText(rt, pos, STR_FT_FLUID);
            auto* fluidProto = network->amount > 0 ? getPrototype(network->fluid) : nullptr;
            auto ft = Formatter();
            if (fluidProto != nullptr)
            {
                static std::string fluidName;
                fluidName = fluidProto->GetName();
                ft.Add<const char*>(fluidName.c_str());
                ft.Add<uint32_t>(network->amount);
                ft.Add<uint32_t>(network->capacity);
                drawText(rt, pos + ScreenCoordsXY{ 54, 0 }, STR_FT_FLUID_AMOUNT, ft);
            }
            else
            {
                ft.Add<uint32_t>(network->capacity);
                drawText(rt, pos + ScreenCoordsXY{ 54, 0 }, STR_FT_FLUID_EMPTY, ft);
            }
        }

        void DrawSlots(Drawing::RenderTarget& rt, ScreenCoordsXY pos, const std::vector<ItemStack>& slots)
        {
            ScreenCoordsXY cursor = pos;
            int32_t column = 0;
            for (const auto& slot : slots)
            {
                const ScreenRect frame{ cursor, cursor + ScreenCoordsXY{ kSlotSize - 2, kSlotSize - 2 } };
                Drawing::Rectangle::fillInset(
                    rt, frame, colours[1], Drawing::Rectangle::BorderStyle::inset, Drawing::Rectangle::FillBrightness::dark);
                if (!slot.isEmpty())
                {
                    auto* itemProto = getPrototype(slot.item);
                    if (itemProto != nullptr)
                    {
                        auto icon = itemProto->getItemIconImage();
                        if (icon != kImageIndexUndefined)
                        {
                            GfxDrawSprite(rt, ImageId(icon), cursor + ScreenCoordsXY{ kSlotSize / 2 - 1, kSlotSize / 2 - 3 });
                        }
                    }
                    auto ft = Formatter();
                    ft.Add<StringId>(STR_COMMA16);
                    ft.Add<uint16_t>(slot.count);
                    drawText(
                        rt, cursor + ScreenCoordsXY{ kSlotSize - 4, kSlotSize - 12 }, STR_BLACK_STRING, ft,
                        { TextAlignment::right });
                }
                cursor.x += kSlotSize;
                if (++column >= kSlotsPerRow)
                {
                    column = 0;
                    cursor.x = pos.x;
                    cursor.y += kSlotSize;
                }
            }
        }
    };

    WindowBase* FactoryInfoOpen(const CoordsXYZ& loc)
    {
        auto* windowMgr = GetWindowManager();
        windowMgr->CloseByClass(WindowClass::factoryInfo);
        auto* w = windowMgr->Create<FactoryInfoWindow>(WindowClass::factoryInfo, kWindowSize, {});
        if (w != nullptr)
            w->initialise(loc);
        return w;
    }
} // namespace OpenRCT2::Ui::Windows

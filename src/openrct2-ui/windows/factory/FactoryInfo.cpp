/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file. Info window for a machine, chest or pipe: status, recipe, slots, fluids and progress.

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
#include <openrct2/factory/FactoryPrototypeObject.h>
#include <openrct2/factory/FactoryState.h>
#include <openrct2/factory/FactoryStringIds.h>
#include <openrct2/factory/FactoryTopology.h>
#include <openrct2/factory/Fluids.h>
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
    };

    // clang-format off
    static constexpr auto kWindowFactoryInfoWidgets = makeWidgets(
        makeWindowShim(kWindowTitle, kWindowSize),
        makeWidget({ 60, 44}, {164, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                     ),
        makeWidget({212, 45}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH, STR_FT_SELECT_RECIPE_TIP)
    );
    // clang-format on

    class FactoryInfoWindow final : public Window
    {
    private:
        CoordsXYZ _loc{};
        std::vector<ObjectEntryIndex> _recipeChoices;

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

        void onMouseDown(WidgetIndex widgetIndex) override
        {
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
            if (widgetIndex != WIDX_RECIPE_DROPDOWN_BUTTON || dropdownIndex < 0)
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
            widgets[WIDX_RECIPE_DROPDOWN].text = choosesRecipe && machine->recipe != kObjectEntryIndexNull
                ? static_cast<StringId>(STR_STRING)
                : static_cast<StringId>(STR_FT_NO_RECIPE);
            if (choosesRecipe && machine->recipe != kObjectEntryIndexNull)
            {
                auto* recipeProto = getPrototype(machine->recipe);
                static std::string recipeName;
                recipeName = recipeProto != nullptr ? recipeProto->GetName() : std::string();
                widgets[WIDX_RECIPE_DROPDOWN].string = recipeName.c_str();
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
                drawText(rt, pos, STR_STRINGID, ft);
            }
            pos.y += 12;

            if (element->getSubtype() == FactoryElementSubtype::container)
            {
                auto* container = state.containers.get(element->getRecordId());
                if (container == nullptr)
                    return;
                drawText(rt, pos, STR_FT_CONTENTS);
                pos.y += 12;
                DrawSlots(rt, pos, container->slots);
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
                    && machineProto->machineHandlesCategory(proto->getRecipe().category))
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
                    ft.Add<uint16_t>(slot.count);
                    drawText(
                        rt, cursor + ScreenCoordsXY{ kSlotSize - 4, kSlotSize - 12 }, STR_COMMA16, ft,
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

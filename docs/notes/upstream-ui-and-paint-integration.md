<!-- Research note produced 2026-10-03 against upstream commit 5d86c6b. Line numbers drift as upstream moves; verify before editing. -->

# Integration brief: FactoryBuild window and factory tile painting (OpenRCT2 fork at /home/user/Documents/Projects/factory-tour)

HEAD is c789149677, "Add FactoryElement tile element type with stub paint". `TileElementType::factory = 9` already exists. `PaintFactory` is an empty stub (`src/openrct2/paint/tile_element/Paint.Factory.cpp:25-32`) and is already dispatched from `Paint.TileElement.cpp:289-292`. The `FactoryElement` payload is `subtype`, `recordId` (`kFactoryRecordNull = 0xFFFFFFFF` for ghosts), `footprintIndex`, `entry` (prototype `ObjectEntryIndex`), `connectionCache` and `factoryFlags` (`FACTORY_ELEMENT_FLAG_ORIGIN/WORKING/DAMAGED`). There is no `factoryPrototype` ObjectType yet.

Some paths in the task don't match this checkout:
- **WindowClasses.h** is at `src/openrct2/interface/WindowClasses.h`, not under `openrct2-ui`.
- **`MapInvalidateSelectionRect` does not exist.** The equivalent is `MapSelection::invalidate()`.
- **`kMaxScenerySelections` does not exist.**
- **Scenery animation uses `getGameState().currentTicks`**, not `gCurrentTicks`.

---

## A. UI window

### A1. WindowClass enum (`src/openrct2/interface/WindowClasses.h:16-110`)
`enum class WindowClass : uint8_t` with explicit values:
```cpp
mainWindow = 0, ... editorScenarioOptions = 45, manageTrackDesign = 47, ... sceneryScatter = 51,
cheats = 110, ... changelog = 121, multiplayer = 124, ... editorStatusLine = 141,
// Only used for colour schemes
staff = 220, editorStepControlTrack = 221, ... custom = 225,
null = 255,
```
- **Unused values:** 9, 10 (commented-out credits), 46, 52-109, 122-123, **142-219** (all unused), and 226-254.
- **Something like `factoryBuild = 142` is safe.** Values are only used in-process.
- **Plugins can see the number:** `ScWindow` exposes `classification` as an int (`src/openrct2-ui/scripting/ScWindow.cpp:34`).
- **Theme JSON is keyed by the string name** (`"WC_..."`), not the number.
- **Forward declarations:** `WindowTypes.h:26`, `Window.h:36`, `Theme.h:20`.

### A2. Open functions, WindowManager dispatch and Theme table

**Declarations** go in `src/openrct2-ui/windows/Windows.h`, inside `namespace OpenRCT2::Ui::Windows`, one block per window:
```cpp
122    // Footpath
123    WindowBase* FootpathOpen();
125    void ToggleFootpathWindow();
126    void WindowFootpathKeyboardShortcutTurnLeft();
...
295    // Scenery
296    WindowBase* SceneryOpen();
305    void ToggleSceneryWindow();
```
Shared globals are declared at lines 55-59 (`extern uint8_t gWindowSceneryRotation;` and others).

**`ContextOpenWindow` needs a case.** It goes through `src/openrct2-ui/WindowManager.cpp:62-173`:
```cpp
90            case WindowClass::footpath:
91                return FootpathOpen();
...
170            default:
171                Console::Error::WriteLine("Unhandled window class (%d)", wc);
172                return nullptr;
```
Without a case for the new class, `ContextOpenWindow` returns nullptr. `ToggleSceneryWindow`-style code then dereferences it with `ToolSet(*toolWindow, …)` (`Scenery.cpp:3583-3584`).

`CloseConstructionWindows()` (`WindowManager.cpp:1124-1130`) closes the construction windows. Consider adding the factory window there.

**Open patterns:**
- `windowMgr->FocusOrCreate<FootpathWindow>(WindowClass::footpath, kWindowSize, {})` (`Footpath.cpp:1951`).
- `Create<SceneryWindow>(WindowClass::scenery)` after `BringToFrontByClass` (`Scenery.cpp:3500-3509`).
- The signatures are in `src/openrct2/ui/WindowManager.h:52-82`.

**Theme table** (`src/openrct2-ui/interface/Theme.cpp:135-141`, 156-229):
```cpp
struct WindowThemeDesc { OpenRCT2::WindowClass WindowClass; const utf8* WindowClassSZ; StringId WindowName; VariableWindowColours windowColours; };
...
175  { WindowClass::footpath, "WC_FOOTPATH", STR_THEMES_WINDOW_FOOTPATH, { opaque(Drawing::Colour::darkBrown), opaque(Drawing::Colour::darkBrown), opaque(Drawing::Colour::darkBrown) } },
171  { WindowClass::scenery,  "WC_SCENERY",  STR_THEMES_WINDOW_SCENERY,  { opaque(Drawing::Colour::darkBrown), opaque(Drawing::Colour::darkGreen), opaque(Drawing::Colour::darkGreen) } },
```
`VariableWindowColours` takes 1-4 colours (lines 93-130). Lookup is a linear scan that returns nullptr when there is no match (`GetWindowThemeDescriptor`, 287-297).

If the new class has no row:
- **The window gets no colours.** `ColourSchemeUpdateByClass` (987-1015) returns early: "Some windows don't have a theme set". `window->colours[]` stays `{}` (black, opaque), and `WindowFlag::transparent` is never set.
- **Theme lookups return empty values.** `ThemeGetColour` returns `{}`, `ThemeDescGetNumColours` returns 0, and `ThemeDescGetName` returns `kStringIdEmpty`.
- **Theme JSON skips it.** `UIThemeWindowEntry::ToJson` returns null.
- **Nothing crashes, but the window will look wrong.**

To make it editable in the Themes window, also add the class to a tab list in `src/openrct2-ui/windows/Themes.cpp`. The Tools tab is `kThemesTabToolsClasses`, around lines 145-158, which lists `scenery`, `sceneryScatter`, `footpath`.

### A3. Footpath.cpp construction-tool flow (`src/openrct2-ui/windows/Footpath.cpp`)

**Widgets.** The enum is at 159-187 (`WIDX_BACKGROUND, WIDX_TITLE, WIDX_CLOSE, …`). The array:
```cpp
190    static constexpr auto kWindowFootpathWidgets = makeWidgets(
191        makeWindowShim(kWindowTitle, kWindowSize),
194        makeWidget({ 3,  17}, {100, 95}, WidgetType::groupbox, WindowColour::primary  , STR_TYPE ),
195        makeWidget({ 6,  30}, { 47, 36}, WidgetType::flatBtn,  WindowColour::secondary, 0xFFFFFFFF, STR_FOOTPATH_TIP ),
...
216        makeWidget({13, 372}, { 36, 36}, WidgetType::flatBtn,  WindowColour::secondary, ImageId(SPR_CONSTRUCTION_FOOTPATH_LAND), STR_CONSTRUCT_FOOTPATH_ON_LAND_TIP ),
```
`makeWindowShim` supplies the first three widgets (background, title, close). The enum order must match the array order.

**onOpen (261-279):**
```cpp
setWidgets(kWindowFootpathWidgets);
widgetsSetHoldable(*this, { WIDX_CONSTRUCT, WIDX_REMOVE });
WindowInitScrollWidgets(*this);
WindowPushOthersRight(*this);
ShowGridlines();
ToolCancel();
_footpathConstructionMode = PathConstructionMode::onLand;
ToolSet(*this, WIDX_CONSTRUCT_ON_LAND, Tool::pathDown);
gInputFlags.set(InputFlag::allowRightMouseRemoval);
```

**onClose (281-290):**
```cpp
FootpathUpdateProvisional();
ViewportSetVisibility(ViewportVisibility::standard);
gMapSelectFlags.unset(MapSelectFlag::enableConstruct);
windowMgr->InvalidateByClass(WindowClass::topToolbar);
HideGridlines();
```

**onUpdate (292-325):** the window closes itself when its tool is replaced.
```cpp
case PathConstructionMode::onLand:
    if (!isToolActive(WindowClass::footpath, WIDX_CONSTRUCT_ON_LAND))
        close();
```

**Mouse handlers:**
- `onMouseDown` (327-368) handles dropdowns, direction and slope buttons.
- `onMouseUp` (383-431) switches mode: `ToolCancel(); FootpathUpdateProvisional(); gMapSelectFlags.unset(MapSelectFlag::enableConstruct); ToolSet(*this, WIDX_…, Tool::pathDown); gInputFlags.set(InputFlag::allowRightMouseRemoval);`.
- `onDropdown` (433-477) bounds-checks `selectedIndex` against `_dropdownEntries`, then `FootpathUpdateProvisional(); _windowFootpathCost = kMoney64Undefined; invalidate();`.

**Tool handlers**, all keyed on the widget index passed to `ToolSet`:
```cpp
479  void onToolUpdate(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override { if (widgetIndex == WIDX_CONSTRUCT_ON_LAND) WindowFootpathSetProvisionalPathAtPoint(screenCoords); ... }
495  void onToolUp(...)   // drag-area commit
508  void onToolDown(WidgetIndex widgetIndex, const ScreenCoordsXY& screenCoords) override { if (widgetIndex == WIDX_CONSTRUCT_ON_LAND) WindowFootpathPlacePathAtPoint(screenCoords); ... }
524  void onToolDrag(...) // repeats PlacePathAtPoint while dragging
536  void onToolAbort(WidgetIndex widgetIndex) override
```
These are called from `src/openrct2-ui/input/MouseInput.cpp`:
- **Hover:** `ProcessMouseTool` calls `onToolUpdate` (1234-1246).
- **Left press on a viewport:** `onToolDown` (1094-1101).
- **Drag:** `onToolDrag` (405).
- **Release:** `onToolUp` (417).
- **Right-click without drag:** `ViewportInteractionRightClick` (366).

**Tool API** (`src/openrct2/interface/Window.h:78-84`, implementation in `Window.cpp:635-720`):
```cpp
bool ToolSet(const WindowBase& w, WidgetIndex widgetIndex, Tool tool); // toggles off if same w/widget already active
void ToolCancel();  // clears gMapSelectFlags, invalidates tool widget, calls w->onToolAbort(widget)
bool isToolActive(WindowClass cls, WidgetIndex widgetIndex);
```
`enum class Tool` (`src/openrct2/interface/WindowTypes.h:132-146`): `arrow=0, upArrow=2, upDownArrow=3, picker=7, crosshair=12, pathDown=17, digDown=18, waterDown=19, walkDown=22, paintDown=23, entranceDown=24, bulldozer=27`.

**Hit-testing under the cursor** (`src/openrct2/interface/Viewport.h:181-191`):
```cpp
InteractionInfo GetMapCoordinatesFromPos(const ScreenCoordsXY& screenCoords, ViewportInteractionItems flags);
std::optional<CoordsXY> ScreenGetMapXYWithZ(const ScreenCoordsXY& screenCoords, int32_t z);
std::optional<CoordsXY> ScreenGetMapXYQuadrant(...); ScreenGetMapXYSide(...);
```
`InteractionInfo` fields: `Loc`, `Element`, `Entity`, `interactionType` (Viewport.h:130-138).

Footpath usage:
```cpp
975  auto info = GetMapCoordinatesFromPos(screenCoords, { ViewportInteractionItem::terrain, ViewportInteractionItem::footpath });
978  if (info.interactionType == ViewportInteractionItem::none) return std::nullopt;
1003 auto mapXYCoords = ScreenGetMapXYWithZ(screenCoords, mapZ);   // ctrl = height-lock mode
1023 return mapCoords.toTileStart();
```
Scenery's large-scenery placement uses `ViewportInteractionGetTileStartAtCursor(screenPos)`, which filters terrain plus water (`Scenery.cpp:2793`; implementation `ViewportInteraction.cpp:788-831`).

**Map selection highlight** (`src/openrct2/world/MapSelection.h:21-79`): `MapSelectFlag {enable, enableConstruct, enableArrow, green}` with globals `gMapSelectFlags`, `gMapSelectType`, `gMapSelectPositionA/B`, and `setMapSelectRange(CoordsXY)`. Footpath, at 1097-1101:
```cpp
gMapSelectFlags.set(MapSelectFlag::enable);
gMapSelectType = MapSelectType::full;
setMapSelectRange(*mapPos);
```
For a multi-tile footprint, use the large-scenery approach (`Scenery.cpp:1962-1976`):
```cpp
gMapSelectFlags.set(MapSelectFlag::enableConstruct);
MapSelection::clearSelectedTiles();
for (auto& tile : sceneryEntry->tiles) MapSelection::addSelectedTile(mapTile + tile.offset.rotate(direction));
```
- **Colour:** `Paint.Surface.cpp:1169-1187` draws these tiles. They are green if `MapSelectFlag::green` is set, otherwise the scenery marker colour.
- **Redraw:** invalidation is automatic. `MapSelection::invalidate()` (`MapSelection.cpp:71-127`) diffs against the previous state and is called each frame from `Painter.cpp:57`. There is no explicit "invalidate selection rect" call.

**Ghost placement** (`FootpathProvisionalSet`, 2065-2141):
```cpp
2069  FootpathRemoveProvisional();
2076  auto footpathPlaceAction = GameActions::FootpathPlaceAction(tile.position, tile.slope, type, railingsType, kInvalidDirection, constructFlags);
2078  footpathPlaceAction.SetFlags({ CommandFlag::ghost, CommandFlag::allowDuringPaused });
2079  res = GameActions::Execute(&footpathPlaceAction, getGameState());
2082  if (res.error == GameActions::Status::ok || res.error == GameActions::Status::itemAlreadyPlaced) { ... cost += res.cost; }
...
2140  return anySuccessful ? cost : kMoney64Undefined;
```

**Ghost removal** (2147-2160):
```cpp
auto action = GameActions::FootpathRemoveAction(tile.position);
action.SetFlags({ CommandFlag::allowDuringPaused, CommandFlag::noSpend, CommandFlag::ghost });
GameActions::Execute(&action, getGameState());
```
The hover handler (1072-1130) skips work when the position is unchanged (1090-1095), then sets the selection, re-places the ghost and caches the cost:
```cpp
if (_windowFootpathCost != footpathCost) { _windowFootpathCost = footpathCost; invalidateWidget(WIDX_CONSTRUCT); }
```

**The click places through the real action** (1458-1495):
```cpp
FootpathUpdateProvisional();  // remove ghost first
auto footpathPlaceAction = GameActions::FootpathPlaceAction({ *mapPos, placement.baseZ }, ...);
footpathPlaceAction.SetCallback([this](const GameActions::GameAction* ga, const GameActions::Result* result) {
    if (result->error == GameActions::Status::ok) { if (result->cost != 0) Audio::Play3D(Audio::SoundId::placeItem, result->position); }
    else _footpathErrorOccurred = true;
});
GameActions::Execute(&footpathPlaceAction, getGameState());
```
Scenery's equivalents:
- **Ghost helper** `TryPlaceGhostLargeScenery` (`Scenery.cpp:2179-2210`): `SetFlags({ CommandFlag::ghost, CommandFlag::allowDuringPaused, CommandFlag::noSpend })`. It records the ghost in globals and sets `gSceneryGhostType |= SCENERY_GHOST_FLAG_3`.
- **Removal** `SceneryRemoveGhostToolPlacement()` (`src/openrct2/world/Scenery.cpp:228+`).
- **Place on click** `onToolDownLargeScenery` (3350-3409): `GameActions::Query` first, then `Execute` with a sound callback.

**Ghost semantics:**
- **Not networked, no error popup:** `CommandFlag::ghost` actions are not sent over the network (`CommandFlag.h:33-36`) and don't show an error (`GameActionRunner.cpp:421`).
- **Ghost flag:** the place action calls `element->setGhost(GetFlags().has(CommandFlag::ghost))` (`SmallSceneryPlaceAction.cpp:433`).
- **Stripped on save:** `ParkFile.cpp:1274` uses `GetReorganisedTileElementsWithoutGhosts`.
- **Pulled out during the guest update:** `GameState.cpp:314-321` broadcasts `INTENT_ACTION_REMOVE/RESTORE_PROVISIONAL_ELEMENTS` around `PeepUpdateAll()`. `src/openrct2-ui/ProvisionalElements.cpp` handles that for footpaths, track and track designs only. Add factory ghosts there if they must be pulled out during simulation.

**Rotation and direction:**
- **Footpath** keeps a per-window `_footpathConstructDirection` and converts view direction to world direction with `(direction - GetCurrentRotation()) & 3` (886). Turn-left/right helpers are at 1833-1857.
- **Scenery** uses the global `gWindowSceneryRotation`. The rotate button does `gWindowSceneryRotation++; gWindowSceneryRotation %= 4; SceneryRemoveGhostToolPlacement(); invalidate();` (312-317). It is converted to world direction at placement time:
```cpp
2849  Direction rotation = gWindowSceneryRotation;
2850  rotation -= GetCurrentRotation();
2851  rotation &= 0x3;
```
`gWindowSceneryRotation` is reset to 3 on open (251) and by `WindowScenerySetDefaultPlacementConfiguration` (3543). Use a separate variable for the factory window, not this shared global.

**Cost display** (`onDraw`, 623-634):
```cpp
if (_windowFootpathCost != kMoney64Undefined)
    if (!getGameState().park.flags.has(ParkFlag::noMoney)) {
        auto ft = Formatter(); ft.Add<money64>(_windowFootpathCost);
        drawText(rt, screenCoords, STR_COST_LABEL, ft, { TextAlignment::centre });
    }
```
The preview sprite is drawn with `GfxDrawSprite(rt, ImageId(image), screenCoords)` (614).

**Toggle** (2047-2059):
```cpp
if (windowMgr->FindByClass(WindowClass::footpath) == nullptr) ContextOpenWindow(WindowClass::footpath);
else { ToolCancel(); windowMgr->CloseByClass(WindowClass::footpath); }
```
Scenery's toggle (3575-3587) also does `ToolSet(*toolWindow, WIDX_SCENERY_BACKGROUND, Tool::arrow); gInputFlags.set(InputFlag::allowRightMouseRemoval);`. Scenery closes itself in `onUpdate` when `!isToolActive(WindowClass::scenery)` (567-583).

### A4. Scenery palette (`src/openrct2-ui/windows/Scenery.cpp`)
- **Constants:** `kSceneryButtonWidth = 66`, `kSceneryButtonHeight = 80`, `kMaxTabs = 257`, `kMaxTabsPerRow = 20` (80-90).
- **Grid widget:** `makeWidget({2,62},{607,80}, WidgetType::scroll, WindowColour::secondary, SCROLL_VERTICAL)` (123). The rotate button `ImageId(SPR_ROTATE_ARROW)` with `STR_ROTATE_OBJECTS_90` is at 124.
- **Tabs** are built from the entry list into `_tabEntries`:
  - `init()` (1002-1060+) loops scenery groups, then `ObjectEntryManager::GetObjectEntry<SmallSceneryEntry>(id)` and the other types.
  - `PrepareWidgets()` (1407-1450) appends one `makeTab(pos, STR_STRING_DEFINED_TOOLTIP)` per tab with `widget.image = ImageId(scgEntry->image, colours[1].colour)`.
- **Grid math:** `GetNumColumns()` = (list width - 1 - `kScrollBarWidth`) / 66 (1123-1128). `onScrollGetSize` returns `{0, rows*80}` (1502-1506).
- **Hit test:**
```cpp
1489  const auto colIndex = screenCoords.x / kSceneryButtonWidth;
1490  const auto rowIndex = screenCoords.y / kSceneryButtonHeight;
1493  const auto tabSceneryIndex = static_cast<size_t>((rowIndex * numColumns) + colIndex);
```
  `onScrollMouseDown` (1508-1528) selects the item, plays `click1`, resets the cost and invalidates. `onScrollMouseOver` (1530-1538) sets the hover item.
- **Drawing (`onScrollDraw`, 1720-1777):** `GfxClear(rt, getColourMap(colours[1].colour).midLight)`. Selection highlighting:
```cpp
if (tabSelectedScenery == currentSceneryGlobal)
    Rectangle::fillInset(rt, { topLeft, topLeft + ScreenCoordsXY{ kSceneryButtonWidth - 1, kSceneryButtonHeight - 1 } },
        colours[1], Rectangle::BorderStyle::inset, Rectangle::FillBrightness::dark);   // selected
else if (_selectedScenery == currentSceneryGlobal)   // hover
    Rectangle::fillInset(..., Rectangle::BorderStyle::outset, Rectangle::FillBrightness::dark);
RenderTarget clippedRT;
if (ClipRenderTarget(clippedRT, rt, topLeft + ScreenCoordsXY{ 1, 1 }, kSceneryButtonWidth - 2, kSceneryButtonHeight - 2))
    DrawSceneryItem(clippedRT, currentSceneryGlobal);
```
- **Preview image** (`DrawSceneryItem`, 1603-1718) uses the object's loaded base image plus rotation:
```cpp
auto imageId = ImageId(sceneryEntry->image + gWindowSceneryRotation);          // large: drawn at {33, 0}
if (flags.has(LargeSceneryFlag::hasPrimaryColour)) imageId = imageId.WithPrimary(_sceneryPrimaryColour);
GfxDrawSprite(rt, imageId, { 33, 0 });
// small scenery: spriteTop = (height / 4) + 43; GfxDrawSprite(rt, imageId, { 32, spriteTop });
```
  The `->image` field is the object's base image id, filled in by `LoadImages()` (see B11).
- **Selection type:** `ScenerySelection { uint8_t SceneryType; ObjectEntryIndex EntryIndex; }` (`src/openrct2/world/ScenerySelection.h:16-28`). Don't reuse `SCENERY_TYPE_*` for prototypes.

### A5. TopToolbar (`src/openrct2-ui/windows/TopToolbar.cpp`)
- **WIDX enum (57-90):** `WIDX_PAUSE … WIDX_SCENERY, WIDX_PATH, WIDX_CONSTRUCT_RIDE … WIDX_CHAT, WIDX_SEPARATOR,`. The enum index equals the position in `_topToolbarWidgets` (240-269). `WIDX_SEPARATOR` is last in both.
- **Widget definition** (`makeRemapWidget` in `Widget.h:77-82` wraps `ImageId(content, FilterPaletteID::paletteNull)`):
```cpp
253 makeRemapWidget({387, 0}, {30, kTopToolbarHeight + 1}, WidgetType::trnBtn, WindowColour::tertiary, SPR_TOOLBAR_FOOTPATH, STR_BUILD_FOOTPATH_TIP), // Path
```
  - **Height:** `kTopToolbarHeight = 27` (`src/openrct2/interface/Widget.h:228`).
  - **Sprites:** `SPR_TOOLBAR_SCENERY = 5615`, `SPR_TOOLBAR_FOOTPATH = 5623`, `SPR_TAB_TOOLBAR = 5625` (`SpriteIds.h:682-700`).
  - **Custom icons:** use `SPR_TAB_TOOLBAR` (a blank button) and draw the icon yourself in `onDraw`, as Finances/Research/News do (`onDraw`, 1328-1456, e.g. `GfxDrawSprite(rt, ImageId(SPR_FINANCE), screenPos)` with `screenPos.y++` when pressed).
  - **x positions don't matter:** `AlignButtons` overwrites them.
- **Display order:** `kWidgetOrderRightGroup` (206-223: `WIDX_CLEAR_SCENERY, WIDX_LAND, WIDX_WATER, WIDX_SCENERY, WIDX_PATH, WIDX_CONSTRUCT_RIDE, WIDX_SEPARATOR, …`). `kWidgetOrderCombined` is derived automatically.
- **Click dispatch (`onMouseUp`, 791-872):**
```cpp
828  case WIDX_SCENERY: ToggleSceneryWindow(); break;
831  case WIDX_PATH:    ToggleFootpathWindow(); break;
834  case WIDX_CONSTRUCT_RIDE: ContextOpenWindow(WindowClass::constructRide); break;
```
- **Pressed state:** `ApplyFootpathPressed()`:
```cpp
setWidgetPressed(WIDX_PATH, windowMgr->FindByClass(WindowClass::footpath) != nullptr);
```
  This is why Footpath's `onClose` invalidates `WindowClass::topToolbar`.
- **Visibility** is recomputed every `onPrepareDraw` (1308-1326):
  1. `ResetWidgetsToDefaultState()` (all visible).
  2. `HideDisabledButtons()` (config toggles).
  3. `ApplyEditorMode()` (1107-1148). Scenery/Path/Map/ClearScenery are hidden unless `editorStep` is `landscapeEditor` or `rollerCoasterDesigner`; Park/Staff/Guests/Finances/Research/News/Network are always hidden in editor mode.
  4. `ApplyNetworkMode()`, then layout via `AlignButtonsLeftRight()` / `AlignButtonsCentre()`.
- **View menu (for a later ore-overlay toggle):**
  - Enum `TopToolbarViewMenuDdidx` (112-138), ending `DDIDX_TRANSPARENCY = 21, TOP_TOOLBAR_VIEW_MENU_COUNT`.
  - `initViewMenu` (277-333) builds `constexpr Dropdown::ItemExt items[] = { ToggleOption(DDIDX_…, STR_…), ExtSeparator(), … }`, then `static_assert(ItemIDsMatchIndices(items)); SetItems(items); WindowDropdownShowText(..., TOP_TOOLBAR_VIEW_MENU_COUNT);` and the checkmarks `gDropdown.items[DDIDX_X].setChecked(mvpFlags.has(ViewportFlag::…))`.
  - Selection is handled in `viewMenuDropdown` (335-413) as `w->viewport->flags.flip(ViewportFlag::…); … w->invalidate();`, routed from `onMouseDown case WIDX_VIEW_MENU` (886) and `onDropdown case WIDX_VIEW_MENU` (1011).
  - **Only one viewport flag bit is free.** `ViewportFlag` (`src/openrct2/interface/ViewportFlags.h:18-56`) has bit **22** unused. The values are fixed for plugin compatibility.

### A6. Viewport interaction and `ViewportInteractionItem`
**Enum** (`src/openrct2/interface/Viewport.h:93-109`):
```cpp
enum class ViewportInteractionItem : uint8_t { none, terrain, entity, ride, water, scenery, footpath, pathAddition, parkEntrance, wall, largeScenery, label, banner };
using ViewportInteractionItems = FlagHolder<uint16_t, ViewportInteractionItem>;
```
`kViewportInteractionItemAll` is at 123-128.

**How each paint struct gets its item:** `Paint.cpp:216-218`, `264-266`:
```cpp
ps->InteractionItem = session.InteractionType;
ps->Element = session.CurrentlyDrawnTileElement;
```
Every paint function sets `session.InteractionType` itself, e.g. `ViewportInteractionItem::scenery` (`Paint.SmallScenery.cpp:339`), set to `none` for ghosts (350).

**Filter cap** (`src/openrct2/interface/Viewport.cpp:1476-1481`):
```cpp
return (ps->InteractionItem != ViewportInteractionItem::none && ps->InteractionItem != ViewportInteractionItem::label
        && ps->InteractionItem <= ViewportInteractionItem::banner)
    && filter.has(ps->InteractionItem);
```
`GetPaintStructVisibility` (1361-1469) switches per item. The `default:` case only handles clipping, so a new `factory` item ignores the see-through and hide toggles unless you add a case (for example, grouped with scenery).

**Right-click dispatch** (`src/openrct2-ui/interface/ViewportInteraction.cpp`):
- **`ViewportInteractionGetItemRight` (263-544)** queries with `kFlags` (275-279: entity, ride, scenery, footpath, pathAddition, parkEntrance, wall, largeScenery, label, banner).
  - **First switch (283-465)** handles tooltips. Unknown items hit `default: break;`.
  - **Removal gate (467-476):** if not `allowRightMouseRemoval` with a tool active, it requires the `rideConstruction` or `footpath` window to be open, otherwise it returns `none`.
  - **Second switch (479-540):** "click to remove" tooltips. Unknown items fall to `default: break;` and then `info.interactionType = ViewportInteractionItem::none;` (542).
  - **So an unknown item is silently ignored.** A factory case needs a tooltip using `STR_MAP_TOOLTIP_STRINGID_CLICK_TO_REMOVE` plus the prototype name, and a `return info;`.
- **`ViewportInteractionRightClick` (557-616)** has no `default:`. It lists every enumerator, e.g. `case ViewportInteractionItem::scenery: ViewportInteractionRemoveScenery(*info.Element->asSmallScenery(), info.Loc);`. The remove helpers are at 622-727 (`GameActions::SmallSceneryRemoveAction` → `Execute`).

**What a factory case needs:**
1. Add the `factory` item after `banner`. A FlagHolder<uint16_t> holds 16 items and 13 are used.
2. Change the `<= banner` check at `Viewport.cpp:1479`.
3. Add it to `kViewportInteractionItemAll`.
4. Add a case in both `GetItemRight` switches.
5. Add a case in `RightClick` that executes a FactoryRemoveAction.
6. Optionally add a case in `GetPaintStructVisibility`.
7. Optionally add a `"factory"` entry to the plugin `ToolFilterMap` (`src/openrct2-ui/scripting/CustomMenu.cpp:98-111`).
8. Optionally add it to the TileInspector click flags (`TileInspector.cpp:466-470`).

Other switches over the interaction type all have `default:` (Footpath.cpp:1035, Scenery.cpp:2242/2320, Ride.cpp:5873/5899).

### A7. Rotate shortcut
- **Id:** `kInterfaceRotateConstruction = "interface.general.rotate_construction"` (`src/openrct2-ui/input/ShortcutIds.h:19`). Open-window ids are at 39-58, e.g. `kInterfaceOpenFootpaths = "interface.open.footpaths"`.
- **Registration** (`src/openrct2-ui/input/Shortcuts.cpp:769`):
```cpp
registerShortcut(ShortcutId::kInterfaceRotateConstruction, STR_SHORTCUT_ROTATE_CONSTRUCTION_OBJECT, "Z", ShortcutRotateConstructionObject);
```
  Open shortcuts: `registerShortcut(ShortcutId::kInterfaceOpenFootpaths, STR_SHORTCUT_BUILD_PATHS, "F4", ShortcutBuildPaths);` (816). `ShortcutBuildPaths` (238-250) returns early in title/editor/track-designer scenes, then calls `ToggleFootpathWindow()`.
- **Binding to windows:** there is no per-window binding. `ShortcutRotateConstructionObject` (82-146) checks open windows in order by class and fires the window's rotate button:
```cpp
WindowBase* w = windowMgr->FindByClass(WindowClass::scenery);
if (w != nullptr && !widgetIsDisabled(*w, WC_SCENERY__WIDX_SCENERY_ROTATE_OBJECTS_BUTTON)
    && w->widgets[WC_SCENERY__WIDX_SCENERY_ROTATE_OBJECTS_BUTTON].isVisible())
{ w->onMouseUp(WC_SCENERY__WIDX_SCENERY_ROTATE_OBJECTS_BUTTON); return; }
// then rideConstruction, trackDesignList, trackDesignPlace, editorParkEntrance, tileInspector
```
  To support the factory window, add a block that looks up `WindowClass::factoryBuild` and calls `onMouseUp(WC_FACTORY_BUILD__WIDX_ROTATE)`.
- **The `WC_*__WIDX_*` constants** live in `src/openrct2/interface/WidgetIndexGlobals.h` (e.g. `WC_SCENERY__WIDX_SCENERY_ROTATE_OBJECTS_BUTTON = 5`, line 34). The window side checks them with:
```cpp
#define VALIDATE_GLOBAL_WIDX(wc, widx) static_assert(widx == wc##__##widx, "Global WIDX of " #widx " doesn't match actual value.")
VALIDATE_GLOBAL_WIDX(WC_SCENERY, WIDX_SCENERY_ROTATE_OBJECTS_BUTTON);   // Scenery.cpp:114-117
```
  `TileInspector.cpp:194` uses a plain `static_assert` instead.

### A8. String ids
- **Two headers:**
  - UI strings: `src/openrct2-ui/UiStringIds.h`, `namespace OpenRCT2 { enum : StringId { … } }`, grouped by window comments. Examples: `STR_THEMES_WINDOW_FOOTPATH = 5198`, `STR_BUILD_FOOTPATH_TIP = 1173`, `STR_SHORTCUT_BUILD_PATHS = 2514`.
  - Core strings: `src/openrct2/localisation/StringIds.h`.
- **Current highest ids:**
  - `UiStringIds.h`: `STR_COLOUR_SILVER_TIP = 7081` (line 111).
  - `StringIds.h`: `STR_GUESTS_WATCHING_NEW_RIDE_BEING_CONSTRUCTED = 7063` (line 1796), followed by the comment `/* MAX_STR_COUNT = 32768 */`.
  - `data/language/en-GB.txt` ends at `STR_7081    :Silver`.
- **en-GB.txt format:** `STR_XXXX    :text`, zero-padded to 4 digits, one per line, `#` for comments. Format codes are allowed, e.g. `STR_0001    :{STRINGID} {COMMA16}`.
- **Range limit:** object strings are allocated at runtime in 0x2000-0x5000 (8192-20480) (`src/openrct2/localisation/LocalisationService.cpp:23-24`). Fork strings must stay at or below 8191, which leaves 7082-8191 (about 1110 ids). Upstream adds ids sequentially from 7082, so a block near the top (e.g. 8000-8191) avoids merge clashes. `StringId` is `uint16_t`, with `kStringIdNone = 0xFFFF` and `kStringIdEmpty = 0` (`StringIdType.h`).

---

## B. Paint

### B9. `PaintSmallScenery` (`src/openrct2/paint/tile_element/Paint.SmallScenery.cpp`)
**Entry point (324-361):**
```cpp
if (session.ViewFlags.has(ViewportFlag::highlightPathIssues)) return;
auto* sceneryEntry = sceneryElement.getEntry(); if (sceneryEntry == nullptr) return;
session.InteractionType = ViewportInteractionItem::scenery;
ImageId imageTemplate;
if (gTrackDesignSaveMode && !TrackDesignSaveContainsTileElement(...)) imageTemplate = ImageId().WithRemap(FilterPaletteID::palette46);
if (sceneryElement.isGhost()) {
    session.InteractionType = ViewportInteractionItem::none;
    imageTemplate = ImageId().WithRemap(FilterPaletteID::paletteGhost);
} else if (session.SelectedElement == reinterpret_cast<const TileElement*>(&sceneryElement))
    imageTemplate = ImageId().WithRemap(FilterPaletteID::paletteGhost);
PaintSmallSceneryBody(session, direction, height, sceneryElement, sceneryEntry, imageTemplate);
PaintSmallScenerySupports(session, *sceneryEntry, sceneryElement, direction, height, imageTemplate);
SetSupportHeights(session, *sceneryEntry, sceneryElement, height);
```
The ghost shortcut constant is `ConstructionMarker = ImageId(0).WithRemap(FilterPaletteID::paletteGhost)` (`Paint.h:243`); use `ConstructionMarker.WithIndex(i)`.

**Body (112-322):**
- **Bounding box:** starts as `BoundBoxXYZ boundBox = { { 0, 0, height }, { 2, 2, 0 } }; CoordsXYZ offset = { 0, 0, height };`. A full tile uses offset (15,15), or (3,3)/26x26 with `vOffsetCentre`. A quarter tile uses `SceneryQuadrantOffsets[(quadrant + session.CurrentRotation) & 3] - 1`. Then `boundBox.length.z = clamp(height-4) - 1`.
- **Image:** `ImageIndex baseImageIndex = sceneryEntry->image + direction;` (176), where `direction` already includes view rotation via `getDirectionWithOffset(rotation)` in `Paint.TileElement.cpp:230`.
- **Colours** apply only if the template is not a remap:
```cpp
auto imageId = imageTemplate.WithIndex(baseImageIndex);
if (!imageTemplate.IsRemap()) { if (hasPrimaryColour) imageId = imageId.WithPrimary(el.getPrimaryColour()); ... }
PaintAddImageAsParent(session, imageId, offset, boundBox);
```
- **Glass layer:** `PaintAddImageAsChild(session, ImageId(baseImageIndex + 4).WithTransparency(primaryColour), offset, boundBox);`.
- **Animation** (215-321), shown only when zoomed in enough unless `isVisibleWhenZoomed`:
```cpp
const auto currentTicks = getGameState().currentTicks;
if (flags.has(isVisibleWhenZoomed) || (session.rt.zoom_level <= ZoomLevel{ 1 })) {
  // fountain: image + 4 + ((currentTicks / 2) & 0xF)  -> PaintAddImageAsChild
  // frame offsets (272-318):
  auto frame = currentTicks;
  if (!isCogwheel) { frame += (SpritePosition.x/4 + SpritePosition.y/4); frame += quadrant << 2; }   // de-sync neighbours
  frame = (frame >> delay) & sceneryEntry->animation_mask;
  imageIndex = (frame_offsets[frame] * 4) + direction + sceneryEntry->image;   // 4 images per frame (one per direction)
  PaintAddImageAsChild(session, imageId, offset, boundBox);
}
```
- **Supports (35-68):** `WoodenBSupportsPaintSetupRotated(session, WoodenSupportType::truss, WoodenSupportSubType::neSw, direction, supportHeight, supportImageTemplate, transitionType);`. A ghost template is passed through when `imageTemplate.IsRemap()`.
- **Support heights (70-110):**
```cpp
PaintUtilSetGeneralSupportHeight(session, ceil2(height + entry.height, 8));
PaintUtilSetSegmentSupportHeight(session, PaintSegment::centre, 0xFFFF, 0);          // block
PaintUtilSetSegmentSupportHeight(session, kSegmentsAll.without(PaintSegment::centre), 0xFFFF, 0);
PaintUtilSetSegmentSupportHeight(session, PaintUtilRotateSegments({top, topLeft, topRight}, direction), h, 0x20);
```
  Signatures are in `Paint.TileElement.h:44-47`.

The multi-tile analogue is `PaintLargeScenery` (`Paint.LargeScenery.cpp:325-408`). It uses `imageIndex = image + 4 + (sequenceNum << 2) + direction;` and `PaintAddImageAsParent(session, imageTemplate.WithIndex(imageIndex), { 0, 0, height }, { bbOffset, bbLength });`.

### B10. Paint API (`src/openrct2/paint/Paint.h`)
```cpp
254 PaintStruct* PaintAddImageAsParent(PaintSession& session, ImageId image_id, const CoordsXYZ& offset, const BoundBoxXYZ& boundBox);
268 inline PaintStruct* PaintAddImageAsParent(PaintSession&, ImageId, const CoordsXYZ& offset, const CoordsXYZ& boundBoxSize); // bb offset = offset
274 [[nodiscard]] PaintStruct* PaintAddImageAsOrphan(PaintSession&, ImageId, const CoordsXYZ& offset, const BoundBoxXYZ&);
276 PaintStruct* PaintAddImageAsChild(PaintSession&, ImageId, const CoordsXYZ& offset, const BoundBoxXYZ&);
279 PaintAddImageAsChildRotated / 282 PaintAddImageAsParentRotated(session, direction, imageId, offset, boundBox)
295 bool PaintAttachToPreviousPS(PaintSession&, ImageId, int32_t x, int32_t y);
```
- **`BoundBoxXYZ`** (`src/openrct2/paint/Boundbox.h:30-41`): `{ CoordsXYZ offset; CoordsXYZ length; }`.
- **`ImageId`** (`src/openrct2/drawing/ImageId.hpp:43-255`, 8 bytes) is immutable with builder methods: `WithIndex`, `WithIndexOffset`, `WithRemap(FilterPaletteID|uint8_t)`, `WithPrimary`, `WithSecondary`, `WithTertiary`, `WithTransparency(Colour|FilterPaletteID)` (blend/glass), `WithBlended`. `IsRemap()` means primary-flag set without secondary, which is how ghost templates are detected.

### B11. Object image addressing
- **Loading:** `SmallSceneryObject::Load()` (`src/openrct2/object/SmallSceneryObject.cpp:71-87`) does `_legacyType.image = LoadImages();`. `Unload()` (89-96) calls `UnloadImages(); _legacyType.image = 0;`. `LargeSceneryObject::Load()` (`LargeSceneryObject.cpp:111-133`) does `_baseImageId = LoadImages(); _legacyType.image = _baseImageId;`.
- **Base implementation** (`src/openrct2/object/Object.cpp:213-229`):
```cpp
ImageIndex Object::LoadImages() {
    if (_baseImageId == kImageIndexUndefined)
        _baseImageId = GfxObjectAllocateImages(GetImageTable().GetImages(), GetImageTable().GetCount());
    return _baseImageId;
}
```
  Accessors are `GetBaseImageId()` and `GetImageTable()` (`Object.h:304-328`). Image *i* of the JSON `images` array is `baseImageId + i`. Zoom variants are appended after all images (`ImageTable.cpp:598-623`), so array indices stay stable.
- **JSON parsing:** `ReadJson` ends with `PopulateTablesFromJson(context, root)` (SmallSceneryObject.cpp:259), which calls `_stringTable.ReadJson(root); _usesFallbackImages = _imageTable.ReadJson(context, root);` (Object.cpp:155-159).
- **`ImageTable::ReadJson`** (`src/openrct2/object/ImageTable.cpp:527-629`) iterates `root["images"]` (or `"noCsgImages"` if CSG is missing). Accepted entry forms:
  - **String forms** (parsed by `ParseImages`, 86-190):
    - `"$CSG[a..b]"`
    - `"$G1[a..b]"`
    - `"$RCT2:OBJDATA/NAME[a..b]"`
    - `"$LGX:file.lgx[a..b]"`, or without a range for all entries. Out-of-range indices become placeholders (226-286).
    - A plain PNG path, imported with zero offsets.
  - **Object with `"gx"`:** the same string syntax, plus optional `"x"`/`"y"` overriding `g1.xOffset`/`yOffset` for every image in the range (557-579).
  - **Object with `"path"`:** a PNG sprite, possibly sliced (583). Fields come from `createImageImportMetaFromJson` (`src/openrct2/drawing/ImageImporter.cpp:441-464`): `"x"`, `"y"` (draw offsets), `"palette":"keep"`, `"format":"raw"` (otherwise RLE), `"srcX"`, `"srcY"`, `"srcWidth"`, `"srcHeight"` (crop from a sheet), `"zoom"`. Each distinct `path` is loaded once (`GetImageSources`, 502-525).
  - **Ranges** (`ParseRange`, 347-380) accept only `[n]` or `[a..b]`, and `[b..a]` gives a reversed sequence.

### B12. Map animation (`src/openrct2/world/MapAnimation.cpp`, API in `MapAnimation.h:17-32`)
- **Two kinds:** `enum class UpdateType { invalidate, update };` (53-57).
  - **`invalidate`:** only repainted. Tiles live in the `_mapAnimationsInvalidate` bit-vector and are processed per visible viewport in `InvalidateAll` (640-687), up to zoom `kMaxZoom{2}`.
  - **`update`:** per-tick logic. Tiles live in the `_mapAnimationsUpdate` set and run every even tick in `UpdateAll` (701-730); clocks and doors use this.
- **Driver:** `MapAnimations::InvalidateAndUpdateAll()` runs once per game tick from `GameState.cpp:337`.
- **Registering a tile** (from the place action): `SmallSceneryPlaceAction.cpp:444-452`:
```cpp
MapInvalidateTileFull(_loc);
if (entry->flags.has(SmallSceneryFlag::isClock)) MapAnimations::MarkTileForUpdate(TileCoordsXY(_loc));
else if (entry->flags.has(SmallSceneryFlag::isAnimated)) MapAnimations::MarkTileForInvalidation(TileCoordsXY(_loc));
```
  This also runs for ghosts, since they go through `Execute`.
- **On load:** `MapAnimations::MarkAllTiles()` (618-638) scans every element via `IsElementAnimated(const TileElementBase&)` (501-594). It is called from `Context.cpp:797`, `NetworkBase.cpp:2891` and `TitleSequencePlayer.cpp`. Its switch ends in `default: break;` and returns `std::nullopt`.
- **Per-tile dispatch:** `UpdateTile<invalidate, invalidateAllViewports>` (324-396) switches on the element type, with `default: break;` (390). Each handler returns whether the tile still animates and calls:
```cpp
Invalidate<invalidateAllViewports>(viewport, loc.x, loc.y, baseZ, el.getClearanceZ(), kMaxZoom);
```
- **The unregister trap:** `InvalidateAll` drops a tile when `UpdateTile` returns nullopt (678-680):
```cpp
if (!UpdateTile<true, false>(tileCoords, viewport)) _mapAnimationsInvalidate[...] = false;
```
  An animated factory tile therefore needs **three things**: a `case TileElementType::factory` in `IsElementAnimated` (for load), a case in `UpdateTile` (otherwise it is unregistered on the first frame), and a `MarkTileForInvalidation` call in the factory place action. Use `MarkTileForUpdate` if per-tick logic is needed.

---

## Hazards and gotchas
1. **`-Werror -Wall -Wextra`** is set in `CMakeLists.txt:347`. `ViewportInteractionRightClick` (`ViewportInteraction.cpp:562-613`) has no `default:`, so adding a `ViewportInteractionItem` enumerator without a case there is a `-Wswitch` build error.
2. **The interaction filter is capped at `banner`** (`Viewport.cpp:1479`). A new item placed after `banner` can never be hit-tested until that line changes. The FlagHolder<uint16_t> fits at most 16 items.
3. **`PaintFactory` must set `session.InteractionType` itself.** Otherwise it inherits `terrain` from the surface painted earlier on the same tile (`Paint.Surface.cpp:1343`), with `ps->Element` set to the factory element. Code that does `info.Element->asSurface()->…` without a null check would then crash. Set `none` for ghosts, as `Paint.SmallScenery.cpp:350` does.
4. **Widget indices are positional.** In TopToolbar, `WindowTopToolbarWidgetIdx` and `_topToolbarWidgets` must stay in the same order, with `WIDX_SEPARATOR` last. Any `WC_*__WIDX_*` constant used by shortcuts must match the real index (`VALIDATE_GLOBAL_WIDX`, `WidgetIndexGlobals.h`). Inserting widgets before an exported index breaks the static_assert.
5. **View menu ids must match positions:** `static_assert(ItemIDsMatchIndices(items))` (TopToolbar.cpp:305). New items must be appended with matching `DDIDX_*` values, and `TOP_TOOLBAR_VIEW_MENU_COUNT` updated. Only `ViewportFlag` bit 22 is free.
6. **Missing pieces fail without a crash:** no `WindowManager::OpenWindow` case means nullptr; no Theme row means black, non-transparent colours; not in a Themes tab list means not editable.
7. **`gWindowSceneryRotation` is shared with Scenery** (reset to 3 on Scenery open). Keep a separate rotation for the factory window, and apply `(rot - GetCurrentRotation()) & 3`.
8. **String ids must stay ≤ 8191** (object strings use 0x2000-0x5000).
9. **Ghosts:** they are stripped on save, but during the guest update tick only footpath, track and track-design ghosts are removed (`ProvisionalElements.cpp`). Ghost actions are not networked. Always remove the old ghost before placing a new one and before the real `Execute`.
10. **There is no `kMaxScenerySelections`.** The real caps are Scenery `kMaxTabs = 257` and `kMaxTabsPerRow = 20` (Scenery.cpp:89-90), and dropdown `kItemsMaxSize = 1024` (Dropdown.h:39).
11. **Upstream bug:** `createImageImportMetaFromJson` (`ImageImporter.cpp:453`) calls `Json::GetBoolean("noDrawOnZoom")` instead of reading `input["noDrawOnZoom"]`, so `noDrawOnZoom` in object JSON is ignored.
12. **Small-scenery animation is skipped above zoom 1** unless `isVisibleWhenZoomed`, and MapAnimation invalidation stops above zoom 2. Paint runs during render and may be multithreaded, so only read state there.
13. **Build files:** CMake globs sources (`src/openrct2-ui/CMakeLists.txt:39`, `src/openrct2/CMakeLists.txt:1`), but new `.cpp` files must be added to `src/openrct2-ui/libopenrct2ui.vcxproj` or `src/openrct2/libopenrct2.vcxproj` by hand, as the factory commit did.

<!-- Research note produced 2026-10-03 against upstream commit 5d86c6b. Line numbers drift as upstream moves; verify before editing. -->

# Integration brief: fork GameActions at `GameCommand` ≥ 10000

Repo: `/home/user/Documents/Projects/factory-tour`, HEAD `c789149677`. This is read-only research; I changed nothing.

**Fork policy already written down:**
- `docs/adr/0006-fork-game-actions-in-a-reserved-command-range.md` says:
  - `kFactoryCommandBase = 10000`.
  - A fork-owned `FactoryActionRegistry` that the upstream registry defers to for ids ≥ base.
  - Actions derive from `GameActionBase<static_cast<GameCommand>(kFactoryCommandBase + n)>`.
  - Actions must support `CommandFlag::ghost` and belong to `Permission::factory`.
  - Bump `kStreamVersion` and `kReplayVersion` whenever an action's wire format changes.
- `CONTEXT.md:189` reserves "`GameCommand` 10000+ via `FactoryCommand`, own registry". `CONTEXT.md:201` reserves "`Permission` `factory` appended".
- `CLAUDE.md` adds:
  - Every edit to an upstream file needs a `// FACTORY-TOUR:` marker on the line above.
  - New files go in `src/openrct2/factory/` (the directory does not exist yet) and must also be added to `src/openrct2/libopenrct2.vcxproj` and `test/tests/tests.vcxproj`.
  - CMake globs `src/openrct2/**/*.cpp` automatically. Test files are listed explicitly in `test/tests/CMakeLists.txt`.
- There is **no `GameActionCompat` file**. The registry is only `GameActionRegistry.h/.cpp`.

---

## 1. `src/openrct2/actions/GameAction.hpp` (197 lines)

**Action flags (lines 31-37):**
```cpp
namespace Flags {
    constexpr uint16_t AllowWhilePaused = 1 << 0;
    constexpr uint16_t ClientOnly = 1 << 1;
    constexpr uint16_t EditorOnly = 1 << 2;
    constexpr uint16_t IgnoreForReplays = 1 << 3;
}
```

**Base class (lines 46-176):**
```cpp
class GameAction {
public:
    using Ptr = std::unique_ptr<GameAction>;
    using Callback_t = std::function<void(const class GameAction*, const Result*)>;
private:
    GameCommand const _type;                       // line 53, NOT serialised by Serialise()
    Network::PlayerId_t _playerId = { -1 };
    CommandFlags _flags = {};
    uint32_t _networkId = 0;
    Callback_t _callback;
public:
    GameAction(GameCommand type) : _type(type) {}  // 61-64
    virtual ~GameAction() = default;
    const char* GetName() const;                   // 68 -> GameActions::GetName(_type) (GameAction.cpp:21-24)
    virtual void AcceptParameters(GameActionParameterVisitor&) {}   // 70
    void AcceptFlags(GameActionParameterVisitor& visitor) { visitor.Visit("flags", _flags.holder); }
    Network::PlayerId_t GetPlayer() const; void SetPlayer(Network::PlayerId_t);
    virtual uint16_t GetActionFlags() const {      // 92-108
        uint16_t flags = 0;
        if (GetFlags().hasAny(CommandFlag::ghost, CommandFlag::noSpend)) flags |= Flags::ClientOnly;
        if (GetFlags().has(CommandFlag::allowDuringPaused)) flags |= Flags::AllowWhilePaused;
        return flags;
    }
    CommandFlags GetFlags() const; CommandFlags SetFlags(CommandFlags flags);
    GameCommand GetType() const { return _type; }  // 120
    void SetCallback(Callback_t); const Callback_t& GetCallback() const;
    void SetNetworkId(uint32_t); uint32_t GetNetworkId() const;
    virtual void Serialise(DataSerialiser& stream) {                 // 145-148
        stream << DS_TAG(_networkId) << DS_TAG(_flags.holder) << DS_TAG(_playerId);
    }
    void Serialise(DataSerialiser& stream) const;  // const_cast helper
    virtual uint32_t GetCooldownTime() const { return 0; }
    virtual Result Query(GameState_t& gameState, Park::ParkData& park) const = 0;   // 168
    virtual Result Execute(GameState_t& gameState, Park::ParkData& park) const = 0; // 173
    bool LocationValid(const CoordsXY& coords) const;  // MapIsLocationValid + plugin actionLocation hook
};
```

**Template and free functions (lines 182-195):**
```cpp
template<GameCommand TType>
struct GameActionBase : GameAction {
    static constexpr GameCommand kType = TType;
    GameActionBase() : GameAction(kType) {}
};
GameAction::Ptr Create(GameCommand id);
GameAction::Ptr Clone(const GameAction* action);
```

**`CommandFlag` (`actions/CommandFlag.h:18-43`):** `apply`, `replay`, `allowDuringPaused=3`, `noSpend=5`, `ghost=6`, `trackDesign=7`, `networked=31`. The type is `using CommandFlags = FlagHolder<uint32_t, CommandFlag>`.

**`GameCommand` (`actions/GameCommand.h:16`):** `enum class GameCommand : int32_t`. It runs from `setRideAppearance` (0) to `setRideVisibility` (84), and `count` = 85 (line 103).

**`Result` (`actions/GameActionResult.h:54-105`):**
- Fields:
  - `Status error = ok`
  - `StringVariant errorTitle`, `errorMessage`
  - `std::array<uint8_t,32> errorMessageArgs`
  - `CoordsXYZ position = {kLocationNull...}`
  - `money64 cost = 0`
  - `ExpenditureType expenditure = ExpenditureType::count`
  - `std::any resultData` (a shared_ptr on Android)
- Constructor `Result(Status, StringId title, StringId message, uint8_t* args = nullptr)`.
- Typed payload via `setData(T&&)` and `getData<T>()`.
- `Status` values (28-49): `ok`, `invalidParameters`, `disallowed`, `gamePaused`, `insufficientFunds`, `notInEditorMode`, `notOwned`, `tooLow`, `tooHigh`, `noClearance`, `itemAlreadyPlaced`, `notClosed`, `broken`, `noFreeElements`, `unknown=0xFFFF`.

**How the type travels:**
- `_type` is **not** written by `Serialise`. It goes out-of-band:
  - **Network:** `packet << getGameState().currentTicks << action->GetType() << stream;` (`network/NetworkBase.cpp:1572` client send, `:1583` server send).
  - `Packet::operator<<(T)` (`network/NetworkPacket.h:75-81`) writes `ByteSwapBE(value)`, and `ByteSwapBE` handles enums by underlying type (`core/Endianness.h:71`). So the type is a **4-byte big-endian int32**, and 10000 fits.
  - Receive does `packet >> tick >> actionType;` into a `GameCommand` (`NetworkBase.cpp:3003`, `:3046`).
  - **Replay:** a `uint32_t actionType` (see §6).
- `Clone` (`GameAction.cpp:74-91`) calls `Create(action->GetType())`, then round-trips through a `DataSerialiser`. **Every enqueue and every replay record clones, so it needs the registry.**

`GameAction.cpp:60-72`:
```cpp
GameAction::Ptr Create(GameCommand id) {
    GameAction* result = nullptr;
    auto factory = getFactory(id);
    if (factory.has_value()) { result = (*factory)(); }
    Guard::ArgumentNotNull(result, "Attempting to create unregistered game action: %u", id);
    return std::unique_ptr<GameAction>(result);
}
```

---

## 2. `GameActionRegistry.h` / `.cpp`

**`GameActionRegistry.h`:**
- Forward-declares `enum class GameCommand : int32_t;` (line 17).
- Lines 24-28:
```cpp
using GameActionFactory = GameAction* (*)();
std::optional<GameActionFactory> getFactory(GameCommand command);
const char* GetName(GameCommand id);
bool IsValidId(uint32_t id);
```

**`GameActionRegistry.cpp` (254 lines):**
- Lines 13-96 include every action header; `general/CustomAction.h` is at line 20.
- Lines 102-125:
```cpp
struct GameActionEntry { GameActionFactory factory{}; const char* name{}; };
using GameActionRegistry = std::array<GameActionEntry, EnumValue(GameCommand::count)>;   // 108  <-- sized by count
template<GameCommand TId>
static constexpr void Register(GameActionRegistry& registry, GameActionFactory factory, const char* name) {
    constexpr auto idx = static_cast<size_t>(TId);
    static_assert(idx < EnumValue(GameCommand::count));                                 // 115  <-- rejects 10000+
    registry[idx] = { factory, name };
}
template<typename T>
static constexpr void Register(GameActionRegistry& registry, const char* name) {
    GameActionFactory factory = []() -> GameAction* { return new T(); };
    Register<T::kType>(registry, factory, name);
}
```
- Lines 127-223, `BuildRegistry()`:
  - `#define REGISTER_ACTION(type) Register<type>(registry, #type)` at line 131.
  - 83 `REGISTER_ACTION(...)` lines (133-215), then `#ifdef ENABLE_SCRIPTING REGISTER_ACTION(CustomAction); #endif` (216-218), then `#undef`.
  - The registered name is the class name string, e.g. `"BannerPlaceAction"`.
- Lines 225-252:
```cpp
static constexpr GameActionRegistry _registry = BuildRegistry();
std::optional<GameActionFactory> getFactory(GameCommand id) {
    const auto idx = static_cast<size_t>(id);
    if (idx < std::size(_registry)) { return _registry[idx].factory; }   // may be an engaged optional holding nullptr!
    return std::nullopt;
}
const char* GetName(GameCommand id) {
    const auto idx = static_cast<size_t>(id);
    Guard::IndexInRange(idx, _registry);          // asserts for any fork id
    return _registry[idx].name;
}
bool IsValidId(uint32_t id) {
    if (id < std::size(_registry)) { return _registry[id].factory != nullptr; }
    return false;
}
```

**Defer points, each a FACTORY-TOUR touch point:**
- `getFactory`: add `if (EnumValue(id) >= kFactoryCommandBase) return Factory::getFactory(id);` before the range check.
- `GetName`: add the same before `Guard::IndexInRange`.
- `IsValidId`: add the same for completeness. It currently has **no callers**.

**Flag 1 (existing upstream crash):** `getFactory` returns an *engaged* optional containing `nullptr` when the id is in range but unregistered. `GameCommand::pickupStaff` (id 68) has **no action class**, and `custom` (id 76) is null without `ENABLE_SCRIPTING`. `Create` then calls `(*factory)()`, which is a null function-pointer call. A client sending id 68 with staff permission reaches `Create` at `NetworkBase.cpp:3065`. The fork's deferring `getFactory` should return `nullopt` for a null factory, and so should a fixed upstream path.

**Flag 2:** `Create` asserts (`Guard::ArgumentNotNull`) *before* callers can null-check.
- `test/tests/tests.cpp:20` sets `Guard::SetAssertBehaviour(AssertBehaviour::abort)`, so in tests an unregistered id aborts the process.
- In release, the default `cAssert` is compiled out and `nullptr` is returned.

---

## 3. Callers of `getFactory` / `GetName` / `IsValidId` / `Create` / `Clone`

| Site | Notes |
|---|---|
| `actions/GameAction.cpp:23` | `GameAction::GetName()` calls `GameActions::GetName(_type)`. Used by runner logging (`GameActionRunner.cpp:114, 230, 309, 322`) on every top-level non-ghost execute, so **`GetName` must handle fork ids**. |
| `actions/GameAction.cpp:64` | `Create` calls `getFactory`. |
| `actions/GameAction.cpp:76` | `Clone` calls `Create(action->GetType())`. Clone is used by `Enqueue(const GameAction*)` (`GameActionRunner.cpp:72-76`) and `ReplayManager::AddGameAction` (`ReplayManager.cpp:156-164`). |
| `network/NetworkBase.cpp:3011` | `Client_Handle_GAME_ACTION` (2998-3032): `Create(actionType)`, then `if (action == nullptr) { LOG_ERROR(...); return; }`, then `Serialise(ds)`, restores callbacks by networkId, then `Enqueue(std::move(action), tick)`. |
| `network/NetworkBase.cpp:3065` | `ServerHandleGameAction` (3034-3096), described below. |
| `ReplayManager.cpp:700` | `Create(static_cast<GameCommand>(actionType))` on load. |
| `scripting/ScriptEngine.cpp:1771` | `CreateGameActionFromActionId` calls `Create(ActionNameToType[name])`. |

`IsValidId` and the free `GameActions::GetName` have no other callers. Tests never call `Create` directly.

**`ServerHandleGameAction` (`NetworkBase.cpp:3034-3096`):**
```cpp
packet >> tick >> actionType;
if (actionType == GameCommand::togglePause || actionType == GameCommand::loadOrQuit) return;   // 3048
if (actionType != GameCommand::custom) {
    NetworkGroup* group = GetGroupByID(connection.player->group);
    if (group == nullptr || group->canPerformCommand(actionType) == false) {                  // 3057
        ServerSendShowError(connection, STR_CANT_DO_THIS, STR_PERMISSION_DENIED); return; }
}
GameActions::GameAction::Ptr ga = GameActions::Create(actionType);                            // 3065
if (ga == nullptr) { LOG_ERROR("Received unregistered game action type: 0x%08X ..."); return; }
// cooldown: player->cooldownTime.find(actionType) / ga->GetCooldownTime()   (3074-3092; std::unordered_map<GameCommand,int32_t>, NetworkPlayer.h:47)
DataSerialiser stream(false); ... ga->Serialise(stream);
ga->SetPlayer(PlayerId_t{ connection.player->id });
GameActions::Enqueue(std::move(ga), tick);
```
Because the permission check runs before `Create`, **a fork id that is not listed in any `Permission` is rejected with "permission denied"** (`canPerformCommand` returns false when `findCommand` returns `Permission::count`).

**Scripting (`scripting/ScriptEngine.cpp`):**
- `ActionNameToType` is a `const static EnumMap<GameCommand>` at 1671-1753. Names are lowercase, e.g. `{ "bannerplace", GameCommand::placeBanner }`.
- `EnumMap` (`core/EnumMap.hpp`) sorts by value and only uses direct indexing when the values are contiguous. Otherwise it binary-searches. The map is already non-contiguous, so adding `{ "factory…", static_cast<GameCommand>(10000+n) }` is safe.
- `GetActionName(GameCommand)` (1756-1764) looks up value to name; it gives the hook `"action"` string.
- `CreateGameActionFromActionId(name)` (1766-1774) returns `GameActions::Create(result->second)` or `nullptr`.
- `ScriptEngine::CreateGameAction(ctx, actionid, args, pluginName)` (1845-1888):
  - If the name is known: `AcceptParameters(JSToGameActionParameterVisitor)`, plus `AcceptFlags` if `args.flags` is a number. It returns `{action, visitor.GetErrorFlag()}`.
  - **Otherwise it falls through** to `std::make_unique<GameActions::CustomAction>(actionid, json, pluginName)` (1882). An unmapped fork action name therefore becomes a CustomAction, not an error.
- `ScContext.hpp:314-348` `QueryOrExecuteAction`: `executeAction` sets a callback, then `GameActions::Execute(action.get(), getGameState())`. `queryAction` calls `GameActions::Query`. A `nullptr` action throws "Unknown action."
- `RunGameActionHooks` (1776-1843):
  - If `GetType() == GameCommand::custom`: `static_cast<const CustomAction&>`.
  - Else: `"action" = GetActionName(id)`, omitted if empty. `"args"` comes from `AcceptParameters` + `AcceptFlags`. `"type" = EnumValue(actionId)` (int32).
- `GameActionResultToJS` (≈1495-1556) special-cases `createRide`, `hireNewStaffMember`, `placeBanner`, `placeLargeScenery`, `placeWall` via `getData<>`. Its `default:` branch is harmless.
- `CustomAction` (`actions/general/CustomAction.h/.cpp`): `GameActionBase<GameCommand::custom>`. `GetActionFlags() | AllowWhilePaused`. `Serialise` adds `_id`, `_json`. Query/Execute go to `ScriptEngine::QueryOrExecuteCustomGameAction` (`ScriptEngine.cpp:1361`).
- Plugin API: `kPluginApiVersion = 124` (`scripting/ScriptEngine.h:46`). Action names are listed in `distribution/scripting/openrct2.d.ts` (`ActionType` union ending ≈772).

**`actions/GameActionRunner.cpp` (454 lines):**
- `Enqueue(const GameAction*)` (72-76) clones, so it needs the registry.
- `Enqueue(Ptr&&)` (78-87) sets the player id when networked.
- `ProcessQueue` (89-149):
  - Lines 122-133: `switch (queued.action->GetType())` with ghost-removal cases (`placeWall`, `placeLargeScenery`, `placeBanner`, `placeScenery`) and `default: break`.
  - Then sets `CommandFlag::networked` (138), `Execute` (140), and the server relays via `Network::SendGameAction` (144).
- `CheckActionInPausedMode` (156-165).
- `QueryInternal` (167-198):
```cpp
uint16_t actionFlags = action->GetActionFlags();
if (topLevel && !CheckActionInPausedMode(gameState, actionFlags)) { ... Status::gamePaused ... }
auto& park = gameState.park;
auto result = action->Query(gameState, park);
if (result.error == Status::ok) {
    if (!FinanceCheckAffordability(result.cost, action->GetFlags())) {          // 189
        result.error = Status::insufficientFunds;
        result.errorTitle = STR_CANT_DO_THIS;
        result.errorMessage = STR_NOT_ENOUGH_CASH_REQUIRES;
        Formatter(result.errorMessageArgs.data()).Add<uint32_t>(result.cost);
    }
    // <-- MATERIALS AFFORDABILITY CHECK SLOTS IN HERE (same guard semantics)
}
return result;
```
- `ExecuteInternal` (264-442):
  - Replay guard (274-288): while replaying or normalising, reject actions that lack `CommandFlag::replay`, unless the action has `IgnoreForReplays`.
  - `QueryInternal` (290), then the query hooks (291-298).
  - **Top-level routing (301-328):**
    - A client sends to the server unless `ClientOnly` or already `networked` (`Network::SendGameAction`, return).
    - Otherwise, `if (server || !gInUpdateCode)` and not `ClientOnly`/`networked`, it calls `Enqueue(action, gameState.currentTicks)` and **returns the query result without executing**.
  - Log begin if not ghost (330-334). `action->Execute(gameState, park)` (340). Execute hooks (341-348). Log finish.
  - `if (!topLevel) return result;` (355-356).
  - Lines 358-363:
```cpp
if (result.error == Status::ok && FinanceCheckMoneyRequired(flags) && result.cost != 0) {
    FinancePayment(result.cost, result.expenditure);
    MoneyEffect::create(result.cost, result.position);
}
// <-- MATERIALS CONSUMPTION SLOTS IN HERE (top-level only, runs on every peer from ProcessQueue -> deterministic)
```
  - Lines 365-404: if not ClientOnly and ok:
    - Networked: `SetPlayerLastAction(playerIndex, action->GetType())`, which calls `NetworkActions::findCommand`, plus money-spent and last-action coordinates.
    - Else: replay recording `replayManager->AddGameAction(currentTicks, action)` (401), but only when not ghost/noSpend.
  - Callback (414-418). Error window only if not ghost, not noSpend and top level (421-439).
- `management/Finance.cpp:60-81`:
  - `FinanceCheckMoneyRequired` is false if the park has the `noMoney` flag, in editor mode, or with `noSpend` or `ghost`.
  - `FinanceCheckAffordability = !required || cost <= 0 || cost <= park.cash`.
  - Reuse this predicate to gate material checks for ghost/noSpend.

---

## 4. Network permissions

**`network/NetworkAction.h`:**
- `enum class Permission : uint32_t` (21-49): `chat, terraform, setWaterLevel, togglePause, createRide, removeRide, buildRide, rideProperties, scenery, path, clearLandscape, guest, staff, parkProperties, parkFunding, kickPlayer, modifyGroups, setPlayerGroup, cheat, toggleSceneryCluster, passwordlessLogin, modifyTile, editScenarioOptions, dragPathArea,` then `count`. **Append `factory` right before `count`.**
- `class NetworkAction { StringId name; std::string permissionName; std::vector<GameCommand> commands; };` (51-57).
- `static const std::array<NetworkAction, static_cast<size_t>(Permission::count)> kActions;` (62). It is sized by count, so it fails to compile until a matching entry is added.
- `findCommand(GameCommand)` and `findCommandByPermissionName`.

**`network/NetworkAction.cpp`:**
- `findCommand` (20-37) is a linear search over all lists and returns `Permission::count` if not found. **It does not rely on ids being contiguous.**
- The comment at 51-54 says to also add the permission name to `PermissionType` in `distribution/scripting/openrct2.d.ts` (lines 4265-4289; append `"factory"`).
- The table is at 55-278. The last entry, 273-277:
```cpp
NetworkAction{ STR_ACTION_PATH_DRAG_AREA, "PERMISSION_DRAG_PATH_AREA", {}, },
```
- The new entry goes after it:
```cpp
NetworkAction{ STR_ACTION_FACTORY, "PERMISSION_FACTORY", { static_cast<GameCommand>(kFactoryCommandBase + 0), ... } },
```
- The whole file is wrapped in `#ifndef DISABLE_NETWORK`.

**String ids (`localisation/StringIds.h`):**
- `STR_ACTION_CHAT = 5645` … `STR_ACTION_PASSWORDLESS_LOGIN = 5666` (1352-1372), `STR_ACTION_MODIFY_TILE = 6016` (1446), `STR_ACTION_EDIT_SCENARIO_OPTIONS = 6040` (1449), `STR_ACTION_PATH_DRAG_AREA = 7013` (1774).
- Text lives in `data/language/en-GB.txt`, e.g. `STR_7013    :Drag areas of path` (line 3806).
- The highest upstream id is 7063.
- **Caveat:** `LanguagePack.cpp:257` parses `sscanf(identifier, "STR_%4d", &stringId)`, so ids are 4 digits max. The fork's reserved string range 0x5000-0x9FFF (`CONTEXT.md`) needs the planned 5-digit parser patch first.

**`NetworkGroup` (`network/NetworkGroup.h/.cpp`):**
- `std::array<uint8_t, 8> actionsAllowed{}` gives 64 permission bits; there are 24 today, so adding `factory` (bit 24) fits with no packet change.
- `canPerformCommand` (cpp 123-131): `findCommand`, then `canPerformAction`, else **false**.
- `fromJson` (20-49) toggles only the permission names listed in the JSON.
- `toJson` writes names for every set bit.

**Default groups (`NetworkBase::SetupDefaultGroups`, `NetworkBase.cpp:1085-1115`):**
- Admin: `actionsAllowed.fill(0xFF)`.
- Spectator: toggles `chat` only.
- User: `fill(0xFF)`, then toggles off `kickPlayer, modifyGroups, setPlayerGroup, cheat, passwordlessLogin, modifyTile, editScenarioOptions`.
- So `factory` would default ON for Admin and User, with no edit needed.
- **Caveat:** an existing `groups.json` (loaded by `LoadGroups`, `fromJson`) lists permissions by name. Saved groups, Admin included, will not have `PERMISSION_FACTORY` until someone edits them.
- `NetworkUser` only stores `std::optional<uint8_t> groupId` (`NetworkUser.h:26`) and has no permission defaults.

**Other consumers adapt automatically:**
- UI: `Network::GetNumActions()` and `GetActionNameStringID()` (`NetworkBase.cpp:3851-3863`, used by `openrct2-ui/windows/Multiplayer.cpp:303/389/416/625/815`, `Player.cpp:455`).
- Scripting `ScPlayerGroup.cpp:95-135` iterates `kActions`.
- `SetPlayerLastAction` (`NetworkBase.cpp:3491-3497`) stores `findCommand(command)`.

**Stream version:**
- `NetworkBase.cpp:50` `constexpr uint8_t kStreamVersion = 2;`
- `NetworkBase.cpp:52` `kStreamID = kOpenRCT2Version + "-" + to_string(kStreamVersion)`.

---

## 5. Template actions

**`actions/scenery/BannerPlaceAction.h` (lines 10-48, full):**
```cpp
#pragma once
#include "../GameAction.hpp"
namespace OpenRCT2::Drawing { enum class Colour : uint8_t; }
namespace OpenRCT2::GameActions
{
    struct BannerPlaceActionResult { BannerIndex bannerId = BannerIndex::GetNull(); };

    class BannerPlaceAction final : public GameActionBase<GameCommand::placeBanner>
    {
    private:
        CoordsXYZD _loc;
        ObjectEntryIndex _bannerType{ kBannerNull };
        Drawing::Colour _primaryColour{};
    public:
        BannerPlaceAction() = default;
        BannerPlaceAction(const CoordsXYZD& loc, ObjectEntryIndex bannerType, Drawing::Colour primaryColour);
        void AcceptParameters(GameActionParameterVisitor&) final;
        uint16_t GetActionFlags() const override;
        void Serialise(DataSerialiser& stream) override;
        Result Query(GameState_t& gameState, Park::ParkData& park) const override;
        Result Execute(GameState_t& gameState, Park::ParkData& park) const override;
    private:
        PathElement* GetValidPathElement() const;
    };
}
```

**`actions/scenery/BannerPlaceAction.cpp` (lines 10-182, full):**
```cpp
#include "BannerPlaceAction.h"
#include "../../Diagnostic.h"
#include "../../core/Guard.hpp"
#include "../../drawing/TextColour.h"
#include "../../management/Finance.h"
#include "../../object/BannerSceneryEntry.h"
#include "../../object/ObjectEntryManager.h"
#include "../../world/Banner.h"
#include "../../world/Footpath.h"
#include "../../world/Map.h"
#include "../../world/MapAnimation.h"
#include "../../world/TileElementsView.h"
#include "../../world/tile_element/BannerElement.h"
#include "../../world/tile_element/PathElement.h"
#include "../GameAction.hpp"

namespace OpenRCT2::GameActions
{
    BannerPlaceAction::BannerPlaceAction(const CoordsXYZD& loc, ObjectEntryIndex bannerType, Drawing::Colour primaryColour)
        : _loc(loc), _bannerType(bannerType), _primaryColour(primaryColour) {}

    void BannerPlaceAction::AcceptParameters(GameActionParameterVisitor& visitor)
    {
        visitor.Visit(_loc);
        visitor.Visit("object", _bannerType);
        visitor.Visit("primaryColour", _primaryColour);
    }

    uint16_t BannerPlaceAction::GetActionFlags() const { return GameAction::GetActionFlags(); }

    void BannerPlaceAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc) << DS_TAG(_bannerType) << DS_TAG(_primaryColour);
    }

    Result BannerPlaceAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.position.x = _loc.x + 16;
        res.position.y = _loc.y + 16;
        res.position.z = _loc.z;
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_CANT_POSITION_THIS_HERE;

        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_CANT_POSITION_THIS_HERE, STR_OFF_EDGE_OF_MAP);
        if (!MapCheckCapacityAndReorganise(_loc))
        {
            LOG_ERROR("No free map elements.");
            return Result(Status::noFreeElements, STR_CANT_POSITION_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
        }
        auto pathElement = GetValidPathElement();
        if (pathElement == nullptr)
            return Result(Status::invalidParameters, STR_CANT_POSITION_THIS_HERE, STR_CAN_ONLY_BE_BUILT_ACROSS_PATHS);
        if (!MapCanBuildAt(_loc))
            return Result(Status::notOwned, STR_CANT_POSITION_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);

        auto baseHeight = _loc.z + kPathHeightStep;
        BannerElement* existingBannerElement = MapGetBannerElementAt({ _loc.x, _loc.y, baseHeight }, _loc.direction);
        if (existingBannerElement != nullptr)
            return Result(Status::itemAlreadyPlaced, STR_CANT_POSITION_THIS_HERE, STR_BANNER_SIGN_IN_THE_WAY);
        if (HasReachedBannerLimit())
        {
            LOG_ERROR("No free banners available");
            return Result(Status::invalidParameters, STR_CANT_POSITION_THIS_HERE, STR_TOO_MANY_BANNERS_IN_GAME);
        }
        auto* bannerEntry = ObjectEntryManager::GetObjectEntry<BannerSceneryEntry>(_bannerType);
        if (bannerEntry == nullptr)
        {
            LOG_ERROR("Banner entry not found for bannerType %u", _bannerType);
            return Result(Status::invalidParameters, STR_CANT_POSITION_THIS_HERE, STR_ERR_BANNER_ELEMENT_NOT_FOUND);
        }
        res.cost = bannerEntry->price;
        res.setData(BannerPlaceActionResult{});
        return res;
    }

    Result BannerPlaceAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.position.x = _loc.x + 16;
        res.position.y = _loc.y + 16;
        res.position.z = _loc.z;
        res.expenditure = ExpenditureType::landscaping;
        res.errorTitle = STR_CANT_POSITION_THIS_HERE;

        if (!MapCheckCapacityAndReorganise(_loc))
        {
            LOG_ERROR("No free map elements.");
            return Result(Status::noFreeElements, STR_CANT_POSITION_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
        }
        auto* bannerEntry = ObjectEntryManager::GetObjectEntry<BannerSceneryEntry>(_bannerType);
        if (bannerEntry == nullptr)
        {
            LOG_ERROR("Banner entry not found for bannerType %u", _bannerType);
            return Result(Status::invalidParameters, STR_CANT_POSITION_THIS_HERE, STR_ERR_BANNER_ELEMENT_NOT_FOUND);
        }
        auto banner = CreateBanner();
        if (banner == nullptr)
        {
            LOG_ERROR("No free banners available");
            return Result(Status::invalidParameters, STR_CANT_POSITION_THIS_HERE, STR_TOO_MANY_BANNERS_IN_GAME);
        }
        banner->flags = {};
        banner->text = {};
        banner->textColour = Drawing::TextColour::white;
        banner->type = _bannerType; // Banner must be deleted after this point in an early return
        banner->colour = _primaryColour;
        banner->position = TileCoordsXY(_loc);

        res.setData(BannerPlaceActionResult{ banner->id });
        auto* bannerElement = TileElementInsert<BannerElement>({ _loc, _loc.z + (2 * kCoordsZStep) }, 0b0000);
        Guard::Assert(bannerElement != nullptr);

        bannerElement->setClearanceZ(_loc.z + kPathClearance);
        bannerElement->setPosition(_loc.direction);
        bannerElement->resetAllowedEdges();
        bannerElement->setIndex(banner->id);
        bannerElement->setGhost(GetFlags().has(CommandFlag::ghost));

        MapInvalidateTileFull(_loc);
        MapAnimations::MarkTileForInvalidation(TileCoordsXY(_loc));

        res.cost = bannerEntry->price;
        return res;
    }

    PathElement* BannerPlaceAction::GetValidPathElement() const
    {
        for (auto* pathElement : TileElementsView<PathElement>(_loc))
        {
            if (pathElement->getBaseZ() != _loc.z && pathElement->getBaseZ() != _loc.z - kPathHeightStep) continue;
            if (!(pathElement->getEdges() & (1 << _loc.direction))) continue;
            if (pathElement->isGhost() && !GetFlags().has(CommandFlag::ghost)) continue;
            return pathElement;
        }
        return nullptr;
    }
}
```

**`actions/scenery/BannerRemoveAction.cpp` (lines 10-156, full; the header is the same shape with `CoordsXYZD _loc` and `BannerElement* GetBannerElementAt() const`):**
```cpp
#include "BannerRemoveAction.h"
#include "../../Diagnostic.h"
#include "../../management/Finance.h"
#include "../../object/BannerSceneryEntry.h"
#include "../../object/ObjectEntryManager.h"
#include "../../world/Banner.h"
#include "../../world/Map.h"
#include "../../world/TileElementsView.h"
#include "../../world/tile_element/BannerElement.h"
#include "../GameAction.hpp"

namespace OpenRCT2::GameActions
{
    BannerRemoveAction::BannerRemoveAction(const CoordsXYZD& loc) : _loc(loc) {}
    void BannerRemoveAction::AcceptParameters(GameActionParameterVisitor& visitor) { visitor.Visit(_loc); }
    uint16_t BannerRemoveAction::GetActionFlags() const { return GameAction::GetActionFlags(); }
    void BannerRemoveAction::Serialise(DataSerialiser& stream)
    {
        GameAction::Serialise(stream);
        stream << DS_TAG(_loc);
    }

    Result BannerRemoveAction::Query(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.expenditure = ExpenditureType::landscaping;
        res.position.x = _loc.x + 16;
        res.position.y = _loc.y + 16;
        res.position.z = _loc.z;
        res.errorTitle = STR_CANT_REMOVE_THIS;

        if (!LocationValid(_loc))
            return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, STR_OFF_EDGE_OF_MAP);
        if (!MapCanBuildAt({ _loc.x, _loc.y, _loc.z - 16 }))
            return Result(Status::notOwned, STR_CANT_REMOVE_THIS, STR_LAND_NOT_OWNED_BY_PARK);

        BannerElement* bannerElement = GetBannerElementAt();
        if (bannerElement == nullptr)
        {
            LOG_ERROR("Invalid banner location, x = %d, y = %d, z = %d, direction = %d", _loc.x, _loc.y, _loc.z, _loc.direction);
            return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, kStringIdNone);
        }
        auto bannerIndex = bannerElement->getIndex();
        if (bannerIndex == BannerIndex::GetNull())
        {
            LOG_ERROR("Invalid banner index %u", bannerIndex);
            return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, kStringIdNone);
        }
        auto banner = bannerElement->getBanner();
        if (banner == nullptr)
        {
            LOG_ERROR("Invalid banner index %u", bannerIndex);
            return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, kStringIdNone);
        }
        auto* bannerEntry = ObjectEntryManager::GetObjectEntry<BannerSceneryEntry>(banner->type);
        if (bannerEntry != nullptr)
            res.cost = -((bannerEntry->price * 3) / 4);
        return res;
    }

    Result BannerRemoveAction::Execute(GameState_t& gameState, Park::ParkData& park) const
    {
        auto res = Result();
        res.expenditure = ExpenditureType::landscaping;
        res.position.x = _loc.x + 16;
        res.position.y = _loc.y + 16;
        res.position.z = _loc.z;
        res.errorTitle = STR_CANT_REMOVE_THIS;

        BannerElement* bannerElement = GetBannerElementAt();
        if (bannerElement == nullptr) { LOG_ERROR(...same...); return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, kStringIdNone); }
        auto bannerIndex = bannerElement->getIndex();
        if (bannerIndex == BannerIndex::GetNull()) { LOG_ERROR(...); return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, kStringIdNone); }
        auto banner = bannerElement->getBanner();
        if (banner == nullptr) { LOG_ERROR(...); return Result(Status::invalidParameters, STR_CANT_REMOVE_THIS, kStringIdNone); }
        auto* bannerEntry = ObjectEntryManager::GetObjectEntry<BannerSceneryEntry>(banner->type);
        if (bannerEntry != nullptr)
            res.cost = -((bannerEntry->price * 3) / 4);

        reinterpret_cast<TileElement*>(bannerElement)->removeBannerEntry();
        MapInvalidateTileZoom1({ _loc, _loc.z, _loc.z + 32 });
        bannerElement->remove();          // TileElementBase::remove() == TileElementRemove(this) (TileElementBase.cpp:97-100)
        return res;
    }

    BannerElement* BannerRemoveAction::GetBannerElementAt() const
    {
        for (auto* bannerElement : TileElementsView<BannerElement>(_loc))
        {
            if (bannerElement->getBaseZ() != _loc.z) continue;
            if (bannerElement->isGhost() && !GetFlags().has(CommandFlag::ghost)) continue;
            if (bannerElement->getPosition() != _loc.direction) continue;
            return bannerElement;
        }
        return nullptr;
    }
}
```

**APIs the Banner actions do not use**, so I took them from SmallScenery/Wall actions:
- `world/ConstructionClearance.h:65-69`:
```cpp
[[nodiscard]] GameActions::Result MapCanConstructWithClearAt(const CoordsXYRangedZ& pos, ClearingFunction clearFunc,
    QuarterTile quarterTile, GameActions::CommandFlags flags, MapProposedConstructionInfo additionalInfo = {});
[[nodiscard]] GameActions::Result MapCanConstructAt(const CoordsXYRangedZ& pos, QuarterTile bl);
```
  - Clear functions: `MapPlaceSceneryClearFunc` and `MapPlaceNonSceneryClearFunc`.
  - The result carries `cost` and `getData<ConstructClearResult>().GroundFlags`.
- Pattern (`SmallSceneryPlaceAction.cpp:267-279` Query, `:406-437` Execute):
```cpp
auto canBuild = MapCanConstructWithClearAt({ _loc, zLow, zHigh }, MapPlaceSceneryClearFunc, quarterTile, GetFlags());   // Query
auto canBuild = MapCanConstructWithClearAt({ _loc, zLow, zHigh }, MapPlaceSceneryClearFunc, quarterTile,
                                           GetFlags().with(CommandFlag::apply), { .isTree = isTree });                   // Execute
if (canBuild.error != Status::ok) { canBuild.errorTitle = STR_CANT_POSITION_THIS_HERE; return canBuild; }
res.expenditure = ExpenditureType::landscaping;
res.cost = sceneryEntry->price + canBuild.cost;
auto* sceneryElement = TileElementInsert<SmallSceneryElement>(CoordsXYZ{ _loc, zLow }, quarterTile.GetBaseQuarterOccupied());
if (sceneryElement == nullptr) return Result(Status::noFreeElements, STR_CANT_POSITION_THIS_HERE, STR_TILE_ELEMENT_LIMIT_REACHED);
sceneryElement->setClearanceZ(...);
sceneryElement->setGhost(GetFlags().has(CommandFlag::ghost));
MapInvalidateTileFull(_loc);
```
- Ownership pattern (`SmallSceneryPlaceAction.cpp:170-174`):
```cpp
if (gLegacyScene != LegacyScene::scenarioEditor && !gameState.cheats.sandboxMode
    && !MapIsLocationOwned({ _loc.x, _loc.y, targetHeight }))
    return Result(Status::notOwned, STR_CANT_POSITION_THIS_HERE, STR_LAND_NOT_OWNED_BY_PARK);
```
  - `MapCanBuildAt` (`world/Map.cpp:801-810`) is the equivalent of "editor || sandbox || `MapIsLocationOwned`".
  - `MapIsLocationOwned` (816-836) checks `landOwned`, or `constructionRightsOwned` when z is outside the surface clearance band.
  - The remove side (`WallRemoveAction.cpp:52-57`) skips the ownership check for ghosts: `if (!isGhost && !editor && !sandbox && !MapIsLocationOwned(_loc)) notOwned`.
- Removal: `TileElementRemove(TileElement*)` (`Map.h:105`, `Map.cpp:987`), with `MapInvalidateTileFull(_loc)` first (`SmallSceneryRemoveAction.cpp:126-127`).
  - Ghost lookup when removing (`SmallSceneryRemoveAction.cpp:132-152`): `if (isGhost && !element->isGhost()) continue;`.
- Other `Map.h` APIs:
  - `TileElement* MapGetFirstElementAt(const CoordsXY&)` / `(const TileCoordsXY&)` (71-72).
  - `MapGetSurfaceElementAt` (78-79).
  - `MapCheckCapacityAndReorganise(loc, numElements = 1)` (101).
  - `TileElementInsert(loc, occupiedQuadrants, TileElementType)` (106).
  - `template<T> T* TileElementInsert(const CoordsXYZ&, int32_t)` (116-120) uses `T::kElementType`. **`FactoryElement::kElementType = TileElementType::factory` already exists** (`world/tile_element/FactoryElement.h`).
  - `TileElementInsert` (`Map.cpp:1106+`) sets baseZ and clearanceZ to `loc.z`, flags to 0, owner to 0, and zeroes `pad05` and `pad08`. The caller sets the clearance and ghost flag.
- The ghost tool flow: the UI places with `CommandFlag::ghost`, which `GetActionFlags` turns into `ClientOnly`. Ghosts are never networked or replay-recorded, bypass finance, and get no logging or error UI. `ProcessQueue` removes scenery ghosts only for the 4 hard-coded commands (122-133). A factory ghost-removal case would need a touch point there, or the fork tool removes its own ghost.

---

## 6. `ReplayManager.cpp`

- Lines 109-110: `static constexpr uint16_t kReplayVersion = 11;` and `kReplayMinCompatVersion = 10;`. `kReplayMagic = 0x5243524F`.
- `AddGameAction` (156-164) calls `Clone(action)` and emplaces into `commands`.
- `SerialiseCommand` (682-707):
```cpp
serialiser << command.tick;
serialiser << command.commandIndex;
uint32_t actionType = 0;
if (serialiser.isSaving()) { if (!command.action) return false; actionType = EnumValue(command.action->GetType()); }
serialiser << actionType;                                         // stored as uint32
if (serialiser.isLoading()) { command.action = Create(static_cast<GameCommand>(actionType)); }   // 700
Guard::Assert(command.action != nullptr);
command.action->Serialise(serialiser);
```
- The version check (722-727) accepts `version == kReplayVersion || version >= kReplayMinCompatVersion`. Bumping to 12 keeps old upstream replays loadable.
- The network id mismatch (729-737) is only a warning.

---

## 7. Tables sized by `GameCommand::count`

`grep GameCommand::count` across `src/` and `test/` finds exactly two lines:
- `actions/GameActionRegistry.cpp:108`: `using GameActionRegistry = std::array<GameActionEntry, EnumValue(GameCommand::count)>;`
- `actions/GameActionRegistry.cpp:115`: `static_assert(idx < EnumValue(GameCommand::count));`

The related table sized by `Permission::count` is `NetworkActions::kActions` (`NetworkAction.h:62`, `.cpp:55`).

**Code that assumes ids are below `count` or contiguous:**
1. `GameActionRegistry.cpp` `getFactory`, `GetName` (with `Guard::IndexInRange`) and `IsValidId` all index `_registry` directly, and `Register<>` static_asserts `idx < count`. Fork actions must not go through upstream `REGISTER_ACTION`.
2. `getFactory` returns an engaged optional holding `nullptr` for gaps (`pickupStaff` = 68 has no class; `custom` = 76 without scripting). `Create` then calls through a null pointer.
3. `Create` asserts before callers' null checks (`NetworkBase.cpp:3011/3065`, `ReplayManager.cpp:700-703`). Tests abort on asserts.
4. `NetworkBase.cpp:3881` `CanPerformCommand(uint32_t groupindex, int32_t index)` uses `static_cast<GameCommand>(index)` with a `// TODO`. It is int32, so 10000 is fine.
5. `EnumMap` in `ScriptEngine` uses direct indexing only if contiguous and falls back to binary search otherwise. It is safe.

**Code that does not assume contiguity:**
- `NetworkActions::findCommand` (linear search).
- `player->cooldownTime` (`unordered_map`).
- Hook `"type"` int32.
- Wire int32 big-endian.
- Replay uint32.
- Runner `switch` statements with `default`.

---

## 8. Tests

**Direct `GameActions::Execute` from a test (`test/tests/PlayTests.cpp`):**
- `localStartGame` (43-72):
```cpp
gOpenRCT2Headless = true; gOpenRCT2NoGraphics = true;
auto context = CreateContext(); if (!context->Initialise()) return {};
auto importer = ParkImporter::CreateS6(context->GetObjectRepository());
auto loadResult = importer->LoadSavedGame(parkPath.c_str(), false);
context->GetObjectManager().LoadObjects(loadResult.RequiredObjects);
MapAnimations::ClearAll(); auto& gameState = getGameState(); importer->Import(gameState);
gameState.entities.resetEntitySpatialIndices(); ResetAllSpriteQuadrantPlacements(); Drawing::LoadPalette();
EntityTweener::get().reset(); MapAnimations::MarkAllTiles(); FixInvalidVehicleSpriteSizes(); gGameSpeed = 1;
```
- Helpers:
```cpp
template<class GA, class... Args> static void execute(Args&&... args) { GA ga(std::forward<Args>(args)...); GameActions::Execute(&ga, getGameState()); }   // 84-89
TEST_F(PlayTests, NegativeMarketingCampaignDurationIsRejected) {                       // 91-100
    auto context = localStartGame(TestData::GetParkPath("small_park_with_ferris_wheel.sv6"));
    GameActions::ParkMarketingAction action(ADVERTISING_CAMPAIGN_PARK, 0, -1);
    const auto result = GameActions::Query(&action, getGameState());
    ASSERT_EQ(result.error, GameActions::Status::invalidParameters); }
```
- **Gotcha:** in single-player outside `gameStateUpdateLogic`, `gInUpdateCode == false` (`Game.cpp:72`). A top-level `Execute` therefore **enqueues a Clone and returns the Query result** (`GameActionRunner.cpp:315-326`). The action really runs on the next `gameStateUpdateLogic()` → `ProcessQueue` (`GameState.cpp:355`).
  - So `Clone` → `Create` → the registry is exercised even in direct tests.
  - To execute synchronously in a test, use `GameActions::ExecuteNested` (no finance or replay side effects), or set `gInUpdateCode = true` around the call, or tick `gameStateUpdateLogic()` once.

**Headless fixture (`test/tests/TileElements.cpp:25-50`):**
```cpp
static void SetUpTestCase() {
    std::string parkPath = TestData::GetParkPath("tile-element-tests.sv6");
    gOpenRCT2Headless = true; gOpenRCT2NoGraphics = true;
    _context = CreateContext();
    bool initialised = _context->Initialise(); ASSERT_TRUE(initialised);
    GetContext()->LoadParkFromFile(parkPath);
    GameLoadInit(); // NB: calls `setActiveScene`
    _gLegacyScene = gLegacyScene; SUCCEED(); }
static void TearDownTestCase() { if (_context) _context.reset(); gLegacyScene = _gLegacyScene; }
static std::shared_ptr<IContext> _context; static LegacyScene _gLegacyScene;
```

**`test/tests/ReplayTests.cpp`:**
- Parameterised over `TestData::GetBasePath()/replays/*.parkrep` (46-63).
- The `RunReplay` body (70-103):
```cpp
gOpenRCT2Headless = true; gOpenRCT2NoGraphics = true;
auto context = CreateContext(); ASSERT_TRUE(context->Initialise());
IReplayManager* replayManager = context->GetReplayManager();
replayManager->StartPlayback(replayFile);
while (replayManager->IsReplaying()) { gameStateUpdateLogic(); if (replayManager->IsPlaybackStateMismatching()) break; }
ASSERT_FALSE(replayManager->IsReplaying()); ASSERT_FALSE(replayManager->IsPlaybackStateMismatching());
```
- `INSTANTIATE_TEST_SUITE_P(Replay, ReplayTests, testing::ValuesIn(GetReplayFiles()), PrintReplayParameter());`

**Other test details:**
- `test/tests/FactoryElementTests.cpp` is a fork-owned, context-free `TEST(...)` file with a `// FACTORY-TOUR: fork-owned file.` header. It is the model for new `Factory*Tests.cpp` files.
- New test files go in `test/tests/CMakeLists.txt` (explicit list, e.g. line 17) and `test/tests/tests.vcxproj` (e.g. line 91).
- `tests.cpp:20` sets assert behaviour to abort.

---

## Integration checklist (touch points)

1. **`GameActionRegistry.cpp`** `getFactory`, `GetName` and `IsValidId`: defer ids ≥ 10000 to the fork registry. Make the fork registry return `nullopt` for missing or null factories. Optionally harden the upstream null-factory case.
2. **`NetworkAction.h`**: append `factory` before `count`.
3. **`NetworkAction.cpp`**:
   - Append `NetworkAction{ STR_ACTION_FACTORY, "PERMISSION_FACTORY", {fork ids} }`.
   - Add a new string id: ≤ 4 digits today, or use the fork range after the parser patch.
   - Add the `en-GB.txt` entry.
   - Add `"factory"` to `PermissionType` in `openrct2.d.ts`.
4. **`NetworkBase.cpp:50`**: bump `kStreamVersion`.
5. **`ReplayManager.cpp:109`**: bump `kReplayVersion`.
6. **`ScriptEngine.h:46`**: bump `kPluginApiVersion` if exposed to plugins. Optional: plugin names in `ScriptEngine.cpp` `ActionNameToType` (1671-1753), or a fork lookup in `CreateGameActionFromActionId`. Without one, an unknown name silently becomes a `CustomAction`.
7. **Optional:**
   - A materials check in `QueryInternal` after line 189 and consumption in `ExecuteInternal` after line 363, gated like `FinanceCheckMoneyRequired` (skip ghost, noSpend, editor).
   - A factory ghost-removal case in `ProcessQueue` (122-133).

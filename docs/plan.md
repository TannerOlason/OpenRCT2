# Factory Tour — OpenRCT2 × Factorio fork: work breakdown and bootstrap plan

## Context

Greenfield project in `/home/user/Documents/Projects/factory-tour` (currently an empty directory, not a git repo).
Goal: fork OpenRCT2 and add a Factorio-style production layer so a player can mine, smelt, assemble and
build everything from scratch, then run the factory *as* a theme park: guests tour it, rate it, pay for it,
and the park economy and the factory economy feed each other. Combat/defense is an API-complete stub for
other modders. Long term the game expands to interplanetary and interdimensional touring with a deliberately
weird theme. This planning pass identifies all the work and lays out how to start.

Facts established during research (upstream shallow clone studied read-only in the session scratchpad):

- Upstream: OpenRCT2 `develop`, v0.5.5 (Sep 2026), GPLv3, ~790k lines C++ (paint 316k, ride 102k, scripting 22k,
  actions 21k, entity 18k, world 17k, ui 98k). Default branch `develop`.
- Requires original RCT2 data files (Steam/GOG). OpenGraphics (free replacement) is incomplete, so the fork
  keeps the RCT2-data requirement. **No `g1.dat` found in your home directory** (a deeper D-drive search was still
  running when this plan was written) — see Decisions.
- `GameState_t` is a plain struct with `getGameState()` / `swapGameState()` (`src/openrct2/GameState.h`):
  map, entities, rides, research, date, RNG all live in it. This is the hook for multi-world.
- Machine: 16 cores, 62 GB RAM, root disk 99% full (11 GB), `/media/user/D` has 7.4 TB free. CMake, SDL2,
  libzip, openssl, curl, freetype, fontconfig, nlohmann-json, gtest, pkg-config, clang-format are **not
  installed** (only libpng and ICU are). `gh` is authenticated as TannerOlason.
- Your sibling projects (Fulgora Prime, Bordertorio) use CONTEXT.md vocabulary + `docs/adr/` + `wiki/SCOPE.md` +
  `wiki/SPEC.md`; this fork adopts the same documentation shape.

## Decisions assumed (override any of these when approving)

1. **Mergeable fork, not a hard fork.** Keep tracking upstream `develop`; all new code lives under new directories
   (`src/openrct2/factory/`, `src/openrct2-ui/windows/factory/`, `resources/g3/`, `data/factory/`) with a small
   number of well-marked touch points in upstream files. Rebase/merge upstream monthly.
2. **Factory simulation in C++ inside the engine**, not as a QuickJS plugin. Plugins cannot add tile element
   types, per-tile custom paint, or run a belt sim at 40 Hz for thousands of tiles. The plugin API is instead
   *extended* so third parties (combat, content) can mod without C++.
3. **Belt items, inserter hands, fluids, electricity are NOT `EntityBase` entities.** The entity cap is 65 535 and
   is shared with guests. Factory state lives in a separate deterministic `FactoryState` inside `GameState_t`,
   anchored to tiles by a new `TileElementType::factory` element that stores an index into the factory pool
   (same pattern as Track elements referencing a ride index).
4. **Working title "Factory Tour"**; theme is engine-agnostic (content packs), theme bible written later.
5. **Original art only.** No Factorio or RCT2-derived sprites in the repo. Sprites are produced by your existing
   Blender/imagegen pipeline into a `g3.dat`-style sprite pack built the same way upstream builds `g2.dat`.
6. **RCT2 data comes from your GOG installer** `~/Downloads/setup_rollercoaster_tycoon2_2.0.0.6.exe`. M0 extracts
   it with `innoextract` (apt package; OpenRCT2's own first-run flow uses the same tool) into
   `/media/user/D/rct2-data/` (root disk is full) and sets `game_path` via `openrct2 set-rct2`.
7. **Build tree on the D drive** (`/media/user/D/factory-tour-build`), symlinked into the repo as `build/`,
   because root has 11 GB free.
8. **Codex-monitor convention** from `/home/user/AGENTS.md` is used during implementation sessions
   (`CODEX_MONITOR_AGENT=factory-tour`); not run during planning.
9. **One new `ObjectType` (`factoryPrototype`) with a `kind` field** for items, recipes, machines, technologies,
   ores, rather than four or five new object types: each new ObjectType costs ~11 upstream touch points and editor
   tabs; one type pays that once. Cap raised to 8192 (index is uint16).
10. **Fork-owned persistence**: all fork data lives in fork chunks (0x40–0x45) with their own version numbers, and
    fork fields on upstream structs (guest flags, ride health/stock mode, scenario option extras) are side tables
    keyed by id, so `kParkFileCurrentVersion` and upstream chunks are never touched and vanilla saves stay identical.
11. **Combat stays a stub**: health, damage action, one straight-walking threat entity, a turret that consumes ammo,
    and hooks/bindings. No AI, waves, evolution or guest panic in the fork itself.
12. **Autonomous execution ("auto mode")**: you asked me to proceed without confirmation prompts. That covers
    creating the GitHub fork under your account, installing apt packages, and all repo/build work. I will still stop
    for anything destructive or outside this plan's scope.

## Source-architecture notes that drive the design

(Filled from exploration; file paths are upstream `src/openrct2/…` unless stated.)

### Scripting, UI, build, test (verified)

- **Script engine is QuickJS-NG** (`src/thirdparty/quickjs-ng/`), ES2023, no JIT, no modules. `kPluginApiVersion = 124`
  in `scripting/ScriptEngine.h:46` (CI greps that exact line). 18 hooks in `scripting/HookEngine.h:26-46`;
  `interval.tick` runs inside `gameStateUpdateLogic` (`GameState.cpp:363`). Custom actions:
  `actions/general/CustomAction.*` + `ScriptEngine::QueryOrExecuteCustomGameAction`. Bindings are `Sc*` classes
  deriving `ScBase`, registered in `ScriptEngine::RegisterClasses` (`ScriptEngine.cpp:557-616`); UI-side classes
  via `RegisterExtension` (`src/openrct2-ui/scripting/UiExtensions.cpp`). TS defs: `distribution/scripting/openrct2.d.ts`
  (hand-maintained, 6k lines). Plugins cannot define object types, tile element types, or network-synced sprites.
- **UI**: engine lib `libopenrct2` vs GUI exe in `src/openrct2-ui/`; `openrct2-cli` is headless. Windows derive
  `WindowBase` (`interface/WindowBase.h`), widgets via `makeWidgets(...)`, tools via `ToolSet/ToolCancel`
  (`interface/Window.h:83`), ghosts via `CommandFlag::ghost` on actions. Template: `src/openrct2-ui/windows/Footpath.cpp`.
  `WindowClass` enum (`interface/WindowClasses.h`) has **free IDs 142–219** — reserve for fork windows. New class
  needs: enum value, theme row in `src/openrct2-ui/interface/Theme.cpp:156`, factory in `windows/Windows.h`.
  Toolbar: `src/openrct2-ui/windows/TopToolbar.cpp`.
- **Strings**: `data/language/en-GB.txt`, ids hand-numbered in `localisation/StringIds.h` / `UiStringIds.h`;
  highest `STR_7081`; object strings start at 0x2000; parser reads `STR_%4d` (`LanguagePack.cpp:257`) → **only
  ~1,100 free ids**. Fork must patch the parser to 5 digits and claim block 0x5000–0x9FFF, and put all content text
  in object JSON `"strings"` (runtime-allocated, collision-free).
- **Build**: CMake ≥ 3.24, GCC ≥ 12, C++20, `-Werror`. Deps: OpenSSL, curl, fontconfig, freetype, libzip, zlib, zstd,
  libpng, ICU, nlohmann-json, SDL2, FLAC, vorbis, OpenGL, GTest. Assets (objects, OpenMusic, OpenSFX,
  title-sequences, replays) downloaded per `assets.json`. Sprite packs built from `resources/<pack>/sprites.json` by
  `openrct2-cli sprite build` (`CMakeLists.txt:400-421`). `SpriteIds.h` is one contiguous enum: **inserting into G2
  shifts all later ranges** → fork ships its own `g3.dat` appended after upstream ranges. **Windows uses MSBuild
  `.vcxproj` explicit source lists** (`libopenrct2.vcxproj`, 564 entries) → every new file must be added there too.
- **Tests**: gtest binary from explicit list in `test/tests/CMakeLists.txt`; `ReplayTests.cpp` runs every
  `replays/*.parkrep` → **any sim change breaks upstream replays**; fork needs its own replay pack. `scripts/run-tests`.
  CI: `.github/workflows/ci.yml` (clang-format check, changelog check, g2dat, Windows MSBuild, macOS, Linux
  portable/AppImage/Docker, Android, Emscripten). Style: `.clang-format` Allman/128 cols, `.clang-tidy` (`k`-prefixed
  constexpr, camelBack scoped enums, `validate_global_widx|NETWORK_STREAM_VERSION|SFL_*` are the only allowed macros).
- **Version constants**: `kStreamVersion` (`network/NetworkBase.cpp:50`), `kParkFileCurrentVersion = 63`
  (`park/ParkFile.h`; the fork leaves this alone and versions its own chunks), `kReplayVersion = 11`
  (`ReplayManager.cpp:109`), `kPluginApiVersion`. Park chunk ids used:
  0x01–0x09, 0x20, 0x30–0x39, 0x80 → factory chunks take **0x40–0x4F**. `GameCommand` enum (`actions/GameCommand.h`)
  is a serialised int32 with `custom` near the end → fork actions start at a **high reserved base (e.g. 10000)**.
- **Headless**: `openrct2-cli … host park --headless`; UI-side scripting absent there. Emscripten build exists.

### Simulation core (world, entities, tick, actions, save, network) — verified

- **Tile elements**: flat `std::vector<TileElement>` in `GameState_t::tileElements`, exactly 16 bytes each
  (`world/tile_element/TileElementBase.h:52-124`): type/flags/baseHeight/clearanceHeight/owner + **11 payload bytes**.
  Type is bits 2–5 of byte 0 (16 values); used 0–7 (`TileElementType.h:16-26`); RCT1/2 importers use 8, 14, 15 as
  corruption markers → **fork uses 9–13** (plan: `9 = factory`, with a subtype byte in payload). Per-tile elements are
  contiguous, height-sorted; `TileElementInsert<T>` / `TileElementRemove` / `MapGetFirstElementAt` /
  `TileElementsView<T>` (`world/Map.h`, `world/TileElementsView.h`). Map ≤ 1001² tiles, ≤ 16.7M elements.
  Switches that must learn a new type: `paint/tile_element/Paint.TileElement.cpp:262`, `world/ConstructionClearance.cpp:340`,
  `world/Map.cpp:1383 ClearElementAt`, `actions/terraform/ClearAction.cpp`, `world/MapAnimation.cpp`,
  `park/ParkFile.cpp:1293` (version fix-ups), `scripting/bindings/world/ScTileElement.cpp` (~12 switches),
  `world/TileInspector.cpp` + UI `TileInspector.cpp`, `scenes/editor/EditorController.cpp:428`, plus if-chains in
  Viewport/VirtualFloor/GuestPathfinding/Footpath/Scenery/Track/TrackDesignSave/RideRatings/Guest.
- **Entities**: `EntityType` enum (`entity/EntityBase.h:11-28`), 65 535 fixed 512-byte slots (`EntityRegistry.h`),
  misc cap 6 400, `EntityId` is uint16. Adding a type touches the enum (append), `is<T>`, `ParkFile.cpp:2831` hard-coded
  type lists + `ReadWriteEntity`, `getAllEntitiesChecksum` (`EntityRegistry.cpp:204`), `GameStateSnapshots.cpp`,
  `paint/Paint.Entity.cpp:168`, `ScEntity.cpp`. **Conclusion: factory items/arms/fluids are not entities.**
- **Tick**: 40 Hz (`Game.h:21`). Order in `gameStateUpdateLogic` (`GameState.cpp:251-373`): replay → network tick →
  date → scenario → weather → `MapUpdateTiles` → peeps → vehicles → misc entities → `Ride::updateAll` → `Park::Update`
  → research (every 32 ticks) → ride ratings → measurements → news → animations/sound → spatial index →
  `GameActions::ProcessQueue` → network post-tick → `currentTicks++` → script hooks. **Factory update slots in after
  `Ride::updateAll()` and before `Park::Update`** (so park rating/finance see this tick's production).
- **Determinism**: integer/fixed-point only (`core/FixedPoint.hpp`, `core/Money.hpp` money64 = 1dp fixed,
  `math/Trigonometry.hpp` 256-scaled tables). RNG `GameState_t::scenarioRand` via `ScenarioRand()/ScenarioRandMax()`.
  Snapshots (`GameStateSnapshots.cpp`) and replays (`ReplayManager.cpp`, `kReplayVersion = 11`) serialise entities only.
- **GameActions**: `GameActionBase<GameCommand::X>` with `Serialise`, `Query`, `Execute` (`actions/GameAction.hpp`);
  template `actions/scenery/BannerPlaceAction.*`. Registration: `actions/GameCommand.h` (int32 enum, 87 values, sent over
  wire), `actions/GameActionRegistry.cpp:100-130` (`REGISTER_ACTION`), `scripting/ScriptEngine.cpp:1671 ActionNameToType`,
  `network/NetworkAction.cpp:55` permission groups (64-bit bitset, 24 used). Clients send to server; server queues per
  tick and relays (`actions/GameActionRunner.cpp`). Ghosts via `CommandFlag::ghost`.
- **Save**: OrcaStream chunks (`core/OrcaStream.hpp`); unknown chunks are skipped by older builds; `readWriteChunk`
  returns false when missing. Chunk ids in `park/ParkFile.cpp:79-104`; tiles chunk is a raw memcpy of 16-byte elements
  (`ReadWriteTilesChunk` :1260) so a new element type saves for free; per-tile state beyond 11 bytes needs a side
  table in its own chunk. Network map transfer reuses the exporter (`NetworkBase.cpp:1509`) so new chunks reach joiners.
- **Network desync check** (`NetworkBase.cpp:1588-1615`, :844-900): compares `scenarioRand.s0` every tick and
  `getAllEntitiesChecksum()` (Guest/Staff/Vehicle/Litter only) every 100 ticks. **Factory state is invisible to it**;
  the fork must fold a factory checksum into the tick checksum. `kStreamVersion = 2`.
- **Date**: 8 months × 16 384 ticks; year ≈ 55 min at 1×. Periodic hooks in `scenario/Scenario.cpp:295-355`
  (day/week/fortnight/month) — fortnight is where ride upkeep runs; month shifts the expenditure table.
- **Management hooks**: `ExpenditureType` (`management/Finance.h:16-33`, 14 types; table saved with explicit counts so
  adding types is save-safe). Research: `Research::EntryType {scenery, ride}`, `ResearchItem` union, lists in
  `GameState_t`, `ResearchUpdate` every 32 ticks (`management/Research.cpp:309`). News `ItemType`
  (`management/NewsItem.h:33`). Awards ≤ 32 (bitset). Objectives: `scenario/ScenarioObjective.h:28-43` (12 types).

### Objects, paint, rides, guests — verified

- **Objects**: `ObjectType` (`object/ObjectTypes.h:23-49`, 21 types; limits in `ObjectLimits.h`, 2047 for ride/scenery).
  JSON loaded by `ObjectFactory.cpp:529 CreateObjectFromJson` (`"objectType"`, `"properties"`, `"strings"`, `"images"`).
  Images: PNG paths with `x`/`y` anchors (`object/ImageTable.cpp`, `drawing/ImageImporter.cpp:443`), allocated from a
  **1,000,000-slot dynamic pool** → no `SpriteIds.h` churn if all factory art ships as object images. Templates:
  `SmallSceneryObject` (rotatable, animated, 4 spare element bytes) and `LargeSceneryObject` (multi-tile `"tiles"`,
  **never animated**). Adding a new ObjectType touches ~11 files (ObjectTypes.h/.cpp, ObjectLimits.h, ObjectList.cpp,
  ObjectFactory.cpp, ObjectRepository.cpp `kVersion`, ParkFile, `ScInstalledObject.cpp`, `ScObjectManager.cpp`,
  `EditorObjectSelection.cpp`, EditorController, ParkInfoCommands, importers).
- **Paint**: `PaintSession` + `PaintAddImageAsParent/Child/…` (`paint/Paint.h:254-296`), tile dispatch switch at
  `paint/tile_element/Paint.TileElement.cpp:262-289`; small-scenery animation keyed to `currentTicks`
  (`Paint.SmallScenery.cpp:215-320`); repaint registration in `world/MapAnimation.cpp` (`IsElementAnimated`,
  `UpdateTile`); entities skipped at zoom > 2 (`Paint.Entity.cpp`) → **belt items drawn as child images of the belt
  element**. Ghosts are real elements with `isGhost()`; see-through via `interface/ViewportFlags.h`.
- **Rides**: `Ride` struct (`ride/Ride.h:290+`), `RideTypeDescriptor` (`ride/RideData.h:495-574`), table
  `kRideTypeDescriptors` (`RideData.cpp:256`) with **unused dummy slots** `RIDE_TYPE_1D, 1F, 22, 50, 52-55, 59` a new
  type can take; per-type headers `ride/rtd/{gentle,transport,…}/`. `RtdFlag` incl. `interestingToLookAt`,
  `isTransportRide`. Ratings state machine `ride/RideRatings.cpp:314-528`, scenery score `:1753-1806`, modifiers
  `RatingsModifierType` (`RideData.h:109-175`). **Template for Factory Tour: Car Ride** (`rtd/gentle/CarRide.h`,
  continuousCircuit, `bonusScenery/bonusProximity`); Miniature Railway for freight. Payment at `Guest.cpp:3814`.
- **Guests**: states `Peep.h:46-104`; stats `Guest.h:306-318`; thoughts `PeepThoughtType` (`Guest.h:40-182`).
  `GuestAssessSurroundings` (`Guest.cpp:2870-2985`) counts scenery in ±5 tiles every 18 updates (≥40 → "Great
  scenery!" +45 happiness); `GuestFindRideToLookAt` (`:6412`) watches `interestingToLookAt` rides / photogenic large
  scenery. **Guests only pick rides with track within 10 tiles** (`:1786-1830`) → the tour must be a real track ride.
  Park rating `world/Park.cpp:376-489`. Pathfinding only on footpath edges (`peep/GuestPathfinding.cpp`).
- **Placement**: `SmallSceneryElement` (`pad0B[4]` spare), `LargeSceneryElement` (`sequenceIndex` per tile, origin via
  `MapLargeSceneryGetOrigin`), `PathElement` edges/corners bitmasks — the model for 1-tile directional belts and 3×3
  machines with per-tile sequence index.
- **Audio**: `Play3D(SoundId, CoordsXYZ)` (`audio/Audio.h:237`); `SoundId` from audio objects (`openrct2.audio.additional`);
  ride `MusicObject` for looping ambience; crowd-ambience loop pattern (`entity/Peep.cpp:1205-1235`) for machine hum.

## Work breakdown (epics)

Each epic lists the new code (fork-owned directories) and the upstream touch points (kept minimal and marked
`// FACTORY-TOUR:` so upstream merges are mechanical). Sizes are rough engineering-effort buckets: S < 1 week,
M 1–3 weeks, L 1–2 months, XL 2+ months, for one experienced engineer plus agents.

### E0 — Bootstrap (S)

- Fork `OpenRCT2/OpenRCT2` to your GitHub account with `gh repo fork --clone=false`, then clone into
  `/home/user/Documents/Projects/factory-tour` with remotes `origin` (fork) and `upstream`. Work on branch
  `factory-tour/main` off `develop`; never commit to `develop` (it tracks upstream).
- Install build deps (apt): cmake ninja-build pkg-config libsdl2-dev libssl-dev libcurl4-openssl-dev
  libfreetype6-dev libfontconfig1-dev libzip-dev libzstd-dev libpng-dev libicu-dev nlohmann-json3-dev libflac-dev
  libvorbis-dev libgtest-dev clang-format clang-tidy. Build dir on D: `/media/user/D/factory-tour-build` → `build/`
  symlink. Configure `-DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_TESTS=on -G Ninja`, `ccache` on.
- Point `game_path` at an RCT2 install (buy on GOG if absent; `openrct2 set-rct2 <dir>`). Confirm vanilla runs and
  `ctest` passes before any fork change.
- Documentation skeleton in the fork (mirrors your other projects): `CONTEXT.md` (vocabulary: Factory Element,
  Prototype, Segment, Warehouse, Tour Ride, World, Portal…), `docs/adr/0001-…` for each decision below,
  `wiki/SCOPE.md`, `wiki/SPEC.md`, `wiki/ROADMAP.md` (this breakdown), `CLAUDE.md`/`AGENTS.md` with build/test
  commands, the touch-point marker convention, codex-monitor usage, and the "add every new file to `.vcxproj`" rule.
- CI: keep upstream `ci.yml` jobs that matter (clang-format, Linux CMake + tests, Windows MSBuild); drop Android,
  Emscripten, macOS universal initially. Fork's own replay pack and `assets.json` entry (upstream replays break).
- Upstream merge policy: monthly `git merge upstream/develop`; resolve conflicts only at marked touch points.

### E1 — Factory core simulation (XL)

New directory `src/openrct2/factory/`: `FactoryState.h/.cpp`, `FactoryPool.hpp`, `FactoryPrototype.h/.cpp`,
`FactoryUpdate.cpp`, `Belts.*`, `Inserters.*`, `Containers.*`, `Machines.*`, `Power.*`, `Fluids.*`, `Ore.*`,
`FactorySerialisation.*`, `SyncChecksum.*`, `actions/` (see E2). Design (agreed):

- **FactoryElement = `TileElementType::factory = 9`** (`world/tile_element/TileElementType.h`, new
  `world/tile_element/FactoryElement.h/.cpp` after `BannerElement.h`). Payload: `[5] subtype`, `[6..9] uint32 recordId`,
  `[10] footprintIndex`, `[11..12] ObjectEntryIndex entry`, `[13] shape/tier/connection cache`, `[14] flags`, `[15] pad`.
  Direction in the base type byte. Ghosts carry `recordId = 0xFFFFFFFF`.
- **`FactoryState` inside `GameState_t`** (`GameState.h`, after `cheats`), reset in `gameStateInitAll`
  (`GameState.cpp:57-83`), ticked between `Ride::updateAll()` and `Park::Update` (`GameState.cpp:335-341`).
  Pools (stable ids, lowest-free allocation, iterate ascending id): beltSegments, splitters, inserters, containers,
  machines (kind tag: drill/furnace/assembler/boiler/engine/pump/lab/turret), poles, powerNetworks, fluidNetworks;
  `topologyVersion`, `powerDirty`, `fluidDirty`.
- **Prototypes = new `ObjectType::factoryPrototype`** with JSON `properties.kind ∈ {item, recipe, belt, undergroundBelt,
  splitter, inserter, container, machine, pole, pipe, generator, ore, technology}`; cap 8192; runtime registry rebuilt
  on object-list change; `ensurePrototypesLoaded()` on new game/load. Content text lives in object `"strings"`,
  art in object `"images"` (1M-slot dynamic pool, no `SpriteIds.h` edits).
- **Belts**: positions in 1/256 tile, item spacing 64, speeds 12/24/36 units per tick (= Factorio's 15/30/45 items/s);
  transport-line **segments** (≤ 32 tiles, break at splitters/undergrounds/tier change) with two lanes of gap-encoded
  `BeltItem {ObjectEntryIndex proto; uint16 gap}` (4 bytes); O(1) amortised per lane per tick; sideload = link into a
  mid-lane position; splitters round-robin with filter/priority. Inserters: 48-byte records, swing ticks 24/10/10,
  pickup window on the tile ahead, drop to far lane; source/target re-resolved when `topologyVersion` changes.
- **Machines**: integer work units (`energySeconds*40*256`), `progress += speedQ8*satisfactionQ16 >> 16`; furnace
  auto-recipe from `smeltingByInput`; drill scans its area with a rotating cursor and decrements the ore layer.
- **Ore layer**: dense `OreCell {ore, richness, amount}` (8 B) per tile, resized with the map, RLE-saved in chunk 0x42,
  painted as a surface overlay.
- **Power**: poles auto-wire in id order; BFS rebuild on dirty (not incremental); per-tick 64-bit
  `satisfactionQ16 = supply/demand`; steam chain offshore pump → boiler → engine, accumulators, solar later.
  **Fluids**: one volume per connected component (no per-pipe flow), proportional sharing, pumps bridge networks.
- **Persistence**: chunks `0x40 factoryHeader`, `0x41 factoryPools`, `0x42 factoryOre` in `park/ParkFile.cpp:79-104`,
  read after the tiles chunk (`ReadWriteTilesChunk` calls `gameStateInitAll`); each chunk starts with its own
  `uint16 factoryVersion` so **`kParkFileCurrentVersion` is not bumped** (merge hygiene). Pools saved dense with
  alive bytes so ids match the raw tile payloads. `Factory::postLoad` validates/repairs element↔record links.
- **Sync**: new `computeSyncChecksum()` = entities serialisation + `Factory::serialiseForSync` into one
  `ChecksumStream`, replacing the six call sites (`network/NetworkBase.cpp:865,1610`, `ReplayManager.cpp:181,311,802`,
  `command_line/SimulateCommands.cpp:74`); factory blob added to `GameStateSnapshots.cpp` Capture/Compare.
- **Upstream switches to extend** (one `case TileElementType::factory` each): `paint/tile_element/Paint.TileElement.cpp:262`,
  `world/MapAnimation.cpp UpdateTile`, `world/Map.cpp:1405-1470` clearing (must free the record), `world/ConstructionClearance.cpp`,
  `world/TileInspector.cpp:268` + UI TileInspector, `scripting/bindings/world/ScTileElement.cpp`,
  `scenes/editor/EditorController.cpp:428`, `actions/terraform/ClearAction.cpp`, `park/ParkFile.cpp:1293`.
- **Budget**: ≤ 3 ms/tick typical at 50k belts / 10k inserters / 5k machines; hard CI threshold 8 ms on a benchmark
  park; phases wrapped in `PROFILED_FUNCTION()` (`profiling/ProfilingMacros.hpp`). Capacity caps, never adaptive
  throttling (determinism).

### E2 — Factory GameActions and multiplayer (M)

- `src/openrct2/factory/actions/`: `FactoryCommand.h` with `kFactoryCommandBase = 10000` and
  `enum class FactoryCommand : int32_t { place, remove, rotate, setRecipe, setFilter, placeBeltLine, clearArea,
  placeBlueprint, setWire, cheat, end }`; actions derive `GameActionBase<static_cast<GameCommand>(10000+n)>`.
  Own registry `FactoryActionRegistry.*`; patch `getFactory/GetName/IsValidId` in `actions/GameActionRegistry.cpp:228-253`
  to defer to it for ids ≥ 10000 (only contiguity assumption upstream). Permissions: `Permission::factory` in
  `network/NetworkAction.h/.cpp:55` + `"factory"` in `openrct2.d.ts`. Names in `scripting/ScriptEngine.cpp:1671`.
- `FactoryPlaceAction {CoordsXYZ loc; uint8 dir; ObjectEntryIndex entry; uint8 variant}` (multi-tile footprint via
  `MapCanConstructWithClearAt` per tile; ghost support), `FactoryRemoveAction`, `FactoryRotateAction`,
  `FactorySetRecipeAction`, `FactorySetFilterAction`, `FactoryPlaceBeltLineAction` (ExecuteNested per tile, pattern
  `FootpathLayoutPlaceAction`), `FactoryClearAreaAction`, `FactoryPlaceBlueprintAction` (vector of entries; client
  resolves identifiers → indices), `FactorySetWireAction`, `FactoryCheatAction`.
- Bump `kStreamVersion` (`network/NetworkBase.cpp:50`) and `kReplayVersion`; add new files to `libopenrct2.vcxproj`.

### E3 — Rendering and UI (L)

- `paint/tile_element/Paint.Factory.h/.cpp`: belts (`image + shape*64 + dir*16 + frame`), items as
  `PaintAddImageAsChild` from lane walks (skipped at zoom > 1), inserter frames from `progress`, multi-tile animated
  machines (large-scenery-style footprint, frame keyed to crafting progress), ghost palette. Wires v1 = `GfxDrawLine`
  overlay after `ViewportPaint`; v2 = quantised half-wire sprites. Smoke via existing `SteamParticle`.
  `ViewportInteractionItem::factory` (`interface/Viewport.h:93-108`) + cases in `src/openrct2-ui/interface/ViewportInteraction.cpp`.
- Windows (`src/openrct2-ui/windows/factory/`, `WindowClass` ids from the free block 142–219, theme rows in
  `interface/Theme.cpp:156`, factories in `windows/Windows.h`): FactoryBuild (prototype palette, ghost placement,
  rotate, drag belt line — template `windows/Footpath.cpp` + `windows/Scenery.cpp`), Machine (recipe picker,
  inventories, progress), Container, PowerOverview (graph), FluidInfo, Production statistics, Blueprint library,
  Alerts. Toolbar button in `windows/TopToolbar.cpp`. Ore overlay toggle in view menu.
- Sprite pack: all machine art as object images in `.parkobj`s under `data/factory/objects/` (zipped at build);
  icons that must be UI sprites go in a fork `resources/g3/sprites.json` → `g3.dat` appended after upstream ranges in
  `SpriteIds.h`. Art produced by your Blender/imagegen pipeline (4 rotations, N frames, anchors).
- Strings: patch `LanguagePack.cpp:257` to `STR_%5d`, claim ids 0x5000–0x9FFF in a fork `FactoryStringIds.h`; fork
  language file `data/language/factory/en-GB.txt` merged at load.

### E4 — Park intertwine (L)

The factory exposes a small read interface the park side consumes: `FactoryState::infoAt(FactoryElement)` →
`{kindClass, working, pollution, noise, photogenic, hazard, health}`, `pollutionAt(tile)`, `warehouse()`
(`canCover/consume/deposit(MaterialBill)`), `takeSciencePoints(tech)`, `stats()` (`producedTotal[item]`,
`uptimePercent`, `rocketsLaunched`, `avgPollutionNearPaths`).

- **Factory Tour ride type** (ship first; one header + one modifier + one scan helper): take dummy slot
  `RIDE_TYPE_1D` (`ride/Ride.h:611`) → `RIDE_TYPE_FACTORY_TOUR`; replace `kDummyRTD` at `ride/RideData.cpp:286` with
  `kFactoryTourRTD` in new `ride/rtd/gentle/FactoryTour.h` cloned from `CarRide.h:21-98` (reuse `TrackStyle::carRide`,
  swap `slightlyInterestingToLookAt` → `interestingToLookAt`, keep `allowMusic`, `.Name = "factory_tour"` so a JSON
  vehicle object `"type": ["factory_tour"]` resolves via `RideObject::ParseRideType`). New
  `RatingsModifierType::bonusFactoryProximity` (`ride/RideData.h:109-161`, before `requirementLength`), applied in the
  modifier switch `ride/RideRatings.cpp:907-1030`; the track-walking proximity loop (`:464-528`, `:699-720`) gains
  `ride_ratings_score_factory_proximity` scanning a radius-2 square for FactoryElements. Pure scoring function
  `FactoryProximityScore(stats, modifier)` in `ride/RideRatingsFactory.h` (density, variety = popcount of kinds,
  activity = working/machines, pollution → nausea and excitement penalty, hazards → intensity) — unit-testable. Extend
  `RideRating::UpdateState` (`RideRatings.h:55-67`) with the factory counters. Expose `factoryMachines/Working` in the
  `ride.ratings.calculate` hook object (`:1082-1086`).
- **Catwalk / exhibit paths** (second): `FOOTPATH_ENTRY_FLAG_IS_EXHIBIT = 1<<5` on `PathSurfaceDescriptor.flags`
  (`world/Footpath.h:54-60`, JSON `"isExhibit": true`); in `CalculateNextDestination` before the aimless pick
  (`peep/GuestPathfinding.cpp:2016-2020`) bias ~60% (via `ScenarioRand`) toward exhibit edges and don't cull exhibit
  dead-ends (`:1985-1991`). Mark guests `touredFactory` when on an exhibit tile with a machine within 1 tile.
- **Guest appreciation**: `GuestAssessSurroundings` (`entity/Guest.cpp:2870-2985`) counts machines/working/pollution/
  noise via `infoAt`; new thoughts appended at **ids 174–181** (`PeepThoughtType`, `Guest.h:40-182`; don't reuse the
  historical gaps): `factoryImpressive, factorySmell, factoryNoise, factoryWatching, factoryMadeHere, soldOut,
  factoryDanger`; grow `kPeepThoughtIds` (`peep/PeepThoughts.cpp:15-190`), `ThoughtTypeMap` in `ScGuest.cpp:26-152`,
  d.ts union. Negative thoughts subtract happiness (helper `IsNegativeSurroundingsThought`) instead of the +45 at
  `:907-913`; smell adds nausea. `GuestFindRideToLookAt` (`:6412-6527`) watches working photogenic machines (seat
  bit `0x04`), caller at `:5610-5618` inserts `factoryWatching` +20 happiness. Per-128-tick pollution penalty in
  `Guest::update` (`:895-915`). Machine noise reuses the "overbearing music" semantics.
- **Material economy**: `Scenario::Options` (`scenario/ScenarioOptions.h:20-35`) gains `constructionMode ∈ {money,
  hybrid, materials}` and `shopStockMode ∈ {infinite, warehouse}` (editor dropdowns in `EditorScenarioOptions.cpp`).
  `GameActions::Result` (`actions/GameActionResult.h:51-67`) gains `MaterialBill materials` (empty by default) and
  `Status::insufficientMaterials`. Hooks in `actions/GameActionRunner.cpp`: `QueryInternal` (`:186-196`, after
  `FinanceCheckAffordability`; gated by `Materials::Active(gameState)` so **money mode executes nothing new**;
  `FinanceCheckMoneyRequired` already excludes ghost/noSpend so previews compute but never fail), `ExecuteInternal`
  (`:358-363`, next to `FinancePayment`: consume, negative lines deposit). Bill sources: optional `"materials"` on any
  object JSON (parsed in base `Object::ReadJson`, resolved after load) set by `TrackPlaceAction.cpp:416`
  (scaled by `priceModifier`), `RideCreateAction.cpp:301`, scenery/footpath place actions; fallback
  `Materials::BillFromCost(cost, expenditure)` from a per-scenario tariff table. Refunds ride on existing negative-cost
  removal actions. Construction windows append "needs 12 Iron plate" to the cost text.
- **Shops stocked by the factory**: 8 free `ShopItem` ids (56–63; `FlagHolder<uint64_t>` in `ride/ShopItem.h:76-79`)
  for manufactured souvenirs with `ShopItemDescriptor` rows; `Ride::stockMode` (side table, see persistence rule) checked
  in `GuestDecideAndBuyItem` (`Guest.cpp:1438`, `soldOut` thought) and `:1646` (consume from warehouse instead of
  `FinancePayment`); item JSON `"shopItem"` maps vanilla items too. **Off-map market**: per-item base price with
  saturation decay recovering daily (`scenario/Scenario.cpp` day hook), `MarketSellAction` + an "export depot"
  machine kind. `ExpenditureType` appended **after `interest`** (`management/Finance.h:16-33`; `Guest.cpp:1650` relies
  on `stock-1 == sales`): `factoryConstruction, factoryRunningCosts, goodsSales, rawMaterialPurchase`; update
  `Finances.cpp:178-193` labels and d.ts.
- **Park rating & objectives**: `ParkFlag::factoryEnabled = 32, factoryAffectsRating = 33` (uint64 holder,
  `world/ParkData.h:37-61`); in `CalculateParkRating` (`world/Park.cpp:376-489`) after the litter block: pollution
  near paths up to −150, uptime ±25. `ObjectiveType` appended (`scenario/ScenarioObjective.h:28-43`):
  `produceItemsBy, launchRocket, guestsTouredFactory` with `ItemId`/`Quantity` in the `Objective` unions and checks
  in `ScenarioObjective.cpp:222-253`; editor widgets in `EditorObjectiveOptions.cpp`.
- **Persistence rule for park-side fork fields** (guest `factoryFlags`, ride `health`/`stockMode`, scenario option
  extras, objective extras): store them as **side tables keyed by `EntityId`/`RideId` in fork chunk `0x45 parkExt`**,
  not as new fields in upstream chunks. Vanilla saves stay byte-identical and upstream merges don't touch versions.

### E5 — Research and progression (M)

- Technologies are `factoryPrototype` objects with `kind = technology` (`prerequisites`, `cost: {packs, time}`,
  `unlocks: {recipes, machines, rideEntries, sceneryGroups}`, `allowFunding`). Add `Research::EntryType::technology = 2`
  and `ResearchCategory::technology = 7` (`management/Research.h:24-45`; bit 7 of `researchUncompletedCategories` is
  free). `ResearchItem.entryIndex` already addresses objects → no union change. `ResearchItem::getName`
  (`Research.cpp:719-739`) + category strings; `ResearchFinishItem` (`:189`) calls `factory.unlockTechnology` which
  also drives `RideEntrySetInvented`/`SceneryGroupSetInvented` (one unified tree: science packs can unlock coasters).
  `ResearchUpdate` (`:309-367`, after `:335`): technology items progress by `takeSciencePoints()` from Lab machines
  instead of money unless `allowFunding`. Windows: `windows/Research.cpp:336` icon switch, `EditorInventionsList.cpp`.
  `ReadWriteResearchItem` needs no change. Editor object selection auto-inserts technologies into the uninvented list.
- Progression content: a vanilla-like chain (automation → logistics → electricity → steel → oil → modules → rocket)
  interleaved with park unlocks (ride categories, scenery groups, souvenir recipes, tour ride tiers).

### E6 — Modding surface and combat stub (M)

- **Data modding**: every item/recipe/machine/technology/ore is a `.parkobj` (`object.json` + PNGs) of
  `objectType: "factory_prototype"` — modders need no C++. Schemas: item `{stackSize, fuelValue, shopItem?,
  marketPrice, saturation, category}`; recipe `{ingredients, results, time, category}`; machine `{kind, size,
  clearance, power:{consumption, drain}, recipeCategories, speed, pollution, noise, photogenic, hazard, health,
  materials, turret:{range, damage, ammoItem}?}` with images grouped by rotation/frame; technology as above.
  Identifier strings resolve to indices after load (scenery-group `entries` pattern). Fork content under
  `data/factory/objects/**` (extra ObjectRepository root in `PlatformEnvironment`), zipped and fetched via
  `assets.json` at release. Object-selection editor: new page + sub-tabs by kind
  (`src/openrct2-ui/windows/EditorObjectSelection.cpp:144-191`); audit loops over `ObjectType::count`.
- **Script API**: new globals `factory` (`ScFactory`: `getMachine(x,y)`, `machines`, `warehouse`, `market`,
  `technologies`, `spawnThreat()`), `ScMachine` (`id, kind, status, health, recipe, inventory, setRecipe()`),
  `ScItemStack`, `ScThreat`; registered in `ScriptEngine::RegisterClasses`; setters guarded by `IsGameStateMutable()`
  and routed through fork GameActions. Hooks appended to `HookType` (`scripting/HookEngine.h:26-46`, names table
  `HookEngine.cpp:29`): `factory.tick`, `factory.damage`, `factory.threat.spawn/despawn`, `factory.turret.fire`,
  `factory.machine.status`, `factory.research.complete`; call sites use the `HasSubscriptions` guard. Update
  `openrct2.d.ts` (fix the two already-missing hook names while there) and bump `kPluginApiVersion`.
- **Combat stub (API-complete, content-minimal)**: `health/maxHealth` on machine records (0xFFFF = indestructible;
  ride health in the `parkExt` side table); `FactoryDamageAction {target variant<tile, RideId, EntityId>, amount,
  damageType, sourceId}` in the fork action range → destroyed machines stop and show a damaged frame, rides get
  `RideFlag::brokenDown` with a new `Breakdown::damage`; `ThreatEntity` as a new `EntityType::threat` (append before
  `count`; add to `ParkFile.cpp:2831` type lists, `getAllEntitiesChecksum`, snapshots, paint, `ScEntity`) with the
  only built-in behaviour "walk straight at target, hit when adjacent"; `ThreatSpawn/DespawnAction`; turret machine
  kind consumes `ammoItem`, nearest-threat linear scan, fires hook. **Explicitly not built**: AI, pathing, waves,
  evolution, guest reactions to threats, rubble/repair, vehicle damage — those are the modders' playground.

### E7 — Logistics at scale (L)

- Freight: cargo cars on existing track rides (Miniature Railway RTD clone `freightRailway` with a `CarEntry` that
  carries `InvSlot[]`; stations become `FactoryElement` loaders/unloaders adjacent to track; vehicle update hook in
  `ride/Vehicle.cpp` on station arrival). Later: logistic bots as misc-like factory records painted per tile group.
- Undergrounds/splitters/filters shipped in E1; this epic adds priorities, lane filters, blueprint rotation/flip,
  copy/paste tool, production graphs, alerts (no power, no ore, output full), map overlay for pollution.

### E8 — Multi-world: interplanetary and interdimensional touring (XL)

Feasible as **N `GameState_t` instances ticked in lockstep by a `WorldManager`** (approach A). Nearly all sim code
reaches state through `getGameState()`, `GetDate()`, `ScenarioRand()`, `FinancePayment`. `swapGameState`
(`GameState.cpp:49-52`) has **zero callers** today and fixes nothing up; `StashMap/UnstashMap` (`world/Map.cpp:89-105`)
is the only precedent. Upstream TODOs at `Context.cpp:786` and `NetworkBase.cpp:2885` point the same way.
Rejected: one partitioned giant map (shared date/weather/rating, 1001² cap, whole-map loops), separate processes.
Optional later knob: tick non-viewed worlds every Nth master tick.

- **Stage 0 — groundwork**: `WorldId` strong type in `Identifiers.h`; `src/openrct2/world/WorldManager.h/.cpp` owning
  `vector<unique_ptr<GameState_t>>`, `activeIndex`, `viewedIndex`, `withWorld(id, fn)`, `setViewedWorld(id)`;
  `swapGameState` becomes its private primitive. Rule: the viewed world is always active outside the tick loop
  (paint reads the active `_tileIndex`).
- **Stage 1 — single world, factor caches into `GameState_t`/`EntityRegistry`** (zero behaviour change, validated by
  replays): `_tileIndex` + `_tileElementsInUse` (`world/Map.cpp:82,85`, rebuilt by a 1001² scan so it must be a
  member, not rebuilt per swap); map-animation sets (`world/MapAnimation.cpp:68-72`); researched tables +
  `_seenRideType` (`management/Research.cpp:49-51,1022`); `_consolidatedPatrolArea` (`entity/PatrolArea.cpp:22`);
  land-rights counters (`Map.cpp:78-79`); **`RideUse::_history/_typeHistory` (`peep/RideUseSystem.cpp:14-15`) — the one
  hard correctness bug, keyed by `EntityId` which collides across worlds; move into `EntityRegistry`**. Keep accessor
  functions forwarding so call sites don't change. Split `gameStateUpdateLogic` into `tickWorld(GameState_t&)` + a
  master driver. Add `_worldId` to `GameAction` (`actions/GameAction.hpp:53-58`) and the action queue
  (`actions/GameActionRunner.cpp:40-58`). Note `gameStateInitAll` mixes explicit `gameState&` with implicit-global
  callees (`MapInit`, `FinanceInit`, `RideInitAll`, `Weather::reset`) → creating world N requires swapping it active first.
- **Stage 2 — two worlds, items only**: `CompanyState` (new struct beside `GameState_t`) holding finance
  (`ParkData::cash…expenditureTable`, `world/ParkData.h:79-115`), all research fields, scenario options/records,
  `nextGuestNumber`, `pluginStorage`, `cheats`, master `currentTicks`, **shared `date`** (periodic ledgers in
  `scenario/Scenario.cpp:404-425` must not double-apply), `TransferQueue {from, to, item, qty, arrivalTick}`,
  `PortalLink` table, world directory. Per-world: tiles, banners, entities, rides, weather, **own `scenarioRand`**,
  spawns, news, rating. Save: world 0 stays in the existing top-level chunks (a single-world save remains a valid
  vanilla `.park`); add `company = 0x43` and `worlds = 0x44` whose payload is N length-prefixed inner uncompressed
  OrcaStreams of the per-world chunks (`ParkFile::Import/Save` already take `GameState_t&`). Network: `SaveMap/LoadMap`
  (`NetworkBase.cpp:2874-2920`) call the multi-world save; tick sync sends a hash of all worlds' `s0`;
  `GameStateSnapshots::Capture` loops worlds. Lift `kMaxClimateObjects = 1` (per-world index at `world/Weather.cpp:111,233`,
  `Guest.cpp:1410`) and `kMaxWaterObjects`. **One superset object list per save** (`ObjectManager::LoadObjects` is
  delta-based and renumbers indices, so per-world lists are impossible). UI: world selector in `TopToolbar.cpp`;
  switch procedure mirrors `GameLoadInit` (`Game.cpp:336-390`): cancel tools, close construction + id-bearing windows,
  `Audio::StopAll()`, unfollow sprite, swap, `SetMainView` from the new world's saved view, `EntityTweener::reset`,
  `ResetAllSpriteQuadrantPlacements`, `LoadPalette`, `ScrollingText::invalidate`, `GfxInvalidateScreen`.
  Memory: ~80–100 MiB per world (32 MiB entity array + 24 MiB spatial index + rides).
- **Stage 3 — guests and portal rides**: a `portalTerminal` RTD (single-station ride per side) linked by
  `PortalLink{worldA, rideA, worldB, rideB}` via `PortalLinkAction`; **never span a ride across worlds**
  (`Vehicle` holds `RideId`, track location, train links). `GuestTransfer::capture/apply`: keep world-neutral stats
  (`peepId`, name, happiness/nausea/hunger/thirst/toilet/energy, cash and spend counters, item flags, flags,
  animation object index); reset world-local ids (`currentRide*`, `interactionRideIndex`, `pathfindGoal/History`,
  `guestNextInQueue`, `guestHeadingToRideId`, `photo*RideRef`, `previousRide`, ride-typed thoughts, `voucherRideId`,
  `favouriteRide`, queue timers, `parkEntryTime`, RideUse history). Respawn via `Guest::generate` at the paired exit.
  `RideDemolishAction` clears dangling links. Items move via the `TransferQueue` drained after all world ticks.
- **Stage 4 — planet/dimension rules as content**: per-world climate, water, terrain objects, `restrictedScenery`,
  allowed-ride mask (research is company-level), and a small `PlanetParams` (gravity scale read in
  `ride/Vehicle.TrackMotion.cpp`, guest energy drain scale, belt speed scale, ore table) stored in the per-world
  park chunk and editable in the scenario editor. Rocket silo = portal with item-only link and a launch animation;
  "interdimensional" = a world whose PlanetParams and content pack are deliberately broken-weird.
- **Scale**: Stage 1 ~25–35 files, medium risk; Stage 2 ~20 files + UI, medium-high (network); Stage 3 ~15 files;
  Stage 4 mostly content. Plugin API assumes one `map`/`park` — add `context.worlds` later.

### E9 — Theme, content, release (L)

- Theme bible (the "really weird" part) as content: planet/dimension packs = terrain surface/edge objects, ore
  prototypes, machine skins, music objects, scenario text. Engine stays theme-agnostic.
- Content pack `data/factory/` with a vanilla-equivalent chain (iron/copper/coal/stone → plates → gears/circuits →
  belts/inserters/machines → science packs → rocket parts) plus park goods (track segments, car bodies, souvenirs).
- Packaging: AppImage + Windows zip via trimmed CI; naming/trademark review (no "Factorio"/"RCT" in product name);
  GPLv3 for code, CC-BY-SA 4.0 for original art; changelog convention kept.

## Reserved id map (single source of truth, goes into `CONTEXT.md`)

| Space | Fork reservation |
|---|---|
| `TileElementType` | `factory = 9` (10–13 spare; never 8/14/15) |
| `ObjectType` | `factoryPrototype` (one type, `properties.kind`), cap 8192 |
| `GameCommand` | 10000+ via `FactoryCommand`, own registry |
| Park chunks | `0x40 factoryHeader`, `0x41 factoryPools`, `0x42 factoryOre`, `0x43 company`, `0x44 worlds`, `0x45 parkExt` |
| `WindowClass` | 142–219 |
| String ids | 0x5000–0x9FFF after the 5-digit parser patch |
| Sprites | `g3.dat` appended after upstream ranges; everything else as object images |
| `RIDE_TYPE` | `RIDE_TYPE_1D` → Factory Tour; `1F` → Freight Railway; `22` → Portal Terminal |
| `PeepThoughtType` | 174–181 (+182–197 for souvenir value thoughts) |
| `ShopItem` | 56–63 |
| `ExpenditureType` | appended after `interest` |
| `ParkFlag` | bits 32–33 |
| `EntityType` | `threat` appended before `count` |
| `HookType` | `factory.*` appended |
| `Permission` | `factory` appended |
| Version bumps on sim change | `kStreamVersion`, `kReplayVersion`, `kPluginApiVersion`; **not** `kParkFileCurrentVersion` |

## Milestones (vertical slices, in order)

Each milestone ends with: clang-format clean, `ctest` green, a fork replay recorded, a two-client desync soak,
a changelog line, and an ADR if a decision was made.

1. **M0 Bootstrap** — E0 complete; vanilla fork builds and runs on this machine; docs skeleton; CI trimmed.
2. **M1 Hello conveyor** — chest → inserter → belt → inserter → chest, placeable from a toolbar window with ghost
   preview, rotatable, saved/loaded, multiplayer-synced, with determinism + save/load + throughput gtests.
   (E1 steps 1–9 of the vertical slice: element type, object type with 4 JSON objects, state/pools, belts/inserters/
   containers, actions + registry, chunks 0x40/0x41 + composite checksum, paint, UI, `.vcxproj` entries.)
3. **M2 Production chain** — ore layer + overlay, mining drill, furnace, assembler with recipe window, power (poles,
   offshore pump, boiler, steam engine, power overview), pipes/fluid networks, undergrounds, splitters, filters,
   belt-line drag, benchmark CLI and the 8 ms CI gate.
4. **M3 Park intertwine I** — Factory Tour ride type + ratings modifier + tour vehicle object; guest thoughts and
   watching; pollution and noise effects; `parkExt` side-table chunk. First moment the two games touch.
5. **M4 Park intertwine II** — material economy (construction modes, bills, warehouse, refunds, cost text),
   warehouse-stocked shops + souvenir items, market, new expenditure rows, park-rating terms, objectives, scenario
   editor options, exhibit paths.
6. **M5 Progression & modding** — technology prototypes in the research system, labs, unified unlock tree, script
   bindings + hooks + d.ts, object-selection tabs, combat stub (health, damage, threat, turret), first content pack
   (vanilla-like chain + park goods). Modders can ship a `.parkobj` turret.
7. **M6 Logistics & polish** — freight railway with cargo cars and loader stations, blueprints copy/paste, production
   graphs, alerts, pollution overlay, machine audio (3D one-shots + hum loop), smoke.
8. **M7 Multi-world I** — E8 Stages 0–2: WorldManager, caches into GameState, CompanyState, two worlds with item
   transfer (rocket silo), world selector, nested save, network map/tick changes.
9. **M8 Multi-world II** — E8 Stages 3–4: portal terminal rides, guest transfer, planet params, per-world climate/
   water/terrain, first "weird" dimension content pack, `context.worlds` binding.
10. **M9 Theme & release** — theme bible, full content pass, trailer scenarios, packaging, licensing, naming.

## Verification

- **Build**: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_TESTS=on && ninja -C build`;
  `scripts/run-clang-format` (or `clang-format --dry-run -Werror`) on changed files; clang-tidy on fork files.
- **Unit/integration (gtest, `test/tests/`)**: `FactoryBeltTests` (15/30/45 items/s ±1% straight, corner,
  underground, sideload; splitter 50/50; spacing never < 64), `FactoryDeterminismTests` (two fresh runs of 4000 ticks
  compare checksums every 100 ticks; save at 2000 → load → continue equals uninterrupted run), `FactorySaveLoadTests`
  (one of every kind round-trips byte-identical through `serialiseForSync`), `FactoryActionTests` (clearance/ownership
  failures, ghosts leave no record, map-clear frees records), `FactoryProximityScore` and `BillFromCost` pure tests,
  `Warehouse` consume/deposit/overflow, market price formula, `CalculateParkRating` equality with flag off, research
  progression with mocked science points, scripting test invoking `FactoryDamageAction` and asserting the hook fires.
  Run with `ctest --test-dir build --output-on-failure`.
- **Vanilla equivalence guard**: upstream replay pack must still pass while `constructionMode = money` and no
  FactoryElements exist (byte-identical behaviour); fork replay pack added per milestone.
- **Performance**: `openrct2-cli factory-bench 4000` builds the 50k-belt layout procedurally and prints per-phase µs
  and checksum; CI asserts ≤ 8 ms/tick; profiler export via `PROFILED_FUNCTION`.
- **Multiplayer**: headless `openrct2-cli host park.park --headless` + GUI client, 10-minute soak building the slice
  concurrently, assert no desync; later the same with two worlds.
- **Manual in-game**: open the fork, place the slice, watch items move at all zooms and 4 rotations, save/load,
  open a Factory Tour ride next to machines and confirm excitement rises and guests queue; toggle construction mode
  and confirm a coaster piece reports "needs N Iron plate".

## First actions on approval (M0, in order)

1. `gh repo fork OpenRCT2/OpenRCT2 --clone=false` (creates `TannerOlason/OpenRCT2`), then clone into
   `/home/user/Documents/Projects/factory-tour`, add `upstream`, create `factory-tour/main`.
2. `sudo apt install` the dependency list in E0 plus `innoextract` (if sudo needs your password I'll hand you the
   command to run with `!`).
3. Extract the GOG installer: `innoextract -d /media/user/D/rct2-data ~/Downloads/setup_rollercoaster_tycoon2_2.0.0.6.exe`.
4. Build on D (`/media/user/D/factory-tour-build`), run `ctest`, `openrct2 set-rct2 /media/user/D/rct2-data/app`
   (or wherever `g1.dat` lands), launch once to confirm vanilla runs.
5. Write `CONTEXT.md`, `docs/adr/0001–0008` (mergeable fork; C++ core not plugin; non-entity items; element type 9 +
   pools; one prototype object type; fork action range; fork chunks with own versions; side-table persistence),
   `wiki/SCOPE.md`, `wiki/SPEC.md`, `wiki/ROADMAP.md` (this breakdown), `CLAUDE.md`, trimmed CI, fork `assets.json`.
6. Start M1 step 1 (FactoryElement type + stub paint case) as the first code commit.

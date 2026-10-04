# Specification

The engineering design behind [`SCOPE.md`](SCOPE.md). Vocabulary is in [`CONTEXT.md`](../CONTEXT.md);
decisions are in `docs/adr/`. Paths are upstream `src/openrct2/…` unless stated. Line references were taken
from upstream commit `5d86c6b` (v0.5.5 / 0.5.6 in development) and drift as upstream moves.

## Upstream facts that shape the design

- Tile elements are 16 bytes with 11 payload bytes; the type field has 16 values and upstream uses 0–7
  (`world/tile_element/TileElementBase.h`, `TileElementType.h`). Importers use 8, 14 and 15 as corruption
  markers.
- Entities are 65 535 fixed 512-byte slots shared with guests (`entity/EntityRegistry.h`); the desync check
  hashes only Guest, Staff, Vehicle and Litter every 100 ticks (`network/NetworkBase.cpp`).
- The tick is 40 Hz; `gameStateUpdateLogic` (`GameState.cpp`) runs rides, then `Park::Update`, then
  research, ratings, news, actions and script hooks.
- Object images come from a one-million-slot dynamic pool; `SpriteIds.h` is one contiguous enum.
- The language parser reads `STR_%4d`, leaving about 1 100 free ids; object strings start at 0x2000.
- `GameCommand` is a serialised `int32` with `custom` near the end; `kStreamVersion`, `kReplayVersion`
  and `kPluginApiVersion` are the version constants a sim change must bump.
- Park files are OrcaStream chunks; unknown chunks are skipped. Used ids: 0x01–0x09, 0x20, 0x30–0x39, 0x80.
- Windows CI builds with MSBuild from explicit `.vcxproj` source lists.
- Guests only consider rides with track within 10 tiles (`entity/Guest.cpp`); large scenery never animates.
- `GameState_t` is a plain struct with `getGameState()` / `swapGameState()`; `swapGameState` has no callers.

## E1 Factory core

**Factory Element.** `TileElementType::factory = 9` in `world/tile_element/TileElementType.h`, class in new
`world/tile_element/FactoryElement.h/.cpp`. Payload: `[5] subtype`, `[6..9] uint32 recordId`,
`[10] footprintIndex`, `[11..12] ObjectEntryIndex entry`, `[13] shape/tier/connection cache`, `[14] flags`,
`[15] pad`. Direction in the base type byte. Ghosts carry `recordId = 0xFFFFFFFF`.

**Records and visitors.** Every record type in `factory/FactoryRecords.h` exposes `template<typename V> void
visit(V& v)` enumerating its fields in a fixed order. `ChunkVisitor` (park chunks), `SerialiserVisitor` (sync
checksum, snapshots, tests) and `Pool<T>::visit` (dense layout with alive flags) all drive the same function, so a
field can never be saved but not hashed. Records hold only integers, `std::vector`s and `std::array`s.

**FactoryState** lives in `GameState_t` after `cheats`, reset in `gameStateInitAll`, ticked between
`Ride::updateAll()` and `Park::Update`. Pools (stable ids, lowest-free allocation, ascending iteration):
`beltSegments, splitters, inserters, containers, machines, poles, powerNetworks, pipes, fluidNetworks`; plus
`topologyVersion, powerDirty, fluidDirty`, the Ore Layer and the Warehouse.

**Prototypes.** `ObjectType::factoryPrototype`, cap 8192, JSON `properties.kind`. A runtime registry
(`PrototypeRegistry`) is rebuilt on object-list change and `ensurePrototypesLoaded()` runs on new game and
load. Identifier strings in JSON resolve to entry indices after load (scenery-group `entries` pattern).

**Belts.** Positions in 1/256 tile, item spacing 64, speeds 12/24/36 units per tick (15/30/45 items/s).
Segments of ≤ 32 tiles break at splitters, undergrounds and tier changes. Two Lanes of gap-encoded
`BeltItem {ObjectEntryIndex proto; uint16 gap}`; O(1) amortised per lane per tick. Sideload links into a
mid-lane position. Splitters alternate outputs; an output priority tries one side first, a filter sends the
filter item only to the priority side (left by default) and everything else only to the other, and an input
priority serves that input first each tick (`FactorySetFilterAction`).

**Inserters.** 48-byte records; swing ticks 24/10/10; pickup window on the tile behind, drop to the far lane
of the tile ahead; source and target re-resolved when `topologyVersion` changes.

**Machines.** Integer work units (`energySeconds * 40 * 256`); `progress += speedQ8 * satisfactionQ16 >> 16`.
Furnaces auto-select a recipe from `smeltingByInput`. Drills scan their area with a rotating cursor and
decrement the Ore Layer. Kinds: `drill, furnace, assembler, boiler, engine, pump, lab, turret, exportDepot`.

**Footprints.** A machine prototype's `size` makes it a size x size square whose origin (the record location) is
the minimum corner. Each tile has its own element with the record id and `footprintIndex = dy * size + dx`; only
the origin carries `FACTORY_ELEMENT_FLAG_ORIGIN`. Placement validates every tile; removing any tile removes all.
Drills mine a square of `miningRadius` around the centre and drop onto the tile beyond the front edge's centre;
power reaches a machine when a pole's supply radius touches any footprint tile; fluid boxes connect through the
tile beyond each edge's centre, and the other node must face back from exactly that tile. Images hold one slice per
tile per direction and frame, indexed by the tile's row-major place in the view-rotated square, so lids join and
only outer edges get walls; an optional last image is the palette preview. Splitters keep their own two-tile rule.

**Ore Layer.** Dense `OreCell {ore, richness, amount}` (8 bytes) per tile, resized with the map, RLE-saved
in chunk 0x42, painted as a surface overlay.

**Power.** Poles auto-wire in id order; BFS rebuild when dirty; per-tick 64-bit
`satisfactionQ16 = supply / demand`; generators react to last tick's load (`lastDemand / lastSupply`);
accumulators and solar later.

**Fluids** (ADR 0009). A fluid is an item prototype with `"fluid": true`; it never travels on belts or sits in
chests. Machines declare `fluidBoxes` (`role` input or output, `sides` relative to their facing: front, right,
back, left, and a `capacity`). A Fluid Network is a connected component of nodes, where a node is a pipe (facing
all four sides) or one machine box (facing its sides); neighbouring nodes connect when each faces the other, so
a box with two sides is a pass-through and two machines can share a network without a pipe. The component holds
one fluid as one volume (`amount` of `capacity`, the sum of its nodes). Networks rebuild when `fluidDirty`:
the old volumes are first shared over their surviving nodes by capacity, then summed per new component, so
splitting and joining conserve fluid; a component that ends up with two fluids keeps the larger. Consumers
register requests in `demand` and draw `request * satisfactionQ16`, where satisfaction comes from last tick's
demand, so scarce fluid is shared in proportion rather than by id order. The steam chain: the offshore pump
(`kind pump`, needs water on the tile behind it) fills its output network at `fluidRate`; the boiler (burner)
converts up to `fluidRate` water from its input box (left and right) into steam in its output box (front); the
steam engine (`kind engine`, `energy fluid`) offers `powerOutput` scaled by the steam on hand and burns steam in
proportion to its power network's load. Pipe connection masks are cached in the element's connection byte
(map directions) and rotated into the view by the painter (16 images).

**Performance.** `openrct2-cli factory-bench` measures the factory update alone on a generated park; CI fails a
build whose average tick exceeds 8 ms on 5000 cells. Network rebuilds that compare records pairwise use a spatial
grid of 16x16-tile buckets filled in ascending id order, so results match the brute-force order exactly.

**Persistence.** Chunks `0x40 factoryHeader`, `0x41 factoryPools`, `0x42 factoryOre` registered in
`park/ParkFile.cpp`, read after the tiles chunk. Each starts with `uint16 factoryVersion`. Pools saved dense
with alive bytes. `Factory::postLoad` validates element↔record links.

**Sync.** `computeSyncChecksum()` = entity serialisation + `Factory::serialiseForSync` into one
`ChecksumStream`, replacing the entity-only call sites in `NetworkBase.cpp`, `ReplayManager.cpp` and
`command_line/SimulateCommands.cpp`; the factory blob is captured and compared in `GameStateSnapshots.cpp`.

**Touch Points.** One `case TileElementType::factory` in `paint/tile_element/Paint.TileElement.cpp`,
`world/MapAnimation.cpp`, `world/Map.cpp` (clearing frees the record), `world/ConstructionClearance.cpp`,
`world/TileInspector.cpp` and the UI inspector, `scripting/bindings/world/ScTileElement.cpp`,
`scenes/editor/EditorController.cpp`, `actions/terraform/ClearAction.cpp`, `park/ParkFile.cpp`.

**Budget.** ≤ 3 ms per tick typical at 50k belts, 10k inserters and 5k machines; CI gate 8 ms on a
benchmark park; phases wrapped in `PROFILED_FUNCTION()`. Capacity caps, never adaptive throttling.

## E2 GameActions and multiplayer

`src/openrct2/factory/actions/FactoryCommand.h`: `kFactoryCommandBase = 10000`,
`enum class FactoryCommand : int32_t { place, remove, rotate, setRecipe, setFilter, placeBeltLine, clearArea,
placeBlueprint, setWire, cheat, end }`. Fork-owned `FactoryActionRegistry` that
`actions/GameActionRegistry.cpp` defers to for ids ≥ 10000. `Permission::factory` in `network/NetworkAction.*`
and `openrct2.d.ts`; names in `scripting/ScriptEngine.cpp`. Actions: `FactoryPlaceAction {CoordsXYZ loc;
uint8 dir; ObjectEntryIndex entry; uint8 variant}` (footprint via `MapCanConstructWithClearAt`, ghosts),
`FactoryRemoveAction`, `FactoryRotateAction`, `FactorySetRecipeAction`, `FactorySetFilterAction`,
`FactoryPlaceBeltLineAction` (ExecuteNested per tile), `FactoryClearAreaAction`, `FactoryPlaceBlueprintAction`,
`FactorySetWireAction`, `FactoryCheatAction`. Bump `kStreamVersion` and `kReplayVersion`; add files to
`libopenrct2.vcxproj`.

## E3 Rendering and UI

`paint/tile_element/Paint.Factory.h/.cpp`: belts (`image + shape*64 + dir*16 + frame`), items as
`PaintAddImageAsChild` from lane walks (skipped at zoom > 1), inserter frames from `progress`, multi-tile
animated machines with a large-scenery-style footprint and frames keyed to progress, ghost palette. Wires v1
as a `GfxDrawLine` overlay, v2 as quantised half-wire sprites. Smoke via `SteamParticle`.
`ViewportInteractionItem::factory` plus cases in the UI `ViewportInteraction.cpp`.

Windows in `src/openrct2-ui/windows/factory/` with `WindowClass` ids 142–219, theme rows in
`interface/Theme.cpp`, factories in `windows/Windows.h`: FactoryBuild (palette, ghost placement, rotate, drag
belt line), Machine, Container, PowerOverview, FluidInfo, Production, Blueprint library, Alerts. Toolbar
button in `windows/TopToolbar.cpp`; ore overlay toggle in the view menu.

Art ships as object images in `.parkobj`s under `data/factory/objects/`; UI-only icons go in
`resources/g3/sprites.json` → `g3.dat` appended after upstream ranges. Strings: patch `LanguagePack.cpp` to
`STR_%5d`, claim 0x5000–0x9FFF in `FactoryStringIds.h`, fork language file `data/language/factory/en-GB.txt`.

## E4 Park intertwine

Read interface: `FactoryState::infoAt(FactoryElement)` → `{kindClass, working, pollution, noise, photogenic,
hazard, health}`, `pollutionAt(tile)`, `warehouse()` (`canCover / consume / deposit(MaterialBill)`),
`takeSciencePoints(tech)`, `stats()`.

- **Factory Tour ride** (ADR 0010): `RIDE_TYPE_1D` → `RIDE_TYPE_FACTORY_TOUR`, RTD in `factory/FactoryTourRTD.h`
  cloned from the car ride: flat track only (straight, station, small and very small curves), `interestingToLookAt`,
  `.Name = "factory_tour"`, base ratings 1.50 / 0.40 / 0.10 and `RatingsModifierType::bonusFactoryProximity`
  (appended to the enum) worth up to 3.00 excitement, 0.60 intensity and 0.30 nausea. While the rating state machine
  walks the track, each piece counts factory elements in the 5x5 tiles around it (within 24 height steps): elements,
  machines, working machines and a kinds bitmask. The totals are game state (`FactoryState::rideProximity`, sorted by
  ride id, pools chunk version 5) because rating spans ticks, saves and joins; only ride types with the modifier are
  tracked. At calculation `factoryProximityScore` (pure, `factory/RideRatingsFactory.h`) turns them into the bonus:
  density saturating at six elements per piece, plus variety (popcount of kinds, up to eight) and activity
  (working / machines). Vehicle: `factory-tour.ride.tour_tram`, generated art with 32 flat rotations, body and
  riders in the primary remap ramp (palette 245-254). The `ride.ratings.calculate` hook exposure is still to do.
- **Pollution and noise**: machine prototypes take `pollution` (added to `PollutionLayer` every working tick at the
  footprint centre) and `noise`. The layer is a grid of 8x8-tile cells; every 64 ticks each cell gives a sixteenth to
  each neighbour (lost off the map) and loses a thirty-second, at least one unit, so it always drains. It is saved and
  hashed sparsely (non-zero cells) inside the pools chunk.
- **Guest appreciation (first part)**: `Factory::assessGuestSurroundings` runs where a walking guest assesses their
  surroundings, before upstream's checks. Pollution in the guest's cell >= 4000 gives `factorySmell` (-20 happiness,
  +12 nausea); summed `noise / (1 + distance)` of working machines within three tiles >= 40 gives `factoryNoise` (-10);
  at least four machines within four tiles, half working, of two kinds gives `factoryImpressive` (+30). No factory
  thought (and always in parks without a factory) falls through to upstream's scenery, fountain and music thoughts.
- **Watching and touring**: `GuestFindRideToLookAt` treats a working machine whose prototype is `photogenic` (default)
  like photogenic scenery, with seat bit 0x04; the watching guest thinks `factoryWatching` (+20 happiness) and is
  marked `touredFactory`, as is any guest leaving a Factory Tour ride.
- **parkExt** (ADR 0008): `Factory::ParkExt` holds side tables keyed by upstream ids, saved in fork chunk 0x45 with
  its own version and included in the sync checksum. Now: guest flags (`kGuestTouredFactory`), sorted by entity id,
  pruned of ids that are no longer guests every 256 ticks.
- **Exhibit Paths**: `FOOTPATH_ENTRY_FLAG_IS_EXHIBIT = 1 << 5`, JSON `"isExhibit": true`. In
  `CalculateNextDestination`: `Factory::onGuestPathStep` marks a guest `touredFactory` on an exhibit tile within one
  tile of a machine; the dead-end cull skips edges onto exhibit paths; aimless guests' edges go through
  `biasTowardsExhibits`, which keeps only exhibit edges when `ScenarioRand() & 0xFFFF < 39322` and draws the random
  number only when some exist (vanilla parks keep their sequence).
- **Guest appreciation**: `GuestAssessSurroundings` counts machines, working, pollution and noise via
  `infoAt`; thoughts 174–181 (`factoryImpressive, factorySmell, factoryNoise, factoryWatching,
  factoryMadeHere, soldOut, factoryDanger`) in `PeepThoughtType`, `kPeepThoughtIds`, `ThoughtTypeMap`, d.ts.
  Negative thoughts subtract happiness; smell adds nausea. `GuestFindRideToLookAt` watches working photogenic
  machines. Per-128-tick pollution penalty in `Guest::update`.
- **Material economy**: `constructionMode ∈ {money, hybrid, materials}` and `shopStockMode ∈ {infinite,
  warehouse}` live in `parkExt` (chunk version 2), set by `FactorySetParkOptionAction` (command `setParkOption`).
  The Warehouse (`Factory::Warehouse`, pools chunk version 7) is a sorted item → count store; containers whose
  prototype has `"warehouse": true` (the warehouse depot) deposit into it. `Status::insufficientMaterials` is
  appended to `GameActions::Status`; `Result` is unchanged. Two runner hooks, top-level only:
  `Factory::onQuery` runs before the affordability check (so materials mode can zero the money cost first) and
  fails when the Warehouse cannot cover the bill; `Factory::onExecute` runs before `FinancePayment`, taking the bill
  or, for a negative cost, depositing the equivalent refund. Bills come from `billFromCost`: one
  `factory-tour.factory_prototype.iron_plate` per 5.00, for `rideConstruction` and `landscaping` only, never for
  fork actions, ghosts, noSpend, the editor or no-money parks. Object JSON `"materials"` bills come later.
- **Market** (ADR 0011): item prototypes take `marketPrice` and `marketSaturation`. `Factory::Market` (pools chunk
  version 8) keeps a saturation per item: each unit sold raises it, the price is `marketPrice * (1024 -
  min(saturation, 896)) / 1024`, and once a day every entry drops an eighth (at least one point). Export depots
  (`"exportDepot": true` containers) sell whatever is put in and refuse unsellable items; `FactoryMarketSellAction`
  (command `marketSell`) sells Warehouse stock, returning the income as a negative cost. Income is booked as Shop
  sales, not as new expenditure rows, because the expenditure table's save layout depends on `ExpenditureType::count`;
  `Market::goodsSold` keeps the fork-side total.
- **Shops**: `ShopItem::factoryModel = 56` and `gearKeyring = 57` (54-55 stay unused; 56-63 are reserved for
  manufactured souvenirs), with descriptor rows, strings, ride-object names (`factory_model`, `gear_keyring`) and
  plugin names; their icons borrow upstream sprites until the fork's sprite pack exists. Item prototypes take
  `"shopItem"` (parsed by `RideObject::ParseShopItem`, made public). In warehouse stock mode (`parkExt.shopStockMode
  = 1`), `GuestDecideAndBuyItem` refuses a mapped item the Warehouse lacks (thought `soldOut`) and a sale takes one
  from the Warehouse instead of paying the stock cost; unmapped items behave as upstream. A per-ride stock mode can
  come later as a parkExt ride table.
- **Rating**: `ParkFlag::factoryAffectsRating = 33` (32 stays reserved for `factoryEnabled`). With it set,
  `CalculateParkRating` adds `Factory::parkRatingAdjustment` after the litter term: minus up to 150 for the average
  pollution (capped at 8000) in the cells of guests inside the park, and `working * 50 / machines - 25` for uptime.
- **Objectives**: `ObjectiveType::produceItemsBy` and `guestsTouredFactory` are appended. The produce objective
  keeps its item entry in `NumGuests` and its quantity in `Currency`, so upstream's objective save layout and editor
  plumbing are reused; the toured objective uses `NumGuests`. Both succeed once reached and fail at the end of October
  of their year (`Factory::checkObjective`). `ProductionStats` (pools chunk version 9) counts crafted results and
  mined ore per item; `ParkExt::guestsToured` (version 3) counts guests ever marked toured. Rocket launches wait for
  the rocket silo (M7). Options: the Factory options window (WindowClass 145).
- **Persistence rule**: guest, ride, scenario and objective fork fields are Side Tables in chunk `0x45 parkExt`.

## E5 Research and progression

Technologies are prototypes of kind `technology` (`prerequisites`, `packs` per unit, `units`, `unitTicks`, `unlocks`
for factory prototypes, `rideEntries`, `sceneryGroups`). They form a fork tree beside upstream research (ADR 0012):
`Factory::ResearchState` (pools chunk version 10) holds researched technologies, units done and the current target.
Labs (`machineKind: "lab"`) accept only the current technology's packs (two units' worth), take a unit's packs when
it starts, work `unitTicks` at their speed and power satisfaction, and credit the unit to the technology it started
for. When the last unit lands, `completeTechnology` marks it researched, invents its rides and scenery groups and
announces it. A prototype is locked while some loaded technology unlocks it and none of those is researched:
placement and recipe actions refuse it ("Not yet researched"), the build palette and recipe dropdowns hide it and
furnaces skip locked recipes. `Research.cpp` hooks (`withholdGatedResearch`, `applyTechnologyUnlocks`) in
`ResearchResetCurrentItem` and `ResearchFix` keep upstream's research lists and invented tables consistent; both do
nothing without technologies. The "ignore research status" cheat unlocks everything. The target is set with
`FactorySetParkOptionAction` option `researchTarget` (an available technology, or 0xFFFF) from the Factory research
window (WindowClass 146). Content: research kits (gear + plate) and engineering kits (steel + 2 gears), the lab,
and the technologies Logistics, Electric mining, Steam power, Warehousing (after Logistics), Souvenir manufacturing
(unlocks the gift shop), Factory tours (unlocks the tour tram, after Logistics), Factory defence (turret and
magazines), Steel processing (steel smelting, engineering kits) and Fast inserters (both kits, after Steel and
Logistics). Longer chains (oil, modules, rocket) come with their content. Modding: `wiki/MODDING.md`.

## E6 Modding surface and combat stub

Every prototype is a `.parkobj` (`object.json` + PNGs) of `objectType: "factory_prototype"`. Schemas: item
`{stackSize, fuelValue, shopItem?, marketPrice, saturation, category}`; recipe `{ingredients, results, time,
category}`; machine `{kind, size, clearance, power: {consumption, drain}, recipeCategories, speed, pollution,
noise, photogenic, hazard, health, materials, turret: {range, damage, ammoItem}?}`; technology as above.
Fork content under `data/factory/objects/**` via an extra `ObjectRepository` root. The object-selection page has
sub-tabs by kind group (`Factory::isInSelectionGroup`), filtering on the kind stored in the object index
(`FactoryPrototypeInfo.Kind`, set by `FactoryPrototypeObject::SetRepositoryItem`; index version 33).

Script API (plugin API 134, `factory/scripting/ScFactory.cpp`, registered by three one-line touch points in
`ScriptEngine.cpp`): the `factory` global with `machines` and `getMachine(x, y)` (tile coordinates, any footprint
tile) returning plain snapshots (`id, object, kind, status, x, y, baseHeight, direction, recipe`, `progress` in
percent, `inputs, outputs, fuel, powered`), `setRecipe(x, y, recipe)`, `warehouse`, `production`, `marketIncome`,
`technologies` (`object, name, researched, available, units, unitsDone, prerequisites, unlocks`), `researchTarget`
(get/set) and `isUnlocked(object)`. Objects are named by identifier. Setters run the fork's game actions
(`factorysetrecipe`, `factorysetparkoption`), so they are queued for the next tick and work in multiplayer. Hooks
(non-mutable, only with subscribers): `factory.machine.status {x, y, object, status, previousStatus}` when a
machine's status changes and `factory.research.complete {technology}`; the combat stub adds `threats`,
`spawnThreat(object, x, y)`, `damage(target, id, amount, damageType)`, machine `health`/`maxHealth` and the hooks
`factory.damage`, `factory.threat.spawn/despawn` and `factory.turret.fire` (plugin API 135). No `factory.tick`:
`interval.tick` already covers it. `openrct2.d.ts` now also lists the upstream `map.resize` and `ride.breakdown`
hook names that were missing from `HookType`.

Combat stub (ADR 0013, `factory/Combat.cpp`, pools chunk version 11, parkExt version 4): machine prototypes
take `health` (0 = indestructible) and machine records carry `health`; at zero a machine becomes
`MachineStatus::destroyed`, stops, refuses items and is painted as a dark wreck until removed. Rides collect damage in
`ParkExt::rides`; reaching `kRideHealth` (1000) triggers a safety cut-out breakdown and clears it. Threats are
`ThreatRecord`s in `State::threats` (world-unit position, health, target, cooldown) of prototypes with
`kind: "threat"` (`health, speedQ8, damage, attackTicks, frames`); each tick a threat targets the nearest
destructible machine (lowest id on ties), walks straight at its footprint centre and hits it every `attackTicks` once
within 12 units of the footprint. Turret machines (`machineKind: "turret"`, `turretRange, turretDamage,
turretCooldownTicks, shotsPerAmmo, ammoItem`) load one ammo item into `shotsPerAmmo` rounds (kept in `fuelEnergy`),
pick the nearest threat in range and fire every cooldown. Actions: `FactoryDamageAction {target, id, amount,
damageType}` (13), `FactoryThreatSpawnAction {object, x, y}` (14), `FactoryThreatDespawnAction {id}` (15). Hooks:
`factory.damage`, `factory.threat.spawn`, `factory.threat.despawn`, `factory.turret.fire`. Content: the Scrap
crawler, the Bolt turret and Bolt magazines. Not built: AI, pathing, waves, evolution, guest reactions, rubble,
repair, vehicle damage.

## E7 Logistics at scale

Done so far (`factory/Alerts.cpp`, `ProductionStats` history, `Paint.Factory.cpp` overlay, `FactoryProduction.cpp`):
alerts as blank news items located at the machine (`AlertState` in the pools chunk, version 12), the pollution
overlay as a second mode (`Factory::gOverlay`) of `ViewportFlag::factoryOre`, and production/consumption totals plus
a ring of `kProductionSamples` samples per item (`advanceSample` every `kProductionSampleTicks`; idle items drop
out) shown by the Factory production window. Audio and smoke never touch state: `Factory::updateMachineSounds`
(`FactoryAudio.cpp`) plays `mechanicFix` at noisy working machines through `Audio::Play3D`, and `PaintFactory`
draws two staggered `SPR_STEAM_PARTICLE` puffs above working machines whose prototype has `smoke`.

Blueprints (`factory/Blueprint.cpp`): `captureBlueprint` copies placed pieces in a tile range (one entry per
piece: multi-tile machines at footprint 0, splitters at their record tile; underground exits last so they pair),
`serialiseBlueprint`/`parseBlueprint` use `FTBP1;w;h;n;ids...;dx,dy,dz,dir,object,recipe;...` (at most 1000 pieces,
256 x 256 tiles), and `rotateBlueprint` maps (x, y) to (y, width - size - x) with d -> d + 1.
`FactoryPlaceBlueprintAction {origin, rotation, blueprint}` (command 7) runs nested place and set-recipe actions.

Freight (ADR 0014, `factory/Freight.cpp`, `FreightRailwayRTD.h`, pools chunk version 13): `RIDE_TYPE_FREIGHT_RAILWAY`
takes the 1F slot; `RideCheckForEntranceExit` exempts it; `updateFreight` (each tick, after threats) finds trains
whose head car is stationary and, for each car on a station track element, trades with freight containers on the
two tiles beside it (`freightLoader` puts one item a tick in, `freightUnloader` takes one out; one item kind per car,
`kFreightCarCapacity` 200); `FreightState::cargo` is pruned every 256 ticks. Planned refinement: departure that
waits for loading to finish.

Original plan: `RIDE_TYPE_1F` → `freightRailway` cloned from Miniature Railway with a `CarEntry` carrying
`InvSlot[]`; loader and unloader Factory Elements adjacent to stations; vehicle hook on station arrival in
`ride/Vehicle.cpp`. Later: logistic bots as factory records painted per tile group. Also lane filters,
priorities, blueprint rotation and flip, copy/paste tool, production graphs, alerts (no power, no ore, output
full), pollution overlay.

## E8 Multi-world

N `GameState_t` instances ticked in lockstep by `WorldManager` (`src/openrct2/world/WorldManager.h/.cpp`).

- **Stage 0**: `WorldId` strong type; `WorldManager` owning `vector<unique_ptr<GameState_t>>`,
  `activeIndex`, `viewedIndex`, `withWorld(id, fn)`, `setViewedWorld(id)`; `swapGameState` becomes its
  private primitive. The viewed world is always active outside the tick loop.
- **Stage 1** (single world, zero behaviour change, validated by replays): move `_tileIndex`,
  `_tileElementsInUse`, map-animation sets, researched tables and `_seenRideType`, `_consolidatedPatrolArea`,
  land-rights counters and `RideUse::_history/_typeHistory` (keyed by `EntityId`, collides across worlds) into
  `GameState_t` or `EntityRegistry` with forwarding accessors. Split `gameStateUpdateLogic` into
  `tickWorld(GameState_t&)` plus a master driver. Add `_worldId` to `GameAction` and the action queue.
- **Stage 2** (two worlds, items only): `CompanyState` with finance, research, scenario options and records,
  `nextGuestNumber`, `pluginStorage`, `cheats`, master `currentTicks`, shared `date`, `TransferQueue`,
  `PortalLink` table and world directory. Per world: tiles, banners, entities, rides, weather, own
  `scenarioRand`, spawns, news, rating. Save: world 0 in the existing top-level chunks; `company = 0x43`,
  `worlds = 0x44` holding N length-prefixed inner OrcaStreams. Network: multi-world `SaveMap/LoadMap`, tick
  sync hashes all worlds' `s0`, snapshots loop worlds. Lift `kMaxClimateObjects` and `kMaxWaterObjects`. One
  superset object list per save. World selector in `TopToolbar.cpp` with a switch procedure mirroring
  `GameLoadInit`. About 80–100 MiB per world.
- **Stage 3**: `portalTerminal` RTD (`RIDE_TYPE_22`) linked by `PortalLink` via `PortalLinkAction`; rides
  never span worlds. `GuestTransfer::capture/apply` keeps world-neutral stats and resets world-local ids;
  respawn via `Guest::generate` at the paired exit. Items move via the Transfer Queue after all world ticks.
- **Stage 4**: Planet Params (gravity scale in `Vehicle.TrackMotion.cpp`, guest energy drain, belt speed,
  ore table), per-world climate, water and terrain objects, allowed-ride mask. Rocket silo = portal with an
  item-only link. Plugin API gains `context.worlds`.

## E8 as built (M7)

`Factory::Worlds` (ADR 0015): world 0 plus up to seven more `GameState_t`s, one active at a time. Activation swaps
the per-world caches (tile index, map animations, ride-use history, land-rights counters; patrol areas rebuilt) and
copies company state from the previously active world (money and finance history, loans, park flags, scenario
options and date, upstream and fork research, cheats, Market, production, factory options). `gameStateUpdateLogic`
ticks every world in ascending order when there is more than one; network, replay, date and scenario, research, the
action queue and script hooks run in world 0's pass, audio and provisional ghosts in the viewed world's. Actions
carry their world in `CommandFlags` bits 16-23 and run with it active. Saves: world 0 top-level, others nested in
chunk 0x44 (version 1). Item transfer (`factory/Transfers.cpp`, pools version 14): `ContainerRecord::targetWorld`
(0xFF = next world) on containers with `launchPad`; every `kLaunchTicks` (200) a launch pad empties into
`TransferState::queue` entries `{toWorld, item, count, arrivalTick = now + kTransitTicks (400)}`; each world, at the
end of its factory tick, lands due entries into its `landingPad` containers (ascending id, partial delivery waits).
The queue is company state. Portals (ADR 0016, `factory/Portals.cpp`, pools version 15): `Guest::onExitRide` calls
`onGuestExitRide`, which for a Portal Terminal queues a `GuestTransfer` (world-neutral fields) to the next world and
a `GuestDeparture`; `updatePortals` (each world's factory tick) removes departing guests of the active world and
generates arrivals beside the matching terminal's exit (terminal index mod terminal count).

## E9 Theme, content, release

Theme bible as content packs (terrain surface and edge objects, ore prototypes, machine skins, music,
scenario text). Content pack `data/factory/` with a vanilla-equivalent chain plus park goods (track segments,
car bodies, souvenirs). AppImage and Windows zip via trimmed CI; no "Factorio" or "RCT" in the product name;
GPLv3 code, CC-BY-SA 4.0 original art; upstream changelog convention kept.

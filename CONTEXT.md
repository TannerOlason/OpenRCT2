# Factory Tour

Factory Tour is a mergeable fork of OpenRCT2 that adds a Factorio-style production layer to the park
simulation. A player mines, smelts and assembles everything from scratch, then runs the factory *as* a theme
park: guests tour it, rate it and pay for it, and the two economies feed each other. Combat is an
API-complete stub for other modders. Long term the game expands to interplanetary and interdimensional
touring under a deliberately weird theme that lives in content packs, never in the engine.

This file is the vocabulary. Design decisions are in `docs/adr/`, the product boundary is
[`wiki/SCOPE.md`](wiki/SCOPE.md), the detailed design is [`wiki/SPEC.md`](wiki/SPEC.md), and the work
breakdown and milestone status are [`wiki/ROADMAP.md`](wiki/ROADMAP.md). Build and working rules for agents
are in [`CLAUDE.md`](CLAUDE.md).

## Fork mechanics

**Upstream**:
The OpenRCT2 `develop` branch, tracked as the git remote `upstream` and mirrored locally on the `develop`
branch. Never committed to directly.
_Avoid_: "master", "vanilla OpenRCT2" when the fork's own vanilla mode is meant

**Fork Code**:
New code that lives only in fork-owned directories: `src/openrct2/factory/`, `src/openrct2-ui/windows/factory/`,
`resources/g3/`, `data/factory/`, `wiki/`, `docs/adr/`.
_Avoid_: edits spread through upstream files

**Touch Point**:
A deliberate, minimal edit to an upstream file, always marked with a `// FACTORY-TOUR:` comment so upstream
merges stay mechanical.
_Avoid_: unmarked edits, refactors of upstream code

**Vanilla Mode**:
A park with no Factory Elements and `constructionMode = money`. Behaviour and save bytes must equal upstream's.
_Avoid_: "classic mode", silently changed defaults

**Fork Chunk**:
An OrcaStream park-file chunk in the reserved id range `0x40`–`0x4F`, carrying its own version number so
`kParkFileCurrentVersion` is never bumped by the fork.
_Avoid_: new fields in upstream chunks

**Side Table**:
A fork-owned map keyed by an upstream id (`EntityId`, `RideId`) that stores fork fields for upstream structs,
persisted in the `parkExt` Fork Chunk.
_Avoid_: new members on `Guest`, `Ride`, `Scenario::Options`

## Factory simulation

**Factory Element**:
The tile element of type `TileElementType::factory` (9). It anchors a factory Record to a tile and stores the
record id, subtype, footprint index, prototype entry and flags in its 11 payload bytes.
_Avoid_: scenery element, entity

**Record**:
One live factory object (belt segment, splitter, inserter, container, machine, pole, network) stored in a
Pool inside `FactoryState`. Records have stable ids; iteration is in ascending id order.
_Avoid_: entity, sprite

**Pool**:
A dense, id-stable container of Records with lowest-free allocation and alive flags, serialised densely so
ids match the raw tile payloads.
_Avoid_: `std::map`, pointer graphs

**FactoryState**:
The deterministic struct inside `GameState_t` that owns every Pool, the Ore Layer, topology/dirty versions
and the Warehouse. Ticked after `Ride::updateAll()` and before `Park::Update`.
_Avoid_: global singletons, static caches

**Prototype**:
A definition object of `ObjectType::factoryPrototype` with `properties.kind ∈ {item, recipe, belt,
undergroundBelt, splitter, inserter, container, machine, pole, pipe, generator, ore, technology}`. Content
text lives in its `"strings"`, art in its `"images"`.
_Avoid_: hard-coded item tables, separate object types per kind

**Segment**:
A run of up to 32 belt tiles with two Lanes, broken at splitters, undergrounds and tier changes. The unit of
belt simulation.
_Avoid_: per-tile item lists

**Lane**:
One side of a Segment holding gap-encoded `BeltItem {proto, gap}` entries in 1/256-tile units with minimum
spacing 64.
_Avoid_: float positions

**Inserter**:
A Record that swings items from the tile behind it to the tile ahead (far lane first) on a fixed tick
schedule, re-resolving source and target when `topologyVersion` changes.
_Avoid_: arm entity

**Container**:
A Record with item slots (chest, provider, loader buffer).
_Avoid_: ride stock, shop inventory

**Machine**:
A Record with a kind tag (`drill, furnace, assembler, boiler, engine, pump, lab, turret, exportDepot`) that
consumes inputs, power or fuel and makes progress in integer work units.
_Avoid_: ride, large scenery

**Ore Layer**:
A dense per-tile array of `OreCell {ore, richness, amount}` resized with the map and RLE-saved in the
`factoryOre` Fork Chunk, painted as a surface overlay.
_Avoid_: ore entities, ore scenery

**Power Network**:
A connected component of poles with per-tick `satisfactionQ16 = supply / demand`, rebuilt by BFS when dirty.
_Avoid_: incremental wire graphs

**Fluid Network**:
A connected component of pipes and machine Fluid Boxes sharing one volume of one fluid, drawn from in proportion
to demand; pumps and boilers bridge networks.
_Avoid_: per-pipe flow simulation

**Fluid Box**:
A machine's connection to fluid, declared in its prototype with a role (input or output), the sides it faces
relative to the machine's direction and a capacity it adds to its network. A box facing two sides is a
pass-through.
_Avoid_: tank (a tank is a placeable storage machine)

**Sync Checksum**:
The composite checksum (`entities + Factory::serialiseForSync`) that replaces upstream's entity-only
desync check and feeds replays and snapshots.
_Avoid_: entity-only checksum

## Park intertwine

**Tour Ride**:
The `RIDE_TYPE_FACTORY_TOUR` ride, cloned from Car Ride, whose ratings gain a factory-proximity modifier.
Guests only ride things with real track nearby, so the tour is a true ride type.
_Avoid_: viewing platform scenery

**Exhibit Path**:
A footpath surface flagged `isExhibit` that guest pathfinding is biased toward and never culls as a dead end;
walking one next to a Machine marks the guest `touredFactory`.
_Avoid_: queue path, ordinary footpath

**Warehouse**:
The park-wide material store the factory deposits into and construction, shops and the Market draw from.
_Avoid_: per-chest totals

**Material Bill**:
A list of `{item, quantity}` attached to a `GameActions::Result`, consumed on execute when the scenario's
`constructionMode` is `materials` or `hybrid`.
_Avoid_: money cost

**Market**:
The off-map buyer with per-item base price and saturation decay recovering daily, reached through an
Export Depot machine and `MarketSellAction`.
_Avoid_: shop, guest purchase

**Technology**:
A Prototype of kind `technology`, researched unit by unit by Labs consuming packs, in a fork tree beside upstream
research (ADR 0012). Researching it unlocks factory prototypes, ride entries and scenery groups together; until then
they are locked (Prototypes) or withheld from upstream's research lists (rides, scenery).
_Avoid_: research item (that is upstream's funded research)

**Freight Railway**:
The ride type in the 1F slot whose cars carry cargo (side table, not a `CarEntry` field) between Freight Loader and
Freight Unloader containers beside its stations (ADR 0014).
_Avoid_: cargo train ride, goods monorail

**Lab**:
A machine of kind `lab` that consumes the current Technology's packs to research it.
_Avoid_: research centre

**Threat**:
A fork record (not an entity, ADR 0013) of a `threat` Prototype that walks straight at the nearest destructible
machine and hits it when adjacent. Everything smarter is a modder's job.
_Avoid_: biter, enemy AI, enemy entity

## Multi-world

**World**:
One `GameState_t` instance (tiles, entities, rides, weather, own RNG) owned by the `WorldManager` and ticked
in lockstep with its siblings.
_Avoid_: map region, layer

**Company**:
The state shared across Worlds: finance, research, scenario options, master tick, shared date, the
Transfer Queue, Portal Links and the world directory.
_Avoid_: "global state"

**Portal**:
A `portalTerminal` ride on each side of a `PortalLink{worldA, rideA, worldB, rideB}` that moves guests
between Worlds. Rides never span worlds.
_Avoid_: cross-world track

**Transfer Queue**:
The Company-level list of `{from, to, item, qty, arrivalTick}` drained after all world ticks; the only way
items cross worlds.
_Avoid_: direct cross-world inventory access

**Planet Params**:
Per-World content knobs (gravity scale, energy drain, belt speed scale, ore table, climate, water, terrain)
stored in the per-world park chunk and edited in the scenario editor.
_Avoid_: engine constants per planet

**Content Pack**:
A set of `.parkobj` objects (Prototypes, terrain, music, scenario text) that carries the theme. The engine
never knows a planet's name.
_Avoid_: hard-coded theme strings

## Reserved id map (single source of truth)

| Space | Fork reservation |
|---|---|
| `TileElementType` | `factory = 9` (10–13 spare; never 8/14/15, importer corruption markers) |
| `ObjectType` | `factoryPrototype` (one type, `properties.kind`), cap 8192 |
| `GameCommand` | 10000+ via `FactoryCommand`, own registry |
| Park chunks | `0x40 factoryHeader`, `0x41 factoryPools`, `0x42 factoryOre`, `0x43 company`, `0x44 worlds`, `0x45 parkExt` |
| `WindowClass` | 142–219 |
| String ids | 0x5000–0x9FFF after the 5-digit `LanguagePack` parser patch |
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

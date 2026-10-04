# Roadmap

Epics are the work breakdown; milestones are vertical slices shipped in order. Sizes: S < 1 week, M 1–3
weeks, L 1–2 months, XL 2+ months for one engineer plus agents. Status legend: `[ ]` not started,
`[~]` in progress, `[x]` done. Each milestone ends with clang-format clean, `ctest` green, a fork replay
recorded, a two-client desync soak, a changelog line and an ADR for any new decision.

## Epics

| Id | Epic | Size | Summary |
|---|---|---|---|
| E0 | Bootstrap | S | Fork, toolchain, build on D, RCT2 data, docs skeleton, trimmed CI, merge policy |
| E1 | Factory core simulation | XL | Factory Element, FactoryState and Pools, prototypes, belts, inserters, machines, ore, power, fluids, Fork Chunks, Sync Checksum |
| E2 | Factory GameActions and multiplayer | M | Reserved command range, own registry, place/remove/rotate/recipe/filter/belt-line/clear/blueprint/wire/cheat actions, permissions |
| E3 | Rendering and UI | L | Paint for belts, items, inserters, machines, wires, smoke; build and info windows; sprite pack; 5-digit string ids |
| E4 | Park intertwine | L | Factory Tour ride, Exhibit Paths, guest thoughts, material economy, stocked shops, Market, rating, objectives, parkExt side tables |
| E5 | Research and progression | M | Technology prototypes in the research system, labs, unified unlock tree, content chain |
| E6 | Modding surface and combat stub | M | `.parkobj` schemas, `factory` script global, hooks, d.ts, health, damage, Threat, turret |
| E7 | Logistics at scale | L | Freight railway, loaders, blueprints, production graphs, alerts, pollution overlay |
| E8 | Multi-world | XL | WorldManager, caches into GameState, Company, Transfer Queue, Portals, guest transfer, Planet Params |
| E9 | Theme, content, release | L | Theme bible as content packs, vanilla-equivalent chain, packaging, licensing |

## Milestones

### M0 Bootstrap `[x]`

- [x] Fork `OpenRCT2/OpenRCT2` to `TannerOlason/OpenRCT2`; clone with `upstream` and `origin` remotes.
- [x] Branch `factory-tour/main` off `develop`; `develop` tracks upstream and is never committed to.
- [x] RCT2 data extracted from the GOG installer to `/media/user/D/rct2-data/app`.
- [x] User-space toolchain (conda: cmake, ninja, GCC 13, SDL2, OpenSSL, curl, freetype, fontconfig, libzip,
  zstd, libpng, ICU, nlohmann_json 3.11, FLAC, vorbis, gtest, clang-format, clang-tidy, expat); build tree on
  `/home/user/Documents/Projects/factory-tour-build` symlinked as `build/` (the NTFS D drive hangs on heavy
  unlink traffic, so it holds only the read-only RCT2 data).
- [x] Vanilla build runs (title scene loads, headless `simulate` completes); all 279 upstream tests pass
  including the replay pack; `game_path` set to the extracted GOG data.
- [x] Docs skeleton: `CONTEXT.md`, `docs/adr/0001–0008`, `wiki/SCOPE.md`, `wiki/SPEC.md`, `wiki/ROADMAP.md`,
  `CLAUDE.md`, `AGENTS.md`.
- [x] CI trimmed: fork-owned `.github/workflows/factory-tour-ci.yml` (clang-format, changelog, Linux noble
  build + tests, Windows x64 MSBuild) runs on `factory-tour/**`; upstream `ci.yml` and `clang-tidy.yml` ignore
  those branches (two-line Touch Points).
- [x] First FactoryElement commit: `TileElementType::factory = 9`, `FactoryElement` struct with the planned
  payload layout, `asFactory()` accessors, `PaintFactory` stub, `factory` scripting type name, cases in every
  exhaustive switch, `.vcxproj` entries and `FactoryElementTests`.

### M1 Hello conveyor `[x]`

Chest → inserter → belt → inserter → chest, placeable from a toolbar window with ghost preview, rotatable,
saved and loaded, multiplayer-synced, with determinism, save/load and throughput gtests.

- [x] Element type (`TileElementType::factory = 9`) and `FactoryElement` payload.
- [x] `factory_prototype` object type, four content objects, placeholder art generator (palette-snapped).
- [x] FactoryState, pools, fork chunks 0x40/0x41, composite Sync Checksum (upstream replays still pass).
- [x] Belt lanes (15 items/s over two lanes), topology (segments join/split, curves), inserters, containers.
- [x] Fork actions place/remove/rotate at command 10000+, `Permission::factory`, plugin names, version bumps.
- [x] Painter: belts with shape/direction/frame, items as child images, inserter frames, chests; verified by
  headless `openrct2-cli screenshot` renders at three rotations.
- [x] Right-click removal via `ViewportInteractionItem::factory`; FactoryBuild window (WindowClass 142) with
  prototype palette, rotate button (Z shortcut), ghost preview and click-to-place; toolbar button.
- [x] Tests: element layout, pools and serialisation, prototypes, belt lanes, topology and the hello-conveyor
  run, determinism, actions, save/load round trip with identical checksum.
- [x] Two-client desync soak: 10 minutes, headless host plus a GUI client on Xvfb building and removing at random
  (`scripts/factory-tour/mp-soak.py`): 733 client actions, 326 fork actions run by the server, no desync. Fork
  replay pack (`test/tests/testdata/factory-replays/`, three recordings covering every fork action) plays back in
  sync; recording it found that an empty ore layer's size leaked into the sync checksum (a join desync), now fixed.
- [x] Changelog line added; `openrct2.d.ts` declares every fork action (`factoryplace`, `factoryplacebeltline`,
  `factoryremove`, `factoryrotate`, `factorysetfilter`, `factorysetore`, `factorysetrecipe`) with argument shapes.

### M2 Production chain `[x]`

Ore Layer and overlay, mining drill, furnace, assembler with recipe window, power (poles, offshore pump,
boiler, steam engine, power overview), pipes and Fluid Networks, undergrounds, splitters, filters, belt-line
drag, `factory-bench` CLI and the 8 ms CI gate.

- [x] Ore Layer (`OreLayer`, chunk 0x42 RLE, order-independent hash in the sync checksum), ground overlay
  toggled from the view menu (`ViewportFlag::factoryOre`, bit 22), `FactorySetOreAction` for the editor,
  sandbox and tests.
- [x] Machines as 1x1 records: burner mining drill (scans a radius, drops output onto the tile ahead) and
  stone furnace (auto recipe from its input); recipe, ore and fuel prototypes; `FactorySetRecipeAction` for
  assemblers; inserters feed and empty machines. Test: ore → drill → belt → inserter → furnace → inserter →
  chest yields 90 plates from 90 ore.
- [x] Multi-tile footprints: square machines (`size`), one element per tile sharing the record, validated per
  tile, removed as a whole from any tile; drills mine around the centre and drop past the front edge's centre; power
  reaches the nearest footprint tile; fluid boxes connect at edge centres; the painter draws a per-tile slice chosen
  by view position. Content: the 3x3 electric mining drill. Checked in the GUI under Xvfb.
- [x] Assembler + electric power: pole prototypes (wire reach, supply radius), power networks rebuilt by
  flood fill when poles or machines change, burner generators that burn fuel only under load, consumers
  scaling progress by last tick's satisfaction. Test: plates → assembler → gears only once a fuelled
  generator shares a network, and the assembler loses power when the linking pole is removed.
- [x] Steam chain: offshore pump (needs water behind it), burner boiler (water → steam), steam engine (steam →
  power, burns in proportion to load). Test: pump → pipe → boiler → engine powers an assembler that makes gears.
- [x] Pipes and fluid networks (`pipes` and `fluidNetworks` pools, `fluidDirty`, one volume per component,
  proportional sharing, machine fluid boxes as pass-through nodes, ADR 0009). Info window shows network contents.
- [x] Underground belts, splitters, lane filters, sideloading. Splitters take an item filter and input/output
  priorities through `FactorySetFilterAction` (command `setFilter`), set from the info window.
- [x] Machine window (recipe picker, inventories, status, fluid boxes), belt-line drag tool, power overview
  (WindowClass 144: counts, supply, demand, satisfaction and a history graph; opened from a pole or the build
  window). All checked in the real GUI under Xvfb (`wiki/TESTING.md`), which found and fixed left clicks on
  factory elements never reaching the info window.
- [x] `factory-bench` CLI (`openrct2-cli factory-bench [ticks] [cells] [budget ms]`, per-phase µs, checksum) and
  the 8 ms per tick CI gate on 5000 cells (60k belts, 20k inserters, 20k machines: about 0.8 ms locally). Power
  networks rebuild through a spatial pole grid (a 12000-cell rebuild went from 1.46 s to 18 ms).

### M3 Park intertwine I `[x]`

Factory Tour ride type, ratings modifier and tour vehicle object; guest thoughts and watching; pollution and
noise effects; `parkExt` Side Table chunk. The first moment the two games touch.

- [x] Factory Tour ride type (RIDE_TYPE_1D slot, flat track, ADR 0010), `bonusFactoryProximity` ratings modifier fed
  by a 5x5 factory scan along the track walk, tour tram vehicle object with generated art. Tests: pure score,
  per-piece counting, and a built circuit whose excitement rises beside a working furnace; trams render running.
- [x] Guest thoughts 174-180 declared (strings, action map, plugin names, d.ts); walking guests near a factory think
  `factoryImpressive` (+30 happiness), `factoryNoise` (-10) or `factorySmell` (-20, +12 nausea) before upstream's
  scenery thoughts; parks without a factory are untouched.
- [x] Guests stop to watch working photogenic machines (`factoryWatching`, +20 happiness, seat bit 0x04) and are
  marked `touredFactory` when they watch or ride a Factory Tour.
- [x] Pollution and noise: machine `pollution` feeds an 8x8-tile pollution grid that spreads and decays every 64 ticks
  (sparse in saves and the checksum, pools chunk version 6); machine `noise` within three tiles annoys guests.
- [x] `parkExt` Side Table chunk (0x45, own version): sorted guest flags, pruned of departed guests every 256 ticks,
  part of the sync checksum; saves round-trip it.

### M4 Park intertwine II `[x]`

Material economy (construction modes, Material Bills, Warehouse, refunds, cost text), Warehouse-stocked shops
and souvenir items, Market, new expenditure rows, park-rating terms, objectives, scenario editor options,
Exhibit Paths.

- [x] Warehouse (park-wide stock, fed by warehouse depot containers) and construction modes (money, hybrid,
  materials) in `parkExt`, set by `FactorySetParkOptionAction`. In hybrid and materials mode upstream construction
  (ride construction and landscaping, not fork actions) carries a bill of one Iron plate per 5.00 of cost: the query
  fails with `insufficientMaterials` ("Needs N Iron plate") when the warehouse is short, execute takes it (materials
  mode charges no money), and refunds put plates back.
- [x] Construction window cost text: the ride construction, footpath and scenery windows show "$12 + 3 Iron plate"
  in hybrid mode and "Needs 3 Iron plate" in materials mode (`Ui::Factory::constructionCostText`); money mode is
  unchanged. Checked in the GUI with a plugin calling `factorysetparkoption`.
- [x] Market and export depot: item `marketPrice`, saturation per sale with daily recovery and a 1/8 price floor;
  export depots sell what is put in, `FactoryMarketSellAction` sells Warehouse stock (click a stack in the depot
  window). Money is booked under Shop sales instead of new expenditure rows (ADR 0011: the expenditure table's save
  format depends on the enum's size).
- [x] Warehouse-stocked shops and souvenir shop items: `ShopItem::factoryModel` (56) and `gearKeyring` (57) with
  their own strings and plugin names, made by assembler recipes and sold by the Factory gift shop stall. In warehouse
  stock mode a shop item that a factory item maps to (item `shopItem`) is taken from the Warehouse instead of paying
  the stock cost, and guests think "It's sold out!" when it runs out.
- [x] Park rating term behind `ParkFlag::factoryAffectsRating` (bit 33, off by default so ratings stay upstream's):
  up to -150 for the average pollution where guests stand and -25 to +25 for the share of machines working.
- [x] Objectives `produceItemsBy` (item in `NumGuests`, quantity in `Currency`) and `guestsTouredFactory`, with
  production statistics and an all-time tour count; listed in the scenario editor's objective dropdown with
  quantity/guest and year spinners. A Factory options window (from the build window) sets construction mode, shop
  stock, the rating flag and the produce objective's item, all through `FactorySetParkOptionAction`.
- [x] Exhibit Paths: footpath surfaces with `"isExhibit": true` (`FOOTPATH_ENTRY_FLAG_IS_EXHIBIT`, 1 << 5); aimless
  guests take exhibit edges about 60% of the time, exhibit dead ends are not culled, and stepping onto one within a
  tile of a machine marks the guest toured. Content: the Factory exhibit walkway (path sprites from the player's
  RCT2 data, like upstream's official objects). Parks without exhibit paths keep upstream's random sequence.

### M5 Progression and modding `[x]`

Technology prototypes in the research system, labs, unified unlock tree, script bindings, hooks and d.ts,
object-selection tabs, combat stub (health, damage, Threat, turret), first content pack. A modder can ship a
`.parkobj` turret.

- [x] Technologies and labs (ADR 0012): technology prototypes form a fork tree beside upstream research; labs
  consume packs to research the target chosen in the Factory research window; researched technologies unlock
  factory prototypes, ride entries and scenery groups (withheld from upstream's research lists while gated). Starter
  tree with research kits and labs.
- [x] Script bindings: the `factory` global (machines, recipes, warehouse, production, market income,
  technologies and the research target), hooks `factory.machine.status` and `factory.research.complete`, d.ts
  (plugin API 134). Combat hooks follow with the combat stub.
- [x] Object selection sub-tabs on the factory page: all, items/fluids/ores, recipes, logistics, machines,
  technologies. The kind is kept in the object index (`ObjectRepositoryItem::FactoryPrototypeInfo`, index
  version 33) so the list filters without loading objects.
- [x] Combat stub (ADR 0013): machine health and wrecks, ride damage ending in a breakdown, Threats as factory
  records that walk at the nearest machine, turrets with ammunition, three actions, four hooks, script access, and
  a combat replay in the determinism pack.
- [x] First content pack: steel and engineering kits, fast inserters, Factory defence (turret and ammunition) and
  Steel processing technologies (nine in all, two pack tiers). Modding guide `wiki/MODDING.md` with the example
  turret mod `docs/examples/heavy_turret` (zipped to a `.parkobj` in a user `object` folder, it lists and loads in
  object selection) and the example plugin `docs/examples/threat-waves.js`.

### M6 Logistics and polish `[x]`

Freight railway with cargo cars and loader stations, blueprints and copy/paste, production graphs, alerts,
pollution overlay, machine audio and smoke.

- [x] Alerts: every 1024 ticks machine statuses are counted; a growing count of machines without power, fuel, ore
  or ammunition, or with full output, posts a news item linked to the first of them (at most one per kind per 4096
  ticks); a destroyed machine is announced at once.
- [x] Pollution overlay: View menu "Show factory pollution" shares the ore overlay's viewport flag (no free bits
  remain) with a client-side mode, and tints the ground yellow, orange or red by the 8x8 pollution cell.
- [x] Production graphs: production and consumption per item (ingredients, lab packs, fuel, ammunition) with a
  48-sample history (one sample per 1024 ticks, about 20 minutes) in the Factory production window (WindowClass 147):
  per-minute rates and a bar graph of production up and consumption down.
- [x] Blueprints and copy/paste: the Blueprints window (WindowClass 148, from the build window) copies the pieces in a
  dragged area (with assembler recipes) into a portable text blueprint (`FTBP1`, prototypes by identifier), previews
  it as ghosts at the cursor, rotates it in quarter turns and pastes it with `FactoryPlaceBlueprintAction` (pieces
  that do not fit are skipped; underground pairs re-form). Export and Import go through the system clipboard.
- [x] Machine audio and smoke: working machines with `noise` >= 30 clank (one-shot 3D sounds, staggered, at most
  two a tick, from a touch point beside `VehicleSoundsUpdate`); turrets click when they fire; machines with `smoke`
  (default: burner machines) puff steam-particle smoke from their centre tile, drawn in paint only.
- [x] Freight railway (ADR 0014): `RIDE_TYPE_FREIGHT_RAILWAY` in the 1F slot (flat miniature-railway track, no
  entrance or exit) with the Freight train's crate wagons; freight loader and unloader containers beside station
  track fill and empty a standing train car by car (200 items of one kind per car, cargo in a side table); unlocked
  by the Freight railway technology; `factory.freight` lists cargo for scripts.

### M7 Multi-world I `[x]`

E8 stages 0–2: WorldManager, caches moved into GameState, Company state, two worlds with item transfer
(rocket silo), world selector, nested save, network map and tick changes.

- [x] Worlds core (ADR 0015): `Factory::Worlds` swaps whole game states with stashed per-world caches, moves company
  state (money, research, date, objectives, Market) with the active world, ticks every world in lockstep from one
  hook in `gameStateUpdateLogic`, and routes actions by a world id in their command flags.
- [x] Saving and network maps with several worlds: world 0 is always the top-level park (`ParkFile::Save` saves
  `Worlds::saveTarget()`), the others are nested uncompressed park files in fork chunk 0x44, imported once world 0
  has loaded; the network checksum folds in every other world.
- [x] Worlds window (WindowClass 149, from the build window): lists worlds with size and machine count, views one
  (closing windows that point into the old world), and creates flat, fully owned worlds through
  `FactoryCreateWorldAction` (command 16).
- [x] Item transfer between worlds: launch pads send their contents every 200 ticks to their target world (the
  next world unless set with `FactorySetLaunchTargetAction`, command 17), where shipments land in landing pads 400
  ticks later; the queue is company state. Unlocked by Interworld logistics. (Rocket art and launch animation: M9.)

### M8 Multi-world II `[x]`

E8 stages 3–4: Portal Terminal rides, guest transfer, Planet Params, per-world climate, water and terrain,
first weird-dimension content pack, `context.worlds` binding.

- [x] Portal Terminals (ADR 0016): `RIDE_TYPE_PORTAL_TERMINAL` in the 22 slot, a short shuttle ride (Portal
  shuttle cars, Portal terminals technology); guests leaving one are removed from their world and appear beside the
  exit of the matching terminal (same index, wrapping) in the next world with their mood, needs, money and name.
- [x] Planet Params (per world, parkExt version 5): belt and machine speed percentages and a held weather type;
  `FactoryCreateWorldAction` takes a preset (plain, desert, ice moon, weird dimension) that sets terrain, a lake for
  pumps, ore clusters and the rules. Per-world climate objects were not needed: weather is held by the rules.
- [x] Weird-dimension content: martian terrain, 150% belts and 75% machines, endless storms, Void crystal ore
  painted in clusters and the Void lens recipe.
- [x] Script binding: `factory.worlds`, `factory.activeWorld`, `factory.createWorld(size, preset)` (plugin API 140).

### M9 Theme and release `[ ]`

Theme bible, full content pass, trailer scenarios, packaging, licensing, naming.

## Verification per milestone

- Build: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DWITH_TESTS=on && ninja -C build`;
  clang-format on changed files; clang-tidy on fork files.
- Tests (`test/tests/`): `FactoryBeltTests`, `FactoryDeterminismTests`, `FactorySaveLoadTests`,
  `FactoryActionTests`, pure `FactoryProximityScore` and `BillFromCost` tests, Warehouse, Market, park rating
  equality with the flag off, research with mocked science points, scripting damage hook. Run with
  `ctest --test-dir build --output-on-failure`.
- Vanilla equivalence: upstream replay pack passes in Vanilla Mode; fork replay pack added per milestone.
- Performance: `openrct2-cli factory-bench 4000` prints per-phase µs and checksum; CI asserts ≤ 8 ms/tick.
- Multiplayer: headless host plus GUI client, 10-minute concurrent-building soak with no desync.
- Manual: place the slice, watch items at all zooms and rotations, save/load, run a Factory Tour ride next
  to machines and confirm excitement rises, toggle construction mode and read "needs N Iron plate".

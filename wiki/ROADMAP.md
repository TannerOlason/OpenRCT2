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

### M4 Park intertwine II `[~]`

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
- [ ] Park rating terms (pollution near paths, uptime) and park flags.
- [ ] Objectives and scenario editor options.
- [ ] Exhibit Paths.

### M5 Progression and modding `[ ]`

Technology prototypes in the research system, labs, unified unlock tree, script bindings, hooks and d.ts,
object-selection tabs, combat stub (health, damage, Threat, turret), first content pack. A modder can ship a
`.parkobj` turret.

### M6 Logistics and polish `[ ]`

Freight railway with cargo cars and loader stations, blueprints and copy/paste, production graphs, alerts,
pollution overlay, machine audio and smoke.

### M7 Multi-world I `[ ]`

E8 stages 0–2: WorldManager, caches moved into GameState, Company state, two worlds with item transfer
(rocket silo), world selector, nested save, network map and tick changes.

### M8 Multi-world II `[ ]`

E8 stages 3–4: Portal Terminal rides, guest transfer, Planet Params, per-world climate, water and terrain,
first weird-dimension content pack, `context.worlds` binding.

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

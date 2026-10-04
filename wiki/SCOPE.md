# Scope

This is the authoritative product boundary for Factory Tour. [`CONTEXT.md`](../CONTEXT.md) defines the
vocabulary, `docs/adr/` records the decisions, [`SPEC.md`](SPEC.md) holds the design and
[`ROADMAP.md`](ROADMAP.md) tracks the work.

## Product

Factory Tour is OpenRCT2 with a deterministic, multiplayer-safe production layer. The player mines ore,
smelts plates, assembles parts and researches technologies with belts, inserters, machines, power and
fluids, then runs that factory as a theme park. Guests ride a Factory Tour ride and walk Exhibit Paths past
working machines, form thoughts about what they see, smell and hear, and buy souvenirs the factory made.
Construction can be paid for in money, in materials from the Warehouse, or both, and surplus goods sell to an
off-map Market. Research unifies science packs with the park's ride and scenery unlocks. Later, the Company
spans several Worlds (planets and dimensions) linked by Portal rides and item transfer.

## In scope

- A mergeable fork of OpenRCT2 `develop` with marked Touch Points and reserved id blocks.
- Factory core: Factory Elements, Pools, Segments and Lanes, inserters, containers, machines (drill, furnace,
  assembler, boiler, engine, pump, lab, turret, export depot), Ore Layer, Power and Fluid Networks, Fork
  Chunks, Sync Checksum, performance budget of 3 ms typical and 8 ms hard per tick at 50k belts.
- Fork GameActions at the reserved command base with ghosts, permissions and replay support.
- Paint and UI: belt, item, inserter and machine painting at all zooms and rotations; build, machine,
  container, power, fluid, statistics, blueprint and alert windows; ore and pollution overlays.
- Park intertwine: Factory Tour ride type and ratings modifier, Exhibit Paths, guest thoughts and watching,
  pollution and noise, material economy, Warehouse-stocked shops, Market, expenditure rows, park rating
  terms, objectives, scenario editor options.
- Progression: technologies in the upstream research system, labs, unified unlock tree.
- Modding surface: every prototype as a `.parkobj`, `factory` script global, `factory.*` hooks, TypeScript
  definitions, object-selection tabs.
- Combat stub: health, `FactoryDamageAction`, one straight-walking Threat entity, turret machine kind, hooks.
- Logistics at scale: freight railway with cargo cars and loader stations, blueprints, production graphs,
  alerts, audio and smoke.
- Multi-world: `WorldManager` ticking N `GameState_t` instances, Company state, Transfer Queue, Portal
  Terminal rides, guest transfer, Planet Params, per-world climate, water and terrain.
- Theme and release: content packs carrying the theme, AppImage and Windows packaging, GPLv3 code and
  CC-BY-SA 4.0 original art.

## Out of scope

- Combat beyond the stub: enemy AI, pathing, waves, evolution, guest panic, rubble and repair, vehicle
  damage. These are the modders' playground.
- Any Factorio or RCT2-derived art, names or data. Product name contains neither "Factorio" nor "RCT".
- Replacing the RCT2 data requirement. OpenGraphics is incomplete; the fork needs an RCT2 install.
- Hard-forking upstream, editing upstream chunk layouts, or bumping `kParkFileCurrentVersion`.
- Putting the simulation in a plugin, or belt items in the entity array.
- A single giant partitioned map for multi-world, or separate processes per world.
- Theme content inside the engine; the engine never knows a planet's name.

## Vanilla guarantee

With no Factory Elements and `constructionMode = money`, the fork behaves byte-identically to upstream:
upstream replays pass, saves are identical, and park rating, finances and guest behaviour are unchanged.

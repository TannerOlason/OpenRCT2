# Worlds are swapped game states with a moving company

Several worlds are several `GameState_t` instances (`Factory::Worlds`, `factory/WorldManager.cpp`), as in the plan.
One of them is active, swapped into `GameState.cpp`'s `_gameState`; the others are kept in slots. The plan proposed
moving per-world globals into `GameState_t` and splitting company fields out into a separate `CompanyState`. Both
refactor upstream broadly, so:

- **Per-world globals are stashed, not moved.** An audit found that only a few: the tile pointer index and
  elements-in-use count (`MapSwapWorldCaches`), the three map-animation sets (`MapAnimations::SwapWorldCaches`),
  ride-use history (public accessors) and the land-rights counters. Each world keeps them in an opaque stash
  (`std::any`, so the types stay private to their files) that is swapped on activation. Patrol areas are rebuilt
  on activation; entities, rides, news and ratings already live in `GameState_t`.
- **Company state moves with the active world.** Money and finance history, loans, park flags, scenario options and
  date, research (upstream's and the fork's technologies), cheats, Market, production totals and the factory options
  are copied from the previously active world on every activation. Only the active world is authoritative, so a
  single bank account and research effort result without moving any upstream field.
- **Lockstep ticking** is one hook at the top of `gameStateUpdateLogic`. With several worlds it runs the function
  once per world in ascending order, regardless of which world is on screen, so every peer ticks identically. In
  secondary worlds' passes, once-per-tick work is skipped: network, replay, date and scenario updates, research, the
  action queue and script hooks. Audio and provisional ghosts follow the world on screen.
- **Actions carry their world** in `CommandFlags` bits 16-23, which upstream does not use. World 0 is zero, so
  upstream replays and single-world network traffic are unchanged. A locally issued action is stamped with the active
  world, and `GameActions::Query/Execute` run it with that world active.
- A new or loaded park keeps only the loaded state, as world 0 (`adoptActiveAsPrimary` in `gameStateInitAll`).

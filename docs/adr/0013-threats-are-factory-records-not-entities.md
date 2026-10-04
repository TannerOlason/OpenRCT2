# Threats are factory records, not entities

The plan made the combat stub's Threat a new `EntityType::threat`. Upstream writes the entity chunk as a fixed list
of entity types (`ReadWriteEntitiesChunk`), so another type changes every save, Vanilla Mode included. It also needs
cases in snapshots, paint, the entity checksum and `ScEntity`. Threats are instead records in a fork pool
(`State::threats`), like items (ADR 0003). They are saved in the pools chunk, covered by the sync checksum through the
same visitor, and painted from the entity paint pass through one touch point (`PaintFactoryThreats` in
`EntityPaintSetup`). Positions are world units, so they move smoothly; scripts reach them through
`factory.threats`, `factory.spawnThreat` and the `factorythreatspawn`/`factorythreatdespawn`/`factorydamage` actions
instead of the entity API.

The rest of the stub follows the same rule of no new upstream enum values. Ride damage lives in the `parkExt` side
table and ends in an existing `Breakdown::safetyCutOut` rather than a new breakdown reason. Machine health is a field
on machine records, with `MachineStatus::destroyed` for wrecks that stay until removed. Built-in behaviour stays
minimal: a threat walks straight at the nearest destructible machine and hits it when adjacent, and a turret shoots
the nearest threat in range. AI, pathing, waves and repair are left to scripts.

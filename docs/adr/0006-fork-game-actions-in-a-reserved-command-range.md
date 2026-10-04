# Fork game actions in a reserved command range

Fork actions use `GameCommand` ids from `kFactoryCommandBase = 10000` upward and are registered in a
fork-owned `FactoryActionRegistry` that the upstream registry defers to for ids at or above the base. The
upstream enum is a serialised `int32` sent over the network, and inserting values before `custom` would
shift every later id and conflict on every merge. Every fork action derives from
`GameActionBase<static_cast<GameCommand>(kFactoryCommandBase + n)>`, serialises its fields, supports
`CommandFlag::ghost`, and belongs to the `Permission::factory` group. `kStreamVersion` and `kReplayVersion`
are bumped whenever an action's wire format changes.

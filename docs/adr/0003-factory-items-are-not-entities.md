# Factory items are not entities

Belt items, inserter hands, fluids and electricity are never `EntityBase` entities. The entity array has
65 535 fixed 512-byte slots shared with guests, staff, vehicles and litter, and adding entity types touches
the park file, checksum, snapshots, paint and script bindings. Factory objects are Records in id-stable Pools
inside `FactoryState`, belt items are gap-encoded 4-byte entries in Segment Lanes, and items are painted as
child images of the belt element. This keeps guest capacity intact and makes the factory's memory and tick
cost proportional to the factory alone.

# Anchor factory state to tile element type 9 and pools

Factory objects are anchored to the map by a new `TileElementType::factory = 9`. The 16-byte element stores
a subtype, a 32-bit record id, a footprint index, the prototype entry index and flags in its 11 payload
bytes; the base type byte carries direction. The record id points into a Pool in `FactoryState`, the same
pattern Track elements use to reference a ride. Types 10–13 are spare; 8, 14 and 15 are never used because
the RCT1/RCT2 importers treat them as corruption markers. The tiles chunk is a raw copy of elements, so the
new type saves and loads with no park-file version change; records live in Fork Chunks. Ghosts carry
`recordId = 0xFFFFFFFF` and never allocate a record.

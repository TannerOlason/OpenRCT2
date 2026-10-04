# Persist fork data in fork chunks with their own versions

All fork state is saved in OrcaStream chunks from the reserved range `0x40`–`0x4F`: `factoryHeader`,
`factoryPools`, `factoryOre`, `company`, `worlds`, `parkExt`. Each chunk begins with its own `uint16`
version so `kParkFileCurrentVersion` is never bumped by the fork and upstream version fix-ups are never
touched. Older builds skip unknown chunks, so a fork save with no factory content loads in vanilla OpenRCT2,
and a Vanilla Mode save is byte-identical to upstream's. Pools are saved densely with alive flags so record
ids match the raw tile payloads, and `Factory::postLoad` repairs element-to-record links. The network map
transfer reuses the park exporter, so joiners receive the chunks for free.

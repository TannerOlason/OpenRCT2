# Keep a mergeable fork of upstream develop

Factory Tour tracks the OpenRCT2 `develop` branch rather than hard-forking it. All new code lives in
fork-owned directories (`src/openrct2/factory/`, `src/openrct2-ui/windows/factory/`, `resources/g3/`,
`data/factory/`) and every edit to an upstream file is a marked Touch Point (`// FACTORY-TOUR:`). Upstream is
merged roughly monthly and conflicts are resolved only at Touch Points. New enum values, chunk ids, command
ids and sprite ranges are taken from reserved blocks at the end of or outside upstream ranges so upstream
additions never collide. The cost is a stricter code layout; the benefit is that OpenRCT2's ongoing fixes,
platforms and object content keep flowing into the fork.

# Implement the factory in C++, not as a plugin

The factory simulation is engine code, not a QuickJS plugin. Plugins cannot define tile element types,
object types or per-tile paint, cannot add network-synced state, and cannot run a 40 Hz belt simulation over
thousands of tiles inside the deterministic tick. The plugin API is instead extended (a `factory` global,
`factory.*` hooks, `ScMachine`, `ScThreat`) so that content, combat and park-side mods can be written without
C++. Plugin-visible behaviour is versioned through `kPluginApiVersion` like upstream.

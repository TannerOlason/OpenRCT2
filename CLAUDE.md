# Factory Tour — agent instructions

Factory Tour is a mergeable fork of OpenRCT2 that adds a Factorio-style production layer and runs the
factory as a theme park. Read [`CONTEXT.md`](CONTEXT.md) for vocabulary and the reserved id map,
[`wiki/SCOPE.md`](wiki/SCOPE.md) for the product boundary, [`wiki/SPEC.md`](wiki/SPEC.md) for the design and
[`wiki/ROADMAP.md`](wiki/ROADMAP.md) for milestone status before choosing or continuing a slice. Decisions
live in `docs/adr/`; add an ADR when you make a new one. `docs/notes/` holds upstream integration briefs
(exact touch points for object types, game actions, UI and paint) gathered on 2026-10-03; line numbers drift,
so verify against the source before editing.

## Branches and remotes

- `upstream` = `OpenRCT2/OpenRCT2`; local `develop` mirrors `upstream/develop` and is never committed to.
- `origin` = `TannerOlason/OpenRCT2`; work happens on `factory-tour/main` and feature branches off it.
- Merge upstream roughly monthly with `git merge upstream/develop`; resolve conflicts only at Touch Points.

## Toolchain (no sudo on this machine)

Build dependencies come from a conda environment, not apt. Source the helper before any build command:

```bash
source scripts/factory-tour/env.sh        # conda toolchain on PATH, CC/CXX, CMAKE_PREFIX_PATH
```

The environment is `/home/user/miniconda3/envs/factory-tour` (cmake, ninja, ccache, GCC 13, SDL2, OpenSSL,
curl, freetype, fontconfig, libzip, zstd, libpng, ICU, nlohmann_json, FLAC, vorbis, gtest, clang-format,
clang-tidy). RCT2 data is at `/media/user/D/rct2-data/app` (mount D first with
`udisksctl mount -b /dev/sdb1` if `/media/user/D` is empty). The build tree is
`/home/user/Documents/Projects/factory-tour-build`, symlinked as `build/`, configured with `-g1 -gz` so it
stays small on the nearly full root disk. Never put a build tree on D: the NTFS driver hangs on heavy
unlink/truncate traffic (see Hazards).

## Build and test

```bash
source scripts/factory-tour/env.sh
scripts/factory-tour/configure            # cmake -S . -B build -G Ninja (RelWithDebInfo, tests on)
ninja -C build                            # or: ninja -C build openrct2 openrct2-cli OpenRCT2Tests
ctest --test-dir build --output-on-failure
build/openrct2-cli scan-objects           # once, after a fresh build (tests expect indexed objects)
build/openrct2 set-rct2 /media/user/D/rct2-data/app   # once, points game_path at the RCT2 data
build/openrct2                            # run the game
```

Format fork files with `clang-format -i <files>` (upstream `.clang-format`, Allman braces, 128 columns). The local
clang-format (v23) differs from CI's (v20) on some untouched upstream code, so never format upstream files wholesale:
run `scripts/factory-tour/check-touchpoint-format.sh`, which reports only formatting hunks that touch FACTORY-TOUR
lines (include order included), and `scripts/factory-tour/check-vcxproj.sh` before committing. Fork C++ must pass clang-tidy with upstream's `.clang-tidy`.

GUI behaviour (windows, tools, drags) can be checked headlessly under Xvfb with `scripts/factory-tour/xdrive.py`;
see [`wiki/TESTING.md`](wiki/TESTING.md).

## Fork conventions

- New code goes in fork-owned directories: `src/openrct2/factory/`, `src/openrct2-ui/windows/factory/`,
  `resources/g3/`, `data/factory/`, `test/tests/Factory*.cpp`.
- Every edit to an upstream file is a Touch Point: keep it minimal and mark it with `// FACTORY-TOUR:` on
  the line above. Never refactor upstream code in passing.
- Reserved ids are in the table at the bottom of `CONTEXT.md`. Append to upstream enums only at the reserved
  positions; never insert mid-range.
- **Every new source file must also be added to the matching `.vcxproj`** (`libopenrct2.vcxproj`,
  `openrct2-ui.vcxproj`, `test/tests/tests.vcxproj`). Windows CI uses MSBuild with explicit file lists.
- Fork state is saved in Fork Chunks `0x40`–`0x4F` with their own version numbers; never bump
  `kParkFileCurrentVersion`. Fork fields on upstream structs are Side Tables in the `parkExt` chunk.
- Simulation or wire-format changes bump `kStreamVersion` (`network/NetworkBase.cpp`), `kReplayVersion`
  (`ReplayManager.cpp`) and, for plugin-visible changes, `kPluginApiVersion` (`scripting/ScriptEngine.h`,
  keep the exact `kPluginApiVersion = N` format).
- Determinism: integer and fixed-point only, `ScenarioRand()` for randomness, iterate pools in ascending
  id order, no adaptive throttling. Anything that affects the Sync Checksum must be covered by a determinism
  test.
- Vanilla Mode must stay byte-identical to upstream. Gate fork behaviour on the presence of Factory Elements
  or on scenario options that default to upstream behaviour.
- No Factorio or RCT2-derived art, names or data in the repo.
- Add a changelog line under `distribution/changelog.txt` for user-visible changes, in upstream's format.
- Update `wiki/` with every behaviour, interface, state or workflow change, and tick the milestone box in
  `wiki/ROADMAP.md`.

## Codex monitor

For substantial multi-step work use the shared dashboard from `/home/user/AGENTS.md`:

```bash
export CODEX_MONITOR_LOG=/tmp/codex-monitor/factory-tour.jsonl
export CODEX_MONITOR_AGENT=factory-tour
/home/user/Documents/Scripts/codex-monitor/scripts/codex_monitor.py post "Short progress note"
```

## Hazards

- `/media/user/D` is NTFS (`ntfs3`). Writes and deletes there can hang in uninterruptible D state (a
  truncate in another project, and `rm -rf build/CMakeFiles` on 2026-10-03). A process in D state cannot be
  killed. Use D only for read-only bulk data such as the RCT2 install; never for a build tree or ccache.
- Upstream replays in `test/tests/ReplayTests.cpp` run every `replays/*.parkrep`; any simulation change that
  is not gated to Vanilla Mode breaks them.
- Object PNGs are imported in "standard" mode: every opaque pixel must be an exact OpenRCT2 palette colour
  (`StandardPalette` in `drawing/ImageImporter.h`, stored BGRA) or it becomes transparent. The art generator
  snaps colours; any hand-made art must too.
- The root disk has about 18 GB free. Keep the build tree small (`-g1 -gz`, ccache capped at 3 GB) and put
  large read-only downloads on D.

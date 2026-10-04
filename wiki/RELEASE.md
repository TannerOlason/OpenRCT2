# Release

## Naming review

- **Product name:** "Factory Tour". It contains neither "Factorio" nor "RCT"/"RollerCoaster Tycoon", and it
  describes the game: a factory toured as a theme park.
- **In-game names:** all fork objects, technologies and windows use generic names (Stone furnace, Bolt turret,
  Portal Terminal, Research kit). None reuse names unique to Factorio, such as "science pack" or "biter", or
  RCT2 ride names.
- **Identifiers:** fork object ids use the `factory-tour.` prefix; the reserved id map is in `CONTEXT.md`.
- **Upstream branding:** the executable and data layout stay OpenRCT2's, so the fork merges cleanly. Release
  artefacts are named `FactoryTour-<version>-<platform>`.

## Licensing

- **Code:** GPLv3, as upstream (`licence.txt`).
- **Original art and content** in `data/factory/` and `docs/examples/`: CC-BY-SA 4.0 (`data/factory/LICENSE.md`).
- **RCT2 data:** never distributed. Players point the game at their own install, as with OpenRCT2.

## Packaging

The fork CI (`.github/workflows/factory-tour-ci.yml`) builds and tests on Linux and Windows for every push. On
pushes to `factory-tour/main` it also packages:

- a Windows portable zip (`build-portable`), and
- a Linux AppImage (`build-appimage`).

Both include `data/factory` through the normal install rules (`bin/data/factory` on Windows,
`share/openrct2/factory` in the AppImage).

## Release checklist

1. All CI jobs are green on `factory-tour/main`, including the 8 ms bench gate and the replay pack.
2. `distribution/changelog.txt` has a line for every user-visible change.
3. `wiki/ROADMAP.md` boxes are ticked for the release's milestone.
4. Tag `factory-tour-vX.Y`, then download the CI artefacts and attach them to the GitHub release with the
   changelog excerpt.

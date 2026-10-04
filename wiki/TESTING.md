# Testing

How Factory Tour changes are checked, from fastest to slowest. `CLAUDE.md` has the build commands.

## Unit and simulation tests

`build/OpenRCT2Tests --gtest_filter='Factory*'` (run from `build/`, the tests open `testdata/` relative to it).
`FactoryTopologyTests` places elements on a real map and ticks the simulation; `FactoryActionTests` runs the fork
game actions; `FactoryStateTests` covers pools, serialisation and the sync checksum. `ctest --test-dir build` runs
everything, including upstream's replay pack, which must keep passing in Vanilla Mode.

## Performance

`build/openrct2-cli factory-bench [ticks=400] [cells=2000] [budget ms=8]` builds a synthetic factory on a fresh map
(each 20x4 cell: drill, 12 belts, furnace, chests, a powered assembler, a pole and six pipes), runs only the
factory update and prints the average and worst tick, microseconds per phase and the sync checksum; it exits
non-zero when the average exceeds the budget. CI runs `factory-bench 300 5000 8` after the tests.
`FactoryBenchTests` checks the bench world produces and runs identically twice.

## Headless renders

`FT_SLICE_PARK_OUT=<path> ./OpenRCT2Tests --gtest_filter='FactoryTopologyTests.SaveSlice*'` writes a park with
every placeable kind running (belts, inserters, drill and furnace, undergrounds, a splitter, pipes, boiler, steam
engine, a powered assembler). Render it with
`build/openrct2-cli screenshot <park> out.png 520 320 <x> <y> <z> 0 <rotation>` (map coordinates of the centre).

## GUI checks under Xvfb

Windows, tools and drags can be driven without a desktop. Xvfb, ImageMagick `import` and libXtst are installed;
`scripts/factory-tour/xdrive.py` is a small xdotool stand-in (move, down, up, click, drag, key, type, sleep, shot).

```bash
S=/path/to/scratch                         # never the NTFS D drive
mkdir -p $S/gui-user && cp ~/.config/OpenRCT2/config.ini $S/gui-user/   # isolated user data, same game_path
Xvfb :77 -screen 0 1280x720x24 -nolisten tcp &   # pick a free display; another session may own :99
export DISPLAY=:77 XAUTHORITY=/dev/null
SDL_AUDIODRIVER=dummy build/openrct2 $S/slice.park --user-data-path=$S/gui-user &   # options after the park
scripts/factory-tour/xdrive.py sleep 12 move 1075 12 click sleep 1 shot $S/build-window.png
```

At 1280x720 the factory toolbar button is at (1075, 12). Reinstall data after changing strings or objects
(`DESTDIR=$FT_BUILD_DIR/install ninja -C build install`), otherwise the game shows "(undefined string)".
Checked this way so far: belt-line drag (ghost run, cost line, build on release), machine, chest, pipe and splitter
info windows (recipe and filter dropdowns), the power overview from a pole and from the build window.

## Text colour convention

Plain `drawText` with the default paint renders a pale grey. Like upstream, fork strings carry their colour:
labels start with `{WINDOW_COLOUR_2}`, values with `{BLACK}`; names go through `STR_BLACK_STRING`. Widget text
that is a `const char*` must be set with `Widget::setString(const utf8*)` so the `textIsString` flag follows.

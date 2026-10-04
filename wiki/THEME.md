# Theme bible

Factory Tour runs a factory as a theme park, then sends both through increasingly strange worlds. The engine
stays theme-agnostic: everything below lives in content packs (prototypes, terrain choices, presets, scenario
text) and could be replaced wholesale by a mod.

## Premise and tone

The player is an industrialist who discovered that the factory floor is the best attraction they own. Visitors pay
to watch ore become plates, plates become gears, and gears become the very rides they queue for. The tone is dry
and affectionate. Machines are named for what they do ("Stone furnace", "Bolt turret"), never for brands. The
strangeness grows quietly: first a desert outpost, then an ice moon, then a dimension where belts run fast,
machines dawdle and the storm never ends.

## Worlds

| World | Rules (Planet Params) | Character |
| --- | --- | --- |
| Home | Normal climate, 100% belts and machines | The park and its first factory. |
| Desert outpost | Always sunny | Sand, heat, room to sprawl; a lake for pumps. |
| Ice moon | Always snowing; machines at 80% | Slow, cold industry; long freight lines. |
| Weird dimension | Endless storm; belts 150%, machines 75%; Void crystal | Red ground, fast logistics, rare exotic ore. |

Worlds are linked by Portal Terminals (guests) and launch and landing pads (goods). Each launch costs a rocket
part, so space travel is a product, not a convenience.

## Production chain

- **Raw materials:** iron ore, copper ore, coal, stone, water, and Void crystal (weird dimension only).
- **Intermediates:** iron, copper and steel plates; gears and copper cable; electronic circuits; stone bricks.
- **Research:** research kits (gear + plate), then engineering kits (steel + gears).
- **Park goods:** track segments (rides are built from them), car bodies, souvenirs (factory models, gear
  keyrings) and Void lenses.
- **Space:** rocket parts, burnt by launch pads.

## Art direction

- Isometric RCT2 projection: 64x32 tiles, 4 rotations, frames for working machines.
- Every opaque pixel is an exact OpenRCT2 palette colour (see Hazards in `CLAUDE.md`).
- Machines are boxes with one readable detail each: a flame for furnaces, a gear for assemblers, a dome for labs.
  Ride vehicles use the primary remap ramp, so they take the ride's colours.
- The generated art in `data/factory/objects` is placeholder-quality but original. All of it comes from
  `scripts/factory-tour/gen-placeholder-art.py` and it is licensed CC-BY-SA 4.0 (`data/factory/LICENSE.md`).
- No Factorio or RCT2-derived art, names or data. Upstream path sprites, steam particles and sounds are drawn
  from the player's own RCT2 install at runtime, as upstream's own objects are.

## Writing voice

Scenario and object text is short, plain and slightly wry: "Have your guests tour the factory", "Shuttle cars that
run through the portal to the terminal in the next world". Typographic quotes and apostrophes are always used
(the changelog checker enforces them).

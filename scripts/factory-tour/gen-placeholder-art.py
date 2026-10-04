#!/usr/bin/env python3
"""Generates original placeholder sprites and object.json files for the Factory Tour content pack.

Run from the repo root:  python3 scripts/factory-tour/gen-placeholder-art.py
Output: data/factory/objects/<name>/object.json and images/*.png (loose object folders, see docs/notes).

Isometric conventions (zoom 0): a tile is a 64x32 diamond; screen = (y - x, (x + y) / 2 - z). The sprite origin
is the tile's (0, 0) corner at the element's base height, so a ground-level full-tile sprite uses x = -32, y = 0.
Map directions follow CoordsDirectionDelta: 0 = -x, 1 = +y, 2 = +x, 3 = -y. Their screen vectors are
0: (+32, -16) up-right, 1: (+32, +16) down-right, 2: (-32, +16) down-left, 3: (-32, -16) up-left.
Belt shapes: 0 straight (enters travelling in d), 1 turn left (enters travelling in (d + 1) & 3, since direction
(d + 1) & 3 is 90 degrees clockwise on screen), 2 turn right (enters travelling in (d + 3) & 3). Inserters pick up from the tile behind (-d) and drop ahead (+d).
"""
import json
import math
import os
from PIL import Image, ImageDraw

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "data", "factory", "objects")
PALETTE_HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "src", "openrct2", "drawing",
                              "ImageImporter.h")


def load_palette():
    """Reads StandardPalette from ImageImporter.h. Object PNGs are imported in "standard" mode, which only
    accepts exact palette colours (anything else becomes transparent), so every pixel must be snapped to it.
    Indices 10-229 are the plain colour ramps; 0-9 and 230+ are transparent, special or animated."""
    import re
    text = open(PALETTE_HEADER).read()
    start = text.index("StandardPalette")
    # Entries are BGRAColour: { blue, green, red, alpha }.
    entries = re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*255\s*\}", text[start:])
    colours = [(int(r), int(g), int(b)) for b, g, r in entries[:256]]
    return colours[10:230]


PALETTE = load_palette()
_snap_cache = {}


def snap(rgb):
    """Nearest usable palette colour for an (r, g, b) tuple."""
    if rgb in _snap_cache:
        return _snap_cache[rgb]
    best = min(PALETTE, key=lambda c: (c[0] - rgb[0]) ** 2 + (c[1] - rgb[1]) ** 2 + (c[2] - rgb[2]) ** 2)
    _snap_cache[rgb] = best
    return best


def snap_image(img):
    """Returns a copy with every opaque pixel snapped to the palette and every other pixel fully transparent."""
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    src = img.load()
    dst = out.load()
    for y in range(img.height):
        for x in range(img.width):
            r, g, b, a = src[x, y]
            if a >= 128:
                dst[x, y] = snap((r, g, b)) + (255,)
    return out
AUTHOR = "Factory Tour contributors"
SCREEN_DIR = {0: (32, -16), 1: (32, 16), 2: (-32, 16), 3: (-32, -16)}
TILE_CENTRE = (32, 16)  # inside a 64x32 image anchored at x=-32, y=0


def tile_polygon(cx, cy, scale=1.0):
    return [(cx, cy - 16 * scale), (cx + 32 * scale, cy), (cx, cy + 16 * scale), (cx - 32 * scale, cy)]


def lerp(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def edge_midpoint(centre, direction, sign):
    dx, dy = SCREEN_DIR[direction]
    return (centre[0] + sign * dx / 2, centre[1] + sign * dy / 2)


def write_object(name, kind, props, images, strings_name, extra=None):
    folder = os.path.join(ROOT, name)
    os.makedirs(os.path.join(folder, "images"), exist_ok=True)
    obj = {
        "id": f"factory-tour.factory_prototype.{name}",
        "authors": [AUTHOR],
        "version": "1.0",
        "sourceGame": "official",
        "objectType": "factory_prototype",
        "properties": {"kind": kind, **props},
        "images": images,
        "strings": {"name": {"en-GB": strings_name}},
    }
    if extra:
        obj.update(extra)
    with open(os.path.join(folder, "object.json"), "w") as f:
        json.dump(obj, f, indent=4)
        f.write("\n")
    return folder


def save(img, folder, filename):
    snap_image(img).save(os.path.join(folder, "images", filename))


def belt_strip_points(shape, d, frame, frames):
    """Returns the centre line of the belt as a list of screen points inside the 64x34 image (y shifted by 2)."""
    centre = (TILE_CENTRE[0], TILE_CENTRE[1] + 2)
    if shape == 0:
        incoming = d
    elif shape == 1:
        incoming = (d + 1) & 3
    else:
        incoming = (d + 3) & 3
    entry = edge_midpoint(centre, incoming, -1)
    exit_ = edge_midpoint(centre, d, +1)
    pts = []
    for i in range(0, 11):
        pts.append(lerp(entry, centre, i / 10))
    for i in range(1, 11):
        pts.append(lerp(centre, exit_, i / 10))
    return pts


def draw_belt(shape, d, frame, frames):
    img = Image.new("RGBA", (64, 34), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    # Tile base (thin dark plate) so the belt reads as part of the ground.
    draw.polygon(tile_polygon(32, 18), fill=(70, 70, 76, 255), outline=(40, 40, 44, 255))
    pts = belt_strip_points(shape, d, frame, frames)
    # Belt body: a wide polyline.
    for width, colour in ((11, (120, 110, 60, 255)), (9, (190, 170, 70, 255))):
        draw.line(pts, fill=colour, width=width, joint="curve")
    # Travelling chevrons: positions slide along the strip with the frame.
    n = len(pts) - 1
    for k in range(3):
        t = ((k / 3) + frame / frames) % 1.0
        idx = min(int(t * n), n - 1)
        p0, p1 = pts[idx], pts[idx + 1]
        vx, vy = p1[0] - p0[0], p1[1] - p0[1]
        length = math.hypot(vx, vy) or 1
        vx, vy = vx / length, vy / length
        px, py = -vy, vx
        tip = lerp(p0, p1, 0.5)
        tail = (tip[0] - vx * 3, tip[1] - vy * 3)
        draw.line([(tail[0] + px * 3, tail[1] + py * 3), tip, (tail[0] - px * 3, tail[1] - py * 3)],
                  fill=(60, 50, 20, 255), width=2)
    return img


def draw_chest():
    # A 32-wide box sitting on the tile centre, 20 px tall.
    h = 20
    img = Image.new("RGBA", (64, 32 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = 32, 16 + h
    top = [(cx, cy - 8 - h), (cx + 16, cy - h), (cx, cy + 8 - h), (cx - 16, cy - h)]
    left = [(cx - 16, cy - h), (cx, cy + 8 - h), (cx, cy + 8), (cx - 16, cy)]
    right = [(cx, cy + 8 - h), (cx + 16, cy - h), (cx + 16, cy), (cx, cy + 8)]
    draw.polygon(left, fill=(120, 80, 40, 255), outline=(60, 40, 20, 255))
    draw.polygon(right, fill=(150, 100, 50, 255), outline=(60, 40, 20, 255))
    draw.polygon(top, fill=(190, 130, 70, 255), outline=(60, 40, 20, 255))
    # Lid line and latch.
    draw.line([(cx - 16, cy - h + 4), (cx, cy + 8 - h + 4), (cx + 16, cy - h + 4)], fill=(60, 40, 20, 255), width=1)
    draw.rectangle([cx - 2, cy - h + 6, cx + 2, cy - h + 10], fill=(220, 200, 90, 255))
    return img, h


def draw_inserter(d, frame, frames):
    h = 28
    img = Image.new("RGBA", (64, 32 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = 32, 16 + h
    # Pedestal.
    draw.polygon(tile_polygon(cx, cy, 0.3), fill=(90, 90, 100, 255), outline=(40, 40, 50, 255))
    draw.rectangle([cx - 4, cy - 10, cx + 4, cy], fill=(110, 110, 120, 255), outline=(40, 40, 50, 255))
    # Arm swings from the pickup side (-d) over the top to the drop side (+d).
    t = frame / max(1, frames - 1)
    angle = math.pi * t  # 0 = pickup, pi = drop
    dx, dy = SCREEN_DIR[d]
    ux, uy = dx / 32, dy / 32  # unit-ish screen vector of +d
    reach = 22
    hx = cx - ux * reach * math.cos(angle)
    hy = (cy - 10) - uy * reach * math.cos(angle) - 14 * math.sin(angle)
    draw.line([(cx, cy - 10), (hx, hy)], fill=(200, 160, 40, 255), width=3)
    draw.ellipse([hx - 3, hy - 3, hx + 3, hy + 3], fill=(60, 60, 70, 255))
    return img, h


def draw_item_icon():
    img = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon([(12, 4), (21, 9), (12, 14), (3, 9)], fill=(170, 175, 185, 255), outline=(60, 60, 70, 255))
    draw.polygon([(3, 9), (12, 14), (12, 19), (3, 14)], fill=(120, 125, 135, 255), outline=(60, 60, 70, 255))
    draw.polygon([(12, 14), (21, 9), (21, 14), (12, 19)], fill=(140, 145, 155, 255), outline=(60, 60, 70, 255))
    return img


def draw_item_belt_sprite():
    img = Image.new("RGBA", (10, 8), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon([(5, 0), (9, 2), (5, 4), (1, 2)], fill=(170, 175, 185, 255), outline=(60, 60, 70, 255))
    draw.polygon([(1, 2), (5, 4), (5, 7), (1, 5)], fill=(120, 125, 135, 255))
    draw.polygon([(5, 4), (9, 2), (9, 5), (5, 7)], fill=(140, 145, 155, 255))
    return img


def draw_ore_overlay(colour, dark):
    """Flat diamond with speckles, drawn as a child of the surface."""
    img = Image.new("RGBA", (64, 32), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon(tile_polygon(32, 16), fill=colour + (255,))
    rnd = 7
    for i in range(40):
        rnd = (rnd * 1103515245 + 12345) & 0x7FFFFFFF
        x = (rnd >> 8) % 56 + 4
        rnd = (rnd * 1103515245 + 12345) & 0x7FFFFFFF
        y = (rnd >> 8) % 28 + 2
        # keep inside the diamond
        if abs(x - 32) / 32 + abs(y - 16) / 16 <= 0.9:
            draw.rectangle([x, y, x + 1, y + 1], fill=dark + (255,))
    return img


def draw_item_small(colour, dark):
    icon = Image.new("RGBA", (24, 24), (0, 0, 0, 0))
    d = ImageDraw.Draw(icon)
    d.ellipse([4, 5, 20, 19], fill=colour + (255,), outline=dark + (255,))
    d.ellipse([8, 8, 13, 12], fill=dark + (255,))
    belt = Image.new("RGBA", (10, 8), (0, 0, 0, 0))
    d = ImageDraw.Draw(belt)
    d.ellipse([1, 1, 8, 6], fill=colour + (255,), outline=dark + (255,))
    return icon, belt


def draw_machine_box(d, body, roof, outline, h=24, top_detail=None, side_detail=None):
    """A full-tile box of height h with a lid; returns image anchored at (-32, -h)."""
    img = Image.new("RGBA", (64, 32 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = 32, 16 + h
    top = [(cx, cy - 16 - h), (cx + 30, cy - 1 - h), (cx, cy + 14 - h), (cx - 30, cy - 1 - h)]
    left = [(cx - 30, cy - 1 - h), (cx, cy + 14 - h), (cx, cy + 14), (cx - 30, cy - 1)]
    right = [(cx, cy + 14 - h), (cx + 30, cy - 1 - h), (cx + 30, cy - 1), (cx, cy + 14)]
    draw.polygon(left, fill=body, outline=outline)
    draw.polygon(right, fill=tuple(min(255, c + 25) for c in body[:3]) + (255,), outline=outline)
    draw.polygon(top, fill=roof, outline=outline)
    if top_detail:
        top_detail(draw, cx, cy - h)
    if side_detail:
        side_detail(draw, cx, cy, h, d)
    return img, h


def draw_drill(d, frame, frames):
    def top_detail(draw, cx, cy):
        # Drill head: a hub with a rotating bar.
        draw.ellipse([cx - 7, cy - 4, cx + 7, cy + 3], fill=(80, 80, 90, 255), outline=(30, 30, 40, 255))
        ang = math.pi * frame / max(1, frames)
        dx, dy = math.cos(ang) * 9, math.sin(ang) * 4
        draw.line([(cx - dx, cy - dy), (cx + dx, cy + dy)], fill=(230, 200, 60, 255), width=2)

    def side_detail(draw, cx, cy, h, dd):
        # Output chute on the side facing direction d.
        sx, sy = SCREEN_DIR[dd]
        ex, ey = cx + sx * 0.6, cy - h // 2 + sy * 0.6
        draw.rectangle([ex - 4, ey - 3, ex + 4, ey + 3], fill=(60, 60, 70, 255), outline=(20, 20, 30, 255))

    return draw_machine_box(d, (120, 110, 90, 255), (150, 140, 110, 255), (40, 35, 25, 255), 20, top_detail, side_detail)


def draw_furnace(d, frame, frames):
    def top_detail(draw, cx, cy):
        draw.rectangle([cx + 6, cy - 22, cx + 12, cy - 4], fill=(90, 90, 90, 255), outline=(30, 30, 30, 255))
        if frame > 0:
            glow = (255, 160 + (frame % 3) * 30, 40, 255)
            draw.ellipse([cx - 6, cy - 2, cx + 6, cy + 6], fill=glow)
        else:
            draw.ellipse([cx - 6, cy - 2, cx + 6, cy + 6], fill=(60, 50, 50, 255))

    def side_detail(draw, cx, cy, h, dd):
        draw.rectangle([cx - 8, cy - 2, cx - 2, cy + 4], fill=(40, 30, 30, 255))

    return draw_machine_box(d, (130, 100, 90, 255), (160, 130, 110, 255), (50, 30, 30, 255), 26, top_detail, side_detail)


def write_machine(name, display, props, draw_fn, frames):
    images = []
    folder = write_object(name, "machine", {**props, "frames": frames, "rotations": 4}, [], display)
    for d in range(4):
        for f in range(frames):
            fname = f"m_d{d}_f{f}.png"
            img, h = draw_fn(d, f, frames)
            save(img, folder, fname)
            images.append({"path": f"images/{fname}", "x": -32, "y": -h})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def write_ore_and_item(ore_name, item_name, display_ore, display_item, colour, dark, fuel_ticks=0):
    icon, belt = draw_item_small(colour, dark)
    item_props = {"stackSize": 50}
    if fuel_ticks:
        item_props["fuelTicks"] = fuel_ticks
    folder = write_object(item_name, "item", item_props,
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          display_item)
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")
    folder = write_object(ore_name, "ore",
                          {"item": f"factory-tour.factory_prototype.{item_name}", "defaultAmount": 500},
                          [{"path": "images/overlay.png", "x": -32, "y": 0}, {"path": "images/icon.png", "x": -12, "y": -12}],
                          display_ore)
    save(draw_ore_overlay(colour, dark), folder, "overlay.png")
    save(icon, folder, "icon.png")


def main():
    os.makedirs(ROOT, exist_ok=True)

    # Ores and their items.
    write_ore_and_item("iron_ore_patch", "iron_ore", "Iron ore", "Iron ore", (110, 120, 140), (60, 70, 90))
    write_ore_and_item("coal_patch", "coal", "Coal", "Coal", (50, 50, 55), (20, 20, 25), fuel_ticks=1600)

    # Recipes.
    write_object("iron_plate_smelting", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_ore", "count": 1}],
        "results": [{"item": "factory-tour.factory_prototype.iron_plate", "count": 1}],
        "timeTicks": 128, "category": "smelting"}, [], "Iron plate")

    # Machines (1x1 in M2; multi-tile footprints come later).
    write_machine("burner_drill", "Burner mining drill", {
        "machineKind": "drill", "energy": "burner", "speedQ8": 256, "miningRadius": 1, "miningTimeTicks": 100,
        "inputSlots": 0, "outputSlots": 1, "price": 80, "removalPrice": -60, "clearance": 5}, draw_drill, 4)
    write_machine("stone_furnace", "Stone furnace", {
        "machineKind": "furnace", "energy": "burner", "speedQ8": 256, "recipeCategories": ["smelting"],
        "inputSlots": 1, "outputSlots": 1, "price": 60, "removalPrice": -45, "clearance": 7}, draw_furnace, 4)

    # Item: iron plate.
    folder = write_object(
        "iron_plate", "item", {"stackSize": 100},
        [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
        "Iron plate")
    save(draw_item_icon(), folder, "icon.png")
    save(draw_item_belt_sprite(), folder, "belt.png")

    # Belt: 3 shapes x 4 directions x 8 frames.
    frames = 8
    images = []
    folder = write_object(
        "belt_basic", "belt", {"speed": 12, "frames": frames, "price": 20, "removalPrice": -15, "clearance": 2},
        [], "Basic transport belt")
    for shape in range(3):
        for d in range(4):
            for f in range(frames):
                name = f"belt_s{shape}_d{d}_f{f}.png"
                save(draw_belt(shape, d, f, frames), folder, name)
                images.append({"path": f"images/{name}", "x": -32, "y": -2})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")

    # Inserter: 4 directions x 8 frames.
    frames = 8
    images = []
    folder = write_object(
        "inserter_basic", "inserter",
        {"frames": frames, "swingTicks": 24, "reach": 1, "price": 40, "removalPrice": -30, "clearance": 8},
        [], "Basic inserter")
    for d in range(4):
        for f in range(frames):
            name = f"inserter_d{d}_f{f}.png"
            img, h = draw_inserter(d, f, frames)
            save(img, folder, name)
            images.append({"path": f"images/{name}", "x": -32, "y": -h})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")

    # Container: wooden chest, one rotation.
    img, h = draw_chest()
    folder = write_object(
        "chest_wooden", "container",
        {"slots": 16, "rotations": 1, "price": 30, "removalPrice": -20, "clearance": 6},
        [{"path": "images/chest.png", "x": -32, "y": -h}], "Wooden chest")
    save(img, folder, "chest.png")
    print("wrote content pack to", os.path.relpath(ROOT))


if __name__ == "__main__":
    main()

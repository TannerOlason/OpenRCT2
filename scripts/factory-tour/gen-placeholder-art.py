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


def draw_warehouse(body=(120, 130, 110, 255), roof=(150, 160, 140, 255), outline=(50, 55, 45, 255)):
    """A full-tile depot shed with a roller door: where the factory's output joins the park-wide warehouse."""
    def top_detail(draw, cx, cy):
        draw.polygon([(cx - 14, cy - 2), (cx, cy - 9), (cx + 14, cy - 2), (cx, cy + 5)], fill=(150, 150, 160, 255),
                     outline=(70, 70, 80, 255))

    def side_detail(draw, cx, cy, h, dd):
        for k in range(4):
            draw.line([(cx - 24, cy - 14 + k * 3), (cx - 8, cy - 6 + k * 3)], fill=(90, 90, 100, 255))

    img, h = draw_machine_box(0, body, roof, outline, 22, top_detail, side_detail)
    return img, h


def draw_inserter(d, frame, frames, arm=(200, 160, 40, 255)):
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
    draw.line([(cx, cy - 10), (hx, hy)], fill=arm, width=3)
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


def draw_generator(d, frame, frames):
    def top_detail(draw, cx, cy):
        # Flywheel: spokes rotate while working.
        draw.ellipse([cx - 10, cy - 6, cx + 10, cy + 4], fill=(70, 70, 80, 255), outline=(30, 30, 40, 255))
        for k in range(3):
            ang = math.pi * (frame / max(1, frames) + k / 3)
            draw.line([(cx - math.cos(ang) * 9, cy - 1 - math.sin(ang) * 4), (cx + math.cos(ang) * 9, cy - 1 + math.sin(ang) * 4)],
                      fill=(220, 190, 70, 255), width=2)

    def side_detail(draw, cx, cy, h, dd):
        draw.rectangle([cx - 24, cy - 4, cx - 16, cy + 2], fill=(40, 30, 30, 255))  # firebox door

    return draw_machine_box(d, (110, 60, 50, 255), (140, 90, 70, 255), (40, 20, 20, 255), 24, top_detail, side_detail)


def draw_assembler(d, frame, frames):
    def top_detail(draw, cx, cy):
        # A gear outline that turns while working.
        r = 9
        for k in range(6):
            ang = 2 * math.pi * (k / 6 + frame / (6 * max(1, frames)))
            draw.line([(cx, cy), (cx + math.cos(ang) * r, cy + math.sin(ang) * r * 0.5)], fill=(90, 160, 200, 255), width=2)
        draw.ellipse([cx - 4, cy - 2, cx + 4, cy + 2], fill=(60, 110, 150, 255))

    def side_detail(draw, cx, cy, h, dd):
        draw.rectangle([cx + 4, cy - 10, cx + 22, cy - 2], fill=(60, 90, 120, 255))  # window

    return draw_machine_box(d, (90, 110, 130, 255), (120, 140, 160, 255), (30, 40, 50, 255), 24, top_detail, side_detail)


def draw_pole():
    h = 40
    img = Image.new("RGBA", (64, 32 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = 32, 16 + h
    draw.polygon(tile_polygon(cx, cy, 0.18), fill=(90, 80, 70, 255), outline=(40, 30, 20, 255))
    draw.rectangle([cx - 2, cy - h + 4, cx + 2, cy], fill=(120, 90, 60, 255), outline=(50, 35, 20, 255))
    draw.rectangle([cx - 10, cy - h + 6, cx + 10, cy - h + 9], fill=(120, 90, 60, 255), outline=(50, 35, 20, 255))
    for x in (cx - 8, cx + 8):
        draw.rectangle([x - 1, cy - h + 3, x + 1, cy - h + 6], fill=(200, 200, 210, 255))
    return img


def draw_underground(d, is_exit):
    """A belt stub on the tile with a hood where it enters (entrance) or leaves (exit) the ground."""
    img = Image.new("RGBA", (64, 46), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cy = 16 + 14
    draw.polygon(tile_polygon(32, cy), fill=(70, 70, 76, 255), outline=(40, 40, 44, 255))
    centre = (32, cy)
    # Entrance: belt visible from the entry edge to the centre, hood at the centre. Exit: hood at the centre,
    # belt from the centre to the exit edge.
    a = edge_midpoint(centre, d, -1) if not is_exit else centre
    b = centre if not is_exit else edge_midpoint(centre, d, +1)
    for width, colour in ((11, (120, 110, 60, 255)), (9, (190, 170, 70, 255))):
        draw.line([a, b], fill=colour, width=width)
    hx, hy = centre
    draw.polygon([(hx - 12, hy - 2), (hx, hy - 20), (hx + 12, hy - 2), (hx, hy + 6)], fill=(90, 90, 110, 255),
                 outline=(30, 30, 40, 255))
    sx, sy = SCREEN_DIR[d]
    draw.line([(hx, hy - 10), (hx + sx * 0.3, hy - 10 + sy * 0.3)], fill=(230, 200, 60, 255), width=2)
    return img, 14


def draw_splitter(d, side):
    """One tile of a 1x2 splitter: a belt strip with a raised bar across the middle."""
    img = Image.new("RGBA", (64, 46), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cy = 16 + 14
    draw.polygon(tile_polygon(32, cy), fill=(70, 70, 76, 255), outline=(40, 40, 44, 255))
    centre = (32, cy)
    a = edge_midpoint(centre, d, -1)
    b = edge_midpoint(centre, d, +1)
    for width, colour in ((11, (120, 110, 60, 255)), (9, (190, 170, 70, 255))):
        draw.line([a, b], fill=colour, width=width)
    # Bar perpendicular to travel, offset toward the shared edge so the two tiles read as one machine.
    px, py = SCREEN_DIR[(d + 1) & 3]
    bx, by = centre[0] + (px * 0.25 if side == 0 else -px * 0.25), centre[1] + (py * 0.25 if side == 0 else -py * 0.25)
    draw.line([(bx - px * 0.5, by - py * 0.5 - 8), (bx + px * 0.5, by + py * 0.5 - 8)], fill=(60, 110, 150, 255), width=6)
    draw.line([(bx - px * 0.5, by - py * 0.5 - 8), (bx - px * 0.5, by - py * 0.5)], fill=(40, 70, 100, 255), width=2)
    draw.line([(bx + px * 0.5, by + py * 0.5 - 8), (bx + px * 0.5, by + py * 0.5)], fill=(40, 70, 100, 255), width=2)
    return img, 14


def draw_pipe(mask):
    """A pipe hub at the tile centre with an arm towards each view direction in mask (bit d = SCREEN_DIR[d])."""
    h = 8
    img = Image.new("RGBA", (64, 32 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    centre = (32, 16 + h - 6)
    body, rim = (120, 130, 140, 255), (50, 55, 60, 255)
    # Arms first (back to front so the near ones overlap), then the hub.
    for d in sorted(range(4), key=lambda dd: SCREEN_DIR[dd][1]):
        if mask & (1 << d):
            end = edge_midpoint(centre, d, +1)
            draw.line([centre, end], fill=rim, width=9)
            draw.line([centre, end], fill=body, width=6)
            draw.line([(centre[0], centre[1] - 2), (end[0], end[1] - 2)], fill=(170, 180, 190, 255), width=1)
    draw.ellipse([centre[0] - 6, centre[1] - 4, centre[0] + 6, centre[1] + 4], fill=body, outline=rim)
    return img, h


def write_pipe():
    images = []
    folder = write_object("pipe_basic", "pipe", {"capacity": 1000, "price": 5, "removalPrice": -3, "clearance": 3}, [],
                          "Pipe")
    for mask in range(16):
        fname = f"pipe_{mask:02d}.png"
        img, h = draw_pipe(mask)
        save(img, folder, fname)
        images.append({"path": f"images/{fname}", "x": -32, "y": -h})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def draw_pump(d, frame, frames):
    def top_detail(draw, cx, cy):
        # A pump wheel that turns while working.
        draw.ellipse([cx - 6, cy - 4, cx + 6, cy + 3], fill=(60, 90, 120, 255), outline=(20, 30, 40, 255))
        ang = math.pi * frame / max(1, frames)
        draw.line([(cx - math.cos(ang) * 6, cy - math.sin(ang) * 3), (cx + math.cos(ang) * 6, cy + math.sin(ang) * 3)],
                  fill=(200, 220, 240, 255), width=2)

    def side_detail(draw, cx, cy, h, dd):
        # Outlet on the front, intake grille on the back (which faces the water).
        sx, sy = SCREEN_DIR[dd]
        ex, ey = cx + sx * 0.6, cy - h // 2 + sy * 0.6
        draw.rectangle([ex - 3, ey - 3, ex + 3, ey + 3], fill=(120, 130, 140, 255), outline=(40, 40, 50, 255))

    return draw_machine_box(d, (70, 110, 120, 255), (100, 140, 150, 255), (20, 40, 45, 255), 14, top_detail, side_detail)


def draw_boiler(d, frame, frames):
    def top_detail(draw, cx, cy):
        draw.rectangle([cx - 4, cy - 20, cx + 2, cy - 2], fill=(80, 70, 70, 255), outline=(30, 25, 25, 255))
        if frame > 0:
            for k in range(2):
                r = 3 + k + frame % 2
                draw.ellipse([cx - 1 - r, cy - 26 - 5 * k - r, cx - 1 + r, cy - 26 - 5 * k + r], fill=(200, 200, 205, 255))

    def side_detail(draw, cx, cy, h, dd):
        glow = (255, 150, 40, 255) if frame > 0 else (50, 40, 40, 255)
        draw.rectangle([cx - 20, cy - 6, cx - 12, cy], fill=glow)
        sx, sy = SCREEN_DIR[dd]
        ex, ey = cx + sx * 0.6, cy - h // 2 + sy * 0.6
        draw.rectangle([ex - 3, ey - 3, ex + 3, ey + 3], fill=(200, 200, 205, 255), outline=(40, 40, 50, 255))

    return draw_machine_box(d, (140, 80, 60, 255), (170, 110, 90, 255), (50, 25, 20, 255), 22, top_detail, side_detail)


def draw_steam_engine(d, frame, frames):
    def top_detail(draw, cx, cy):
        # A piston rod sliding along the facing axis.
        sx, sy = SCREEN_DIR[d]
        t = (frame % max(1, frames)) / max(1, frames)
        off = math.sin(2 * math.pi * t) * 0.15
        a = (cx - sx * 0.3, cy - sy * 0.3)
        b = (cx + sx * (0.1 + off), cy + sy * (0.1 + off))
        draw.line([a, b], fill=(220, 200, 90, 255), width=3)
        draw.ellipse([b[0] - 4, b[1] - 3, b[0] + 4, b[1] + 3], fill=(90, 90, 100, 255), outline=(30, 30, 40, 255))

    def side_detail(draw, cx, cy, h, dd):
        draw.rectangle([cx + 6, cy - 10, cx + 20, cy - 4], fill=(150, 150, 160, 255))

    return draw_machine_box(d, (110, 110, 120, 255), (140, 140, 150, 255), (35, 35, 45, 255), 18, top_detail, side_detail)


def write_fluid(name, display, colour, dark):
    icon, belt = draw_item_small(colour, dark)
    folder = write_object(name, "item", {"stackSize": 1, "fluid": True},
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          display)
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")


def write_underground_and_splitter():
    images = []
    folder = write_object("underground_belt_basic", "underground_belt",
                          {"speed": 12, "reach": 4, "price": 50, "removalPrice": -35, "clearance": 3}, [],
                          "Basic underground belt")
    for is_exit in (False, True):
        for d in range(4):
            fname = f"u_{'exit' if is_exit else 'entry'}_d{d}.png"
            img, h = draw_underground(d, is_exit)
            save(img, folder, fname)
            images.append({"path": f"images/{fname}", "x": -32, "y": -h})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")

    images = []
    folder = write_object("splitter_basic", "splitter", {"speed": 12, "price": 90, "removalPrice": -70, "clearance": 3},
                          [], "Basic splitter")
    for side in range(2):
        for d in range(4):
            fname = f"s_{side}_d{d}.png"
            img, h = draw_splitter(d, side)
            save(img, folder, fname)
            images.append({"path": f"images/{fname}", "x": -32, "y": -h})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def write_machine(name, display, props, draw_fn, frames):
    images = []
    props = {"health": 300, **props}  # destructible by Threats (the combat stub)
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


def draw_machine_slice(n, vx, vy, d, frame, frames, body, roof, outline, h, centre_detail=None):
    """One tile of an n x n machine box seen at a view rotation: (vx, vy) is the tile's place in the view square
    (vx along view +x = screen down-left, vy along view +y = screen down-right). Lids join seamlessly; walls and
    outlines appear only on the outer edges. Returns the image (anchored at -32, -h) and h."""
    img = Image.new("RGBA", (64, 33 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    top, right, bottom, left = (32, 0), (63, 16), (32, 32), (0, 16)
    if vx == n - 1:  # down-left face (view +x)
        draw.polygon([left, bottom, (32, 32 + h), (0, 16 + h)], fill=body)
        draw.line([left, (0, 16 + h), (32, 32 + h), bottom], fill=outline)
    if vy == n - 1:  # down-right face (view +y)
        lighter = tuple(min(255, c + 25) for c in body[:3]) + (255,)
        draw.polygon([bottom, right, (63, 16 + h), (32, 32 + h)], fill=lighter)
        draw.line([bottom, (32, 32 + h), (63, 16 + h), right], fill=outline)
    draw.polygon([top, right, bottom, left], fill=roof)
    # Lid outline on the footprint's outer edges only: up-right faces view -x, up-left view -y.
    if vx == 0:
        draw.line([top, right], fill=outline)
    if vy == 0:
        draw.line([left, top], fill=outline)
    if vx == n - 1:
        draw.line([bottom, left], fill=outline)
    if vy == n - 1:
        draw.line([right, bottom], fill=outline)
    c = n // 2
    if vx == c and vy == c:
        if centre_detail:
            centre_detail(draw, 32, 16, frame, frames)
        # An arrow on the lid towards the facing direction (d is the view direction).
        sx, sy = SCREEN_DIR[d]
        draw.line([(32, 16), (32 + sx * 0.4, 16 + sy * 0.4)], fill=(230, 200, 60, 255), width=2)
    # Output chute on the visible face that the machine faces.
    if (d == 2 and vx == n - 1 and vy == c) or (d == 1 and vy == n - 1 and vx == c):
        sx, sy = SCREEN_DIR[d]
        ex, ey = 32 + sx * 0.5, 16 + h // 2 + sy * 0.5
        draw.rectangle([ex - 4, ey - 3, ex + 4, ey + 3], fill=(60, 60, 70, 255), outline=(20, 20, 30, 255))
    return img, h


def compose_preview(n, slices, h):
    """The whole machine from its rotation-0 slices, scaled down to fit a 64-pixel palette button."""
    width, height = 64 * n, 32 * n + h + 1
    canvas = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    # Draw back to front: tiles with smaller vx + vy are further away.
    for vx, vy in sorted(((x, y) for x in range(n) for y in range(n)), key=lambda t: t[0] + t[1]):
        sx = (vy - vx) * 32 + (n - 1) * 32
        sy = (vx + vy) * 16
        tile = slices[(vx, vy)]
        canvas.alpha_composite(tile, (sx, sy))
    scale = min(1.0, 60 / width, 60 / height)
    return canvas.resize((max(1, int(width * scale)), max(1, int(height * scale))), Image.NEAREST)


def write_multitile_machine(name, display, props, n, h, colours, centre_detail, frames):
    body, roof, outline = colours
    images = []
    props = {"health": 300 * n, **props}
    folder = write_object(name, "machine", {**props, "frames": frames, "rotations": 4, "size": n}, [], display)
    preview_slices = {}
    for d in range(4):
        for f in range(frames):
            for vy in range(n):
                for vx in range(n):
                    fname = f"m_d{d}_f{f}_{vy}{vx}.png"
                    img, hh = draw_machine_slice(n, vx, vy, d, f, frames, body, roof, outline, h, centre_detail)
                    save(img, folder, fname)
                    images.append({"path": f"images/{fname}", "x": -32, "y": -hh})
                    if d == 2 and f == 0:
                        preview_slices[(vx, vy)] = img
    preview = compose_preview(n, preview_slices, h)
    save(preview, folder, "preview.png")
    images.append({"path": "images/preview.png", "x": -(preview.width // 2), "y": -(preview.height // 2)})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def drill_head(draw, cx, cy, frame, frames):
    draw.ellipse([cx - 12, cy - 7, cx + 12, cy + 6], fill=(80, 80, 90, 255), outline=(30, 30, 40, 255))
    ang = math.pi * frame / max(1, frames)
    dx, dy = math.cos(ang) * 14, math.sin(ang) * 6
    draw.line([(cx - dx, cy - dy), (cx + dx, cy + dy)], fill=(230, 200, 60, 255), width=3)


def palette_rgb(index):
    """Exact RGB of a StandardPalette index (for remap ramps, which snap() never picks)."""
    import re
    text = open(PALETTE_HEADER).read()
    start = text.index("StandardPalette")
    entries = re.findall(r"\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}", text[start:])
    b, g, r, a = entries[index]
    return (int(r), int(g), int(b), 255)


def project(x, y, z, ox, oy):
    """Map units (32 per tile) to image pixels around the anchor (ox, oy): screen = (y - x, (x + y) / 2 - z)."""
    return (ox + (y - x), oy + (x + y) / 2 - z)


def draw_tram_frame(i, riders):
    """Frame i of 32: the car heads along (-cos a, sin a) with a = i * 11.25 degrees (0 = -x, 8 = +y, 16 = +x).
    The car body uses the primary remap ramp (palette 245-254) so it takes the ride's main colour; riders are drawn on
    their own transparent image in the same frame, torsos in the same remap ramp (the guests' shirt colours)."""
    w, h, ox, oy = 48, 40, 24, 26
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    a = 2 * math.pi * i / 32
    fx, fy = -math.cos(a), math.sin(a)
    rx, ry = fy, -fx
    half_l, half_w = 11, 6

    def corner(sl, sw, z):
        return project(fx * sl * half_l + rx * sw * half_w, fy * sl * half_l + ry * sw * half_w, z, ox, oy)

    if not riders:
        dark, side, light, top = palette_rgb(246), palette_rgb(248), palette_rgb(250), palette_rgb(252)
        black = snap((30, 30, 30)) + (255,)
        # Wheels at the four corners, then the visible sides (outward normal towards +x+y faces the viewer), then
        # the open top with a darker floor.
        for sl in (-0.75, 0.75):
            for sw in (-1, 1):
                cx, cy = corner(sl, sw, 1)
                draw.ellipse([cx - 2, cy - 1.5, cx + 2, cy + 1.5], fill=black)
        faces = [((1, -1), (1, 1), (fx, fy)), ((-1, 1), (-1, -1), (-fx, -fy)),
                 ((1, 1), (-1, 1), (rx, ry)), ((-1, -1), (1, -1), (-rx, -ry))]
        for (a0, a1), normal in [((f[0], f[1]), f[2]) for f in faces]:
            if normal[0] + normal[1] <= 0:
                continue
            pts = [corner(a0[0], a0[1], 3), corner(a1[0], a1[1], 3), corner(a1[0], a1[1], 10), corner(a0[0], a0[1], 10)]
            draw.polygon(pts, fill=side if abs(normal[0]) > abs(normal[1]) else light, outline=dark)
        rim = [corner(1, 1, 10), corner(1, -1, 10), corner(-1, -1, 10), corner(-1, 1, 10)]
        draw.polygon(rim, fill=top, outline=dark)
        floor = [corner(0.8, 0.7, 10), corner(0.8, -0.7, 10), corner(-0.8, -0.7, 10), corner(-0.8, 0.7, 10)]
        draw.polygon(floor, fill=dark)
        # A headlamp marks the front.
        hx, hy = corner(1.05, 0, 7)
        draw.ellipse([hx - 1.5, hy - 1.5, hx + 1.5, hy + 1.5], fill=snap((255, 235, 120)) + (255,))
    else:
        skin = snap((224, 172, 132)) + (255,)
        shirt = palette_rgb(250)
        for sw in (-0.45, 0.45):
            cx, cy = corner(-0.1, sw, 13)
            draw.ellipse([cx - 2.5, cy - 2, cx + 2.5, cy + 3], fill=shirt)
            hx, hy = corner(-0.1, sw, 18)
            draw.ellipse([hx - 2, hy - 2, hx + 2, hy + 2], fill=skin)
    return img, ox, oy


def draw_wagon_frame(i):
    """Frame i of 32 of a freight wagon (same projection as the tour tram): a low remap-coloured flat car stacked with
    crates, a lamp at the front."""
    w, h, ox, oy = 48, 40, 24, 26
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    a = 2 * math.pi * i / 32
    fx, fy = -math.cos(a), math.sin(a)
    rx, ry = fy, -fx
    half_l, half_w = 12, 6

    def corner(sl, sw, z):
        return project(fx * sl * half_l + rx * sw * half_w, fy * sl * half_l + ry * sw * half_w, z, ox, oy)

    dark, side, light, top = palette_rgb(246), palette_rgb(248), palette_rgb(250), palette_rgb(252)
    black = snap((30, 30, 30)) + (255,)
    crate, crate_dark = snap((170, 120, 60)) + (255,), snap((100, 65, 30)) + (255,)
    for sl in (-0.75, 0.75):
        for sw in (-1, 1):
            cx, cy = corner(sl, sw, 1)
            draw.ellipse([cx - 2, cy - 1.5, cx + 2, cy + 1.5], fill=black)
    faces = [((1, -1), (1, 1), (fx, fy)), ((-1, 1), (-1, -1), (-fx, -fy)),
             ((1, 1), (-1, 1), (rx, ry)), ((-1, -1), (1, -1), (-rx, -ry))]
    for (a0, a1), normal in [((f[0], f[1]), f[2]) for f in faces]:
        if normal[0] + normal[1] <= 0:
            continue
        pts = [corner(a0[0], a0[1], 3), corner(a1[0], a1[1], 3), corner(a1[0], a1[1], 6), corner(a0[0], a0[1], 6)]
        draw.polygon(pts, fill=side if abs(normal[0]) > abs(normal[1]) else light, outline=dark)
    deck = [corner(1, 1, 6), corner(1, -1, 6), corner(-1, -1, 6), corner(-1, 1, 6)]
    draw.polygon(deck, fill=top, outline=dark)
    # Two crates on the deck, back one first.
    for sl in sorted((-0.45, 0.45), key=lambda v: -(fx * v + fy * v)):
        pts_top = [corner(sl + 0.35, 0.7, 13), corner(sl + 0.35, -0.7, 13), corner(sl - 0.35, -0.7, 13), corner(sl - 0.35, 0.7, 13)]
        base = [corner(sl + 0.35, 0.7, 6), corner(sl - 0.35, 0.7, 6), corner(sl - 0.35, 0.7, 13), corner(sl + 0.35, 0.7, 13)]
        draw.polygon(base, fill=crate_dark)
        draw.polygon(pts_top, fill=crate, outline=crate_dark)
    hx, hy = corner(1.05, 0, 5)
    draw.ellipse([hx - 1.5, hy - 1.5, hx + 1.5, hy + 1.5], fill=snap((255, 235, 120)) + (255,))
    return img, ox, oy


def write_freight():
    """The freight railway's wagons, its loader and unloader containers (ADR 0014)."""
    folder = os.path.join(ROOT, "freight_train")
    os.makedirs(os.path.join(folder, "images"), exist_ok=True)
    images = []
    preview = Image.new("RGBA", (112, 64), (0, 0, 0, 0))
    for k, x in enumerate((20, 52, 84)):
        frame, ox, oy = draw_wagon_frame(12)
        preview.alpha_composite(frame, (x - ox, 34 - oy + k * 4))
    save(preview, folder, "preview.png")
    for _ in range(3):
        images.append({"path": "images/preview.png", "x": 0, "y": 0})
    for i in range(32):
        img, ox, oy = draw_wagon_frame(i)
        name = f"wagon_{i:02d}.png"
        img.save(os.path.join(folder, "images", name))  # exact palette colours already: snapping would drop remaps
        images.append({"path": f"images/{name}", "x": -ox, "y": -oy})
    obj = {
        "id": "factory-tour.ride.freight_train",
        "authors": [AUTHOR],
        "version": "1.0",
        "sourceGame": "official",
        "objectType": "ride",
        "properties": {
            "type": "freight_railway",
            "category": "transport",
            "noCollisionCrashes": True,
            "minCarsPerTrain": 2,
            "maxCarsPerTrain": 8,
            "numEmptyCars": 0,
            "tabCar": 0,
            "carColours": [[["dark_brown", "grey", "black"]], [["dark_green", "grey", "black"]]],
            "buildMenuPriority": 1,
            "cars": [{
                "rotationFrameMask": 31,
                "spacing": 130000,
                "mass": 600,
                "numSeats": 0,
                "numSeatRows": 0,
                "poweredAcceleration": 50,
                "poweredMaxSpeed": 8,
                "drawOrder": 9,
                "spriteGroups": {"slopeFlat": 32},
                "isPowered": True,
            }],
        },
        "images": images,
        "strings": {
            "name": {"en-GB": "Freight train"},
            "description": {"en-GB": "Flat wagons that carry factory goods between freight loaders and unloaders"},
            "capacity": {"en-GB": "200 items per wagon"},
        },
    }
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")

    for name, display, colours, key in (
            ("freight_loader", "Freight loader", ((90, 110, 150, 255), (120, 140, 180, 255), (35, 45, 70, 255)), "freightLoader"),
            ("freight_unloader", "Freight unloader", ((150, 100, 90, 255), (180, 130, 120, 255), (70, 40, 35, 255)), "freightUnloader")):
        img, h = draw_warehouse(*colours)
        folder = write_object(
            name, "container",
            {"slots": 8, "rotations": 1, key: True, "price": 180, "removalPrice": -130, "clearance": 7},
            [{"path": "images/depot.png", "x": -32, "y": -h}], display)
        save(img, folder, "depot.png")


def write_interworld():
    """Launch and landing pads: item transfer between worlds (ADR 0015)."""
    for name, display, colours, key in (
            ("launch_pad", "Launch pad", ((170, 170, 180, 255), (210, 90, 60, 255), (60, 60, 70, 255)), "launchPad"),
            ("landing_pad", "Landing pad", ((170, 170, 180, 255), (80, 170, 90, 255), (60, 60, 70, 255)), "landingPad")):
        img, h = draw_warehouse(*colours)
        folder = write_object(
            name, "container",
            {"slots": 8, "rotations": 1, key: True, "price": 400, "removalPrice": -300, "clearance": 7},
            [{"path": "images/depot.png", "x": -32, "y": -h}], display)
        save(img, folder, "depot.png")


def write_tour_tram():
    folder = os.path.join(ROOT, "tour_tram")
    os.makedirs(os.path.join(folder, "images"), exist_ok=True)
    images = []
    # Three ride-type preview slots (only the first is shown), then 32 car frames, then 32 rider frames.
    preview = Image.new("RGBA", (112, 64), (0, 0, 0, 0))
    for k, x in enumerate((20, 52, 84)):
        frame, ox, oy = draw_tram_frame(12, False)
        preview.alpha_composite(frame, (x - ox, 34 - oy + k * 4))
        rider, _, _ = draw_tram_frame(12, True)
        preview.alpha_composite(rider, (x - ox, 34 - oy + k * 4))
    save(preview, folder, "preview.png")
    for _ in range(3):
        images.append({"path": "images/preview.png", "x": 0, "y": 0})
    for riders in (False, True):
        for i in range(32):
            img, ox, oy = draw_tram_frame(i, riders)
            name = f"{'rider' if riders else 'car'}_{i:02d}.png"
            img.save(os.path.join(folder, "images", name))  # exact palette colours already: snapping would drop remaps
            images.append({"path": f"images/{name}", "x": -ox, "y": -oy})
    obj = {
        "id": "factory-tour.ride.tour_tram",
        "authors": [AUTHOR],
        "version": "1.0",
        "sourceGame": "official",
        "objectType": "ride",
        "properties": {
            "type": "factory_tour",
            "category": "gentle",
            "noCollisionCrashes": True,
            "minCarsPerTrain": 2,
            "maxCarsPerTrain": 4,
            "numEmptyCars": 0,
            "tabCar": 0,
            "carColours": [[["dark_green", "grey", "black"]], [["bright_yellow", "dark_brown", "black"]]],
            "buildMenuPriority": 1,
            "cars": [{
                "rotationFrameMask": 31,
                "spacing": 120000,
                "mass": 300,
                "numSeats": 2,
                "numSeatRows": 1,
                "poweredAcceleration": 60,
                "poweredMaxSpeed": 5,
                "drawOrder": 9,
                "spriteGroups": {"slopeFlat": 32},
                "isPowered": True,
                "loadingPositions": [3, -3],
            }],
        },
        "images": images,
        "strings": {
            "name": {"en-GB": "Factory tour tram"},
            "description": {"en-GB": "Open tram cars that carry visitors slowly past the factory floor"},
            "capacity": {"en-GB": "2 passengers per car"},
        },
    }
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def write_portal_shuttle():
    """The Portal Terminal's shuttle (ADR 0016): the tour tram's cars in portal colours."""
    import shutil
    src = os.path.join(ROOT, "tour_tram")
    folder = os.path.join(ROOT, "portal_shuttle")
    if os.path.isdir(os.path.join(folder, "images")):
        shutil.rmtree(os.path.join(folder, "images"))
    shutil.copytree(os.path.join(src, "images"), os.path.join(folder, "images"))
    with open(os.path.join(src, "object.json")) as fh:
        obj = json.load(fh)
    obj["id"] = "factory-tour.ride.portal_shuttle"
    obj["properties"]["type"] = "portal_terminal"
    obj["properties"]["category"] = "transport"
    obj["properties"]["carColours"] = [[["light_purple", "white", "black"]], [["dark_water", "white", "black"]]]
    obj["strings"] = {
        "name": {"en-GB": "Portal shuttle"},
        "description": {"en-GB": "Shuttle cars that run through the portal to the terminal in the next world"},
        "capacity": {"en-GB": "2 passengers per car"},
    }
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def draw_gift_shop(d):
    """A kiosk with a striped awning (primary remap ramp, so it takes the shop's colour) facing view direction d."""
    h = 30
    img = Image.new("RGBA", (64, 33 + h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    cx, cy = 32, 16 + h
    wall, wall_light, outline = snap((200, 190, 170)) + (255,), snap((225, 215, 195)) + (255,), snap((80, 70, 60)) + (255,)
    left = [(cx - 24, cy - 12 - 18), (cx, cy - 18), (cx, cy), (cx - 24, cy - 12)]
    right = [(cx, cy - 18), (cx + 24, cy - 12 - 18), (cx + 24, cy - 12), (cx, cy)]
    draw.polygon(left, fill=wall, outline=outline)
    draw.polygon(right, fill=wall_light, outline=outline)
    # Counter window on the face towards d (left face = view +x, right face = view +y).
    if d in (1, 2):
        face_x = -12 if d == 2 else 12
        draw.polygon([(cx + face_x - 7, cy - 16), (cx + face_x + 7, cy - 16 + (7 if d == 2 else -7)),
                      (cx + face_x + 7, cy - 8 + (7 if d == 2 else -7)), (cx + face_x - 7, cy - 8)],
                     fill=snap((60, 50, 40)) + (255,))
    # Striped awning roof.
    roof = [(cx, cy - 18 - 22), (cx + 28, cy - 18 - 8), (cx, cy - 18 + 6), (cx - 28, cy - 18 - 8)]
    draw.polygon(roof, fill=palette_rgb(250), outline=palette_rgb(246))
    for k in range(-2, 3):
        draw.line([(cx + k * 9 - 7, cy - 18 - 8 - (k * 9 - 7) * 0.5 + 0), (cx + k * 9 + 7, cy - 18 + 6 - (k * 9 + 7) * 0.5)],
                  fill=snap((240, 240, 240)) + (255,), width=2)
    return img, h


def write_gift_shop():
    folder = os.path.join(ROOT, "gift_shop")
    os.makedirs(os.path.join(folder, "images"), exist_ok=True)
    images = []
    preview = Image.new("RGBA", (112, 112), (0, 0, 0, 0))
    big, hh = draw_gift_shop(2)
    preview.alpha_composite(big.resize((big.width * 3 // 2, big.height * 3 // 2), Image.NEAREST), (8, 20))
    preview.save(os.path.join(folder, "images", "preview.png"))
    for _ in range(3):
        images.append({"path": "images/preview.png", "x": 0, "y": 0})
    for d in range(4):
        img, h = draw_gift_shop(d)
        name = f"shop_d{d}.png"
        img.save(os.path.join(folder, "images", name))  # exact palette (remap awning): not snapped again
        images.append({"path": f"images/{name}", "x": -32, "y": -h})
    obj = {
        "id": "factory-tour.ride.gift_shop",
        "authors": [AUTHOR],
        "version": "1.0",
        "sourceGame": "official",
        "objectType": "ride",
        "properties": {
            "type": "shop",
            "category": "stall",
            "clearance": 32,
            "sells": ["factory_model", "gear_keyring"],
            "carsPerFlatRide": 1,
            "carColours": [[["bright_red", "black", "black"]]],
        },
        "images": images,
        "strings": {
            "name": {"en-GB": "Factory gift shop"},
            "description": {"en-GB": "Sells souvenirs made in the park's own factory"},
            "capacity": {"en-GB": ""},
        },
    }
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def write_exhibit_path():
    """An Exhibit Path: a footpath surface guests are drawn to. Its path sprites come from the player's own RCT2 data at
    runtime (like upstream's official objects); only the palette preview is drawn here."""
    folder = os.path.join(ROOT, "exhibit_path")
    os.makedirs(os.path.join(folder, "images"), exist_ok=True)
    img = Image.new("RGBA", (64, 34), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    draw.polygon(tile_polygon(32, 17, 0.9), fill=(110, 115, 125, 255), outline=(40, 40, 45, 255))
    for k in range(-3, 4):
        x = 32 + k * 8
        draw.line([(x - 4, 17 - 12 + abs(k) * 2), (x + 4, 17 - 12 + abs(k) * 2 + 4)], fill=(230, 200, 40, 255), width=2)
    save(img, folder, "preview.png")
    obj = {
        "id": "factory-tour.footpath_surface.exhibit",
        "authors": [AUTHOR],
        "version": "1.0",
        "sourceGame": "official",
        "objectType": "footpath_surface",
        "properties": {"isExhibit": True},
        "images": [{"path": "images/preview.png", "x": -32, "y": -17}, "$RCT2:OBJDATA/PATHSPCE.DAT[0..50]"],
        "strings": {"name": {"en-GB": "Factory exhibit walkway"}},
    }
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")


def draw_lab(d, frame, frames):
    def top_detail(draw, cx, cy):
        # A glass dome whose glow pulses while researching.
        glow = (80 + 40 * frame // max(1, frames - 1), 160 + 20 * frame // max(1, frames - 1), 230, 255)
        draw.ellipse([cx - 14, cy - 9, cx + 14, cy + 5], fill=(40, 60, 90, 255))
        draw.ellipse([cx - 11, cy - 12, cx + 11, cy + 2], fill=glow, outline=(30, 50, 80, 255))

    def side_detail(draw, cx, cy, h, dd):
        draw.rectangle([cx - 24, cy - 14, cx - 8, cy - 6], fill=(200, 200, 210, 255))  # sign board

    return draw_machine_box(d, (150, 150, 165, 255), (180, 180, 195, 255), (50, 50, 60, 255), 20, top_detail, side_detail)


def write_research():
    """The research chain: research kits crafted in assemblers, labs that consume them, and a starter technology tree
    that gates both factory prototypes and park content (ADR 0012)."""
    icon, belt = draw_item_small((70, 150, 220), (30, 70, 130))
    folder = write_object("research_kit", "item", {"stackSize": 200, "marketPrice": 40},
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          "Research kit")
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")
    write_object("research_kit_recipe", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_gear", "count": 1},
                        {"item": "factory-tour.factory_prototype.iron_plate", "count": 1}],
        "results": [{"item": "factory-tour.factory_prototype.research_kit", "count": 1}],
        "timeTicks": 200, "category": "crafting"}, [], "Research kit")
    write_machine("lab", "Lab", {
        "machineKind": "lab", "energy": "electric", "speedQ8": 256, "powerUsage": 60, "noise": 5,
        "inputSlots": 2, "outputSlots": 0, "price": 120, "removalPrice": -90, "clearance": 6}, draw_lab, 4)

    def proto(name):
        return f"factory-tour.factory_prototype.{name}"

    # Steel and the second research kit (the first content pack's longer chain).
    icon, belt = draw_item_small((170, 180, 195), (80, 85, 100))
    folder = write_object("steel_plate", "item", {"stackSize": 100, "marketPrice": 120},
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          "Steel plate")
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")
    write_object("steel_smelting", "recipe", {
        "ingredients": [{"item": proto("iron_plate"), "count": 5}],
        "results": [{"item": proto("steel_plate"), "count": 1}],
        "timeTicks": 512, "category": "smelting"}, [], "Steel plate")
    icon, belt = draw_item_small((90, 190, 90), (30, 100, 30))
    folder = write_object("engineering_kit", "item", {"stackSize": 200, "marketPrice": 90},
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          "Engineering kit")
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")
    write_object("engineering_kit_recipe", "recipe", {
        "ingredients": [{"item": proto("steel_plate"), "count": 1}, {"item": proto("iron_gear"), "count": 2}],
        "results": [{"item": proto("engineering_kit"), "count": 1}],
        "timeTicks": 240, "category": "crafting"}, [], "Engineering kit")

    kit = [{"item": proto("research_kit"), "count": 1}]
    kits = kit + [{"item": proto("engineering_kit"), "count": 1}]
    technologies = [
        ("tech_logistics", "Logistics", [], kit, 10, [proto("splitter_basic"), proto("underground_belt_basic")], [], []),
        ("tech_electric_mining", "Electric mining", [], kit, 15, [proto("electric_drill")], [], []),
        ("tech_steam_power", "Steam power", [], kit, 20,
         [proto("offshore_pump"), proto("boiler"), proto("steam_engine"), proto("pipe_basic")], [], []),
        ("tech_warehousing", "Warehousing", ["tech_logistics"], kit, 15,
         [proto("warehouse_depot"), proto("export_depot")], [], []),
        ("tech_souvenirs", "Souvenir manufacturing", [], kit, 10,
         [proto("factory_model_recipe"), proto("gear_keyring_recipe")], ["factory-tour.ride.gift_shop"], []),
        ("tech_factory_tours", "Factory tours", ["tech_logistics"], kit, 15, [], ["factory-tour.ride.tour_tram"], []),
        ("tech_military", "Factory defence", [], kit, 10,
         [proto("bolt_turret"), proto("bolt_magazine_recipe")], [], []),
        ("tech_steel", "Steel processing", [], kit, 20, [proto("steel_smelting"), proto("engineering_kit_recipe")], [], []),
        ("tech_fast_inserters", "Fast inserters", ["tech_steel", "tech_logistics"], kits, 25,
         [proto("inserter_fast")], [], []),
        ("tech_freight", "Freight railway", ["tech_logistics", "tech_steel"], kits, 30,
         [proto("freight_loader"), proto("freight_unloader")], ["factory-tour.ride.freight_train"], []),
        ("tech_interworld", "Interworld logistics", ["tech_freight"], kits, 40,
         [proto("launch_pad"), proto("landing_pad")], [], []),
        ("tech_portals", "Portal terminals", ["tech_interworld"], kits, 50, [], ["factory-tour.ride.portal_shuttle"], []),
    ]
    for name, display, prerequisites, packs, units, unlocks, rides, scenery in technologies:
        write_object(name, "technology", {
            "prerequisites": [proto(p) for p in prerequisites], "packs": packs, "units": units, "unitTicks": 400,
            "unlocks": unlocks, "rideEntries": rides, "sceneryGroups": scenery}, [], display)


def draw_turret(d, frame, frames):
    def top_detail(draw, cx, cy):
        # A barrel pointing along the view direction; recoils on the firing frame.
        vx, vy = [(1, -0.5), (1, 0.5), (-1, 0.5), (-1, -0.5)][d]
        length = 14 - (4 if frame == 1 else 0)
        draw.ellipse([cx - 8, cy - 5, cx + 8, cy + 3], fill=(70, 80, 70, 255), outline=(30, 35, 30, 255))
        draw.line([(cx, cy - 2), (cx + vx * length, cy - 2 + vy * length)], fill=(40, 40, 45, 255), width=4)
        if frame == 1:
            draw.ellipse([cx + vx * length - 3, cy - 5 + vy * length, cx + vx * length + 3, cy + 1 + vy * length],
                         fill=(250, 210, 60, 255))

    return draw_machine_box(d, (90, 100, 80, 255), (120, 130, 105, 255), (35, 40, 30, 255), 12, top_detail)


def draw_crawler(d, frame, frames):
    """A small rusty crawler seen from the view direction d; legs alternate with the frame."""
    img = Image.new("RGBA", (28, 20), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    vx, vy = [(1, -0.5), (1, 0.5), (-1, 0.5), (-1, -0.5)][d]
    cx, cy = 14, 12
    for k, side in enumerate((-1, 1)):
        for leg in range(3):
            phase = (leg + frame + k) % 2
            ox = cx + (leg - 1) * 5 * vx + side * 6 * -vy * 2 * 0.5
            oy = cy + (leg - 1) * 5 * vy * 0.5 + side * 3
            draw.line([(ox, oy), (ox + side * (3 + phase), oy + 4)], fill=(60, 35, 20, 255), width=1)
    draw.ellipse([cx - 8, cy - 5, cx + 8, cy + 4], fill=(150, 80, 40, 255), outline=(70, 35, 15, 255))
    hx, hy = cx + vx * 7, cy + vy * 7
    draw.ellipse([hx - 4, hy - 4, hx + 4, hy + 3], fill=(110, 55, 30, 255), outline=(60, 30, 10, 255))
    draw.point([(hx + vx * 2 - 1, hy - 1), (hx + vx * 2 + 1, hy - 1)], fill=(250, 220, 60, 255))
    return img


def write_combat():
    """The combat stub's content: a Threat, a turret and its ammunition (ADR 0013)."""
    frames = 4
    images = []
    folder = write_object("scrap_crawler", "threat", {"health": 60, "speedQ8": 96, "damage": 15, "attackTicks": 40,
                                                       "frames": frames}, [], "Scrap crawler")
    for d in range(4):
        for f in range(frames):
            fname = f"t_d{d}_f{f}.png"
            save(draw_crawler(d, f, frames), folder, fname)
            images.append({"path": f"images/{fname}", "x": -14, "y": -16})
    with open(os.path.join(folder, "object.json")) as fh:
        obj = json.load(fh)
    obj["images"] = images
    with open(os.path.join(folder, "object.json"), "w") as fh:
        json.dump(obj, fh, indent=4)
        fh.write("\n")

    icon, belt = draw_item_small((200, 170, 60), (110, 90, 20))
    folder = write_object("bolt_magazine", "item", {"stackSize": 50, "marketPrice": 15},
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          "Bolt magazine")
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")
    write_object("bolt_magazine_recipe", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_plate", "count": 2}],
        "results": [{"item": "factory-tour.factory_prototype.bolt_magazine", "count": 1}],
        "timeTicks": 60, "category": "crafting"}, [], "Bolt magazine")
    write_machine("bolt_turret", "Bolt turret", {
        "machineKind": "turret", "energy": "none", "health": 500, "turretRange": 6, "turretDamage": 20,
        "turretCooldownTicks": 20, "shotsPerAmmo": 10, "ammoItem": "factory-tour.factory_prototype.bolt_magazine",
        "photogenic": True, "inputSlots": 1, "outputSlots": 0, "price": 150, "removalPrice": -110, "clearance": 5},
        draw_turret, 2)


def write_ore_and_item(ore_name, item_name, display_ore, display_item, colour, dark, fuel_ticks=0, market_price=0):
    icon, belt = draw_item_small(colour, dark)
    item_props = {"stackSize": 50}
    if market_price:
        item_props["marketPrice"] = market_price
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
    write_ore_and_item("iron_ore_patch", "iron_ore", "Iron ore", "Iron ore", (110, 120, 140), (60, 70, 90), market_price=5)
    write_ore_and_item("coal_patch", "coal", "Coal", "Coal", (50, 50, 55), (20, 20, 25), fuel_ticks=1600, market_price=8)
    # The weird dimension's ore (painted only in weird worlds) and what it makes.
    write_ore_and_item("void_crystal_patch", "void_crystal", "Void crystal", "Void crystal", (150, 60, 200), (70, 20, 110),
                       market_price=60)
    icon, belt = draw_item_small((230, 120, 240), (120, 40, 140))
    folder = write_object("void_lens", "item", {"stackSize": 50, "marketPrice": 600},
                          [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                          "Void lens")
    save(icon, folder, "icon.png")
    save(belt, folder, "belt.png")
    write_object("void_lens_recipe", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.void_crystal", "count": 3},
                        {"item": "factory-tour.factory_prototype.iron_plate", "count": 2}],
        "results": [{"item": "factory-tour.factory_prototype.void_lens", "count": 1}],
        "timeTicks": 160, "category": "crafting"}, [], "Void lens")

    # Recipes.
    write_object("iron_plate_smelting", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_ore", "count": 1}],
        "results": [{"item": "factory-tour.factory_prototype.iron_plate", "count": 1}],
        "timeTicks": 128, "category": "smelting"}, [], "Iron plate")

    write_underground_and_splitter()

    # Machines (1x1 in M2; multi-tile footprints come later).
    write_machine("burner_drill", "Burner mining drill", {
        "machineKind": "drill", "energy": "burner", "speedQ8": 256, "miningRadius": 1, "miningTimeTicks": 100, "pollution": 10, "noise": 30,
        "inputSlots": 0, "outputSlots": 1, "price": 80, "removalPrice": -60, "clearance": 5}, draw_drill, 4)
    write_machine("burner_generator", "Burner generator", {
        "machineKind": "engine", "energy": "burner", "powerOutput": 200, "pollution": 30, "noise": 30,
        "inputSlots": 0, "outputSlots": 0, "price": 120, "removalPrice": -90, "clearance": 7}, draw_generator, 4)
    write_machine("assembling_machine", "Assembling machine", {
        "machineKind": "assembler", "energy": "electric", "speedQ8": 128, "powerUsage": 75,
        "recipeCategories": ["crafting"], "inputSlots": 4, "outputSlots": 1, "pollution": 4, "noise": 20,
        "price": 150, "removalPrice": -110, "clearance": 7}, draw_assembler, 4)
    write_object("iron_gear", "item", {"stackSize": 100, "marketPrice": 60},
                 [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                 "Iron gear wheel")
    gi, gb = draw_item_small((150, 150, 160), (70, 70, 80))
    save(gi, os.path.join(ROOT, "iron_gear"), "icon.png")
    save(gb, os.path.join(ROOT, "iron_gear"), "belt.png")
    write_object("iron_gear_recipe", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_plate", "count": 2}],
        "results": [{"item": "factory-tour.factory_prototype.iron_gear", "count": 1}],
        "timeTicks": 20, "category": "crafting"}, [], "Iron gear wheel")
    folder = write_object("small_pole", "pole", {"wireReach": 7, "supplyRadius": 2, "price": 10, "removalPrice": -7,
                                                  "clearance": 10},
                          [{"path": "images/pole.png", "x": -32, "y": -40}], "Small electric pole")
    save(draw_pole(), folder, "pole.png")
    write_machine("stone_furnace", "Stone furnace", {
        "machineKind": "furnace", "energy": "burner", "speedQ8": 256, "recipeCategories": ["smelting"],
        "pollution": 20, "noise": 10,
        "inputSlots": 1, "outputSlots": 1, "price": 60, "removalPrice": -45, "clearance": 7}, draw_furnace, 4)

    # The Factory Tour ride's vehicle and the walkway that draws visitors past the machines.
    write_tour_tram()
    write_portal_shuttle()
    write_exhibit_path()

    # Research: kits, labs and the starter technology tree.
    write_research()

    # Combat stub: a threat, a turret and its ammunition.
    write_combat()

    # Freight railway wagons and their loader and unloader.
    write_freight()

    # Launch and landing pads between worlds.
    write_interworld()

    # Manufactured souvenirs, their recipes and the shop that sells them (stocked from the Warehouse).
    for name, display, colour, dark, shop_item in (("factory_model", "Factory model", (190, 120, 60), (110, 60, 30), "factory_model"),
                                                   ("gear_keyring", "Gear keyring", (200, 200, 120), (120, 120, 60), "gear_keyring")):
        icon, belt = draw_item_small(colour, dark)
        folder = write_object(name, "item", {"stackSize": 50, "marketPrice": 30 if name == "gear_keyring" else 90,
                                             "shopItem": shop_item},
                              [{"path": "images/icon.png", "x": -12, "y": -12}, {"path": "images/belt.png", "x": -5, "y": -4}],
                              display)
        save(icon, folder, "icon.png")
        save(belt, folder, "belt.png")
    write_object("factory_model_recipe", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_gear", "count": 2},
                        {"item": "factory-tour.factory_prototype.iron_plate", "count": 2}],
        "results": [{"item": "factory-tour.factory_prototype.factory_model", "count": 1}],
        "timeTicks": 80, "category": "crafting"}, [], "Factory model")
    write_object("gear_keyring_recipe", "recipe", {
        "ingredients": [{"item": "factory-tour.factory_prototype.iron_gear", "count": 1}],
        "results": [{"item": "factory-tour.factory_prototype.gear_keyring", "count": 2}],
        "timeTicks": 30, "category": "crafting"}, [], "Gear keyring")
    write_gift_shop()

    # A 3x3 electric mining drill: per-tile slices so it sorts correctly at every rotation.
    write_multitile_machine("electric_drill", "Electric mining drill", {
        "machineKind": "drill", "energy": "electric", "speedQ8": 256, "powerUsage": 90, "miningRadius": 2,
        "miningTimeTicks": 60, "pollution": 10, "noise": 50, "inputSlots": 0, "outputSlots": 1, "price": 200, "removalPrice": -150,
        "clearance": 6}, 3, 22, ((110, 120, 90, 255), (140, 150, 110, 255), (40, 45, 30, 255)), drill_head, 4)

    # Fluids and the steam chain: offshore pump -> boiler -> steam engine.
    write_fluid("water", "Water", (60, 110, 200), (30, 60, 130))
    write_fluid("steam", "Steam", (220, 220, 225), (150, 150, 160))
    write_pipe()
    write_machine("offshore_pump", "Offshore pump", {
        "machineKind": "pump", "energy": "none", "fluidRate": 120, "noise": 10,
        "outputFluid": "factory-tour.factory_prototype.water",
        "fluidBoxes": [{"role": "output", "sides": ["front"]}],
        "inputSlots": 0, "outputSlots": 0, "price": 50, "removalPrice": -35, "clearance": 4}, draw_pump, 4)
    write_machine("boiler", "Boiler", {
        "machineKind": "boiler", "energy": "burner", "fluidRate": 6, "pollution": 30, "noise": 20,
        "inputFluid": "factory-tour.factory_prototype.water", "outputFluid": "factory-tour.factory_prototype.steam",
        "fluidBoxes": [{"role": "input", "sides": ["left", "right"]}, {"role": "output", "sides": ["front"]}],
        "inputSlots": 0, "outputSlots": 0, "price": 100, "removalPrice": -75, "clearance": 6}, draw_boiler, 4)
    write_machine("steam_engine", "Steam engine", {
        "machineKind": "engine", "energy": "fluid", "fluidRate": 3, "powerOutput": 450, "noise": 40,
        "inputFluid": "factory-tour.factory_prototype.steam",
        "fluidBoxes": [{"role": "input", "sides": ["front", "back"]}],
        "inputSlots": 0, "outputSlots": 0, "price": 150, "removalPrice": -110, "clearance": 6}, draw_steam_engine, 4)

    # Item: iron plate.
    folder = write_object(
        "iron_plate", "item", {"stackSize": 100, "marketPrice": 20},
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

    # Inserters: 4 directions x 8 frames; the fast one swings twice as quickly.
    for inserter_name, display, swing, price, arm in (
            ("inserter_basic", "Basic inserter", 24, 40, (200, 160, 40, 255)),
            ("inserter_fast", "Fast inserter", 12, 70, (60, 120, 210, 255))):
        frames = 8
        images = []
        folder = write_object(
            inserter_name, "inserter",
            {"frames": frames, "swingTicks": swing, "reach": 1, "price": price, "removalPrice": -(price * 3 // 4),
             "clearance": 8},
            [], display)
        for d in range(4):
            for f in range(frames):
                name = f"inserter_d{d}_f{f}.png"
                img, h = draw_inserter(d, f, frames, arm)
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

    # Warehouse depot: what goes in joins the park-wide Warehouse that construction draws on.
    img, h = draw_warehouse()
    folder = write_object(
        "warehouse_depot", "container",
        {"slots": 1, "rotations": 1, "warehouse": True, "price": 200, "removalPrice": -150, "clearance": 7},
        [{"path": "images/depot.png", "x": -32, "y": -h}], "Warehouse depot")
    save(img, folder, "depot.png")

    # Export depot: what goes in is sold to the off-map Market straight away.
    img, h = draw_warehouse((140, 110, 80, 255), (175, 140, 100, 255), (60, 45, 30, 255))
    folder = write_object(
        "export_depot", "container",
        {"slots": 1, "rotations": 1, "exportDepot": True, "price": 250, "removalPrice": -180, "clearance": 7},
        [{"path": "images/depot.png", "x": -32, "y": -h}], "Export depot")
    save(img, folder, "depot.png")
    print("wrote content pack to", os.path.relpath(ROOT))


if __name__ == "__main__":
    main()

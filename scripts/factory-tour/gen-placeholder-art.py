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
    img.save(os.path.join(folder, "images", filename))


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


def main():
    os.makedirs(ROOT, exist_ok=True)

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

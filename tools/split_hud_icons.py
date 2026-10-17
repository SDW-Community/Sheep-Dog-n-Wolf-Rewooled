#!/usr/bin/env python3
"""Splits the HUD button icon sheet (res/spritesheet.png) into the animated icons the HUD draws.

Each family's face buttons are a block of 4 columns x 3 rows on the sheet: a button a column, an animation frame a
row (top to bottom: frames 1..3). They are written as res/hud/icons/<family>/<button>_<frame>.png, the names the icon
table uses (src/engine/hud/hud_button_icons.cpp, HUD_ANIM), and the build embeds them (cmake/embed_hud_icons.cmake).

  playstation   top left           square, triangle, circle, cross
  xbox          top right          X, Y, B, A
  nintendo      right, under Xbox  Y, X, A, B
  joycon        left, under PS     a single Joy-Con's four dots, the one pressed lit: west, north, east, south

Each icon is a 15 x 15 cell; its three frames are cut at the same x (the columns' extent over the three rows,
centred), so the animation does not shift sideways. Nothing is resampled.

Only the families named are written (by default playstation, xbox and joycon: the Nintendo icons come from
res/controller.png for now), and they overwrite the PNGs there.

Needs Pillow (tools/requirements.txt). Run from anywhere: python3 tools/split_hud_icons.py [family ...]
"""
import argparse
import os
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CELL = 15

# family -> (button names left to right, the sheet's x band holding the block, the 3 rows' top y)
FAMILIES = {
    "playstation": (["square", "triangle", "circle", "cross"], (0, 74), [4, 20, 36]),
    "xbox": (["x", "y", "b", "a"], (74, 148), [4, 20, 36]),
    "nintendo": (["y", "x", "a", "b"], (74, 148), [51, 67, 83]),
    "joycon": (["y", "x", "a", "b"], (0, 74), [56, 72, 88]),
}
DEFAULT = ["playstation", "xbox", "joycon"]


def columns(alpha, band, tops):
    """the runs of opaque x over the block's 3 rows together: one per button"""
    x0, x1 = band
    occupied = [any(alpha[x, y] for t in tops for y in range(t, t + CELL)) for x in range(x0, x1)] + [False]
    runs, start = [], None
    for i, v in enumerate(occupied):
        if v and start is None:
            start = i
        if not v and start is not None:
            runs.append((x0 + start, x0 + i - 1))
            start = None
    return runs


def split(sheet, family, out_dir):
    names, band, tops = FAMILIES[family]
    cols = columns(sheet.getchannel("A").load(), band, tops)
    if len(cols) != len(names):
        sys.exit(f"{family}: found {len(cols)} columns on the sheet, expected {len(names)}: {cols}")
    os.makedirs(os.path.join(out_dir, family), exist_ok=True)
    written = []
    for name, (c0, c1) in zip(names, cols):
        width = c1 - c0 + 1
        if width > CELL:
            sys.exit(f"{family}/{name}: {width} pixels wide, more than a {CELL}-pixel cell")
        x0 = c0 - (CELL - width) // 2
        for frame, top in enumerate(tops, 1):
            cell = Image.new("RGBA", (CELL, CELL))
            cell.paste(sheet.crop((max(x0, 0), top, x0 + CELL, top + CELL)), (max(-x0, 0), 0))
            path = os.path.join(out_dir, family, f"{name}_{frame}.png")
            cell.save(path)
            written.append(path)
    return written


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("families", nargs="*", metavar="family",
                    help=f"{', '.join(FAMILIES)} (default: {' '.join(DEFAULT)})")
    ap.add_argument("--sheet", default=os.path.join(ROOT, "res", "spritesheet.png"), help="the icon sheet")
    ap.add_argument("--out", default=os.path.join(ROOT, "res", "hud", "icons"), help="the icons' folder")
    args = ap.parse_args(argv)
    for family in args.families:
        if family not in FAMILIES:
            ap.error(f"unknown family {family!r}: {', '.join(FAMILIES)}")
    sheet = Image.open(args.sheet).convert("RGBA")
    for family in args.families or DEFAULT:
        for path in split(sheet, family, args.out):
            print(os.path.relpath(path, ROOT))


if __name__ == "__main__":
    main(sys.argv[1:])

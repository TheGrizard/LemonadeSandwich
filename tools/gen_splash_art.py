#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The boot screen's picture (tessera-rnd: LEMONADE SANDWICH): a slice of lemon sandwiched between two slices of bread,
in colour, as pixel art of 2 px cells, run-length coded for firmware/src/splash.c:

  firmware/src/splash_art.h   SPL_ART_W x SPL_ART_H cells; SPL_ART[]: each row's runs left to right, a byte each,
                              colour << 5 | (length - 1) (colour 0 the background; 1, 2 the bread's crust and crumb;
                              3, 4, 5 the lemon's rind, pith and flesh: SPL_PAL); a row's runs fill it

  python3 tools/gen_splash_art.py            write it
  python3 tools/gen_splash_art.py --check    fail if it differs from what this writes (tests/run_tests.sh)
  python3 tools/gen_splash_art.py --png F    a preview of the picture, 4 x"""
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "firmware/src/splash_art.h"
GW, GH = 72, 52                                         # the drawing's grid, cells; cropped to the picture (W x H)
CRUST, CRUMB, RIND, PITH, FLESH = 1, 2, 3, 4, 5
PAL = {0: (24, 24, 28), CRUST: (170, 104, 44), CRUMB: (246, 224, 172), RIND: (236, 186, 24), PITH: (252, 246, 206),
       FLESH: (252, 214, 64)}                       # (the preview's; splash.c SPL_PAL the screen's)


def bread_in(cx, cy, w, h, ang, shrink=0.0):
    """a slice of bread, face on: the body with rounded feet, the top puffed out over it"""
    ca, sa = math.cos(math.radians(ang)), math.sin(math.radians(ang))

    def inside(px, py):
        x = (px - cx) * ca + (py - cy) * sa
        y = -(px - cx) * sa + (py - cy) * ca
        hw, top, bot = w / 2 - shrink, -h / 2 + shrink, h / 2 - shrink
        if y > bot or y < top:
            return False
        r = 3 - shrink * 0.3
        if y > bot - r:                                 # the feet, rounded
            dx, dy = abs(x) - (hw - r), y - (bot - r)
            if dx > 0 and dx * dx + dy * dy > r * r:
                return False
        crown = y < -h / 2 + 9
        if abs(x) > hw + (2.5 - shrink if crown else 0):
            return False
        if crown:                                       # the crown: an ellipse, 2.5 wider than the body
            ex, ey, yy = hw + 2.5 - shrink, 9 - shrink * 0.8, y - (-h / 2 + 9)
            if (x / ex) ** 2 + (yy / ey) ** 2 > 1:
                return False
        return True
    return inside


def art():
    g = [[0] * GW for _ in range(GH)]

    def bread(cx, cy, ang):
        o, i = bread_in(cx, cy, 26, 30, ang), bread_in(cx, cy, 26, 30, ang, 2.2)
        for py in range(GH):
            for px in range(GW):
                if o(px + .5, py + .5):
                    g[py][px] = CRUMB if i(px + .5, py + .5) else CRUST

    def lemon(cx, cy, r):                               # the wheel: rind, pith, eight segments, the pith between
        for py in range(GH):
            for px in range(GW):
                x, y = px + .5 - cx, py + .5 - cy
                d = math.hypot(x, y)
                if d > r:
                    continue
                if d > r - 1.8:
                    c = RIND
                elif d > r - 2.9 or d < 1.3:
                    c = PITH
                else:
                    a = (math.degrees(math.atan2(y, x)) + 22.5) % 45
                    c = PITH if (a < 6 or a > 39) and d > 2 else FLESH
                g[py][px] = c
    bread(26, 27, -6)                                   # the back slice, the lemon, the front one
    lemon(46, 22, 12)
    bread(30, 29, 4)
    ys = [y for y in range(GH) if any(g[y])]
    xs = [x for x in range(GW) if any(g[y][x] for y in range(GH))]
    return [row[xs[0]:xs[-1] + 1] for row in g[ys[0]:ys[-1] + 1]]


def runs(g):
    W, out = len(g[0]), []
    for row in g:
        x = 0
        while x < W:
            c, n = row[x], 1
            while x + n < W and row[x + n] == c and n < 32:
                n += 1
            out.append(c << 5 | (n - 1))
            x += n
    return out


def header(g):
    r, W, H = runs(g), len(g[0]), len(g)
    lines = ["/* SPDX-License-Identifier: GPL-3.0-only */",
             "/* The boot screen's picture: written by tools/gen_splash_art.py (edit that, not this) */",
             f"#define SPL_ART_W {W}", f"#define SPL_ART_H {H}",
             f"static const uint8_t SPL_ART[{len(r)}] = {{"]
    for i in range(0, len(r), 16):
        lines.append("    " + ", ".join(f"0x{v:02X}" for v in r[i:i + 16]) + ",")
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    g = art()
    h, W, H = header(g), len(g[0]), len(g)
    if "--png" in sys.argv:
        from PIL import Image
        im = Image.new("RGB", (W, H))
        for y in range(H):
            for x in range(W):
                im.putpixel((x, y), PAL[g[y][x]])
        im.resize((W * 8, H * 8), Image.NEAREST).save(sys.argv[sys.argv.index("--png") + 1])
        return 0
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text() != h:
            print("splash_art.h is not what tools/gen_splash_art.py writes: run it")
            return 1
        print(f"splash art: {len(runs(g))} B, as gen_splash_art.py writes it")
        return 0
    OUT.write_text(h)
    print(f"{OUT}: {len(runs(g))} B")
    return 0


if __name__ == "__main__":
    sys.exit(main())

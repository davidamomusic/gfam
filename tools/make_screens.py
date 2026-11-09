#!/usr/bin/env python3
"""Builds the screenshots in docs/screens/.

It compiles test/host_screens.c (the real UI and synth code, built for the PC), runs it so it plays a few
screens through the UI with key presses, and draws every tilemap it dumps with the app's own tiles and
palette. Needs gcc and Python with Pillow.

    python3 tools/make_screens.py            # writes docs/screens/*.png (3x, 720x480)
    python3 tools/make_screens.py --sheet    # also writes build/screens_sheet.png to look at them all
"""
import os
import struct
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import gen_assets as G  # noqa: E402
from PIL import Image  # noqa: E402

SCALE = 3
OUT = os.path.join(ROOT, "docs", "screens")
TMP = os.path.join(ROOT, "build", "screens")


def col(c):
    return ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31)


def render(path, tiles, pal):
    m = struct.unpack("<1024H", open(path, "rb").read())
    img = Image.new("RGB", (240, 160), col(pal[0]))
    px = img.load()
    for ty in range(20):
        for tx in range(30):
            e = m[ty * 32 + tx]
            t, bank = e & 0x3FF, (e >> 12) & 15
            for y in range(8):
                for x in range(8):
                    ci = tiles[t][y][x]
                    if ci:
                        px[tx * 8 + x, ty * 8 + y] = col(pal[bank * 16 + ci])
    return img


def main():
    version = open(os.path.join(ROOT, "VERSION")).read().strip()
    os.makedirs(TMP, exist_ok=True)
    os.makedirs(OUT, exist_ok=True)
    exe = os.path.join(ROOT, "build", "host_screens")
    srcs = [os.path.join(ROOT, "src", f) for f in ("synth.c", "params.c", "ui.c", "fx.c")]
    subprocess.check_call(["gcc", "-DHOST", "-O2", "-w", "-DGFAM_VERSION=\"%s\"" % version, "-o", exe,
                           os.path.join(ROOT, "test", "host_screens.c")] + srcs + ["-lm"])
    for f in os.listdir(TMP):
        if f.endswith(".bin"):
            os.remove(os.path.join(TMP, f))
    subprocess.check_call([exe, TMP])

    tiles, pal = G.all_tiles(), G.palette_flat()
    names = sorted(f[:-4] for f in os.listdir(TMP) if f.endswith(".bin"))
    shots = []
    for n in names:
        im = render(os.path.join(TMP, n + ".bin"), tiles, pal).resize((240 * SCALE, 160 * SCALE), Image.NEAREST)
        ncol = len(im.getcolors(1 << 16))
        im.convert("P", palette=Image.ADAPTIVE, colors=min(256, ncol)).save(os.path.join(OUT, n + ".png"), optimize=True)
        shots.append((n, im))
    print("wrote %d screenshots to docs/screens/" % len(shots))

    if "--sheet" in sys.argv:
        cols, w, h = 4, 240 + 6, 160 + 6
        rows = (len(shots) + cols - 1) // cols
        sheet = Image.new("RGB", (cols * w, rows * h), (70, 70, 70))
        for i, (n, im) in enumerate(shots):
            sheet.paste(im.resize((240, 160), Image.NEAREST), ((i % cols) * w + 3, (i // cols) * h + 3))
        sheet.save(os.path.join(ROOT, "build", "screens_sheet.png"))
        print("wrote build/screens_sheet.png")


if __name__ == "__main__":
    main()

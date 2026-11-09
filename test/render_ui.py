#!/usr/bin/env python3
"""Rasterize build/ui_N.bin tilemaps with the real tiles/palette -> build/ui_sheet.png"""
import os, struct, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))
import gen_assets as G
from PIL import Image

tiles = G.all_tiles()
pal = G.palette_flat()

def col(c):
    return ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31)

def render(path):
    data = open(path, "rb").read()
    m = struct.unpack("<1024H", data)
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

paths = sorted((p for p in os.listdir("build") if p.startswith("ui_") and p.endswith(".bin")), key=lambda p: int(p[3:-4]))
imgs = [render(os.path.join("build", p)).resize((480, 320), Image.NEAREST) for p in paths]
cols = 3
rows = (len(imgs) + cols - 1) // cols
sheet = Image.new("RGB", (cols * 490, rows * 330), (60, 60, 60))
for i, im in enumerate(imgs):
    sheet.paste(im, ((i % cols) * 490 + 5, (i // cols) * 330 + 5))
sheet.save("build/ui_sheet.png")
print("wrote build/ui_sheet.png with", len(imgs), "screens")

#!/usr/bin/env python3
"""Generates src/gfx.h (tiles + palette) and src/tables.h (DSP lookup tables)."""
import math, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from font import GLYPHS

OUT = os.path.join(os.path.dirname(__file__), "..", "src")

# ---------------- audio constants ----------------
TIMER_PERIOD = 924                      # CPU cycles per sample (16.78 MHz / 924)
FS = 16777216.0 / TIMER_PERIOD          # ~18157 Hz
FRAME_SAMPLES = 160                     # samples per DMA buffer (multiple of 16 and of 4)
GROUP = 4                               # envelopes / pitch / cutoff are updated once per 4 samples
MIDI0_HZ = 8.175798915643707

# ---------------- tile bitmaps (8x8, color index per pixel) ----------------
# Colour indices (the 8 palette banks recolour them):
#   1 text / waveform   2 accent fill / value arc / chip   3 dim line / track
#   4 rim               5 dark body                        6 white (pointers)
# Layout (names exported to src/tiles.h). Multi-tile widgets are stored row-major.
#   0..63 font | 64..127 inverted font | BIG: 64 glyphs x 2x2 tiles (pixel-doubled font)
#   knobs 16 positions x 3x3 | waves 8 x 3x3 | lamps 2 x 3x3 | sel3 3 x 3x3 | sel4 4 x 3x3
#   pads 3 states x 3x3 | bars (left / right half) 9 fills each | line | small LEDs
T_INV0 = 64
T_BIG0 = 128
BIG_CHARS = " +-0123456789"                         # only what the big read-out needs
T_KNOB0 = T_BIG0 + len(BIG_CHARS) * 4
T_WAVE0 = T_KNOB0 + 16 * 9
T_LAMP0 = T_WAVE0 + 8 * 9
T_SEL3 = T_LAMP0 + 2 * 9
T_SEL4 = T_SEL3 + 3 * 9
T_PAD0 = T_SEL4 + 4 * 9
T_BARL0 = T_PAD0 + 3 * 9
T_BARR0 = T_BARL0 + 9
T_HL = T_BARR0 + 9
T_LED_OFF, T_LED_ON = T_HL + 1, T_HL + 2
T_LAB0 = T_HL + 3
# knob labels: pre-rendered with a compact 3x5 font, 3 tiles wide, centred in the knob cell
LABELS = ["WAV", "FRQ", "EG", "LVL", "SYN", "SEQ", "FM", "NSE", "MOD", "CUT", "RES", "VCO", "VCF", "VCA", "ATK",
          "VOL", "BPM", "NMD", "TUN", "SWP", "PDC", "ADC", "CLK", "DRV", "ON", "MIX", "AMT", "SLC", "TIM",
          "FBK", "SIZ", "TON", "1", "2", "3", "4", "5", "6", "7", "8", "VC1", "VC2", "DEC", "OFF", "VEL", "KCK", "RND"]
NTILES = T_LAB0 + 3 * len(LABELS)

G35 = {
 'A': ".#.|#.#|###|#.#|#.#", 'B': "##.|#.#|##.|#.#|##.", 'C': ".##|#..|#..|#..|.##", 'D': "##.|#.#|#.#|#.#|##.",
 'E': "###|#..|##.|#..|###", 'F': "###|#..|##.|#..|#..", 'G': ".##|#..|#.#|#.#|.##", 'H': "#.#|#.#|###|#.#|#.#",
 'I': "###|.#.|.#.|.#.|###", 'J': "..#|..#|..#|#.#|.#.", 'K': "#.#|#.#|##.|#.#|#.#", 'L': "#..|#..|#..|#..|###",
 'M': "#.#|###|###|#.#|#.#", 'N': "##.|#.#|#.#|#.#|#.#", 'O': ".#.|#.#|#.#|#.#|.#.", 'P': "##.|#.#|##.|#..|#..",
 'Q': ".#.|#.#|#.#|##.|.##", 'R': "##.|#.#|##.|#.#|#.#", 'S': ".##|#..|.#.|..#|##.", 'T': "###|.#.|.#.|.#.|.#.",
 'U': "#.#|#.#|#.#|#.#|###", 'V': "#.#|#.#|#.#|#.#|.#.", 'W': "#.#|#.#|###|###|#.#", 'X': "#.#|#.#|.#.|#.#|#.#",
 'Y': "#.#|#.#|.#.|.#.|.#.", 'Z': "###|..#|.#.|#..|###",
 '0': "###|#.#|#.#|#.#|###", '1': ".#.|##.|.#.|.#.|###", '2': "##.|..#|.#.|#..|###", '3': "##.|..#|.#.|..#|##.",
 '4': "#.#|#.#|###|..#|..#", '5': "###|#..|##.|..#|##.", '6': ".##|#..|###|#.#|###", '7': "###|..#|.#.|.#.|.#.",
 '8': "###|#.#|###|#.#|###", '9': "###|#.#|###|..#|##.",
}

def label_tiles(text):
    w = 4 * len(text) - 1
    x0 = (24 - w) // 2
    cv = canvas(3, 1)
    for i, ch in enumerate(text):
        for y, row in enumerate(G35[ch].split('|')):
            for x, c in enumerate(row):
                if c == '#':
                    cv[1 + y + 1][x0 + 4 * i + x] = 1
    return slice_canvas(cv, 3, 1)

def blank():
    return [[0] * 8 for _ in range(8)]

def font_tile(ch):
    t = blank()
    rows = GLYPHS.get(ch)
    if rows:
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c == '#':
                    t[y][x + 1] = 1
    return t

def inv_tile(ch):
    t = [[2] * 8 for _ in range(8)]
    rows = GLYPHS.get(ch)
    if rows:
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c == '#':
                    t[y][x + 1] = 0
    return t

def canvas(wt, ht):
    return [[0] * (wt * 8) for _ in range(ht * 8)]

def slice_canvas(cv, wt, ht):
    out = []
    for ty in range(ht):
        for tx in range(wt):
            out.append([[cv[ty * 8 + y][tx * 8 + x] for x in range(8)] for y in range(8)])
    return out

def big_tiles(ch):                                  # pixel-doubled 5x7 glyph in 2x2 tiles
    cv = canvas(2, 2)
    rows = GLYPHS.get(ch)
    if rows:
        for y, r in enumerate(rows):
            for x, c in enumerate(r):
                if c == '#':
                    for dy in range(2):
                        for dx in range(2):
                            cv[1 + 2 * y + dy][3 + 2 * x + dx] = 1
    return slice_canvas(cv, 2, 2)

def knob_tiles(pos):                                # 24x24: arc (value) + body + pointer
    cv = canvas(3, 3)
    cur = math.radians(-135 + 270 * pos / 15.0)
    lim = math.radians(135)
    for y in range(24):
        for x in range(24):
            dx, dy = x + 0.5 - 12, y + 0.5 - 12
            d = math.hypot(dx, dy)
            th = math.atan2(dx, -dy)
            if d <= 6.7:
                cv[y][x] = 4 if d > 5.6 else 5
            elif 8.2 <= d <= 10.4:
                if abs(th) <= lim + 0.02:
                    cv[y][x] = 2 if th <= cur + 0.02 else 3
    sx, sy = math.sin(cur), -math.cos(cur)
    for y in range(24):
        for x in range(24):
            dx, dy = x + 0.5 - 12, y + 0.5 - 12
            t = dx * sx + dy * sy
            n = abs(dx * sy - dy * sx)
            if 0.0 <= t <= 5.4 and n <= 0.9:
                cv[y][x] = 6
    return slice_canvas(cv, 3, 3)

def wave_val(w, t):
    s = math.sin
    tp = 2 * math.pi * t
    if w == 0:  return 4 * t if t < 0.25 else (2 - 4 * t if t < 0.75 else 4 * t - 4)
    if w == 1:  return 1.0 if t < 0.5 else -1.0
    if w == 2:  return 2 * t - 1
    if w == 3:  return 1.0 if t < 0.25 else -1.0
    if w == 4:  return s(tp)
    if w == 5:  return (s(tp) + 0.6 * s(2 * tp) + 0.4 * s(4 * tp)) / 1.55
    if w == 6:  return s(tp + 2.2 * s(tp))
    return (s(tp) + s(3 * tp) / 3 + s(5 * tp) / 5 + s(7 * tp) / 7) / 1.2

def wave_tiles(w):                                  # framed 24x24 box with the waveform inside
    cv = canvas(3, 3)
    for x in range(2, 22):
        cv[3][x] = cv[20][x] = 3
    for y in range(3, 21):
        cv[y][2] = cv[y][21] = 3
    prev = None
    for i in range(18):
        t = (i + 0.5) / 18
        v = wave_val(w, t)
        y = int(round(11.5 - 6.2 * max(-1.0, min(1.0, v))))
        x = 3 + i
        lo, hi = (y, y) if prev is None else (min(prev, y), max(prev, y))
        for yy in range(lo, hi + 1):
            cv[max(5, min(18, yy))][x] = 1
        prev = y
    return slice_canvas(cv, 3, 3)

def lamp_tiles(on):
    cv = canvas(3, 3)
    for y in range(24):
        for x in range(24):
            d = math.hypot(x + 0.5 - 12, y + 0.5 - 12)
            if d <= 7.5:
                cv[y][x] = (2 if d <= 6.0 else 4) if on else (5 if d <= 6.0 else 3)
    if on:
        for (x, y) in ((9, 8), (10, 8), (9, 9), (10, 9), (11, 8)):
            cv[y][x] = 6
    return slice_canvas(cv, 3, 3)

def sel_tiles(n, cur):                              # segmented switch with n positions
    cv = canvas(3, 3)
    w, gap = (5, 2) if n == 3 else (3, 2)
    x0 = (24 - (n * w + (n - 1) * gap)) // 2
    for i in range(n):
        for y in range(8, 16):
            for x in range(w):
                edge = y in (8, 15) or x in (0, w - 1)
                if i == cur:
                    cv[y][x0 + i * (w + gap) + x] = 6 if edge else 2
                else:
                    cv[y][x0 + i * (w + gap) + x] = 3 if edge else 5
    return slice_canvas(cv, 3, 3)

def pad_tiles(state):                               # 0 off, 1 trig, 2 accent; 24x24 rounded pad
    cv = canvas(3, 3)
    for y in range(1, 23):
        for x in range(1, 23):
            if (y in (1, 22)) and (x in (1, 22)):
                continue
            edge = y in (1, 22) or x in (1, 22)
            if state == 0:
                cv[y][x] = 3 if edge else 5
            else:
                cv[y][x] = 4 if edge else 2
    if state == 2:
        for y in range(8, 16):
            for x in range(8, 16):
                cv[y][x] = 6
    return slice_canvas(cv, 3, 3)

def bar_tile(fill, left):                           # vertical bar half-tile: fill 0..8 px from the bottom
    t = blank()
    x0, x1 = (1, 8) if left else (0, 7)
    for y in range(8):
        for x in range(x0, x1):
            t[y][x] = 2 if y >= 8 - fill else 3
    return t

def hline_tile():
    t = blank()
    for x in range(8):
        t[3][x] = 3
    return t

def led_tile(on):
    pat = ["........", "........", "..####..", ".######.",
           ".######.", "..####..", "........", "........"]
    t = blank()
    for y, r in enumerate(pat):
        for x, c in enumerate(r):
            if c == '#':
                t[y][x] = 2 if on else 3
    return t

def all_tiles():
    tiles = [font_tile(chr(32 + i)) for i in range(64)]
    tiles += [inv_tile(chr(32 + i)) for i in range(64)]
    for ch in BIG_CHARS:
        tiles += big_tiles(ch)
    for p in range(16):
        tiles += knob_tiles(p)
    for w in range(8):
        tiles += wave_tiles(w)
    tiles += lamp_tiles(False) + lamp_tiles(True)
    for c in range(3):
        tiles += sel_tiles(3, c)
    for c in range(4):
        tiles += sel_tiles(4, c)
    for st in range(3):
        tiles += pad_tiles(st)
    tiles += [bar_tile(f, True) for f in range(9)]
    tiles += [bar_tile(f, False) for f in range(9)]
    tiles += [hline_tile(), led_tile(False), led_tile(True)]
    for lab in LABELS:
        tiles += label_tiles(lab)
    assert len(tiles) == NTILES, (len(tiles), NTILES)
    return tiles

def pack4bpp(tile):
    words = []
    for y in range(8):
        w = 0
        for x in range(8):
            w |= (tile[y][x] & 15) << (4 * x)
        words.append(w)
    return words

def rgb(r, g, b):
    return r | (g << 5) | (b << 10)

BG = rgb(1, 2, 5)
BODY = rgb(4, 6, 10)
WHITE = rgb(31, 31, 31)
DIM = rgb(6, 8, 14)
BLUE = rgb(11, 21, 31)
def bank(text, fill, dim=DIM, rim=None):
    return [BG, text, fill, dim, rim if rim is not None else text, BODY, WHITE]
PALETTE = [
    bank(rgb(26, 28, 31), BLUE, DIM, rgb(13, 16, 22)),     # 0 normal: white text, blue value arc
    bank(rgb(31, 27, 6), rgb(31, 27, 6), rgb(14, 11, 2)),  # 1 selected: amber
    bank(rgb(8, 31, 12), rgb(8, 31, 12), rgb(3, 12, 6)),   # 2 playing: green
    bank(BLUE, BLUE, DIM),                                 # 3 titles: blue
    bank(rgb(14, 17, 25), BLUE, DIM, rgb(13, 16, 22)),     # 4 labels: grey-blue
    bank(rgb(9, 11, 17), rgb(9, 11, 17), DIM),             # 5 dim
    bank(rgb(31, 18, 4), rgb(31, 18, 4), rgb(13, 7, 1)),   # 6 queued / accent: orange
    bank(WHITE, WHITE, DIM),                               # 7 bright
]

def palette_flat():
    out = []
    for bank in PALETTE:
        row = bank + [0] * (16 - len(bank))
        out += row
    return out

# ---------------- DSP tables ----------------
def exp2_tab():
    inc0 = MIDI0_HZ / FS * 2 ** 32
    return [int(round(inc0 * 2 ** (i / 256.0))) for i in range(256)]

def cut_g():
    out = []
    for i in range(256):
        fc = 20.0 * (8000.0 / 20.0) ** (i / 255.0)
        g = 1.0 - math.exp(-2 * math.pi * fc / FS)
        out.append(int(round(g * 65536)))
    return out

def dec_coef():
    out = []
    for i in range(256):
        t = 0.010 * (10.0 / 0.010) ** (i / 255.0)      # seconds to fall 60 dB
        c = math.exp(-6.9078 / (t * FS / GROUP))   # per control step (4 samples)
        out.append(min(int(round(c * 2 ** 31)), 2 ** 31 - 1))
    return out

def step_len():
    return [int(round(FS * 15.0 / (40 + i))) for i in range(256)]   # 16th notes

def wave_tabs():
    """4 single-cycle tables of 257 entries (last = first, so linear interpolation needs no wrap).
    All start at 0 and peak at +-28000: SIN, ORG (organ: 4 harmonics), FLD (self-FM sine), HOL (odd harmonics)."""
    def sin(t): return math.sin(t)
    fns = [
        lambda t: sin(t),
        lambda t: sin(t) + 0.55 * sin(2 * t) + 0.35 * sin(3 * t) + 0.22 * sin(4 * t),
        lambda t: sin(t + 2.3 * sin(t)),
        lambda t: sin(t) + 0.6 * sin(3 * t) + 0.4 * sin(5 * t) + 0.25 * sin(7 * t) + 0.12 * sin(9 * t),
    ]
    out = []
    for f in fns:
        v = [f(2 * math.pi * i / 256) for i in range(256)]
        m = max(abs(x) for x in v)
        row = [int(round(28000 * x / m)) for x in v]
        out += row + [row[0]]
    return out

def kclk_coef():
    t = 0.003                                           # click: ~3 ms to fall 60 dB
    return min(int(round(math.exp(-6.9078 / (t * FS / GROUP)) * 2 ** 31)), 2 ** 31 - 1)

def recip():
    # 2^28 / (i*256 + 128): reciprocal of a 16-bit level, indexed by level >> 8 (used by the compressor)
    return [int(round(2 ** 28 / (i * 256 + 128))) for i in range(256)]

def fmt_arr(name, ctype, vals, per=8, attr="IWRAM_DATA"):
    s = f"static {ctype} {name}[{len(vals)}] {attr} = {{\n"
    for i in range(0, len(vals), per):
        s += "  " + ", ".join(str(v) for v in vals[i:i + per]) + ",\n"
    return s + "};\n\n"

def main():
    t = "/* generated by tools/gen_assets.py - do not edit */\n#pragma once\n"
    t += "\n"
    t += fmt_arr("EXP2_TAB", "uint32_t", [f"{v}u" for v in exp2_tab()])
    t += fmt_arr("CUT_G", "int32_t", cut_g())
    t += fmt_arr("DEC_COEF", "int32_t", dec_coef())
    t += fmt_arr("STEP_LEN", "int32_t", step_len())
    t += fmt_arr("RECIP", "int32_t", recip())
    t += fmt_arr("WAVE_TAB", "int16_t", wave_tabs(), 9)
    t += "#define KCLK_COEF %d\n" % kclk_coef()
    open(os.path.join(OUT, "tables.h"), "w").write(t)

    names = ["T_INV0", "T_BIG0", "T_KNOB0", "T_WAVE0", "T_LAMP0", "T_SEL3", "T_SEL4", "T_PAD0",
             "T_BARL0", "T_BARR0", "T_HL", "T_LED_OFF", "T_LED_ON", "T_LAB0"]
    ttxt = "/* generated by tools/gen_assets.py - do not edit */\n#pragma once\n"
    for n in names:
        ttxt += "#define %s %d\n" % (n, globals()[n])
    ttxt += "#define NLAB %d\nstatic const char *const LAB_NAME[NLAB] = { %s };\n" % (
        len(LABELS), ", ".join('"%s"' % l for l in LABELS))
    open(os.path.join(OUT, "tiles.h"), "w").write(ttxt)
    gtxt = "/* generated by tools/gen_assets.py - do not edit */\n#pragma once\n"
    gtxt += f"#define NPAL_ENTRIES {len(PALETTE) * 16}\n#define NTILES {NTILES}\n\n"
    words = []
    for tl in all_tiles():
        words += pack4bpp(tl)
    gtxt += fmt_arr("GFX_TILES", "const uint32_t", [f"0x{w:08X}u" for w in words], 8, "")
    gtxt += fmt_arr("GFX_PAL", "const uint16_t", [f"0x{v:04X}" for v in palette_flat()], 16, "")
    open(os.path.join(OUT, "gfx.h"), "w").write(gtxt)
    print("tables.h + gfx.h written; FS = %.2f Hz" % FS)

if __name__ == "__main__":
    main()

# Changelog

The version is in the file `VERSION`, on the OPTIONS and ABOUT screens, and in the name of the ROM (`GFAM-v6.5.gba`).

## 6.5

- **The ROM is built without the Nintendo logo.** The logo field of the header (`0x04`–`0x9F`) is empty and `tools/fix_header.py` no longer contains the logo. `make LOGO_FROM=file.gba` fills it from a ROM you own, for your own device. `tools/run_tests.sh` checks that the ROMs in `build/` and `releases/` have the field empty. The program itself is the same code as before.

- **`SELECT` (tap) opens OPTIONS** (RESUME · HELP · RESTART APP). It used to be `SELECT + B`.
- **`SELECT + B` switches the BPM edit mode on and off.** It used to be `SELECT` (tap).
- **`B` no longer advances the step** in SYNTH and SEQ. In KICK, `B` is unchanged (STEP EDIT on a pad, audition on the knobs).
- The in-app help is updated to match.
- New test `test/host_buttons.c` (37 checks) for the SELECT / B gestures and the BPM mode.
- `test/host_ui.c` follows the new gesture (`SELECT` tap opens OPTIONS).
- The Makefile creates `build/` if it is missing.

## 6.3 and 6.4

- No changes to the GBA program.

## 6.2

- In PATT the cursor no longer stops at pattern 8 (or at the end of a row): it goes on to the next row while you keep pressing `→`, and `←` goes back the same way.
- References to other devices removed from the code, build file names included.

## 6.1

- **`A + ↑/↓` is the fine adjust** of a value (SYNTH, PATCH, SEQ, FX, kick knobs). It used to be `SELECT + ↑/↓`.
- **`SELECT + ↑/↓`** is now the trig condition on the KICK pads and in SEQ.
- On the KICK pads `A` toggles the trig at once again, and `SELECT + A` is the accent.
- On the tabs where `A` combines with `↑/↓` (SYNTH, SEQ, PATCH, FX, kick knobs and STEP EDIT), `A` acts when you let go of it, and does nothing if it was used for the fine adjust.

## 6.0

- New **PATCH** tab: a patch bay with 8 CV-style inputs, 8 sources and an amount per cable. It is saved in every pattern.
- **Conditional trig for the synth** on every step.
- Conditions extended to **"bar x of N"**: 1:1, 1:2 2:2, 1:3 2:3 3:3, 1:4 … 4:4, 1:8 4:8 8:8, 1:16 8:16 16:16.
- The ROM reaches the 64 KB limit.

## 5.0

- **32 patterns** and a **32-slot song** (8 columns × 4 rows; the PATT grid has 8 columns).
- KICK: **p-locks** (every kick step can have its own TUN, SWP, PDC, ADC, CLK, DRV, WAV and LVL) and **conditional trig** (the kick plays on the first bar of every 1, 2, 3, 4, 8 or 16 bars).
- The kick takes its parameters when it is triggered (they were read live before).
- The control code is compiled with `-Os` so the ROM still fits in 64 KB.

## 4.1

- **Every pattern also keeps its sound** (all synth and kick knobs; BPM and VOL are global).
- `SELECT + START` (random) no longer changes the BPM or the volume.
- The limiter has a **THR** control (threshold from 0 to -10 dB in 0.1 dB steps).

## 4.0

- The whole look is redesigned: a panel with **8 knobs per row** (like 8 pitch knobs and 8 velocity knobs), titled groups, small labels under each knob, big pads for the kick.
- Patterns keep the synth and kick steps.
- New **PATT** tab with 16 patterns, COPY / PASTE / CLEAR / RAND and an 18-slot song with PLAY / STOP.
- `START + ↑/↓` changes the pattern from any tab (at the start of the next bar).
- Presets (`SELECT + L/R`) load only the sound.

## 3.0

- 5 tabs (it was 12 pages), graphic knobs, wave icons, a help window.

## 2.0

- Version number, KICK SEQ with free cursors, 16 kick patterns, SONG MODE.

## 1.x

- Audio engine, kick sequencer, effects, menu, BPM mode.

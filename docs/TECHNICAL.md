# Technical notes

## The program

- **Target**: ARM7TDMI (GBA), `armv4t`. Built with `clang --target=armv4t-none-eabi`, linked with `ld.lld` (`src/link.ld`), turned into a raw binary with `llvm-objcopy`, and finished by `tools/fix_header.py` (game code `ZGFM`, version byte, header checksum, padding). The header's logo field is left empty on purpose: the repository contains no Nintendo material (see [NOTICE.md](../NOTICE.md)); `LOGO_FROM` fills it from a ROM you own, for your own use. No libc, no SDK: `src/crt0.s`, `src/runtime.c` and `src/main.c` are the whole start-up.
- **Size**: the ROM is 64 KB and is at that limit. The control code (`ui.c`, `params.c`) is compiled in Thumb with `-Os`; the DSP (`synth.c`, `fx.c`, `audio.c`) is compiled in ARM with `-O2` and runs from IWRAM.
- **Generated sources**: `tools/gen_assets.py` writes `src/gfx.h` and `src/tiles.h` (tiles and palette, from the 5x7 font in `tools/font.py`) and `src/tables.h` (DSP lookup tables). They are committed, so a build does not need to run it; `make` runs it if you change the script.

## Audio (`src/audio.c`)

- Direct Sound A through DMA1. Timer 0 runs freely at about 18 157 Hz (924 cycles per sample) and is **never restarted**. Timer 1, in cascade, overflows once per buffer (160 samples), and its interrupt re-arms the DMA with the next buffer of a queue of 4.
- The main program keeps the queue full (top priority), and only then handles the buttons and the screen (synchronized with the VBlank interrupt), so a slow redraw does not cut the sound.
- The buffers are a multiple of 16 bytes (the sound DMA moves 16-byte bursts).

## DSP (`src/synth.c`)

- Envelopes, pitch and cutoff are updated once every 4 samples (with linear ramps in the VCAs). Only the oscillators, the filter and the amplifiers run per sample.
- Fixed-point integer arithmetic, no division. Runs from IWRAM in ARM mode.
- The oscillators have no anti-aliasing: some digital grit is normal.
- The patch bay (`PATCH`) scales every source to ±16384 and multiplies it by the cable's amount (-99 to 99) on each of the 8 inputs. `DEC` is applied when the note is triggered, so it only hears SEQ, VEL and RND.

## Effects (`src/fx.c`)

- Compressor, stutter, delay, reverb and a limiter that is always last.
- Delay and stutter live in EWRAM (64 KB and 16 KB). The reverb is a small Freeverb-like one (4 combs and 2 all-pass filters) at half the sample rate, in IWRAM. The delay never plays old buffer content when it is switched on again.
- The limiter has an instant attack, about 100 ms of release, and a threshold `THR` from 0 to -10 dB in 0.1 dB steps (default -0.6 dB). Switched off, it is a hard clip.

## Cost

Measured in mGBA during development (a buffer of 160 samples is 147 840 cycles): about 55 % with no effects, and about 84 % with the four effects on and the kick playing. Redrawing the screen (about 50 000 cycles) is absorbed by the queue. Measure again after a change: `make EXTRA=-DPROFILE` builds a variant that records the render and UI times (`prof_render`, `prof_ui`, `prof_max`), and `-DFX_MASK=n` switches effects on by default to measure them.

## User interface (`src/ui.c`)

- Tilemap UI with a 5x7 bitmap font and a few banks of palette. Six tabs (SYNTH, PATCH, SEQ, KICK, PATT, FX), the OPTIONS menu and a 10-page help.
- The same file compiles on the PC with `-DHOST`, which is how the tests and the screenshots drive it.
- Gestures: the state that decides what a button does is `SELECT` (held, tapped, or used in a combination) and `A` (held for the fine adjust, acting on release). `test/host_buttons.c` checks the `SELECT` / `B` gestures.

## The game code `ZGFM`

gpSP (and mGBA) apply special settings according to the game code in the header. The code that this program used before (`AGFE`) belongs to a commercial game and turned on a workaround of the dynamic recompiler at the IWRAM address 0x030009AC, which here falls inside the audio loop. Do not change the code to one that appears in gpSP's `gba_over.h`.

The header has a correct checksum. It has no logo: a real BIOS needs one, an emulator with its built-in BIOS does not. Besides gpSP and mGBA, some emulators and handheld firmwares load a real BIOS file and play its start-up, which is where the missing logo can show; use `make LOGO_FROM=file.gba` for your own device in that case.

## Tests (`tools/run_tests.sh`)

All of them compile the same DSP and UI code with `gcc -DHOST` and run on the PC.

| Test | What it does |
| --- | --- |
| `host_buttons` | 37 checks: `SELECT` tap, `SELECT + B`, BPM mode, `B` in each tab |
| `host_klock` | Kick p-locks and the 1:N conditions: engine and UI editing |
| `host_pat` | Pattern queue, per-pattern steps, copy / paste, song mode |
| `host_patch` | The patch bay and the synth step conditions |
| `host_rnd` | 300 random presets: all audible, none above the limiter ceiling |
| `host_fxrnd` | Random effect settings (extreme feedback and size) stay bounded by the limiter |
| `host_kick` | Renders every kick preset and every VCO wave |
| `host_fx` | Renders a kick with each effect alone and with all on |
| `host_boot` | Output while stopped, and spurious input at power-on |
| `host_render`, `host_ui` | Render the presets, and dump the UI tilemaps (used by `analyze.py`, `render_ui.py`, `cycles.py`) |

## Screenshots and the cheat sheet

- `tools/make_screens.py` builds `test/host_screens.c`, which drives the real UI code with key presses and dumps the tilemaps; the script then draws them with the app's own tiles and palette into `docs/screens/*.png` (3x). `--sheet` also writes a contact sheet to `build/`.
- `tools/make_cheatsheet.py` has the list of controls in one place and writes `docs/GFAM-cheatsheet.html`, the PDF (through headless Chrome or Chromium), the preview PNGs (`pdftoppm`) and `docs/CONTROLS.md`. When a control changes in `ui.c`, change it there and run it again.

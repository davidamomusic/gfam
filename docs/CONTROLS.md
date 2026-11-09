# GFAM v6.5: controls

Every control of the app, tab by tab. The same list is on the two-page [cheat sheet](GFAM-cheatsheet.pdf) (HTML: [GFAM-cheatsheet.html](GFAM-cheatsheet.html), built by `tools/make_cheatsheet.py`, which also writes this file).

The GBA has these buttons: **D-pad** (← → ↑ ↓), **A**, **B**, **L**, **R**, **START**, **SELECT**.

## Global gestures

These work the same on all 6 tabs (**SYNTH · PATCH · SEQ · KICK · PATT · FX**). On the tabs where A combines with ↑ / ↓ (SYNTH, SEQ, PATCH, FX, the kick knobs and STEP EDIT), A acts when you let go of it, and does nothing if it was used for the fine adjust.

| Button | Action |
| --- | --- |
| `L / R` | Switch tab |
| `← / →` | Select the knob or the step |
| `↑ / ↓` | Turn the knob (hold = repeat). On pads and in PATT they move the cursor |
| `A + ↑ / ↓` | Fine adjust of the value (SYNTH, PATCH, SEQ, FX, kick knobs, STEP EDIT) |
| `SELECT + ↑ / ↓` | Trig condition of the step under the cursor (KICK pads and SEQ) |
| `SELECT + ← / →` | Jump to the next group of knobs |
| `START + ↑ / ↓` | Next / previous pattern, from any tab. If playing, it enters at the start of the next bar; it takes you out of the song |
| `SELECT + L / R` | Previous / next preset: the synth sound on SYNTH and SEQ, the kick sound on KICK (not in STEP EDIT). Steps and BPM untouched |
| `START (tap)` | RUN / STOP. Stopping also stops the song |
| `SELECT (tap)` | Opens OPTIONS: RESUME · HELP · RESTART APP |
| `SELECT + B` | BPM edit mode on / off: ↑ / ↓ ±1, ← / → ±10 |
| `SELECT + START` | RND of what the tab shows: SYNTH the sound · PATCH 2–4 cables · SEQ the steps · KICK the pattern (on the pads) or the kick sound (on the knobs) · PATT everything. Never changes BPM or VOL |

### RND (SELECT + START) by tab

| Tab | What it randomizes |
| --- | --- |
| **SYNTH** | The sound |
| **PATCH** | Plugs 2–4 random cables |
| **SEQ** | The steps |
| **KICK** | The pattern, or the kick sound |
| **PATT** | The whole pattern, steps and sounds |

## SYNTH

23 knobs in rows of 8: VCO 1 · VCO 2 · MOD · FILTER · ENVELOPES · OUTPUT. Waveforms are drawn as icons, switches as lamps.

| Button | Action |
| --- | --- |
| `← / →` | Select the knob |
| `↑ / ↓` | Turn the knob (hold = repeat) |
| `A + ↑ / ↓` | Fine adjust |
| `A (on release)` | Plays the step: the note sounds |
| `SELECT + ← / →` | Jump to the next group of knobs |
| `SELECT + START` | RND of the sound (BPM and VOL untouched) |

VCO waveforms: TRI · SQR · SAW · PLS (25 % pulse) · SIN · ORG (organ) · FLD (sine with auto-FM) · HOL (odd harmonics).

## PATCH

Plugs a source into each of the 8 inputs (jacks), with an amount per cable from -99 to 99 (negative inverts). A new cable starts at 50.

| Button | Action |
| --- | --- |
| `↑ / ↓` | On a jack: pick its source |
| `↑ / ↓` | On the amount knob (bottom row): -99 … 99 |
| `A` | Unplugs the cable (source OFF, amount 0) |
| `A + ↑ / ↓` | Fine adjust |
| `SELECT + START` | Plugs 2–4 random cables |

**Inputs**

| Input | What it modulates |
| --- | --- |
| **VC1 · VC2** | Pitch of VCO 1 and VCO 2 |
| **CUT** | Filter cutoff |
| **RES** | Resonance |
| **FM** | FM amount |
| **VCA** | Level |
| **DEC** | The three decays at once |
| **NSE** | Noise level |

**Sources**

| Source | What it is |
| --- | --- |
| **SEQ** | Pitch of the step |
| **VEL** | Velocity of the step |
| **VCO · VCF · VCA** | The three envelopes |
| **NSE** | Noise |
| **KCK** | Kick envelope |
| **RND** | A new random value on every step |

DEC only hears SEQ, VEL and RND (applied when the note is triggered). Cables belong to the pattern's sound; a preset (SELECT + L / R) removes them. The SEQ knob on the SYNTH tab keeps working on its own.

## SEQ

The synth's 8 steps: a PITCH row and a VELOCITY row (0 = rest), and a conditional trig for each step. Lamps show the step that is playing.

| Button | Action |
| --- | --- |
| `← / →` | Select the step |
| `↑ / ↓` | Turn the pitch or velocity knob |
| `A + ↑ / ↓` | Fine adjust |
| `SELECT + ↑ / ↓` | Change the step's condition (TRIG row); its number turns orange when it has one |
| `SELECT + ← / →` | Jump between the two rows |
| `A (on release)` | Plays the step |
| `SELECT + START` | RND of the steps |

A "bar" is 16 steps, as in the kick, so the 8-step sequencer repeats twice per bar.

## KICK

16 big pads (off / trig / accent) and 8 knobs with the kick sound, which is global to the pattern. A pad's number turns orange when it has a condition or locks.

**On a pad**

| Button | Action |
| --- | --- |
| `← / → / ↑ / ↓` | Move the cursor over the pads (on to the knobs after pad 16) |
| `A` | Toggles the trig, at once |
| `SELECT + A` | Accent |
| `SELECT + ↑ / ↓` | Change the trig condition (TRIG row) |
| `B` | Enters STEP EDIT for that step |
| `SELECT + START` | RND of the pattern or of the kick sound |

**On the kick knobs**

| Button | Action |
| --- | --- |
| `↑ / ↓` | Turn the knob |
| `A + ↑ / ↓` | Fine adjust |
| `B  or  A (on release)` | Hear the kick |
| `SELECT + L / R` | Kick preset |

The 8 knobs: TUN tune · SWP pitch sweep · PDC pitch decay · ADC amplitude decay · CLK click · DRV drive · WAV waveform (SIN / TRI / FLD) · LVL level.

### STEP EDIT (p-locks)

STEP nn LOCKS: the knobs become that step's own. Orange ones are locked, dim ones use the global sound. A step with locks does not change the global sound.

| Button | Action |
| --- | --- |
| `← / →` | Select the knob (the last one is TRIG, the condition) |
| `↑ / ↓` | Turn the knob and lock it (with A = fine adjust) |
| `A (on release)` | Plays that step with its locks |
| `SELECT + A` | Removes the lock (or sets the condition back to every bar) |
| `B` | Back to the pads |

## PATT: patterns and song

32 patterns (4 rows × 8) and a 32-slot song (4 × 8). Green = playing, orange = queued next, dim = empty. Every pattern keeps its own sound.

| Button | Action |
| --- | --- |
| `← / → / ↑ / ↓` | Move the cursor (at the end of a row it goes on to the next) |
| `A on a pattern` | Launches it (at the next bar if playing) |
| `A on COPY / PASTE` | Copy the current pattern, with sound, locks and conditions / paste it over the current one |
| `A on CLEAR` | Erases steps, locks and conditions; keeps the sound |
| `A on RAND` | Randomizes the current pattern |
| `A on a song slot` | Puts the current pattern in it |
| `A + ↑ / ↓` | Held on a song slot: changes its number (-- = empty, skipped) |
| `A on PLAY` | Plays the song from the first filled slot, in a loop; again to stop |
| `START + ↑ / ↓` | Next / previous pattern (from any tab) |
| `SELECT + START` | RND of the whole pattern, steps and sounds |

## FX

A chain of five devices: compressor, stutter, delay, reverb and limiter (always last). They are global: they do not change with the pattern.

| Button | Action |
| --- | --- |
| `← / →` | Select a device or a setting |
| `↑ / ↓` | Turn the value (on a lamp: switch the device on / off) |
| `A + ↑ / ↓` | Fine adjust |
| `A (on release)` | On a lamp: switch the device on / off |

Every device has a lamp (ON) and MIX. AMT = compressor amount · SLC = stutter slice (16 / 32 / 64 / 128) · TIM = delay time (1/8, dotted 1/8, 1/4, dotted 1/4) · FBK = delay feedback · SIZ = reverb size · TON = reverb tone. Limiter: lamp + THR, a threshold from 0 to -10 dB in 0.1 dB steps (default -0.6 dB). The chain is CMP > STU > DLY > REV > LIM.

## Trig conditions

A condition x:N plays the step only on bar x of every N bars; 1:1 = always. Change it with SELECT + ↑ / ↓ on the KICK pads and in SEQ (and as the last knob of STEP EDIT). A bar is 16 steps. The bar counter starts at 0 on RUN and when the pattern changes.

| N | Conditions |
| --- | --- |
| **1** | 1:1 |
| **2** | 1:2 · 2:2 |
| **3** | 1:3 · 2:3 · 3:3 |
| **4** | 1:4 · 2:4 · 3:4 · 4:4 |
| **8** | 1:8 · 4:8 · 8:8 |
| **16** | 1:16 · 8:16 · 16:16 |

## What is saved, and presets

A pattern keeps the synth's 8 steps (pitch + velocity), the kick's 16, both sounds, the PATCH cables, and the kick's locks and conditions. BPM, VOL and the effects are global. Patterns and song are NOT saved when you power off: the factory state comes back (pattern 1 with the default synth steps, kick patterns 1–10 filled in, every pattern with the starting sound, empty song).

| Kind | Presets |
| --- | --- |
| **Synth** | 808 KICK · SEQ BASS · HATS · FM TOMS · MUSICBOX · ZAP SYNC |
| **Kick** | 808 SUB · 909 PUNCH · TECHNO · HARD · DEEP SUB · ZAP KICK |

## Leaving the app

A GBA program cannot close the emulator. RESTART APP (in OPTIONS) does a soft reset; to go back to your launcher use the console's or emulator's own menu button.

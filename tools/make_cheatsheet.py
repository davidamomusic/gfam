#!/usr/bin/env python3
"""Builds the cheat sheet and the controls reference from one list of controls.

    python3 tools/make_cheatsheet.py

writes   docs/GFAM-cheatsheet.html   (two A4 pages; open it in a browser)
         docs/GFAM-cheatsheet.pdf    (the same, through headless Chrome/Chromium, if one is installed)
         docs/cheatsheet-1.png, cheatsheet-2.png   (previews for the README, through pdftoppm)
         docs/CONTROLS.md            (the same controls as Markdown tables)

When a control changes in src/ui.c, change it here too and run this again.
"""
import html
import os
import re
import shutil
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DOCS = os.path.join(ROOT, "docs")
VERSION = open(os.path.join(ROOT, "VERSION")).read().strip()
REPO = "github.com/davidamomusic/gfam"

# ---------------------------------------------------------------------------------------------------------
# The controls. Each table is (button, action); "A (on release)" puts the note in small type.
# ---------------------------------------------------------------------------------------------------------
GLOBAL = [
    ("L / R", "Switch tab"),
    ("← / →", "Select the knob or the step"),
    ("↑ / ↓", "Turn the knob (hold = repeat). On pads and in PATT they move the cursor"),
    ("A + ↑ / ↓", "Fine adjust of the value (SYNTH, PATCH, SEQ, FX, kick knobs, STEP EDIT)"),
    ("SELECT + ↑ / ↓", "Trig condition of the step under the cursor (KICK pads and SEQ)"),
    ("SELECT + ← / →", "Jump to the next group of knobs"),
    ("START + ↑ / ↓", "Next / previous pattern, from any tab. If playing, it enters at the start of the next bar; it takes you out of the song"),
    ("SELECT + L / R", "Previous / next preset: the synth sound on SYNTH and SEQ, the kick sound on KICK (not in STEP EDIT). Steps and BPM untouched"),
    ("START (tap)", "RUN / STOP. Stopping also stops the song"),
    ("SELECT (tap)", "Opens OPTIONS: RESUME · HELP · RESTART APP"),
    ("SELECT + B", "BPM edit mode on / off: ↑ / ↓ ±1, ← / → ±10"),
    ("SELECT + START", "RND of what the tab shows: SYNTH the sound · PATCH 2–4 cables · SEQ the steps · KICK the pattern (on the pads) or the kick sound (on the knobs) · PATT everything. Never changes BPM or VOL"),
]
GLOBAL_NOTE = ("On the tabs where A combines with ↑ / ↓ (SYNTH, SEQ, PATCH, FX, the kick knobs and STEP EDIT), "
               "A acts when you let go of it, and does nothing if it was used for the fine adjust.")

RND = [
    ("SYNTH", "The sound"),
    ("PATCH", "Plugs 2–4 random cables"),
    ("SEQ", "The steps"),
    ("KICK", "The pattern, or the kick sound"),
    ("PATT", "The whole pattern, steps and sounds"),
]

SYNTH_LEAD = ("23 knobs in rows of 8: VCO 1 · VCO 2 · MOD · FILTER · ENVELOPES · OUTPUT. "
              "Waveforms are drawn as icons, switches as lamps.")
SYNTH = [
    ("← / →", "Select the knob"),
    ("↑ / ↓", "Turn the knob (hold = repeat)"),
    ("A + ↑ / ↓", "Fine adjust"),
    ("A (on release)", "Plays the step: the note sounds"),
    ("SELECT + ← / →", "Jump to the next group of knobs"),
    ("SELECT + START", "RND of the sound (BPM and VOL untouched)"),
]
SYNTH_NOTE = "VCO waveforms: TRI · SQR · SAW · PLS (25 % pulse) · SIN · ORG (organ) · FLD (sine with auto-FM) · HOL (odd harmonics)."

PATCH_LEAD = ("Plugs a source into each of the 8 inputs (jacks), with an amount per cable from -99 to 99 "
              "(negative inverts). A new cable starts at 50.")
PATCH = [
    ("↑ / ↓", "On a jack: pick its source"),
    ("↑ / ↓", "On the amount knob (bottom row): -99 … 99"),
    ("A", "Unplugs the cable (source OFF, amount 0)"),
    ("A + ↑ / ↓", "Fine adjust"),
    ("SELECT + START", "Plugs 2–4 random cables"),
]
PATCH_INPUTS = [
    ("VC1 · VC2", "Pitch of VCO 1 and VCO 2"),
    ("CUT", "Filter cutoff"),
    ("RES", "Resonance"),
    ("FM", "FM amount"),
    ("VCA", "Level"),
    ("DEC", "The three decays at once"),
    ("NSE", "Noise level"),
]
PATCH_SOURCES = [
    ("SEQ", "Pitch of the step"),
    ("VEL", "Velocity of the step"),
    ("VCO · VCF · VCA", "The three envelopes"),
    ("NSE", "Noise"),
    ("KCK", "Kick envelope"),
    ("RND", "A new random value on every step"),
]
PATCH_NOTE = ("DEC only hears SEQ, VEL and RND (applied when the note is triggered). Cables belong to the pattern's sound; "
              "a preset (SELECT + L / R) removes them. The SEQ knob on the SYNTH tab keeps working on its own.")

SEQ_LEAD = ("The synth's 8 steps: a PITCH row and a VELOCITY row (0 = rest), and a conditional trig for each step. "
            "Lamps show the step that is playing.")
SEQ = [
    ("← / →", "Select the step"),
    ("↑ / ↓", "Turn the pitch or velocity knob"),
    ("A + ↑ / ↓", "Fine adjust"),
    ("SELECT + ↑ / ↓", "Change the step's condition (TRIG row); its number turns orange when it has one"),
    ("SELECT + ← / →", "Jump between the two rows"),
    ("A (on release)", "Plays the step"),
    ("SELECT + START", "RND of the steps"),
]
SEQ_NOTE = 'A "bar" is 16 steps, as in the kick, so the 8-step sequencer repeats twice per bar.'

KICK_LEAD = ("16 big pads (off / trig / accent) and 8 knobs with the kick sound, which is global to the pattern. "
             "A pad's number turns orange when it has a condition or locks.")
KICK_PADS = [
    ("← / → / ↑ / ↓", "Move the cursor over the pads (on to the knobs after pad 16)"),
    ("A", "Toggles the trig, at once"),
    ("SELECT + A", "Accent"),
    ("SELECT + ↑ / ↓", "Change the trig condition (TRIG row)"),
    ("B", "Enters STEP EDIT for that step"),
    ("SELECT + START", "RND of the pattern or of the kick sound"),
]
KICK_KNOBS = [
    ("↑ / ↓", "Turn the knob"),
    ("A + ↑ / ↓", "Fine adjust"),
    ("B  or  A (on release)", "Hear the kick"),
    ("SELECT + L / R", "Kick preset"),
]
KICK_KNOBS_NOTE = ("The 8 knobs: TUN tune · SWP pitch sweep · PDC pitch decay · ADC amplitude decay · CLK click · "
                   "DRV drive · WAV waveform (SIN / TRI / FLD) · LVL level.")
STEP_LEAD = ("STEP nn LOCKS: the knobs become that step's own. Orange ones are locked, dim ones use the global sound. "
             "A step with locks does not change the global sound.")
STEP = [
    ("← / →", "Select the knob (the last one is TRIG, the condition)"),
    ("↑ / ↓", "Turn the knob and lock it (with A = fine adjust)"),
    ("A (on release)", "Plays that step with its locks"),
    ("SELECT + A", "Removes the lock (or sets the condition back to every bar)"),
    ("B", "Back to the pads"),
]

PATT_LEAD = ("32 patterns (4 rows × 8) and a 32-slot song (4 × 8). Green = playing, orange = queued next, dim = empty. "
             "Every pattern keeps its own sound.")
PATT = [
    ("← / → / ↑ / ↓", "Move the cursor (at the end of a row it goes on to the next)"),
    ("A on a pattern", "Launches it (at the next bar if playing)"),
    ("A on COPY / PASTE", "Copy the current pattern, with sound, locks and conditions / paste it over the current one"),
    ("A on CLEAR", "Erases steps, locks and conditions; keeps the sound"),
    ("A on RAND", "Randomizes the current pattern"),
    ("A on a song slot", "Puts the current pattern in it"),
    ("A + ↑ / ↓", "Held on a song slot: changes its number (-- = empty, skipped)"),
    ("A on PLAY", "Plays the song from the first filled slot, in a loop; again to stop"),
    ("START + ↑ / ↓", "Next / previous pattern (from any tab)"),
    ("SELECT + START", "RND of the whole pattern, steps and sounds"),
]

FX_LEAD = ("A chain of five devices: compressor, stutter, delay, reverb and limiter (always last). "
           "They are global: they do not change with the pattern.")
FX = [
    ("← / →", "Select a device or a setting"),
    ("↑ / ↓", "Turn the value (on a lamp: switch the device on / off)"),
    ("A + ↑ / ↓", "Fine adjust"),
    ("A (on release)", "On a lamp: switch the device on / off"),
]
FX_NOTE = ("Every device has a lamp (ON) and MIX. AMT = compressor amount · SLC = stutter slice (16 / 32 / 64 / 128) · "
           "TIM = delay time (1/8, dotted 1/8, 1/4, dotted 1/4) · FBK = delay feedback · SIZ = reverb size · TON = reverb tone. "
           "Limiter: lamp + THR, a threshold from 0 to -10 dB in 0.1 dB steps (default -0.6 dB). The chain is CMP > STU > DLY > REV > LIM.")

COND_LEAD = ("A condition x:N plays the step only on bar x of every N bars; 1:1 = always. Change it with SELECT + ↑ / ↓ "
             "on the KICK pads and in SEQ (and as the last knob of STEP EDIT). A bar is 16 steps. "
             "The bar counter starts at 0 on RUN and when the pattern changes.")
CONDITIONS = [
    ("1", "1:1"),
    ("2", "1:2 · 2:2"),
    ("3", "1:3 · 2:3 · 3:3"),
    ("4", "1:4 · 2:4 · 3:4 · 4:4"),
    ("8", "1:8 · 4:8 · 8:8"),
    ("16", "1:16 · 8:16 · 16:16"),
]

SAVED = ("A pattern keeps the synth's 8 steps (pitch + velocity), the kick's 16, both sounds, the PATCH cables, and the "
         "kick's locks and conditions. BPM, VOL and the effects are global. Patterns and song are NOT saved when you power off: "
         "the factory state comes back (pattern 1 with the default synth steps, kick patterns 1–10 filled in, "
         "every pattern with the starting sound, empty song).")
PRESETS = [
    ("Synth", "808 KICK · SEQ BASS · HATS · FM TOMS · MUSICBOX · ZAP SYNC"),
    ("Kick", "808 SUB · 909 PUNCH · TECHNO · HARD · DEEP SUB · ZAP KICK"),
]


# ---------------------------------------------------------------------------------------------------------
# HTML
# ---------------------------------------------------------------------------------------------------------
def e(s):
    return html.escape(s, quote=False)


def keys_html(s):
    m = re.match(r"^(.*?)(?: \((.*)\))?$", s)
    main, note = m.group(1), m.group(2)
    out = []
    for i, part in enumerate(main.split(" + ")):
        alts = []
        for alt in part.split(" / "):
            alt = alt.strip()
            if alt in ("A", "B", "L", "R", "START", "SELECT", "←", "→", "↑", "↓"):
                alts.append("<kbd>%s</kbd>" % e(alt))
            else:
                alts.append('<span class="lbl">%s</span>' % e(alt))
        out.append('<span class="slash"> / </span>'.join(alts))
    h = '<span class="plus">+</span>'.join(out)
    if note:
        h += ' <small>(%s)</small>' % e(note)
    return h


def table_html(rows, cls=""):
    r = "".join("<tr><td class=\"k\">%s</td><td>%s</td></tr>" % (keys_html(k), e(v)) for k, v in rows)
    return '<table class="%s">%s</table>' % (cls, r)


def plain_table_html(rows, cls=""):
    r = "".join("<tr><td class=\"k\"><b>%s</b></td><td>%s</td></tr>" % (e(k), e(v)) for k, v in rows)
    return '<table class="%s">%s</table>' % (cls, r)


def section(title, lead, body, color="blue", note=None):
    h = '<section class="%s"><h2>%s</h2>' % (color, e(title))
    if lead:
        h += '<p class="lead">%s</p>' % e(lead)
    h += body
    if note:
        h += '<p class="note">%s</p>' % e(note)
    return h + "</section>"


CSS = """
@page { size: A4; margin: 9mm 10mm; }
* { box-sizing: border-box; }
html { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
body { margin: 0; font: 8.4pt/1.3 "Helvetica Neue", Helvetica, Arial, "Liberation Sans", "DejaVu Sans", sans-serif; color: #151a2c; background: #fff; }
.page { width: 190mm; height: 279mm; position: relative; overflow: hidden; page-break-after: always; }
.page:last-child { page-break-after: auto; }
header { background: #0b1230; color: #fff; padding: 4.2mm 5mm 3.6mm; border-radius: 2.2mm; display: flex; align-items: baseline; justify-content: space-between; margin-bottom: 3.2mm; }
header h1 { margin: 0; font-size: 17pt; letter-spacing: .6pt; }
header h1 span { color: #4aa8ff; }
header .sub { font-size: 8pt; color: #aab4d8; text-align: right; }
.cols { display: flex; gap: 4mm; align-items: flex-start; }
.col { flex: 1 1 0; min-width: 0; }
.grid2 { display: flex; gap: 3mm; }
.grid2 > div { flex: 1 1 0; min-width: 0; }
.grid2 td.k { width: 38%; }
section { break-inside: avoid; margin: 0 0 3mm; border: .3mm solid #d4d9ea; border-top: 1.1mm solid var(--c); border-radius: 1.6mm; padding: 1.8mm 2.6mm 2.2mm; background: #fff; }
section.blue { --c: #3d8bff; } section.green { --c: #2fbf5b; } section.yellow { --c: #f2b705; }
section.orange { --c: #f07c2a; } section.purple { --c: #8b6cf0; } section.gray { --c: #7b84a6; }
h2 { margin: 0 0 1mm; font-size: 10.5pt; letter-spacing: .5pt; color: #0b1230; }
h3 { margin: 1.6mm 0 .6mm; font-size: 8pt; text-transform: uppercase; letter-spacing: .6pt; color: #55608a; }
p.lead { margin: 0 0 1.2mm; color: #3a4262; }
p.note { margin: 1.2mm 0 0; color: #3a4262; font-size: 7.8pt; }
table { width: 100%; border-collapse: collapse; margin: .4mm 0; }
td { padding: .55mm 0; vertical-align: top; border-bottom: .2mm solid #eceff8; }
tr:last-child td { border-bottom: 0; }
td.k { width: 41%; padding-right: 2mm; white-space: nowrap; }
table.wide td.k { width: 30%; }
table.narrow td.k { width: 36%; }
kbd { display: inline-block; font: 700 7.2pt/1 "Helvetica Neue", Helvetica, Arial, sans-serif; background: #eef1fb; border: .25mm solid #b9c2e0; border-bottom-width: .5mm; border-radius: .9mm; padding: .5mm 1.2mm .4mm; color: #0b1230; min-width: 4mm; text-align: center; }
.plus { color: #7b84a6; margin: 0 .5mm; font-weight: 700; }
.slash { color: #7b84a6; }
.lbl { font-weight: 700; }
small { color: #6a7396; font-size: 7pt; }
footer { position: absolute; left: 0; right: 0; bottom: 0; font-size: 7pt; color: #6a7396; border-top: .25mm solid #d4d9ea; padding-top: 1.4mm; display: flex; justify-content: space-between; }
"""


def build_html():
    sec_global = section("Global gestures", "These work the same on all 6 tabs: SYNTH · PATCH · SEQ · KICK · PATT · FX.",
                         table_html(GLOBAL, "wide"), "blue", GLOBAL_NOTE)
    sec_rnd = section("RND (SELECT + START) by tab", None, plain_table_html(RND, "narrow"), "gray")
    sec_synth = section("SYNTH", SYNTH_LEAD, table_html(SYNTH), "green", SYNTH_NOTE)
    sec_seq = section("SEQ", SEQ_LEAD, table_html(SEQ), "yellow", SEQ_NOTE)
    sec_cond = section("Trig conditions", COND_LEAD, plain_table_html(CONDITIONS, "narrow"), "purple")
    patch_body = (table_html(PATCH) + '<div class="grid2"><div><h3>Inputs</h3>' + plain_table_html(PATCH_INPUTS) +
                  '</div><div><h3>Sources</h3>' + plain_table_html(PATCH_SOURCES) + '</div></div>')
    sec_patch = section("PATCH", PATCH_LEAD, patch_body, "purple", PATCH_NOTE)
    kick_body = ("<h3>On a pad</h3>" + table_html(KICK_PADS) + "<h3>On the kick knobs</h3>" + table_html(KICK_KNOBS))
    sec_kick = section("KICK", KICK_LEAD, kick_body, "orange", KICK_KNOBS_NOTE)
    sec_step = section("KICK: STEP EDIT (p-locks)", STEP_LEAD, table_html(STEP), "orange")
    sec_patt = section("PATT: patterns and song", PATT_LEAD, table_html(PATT, "wide"), "blue")
    sec_fx = section("FX", FX_LEAD, table_html(FX), "green", FX_NOTE)
    sec_saved = section("What is saved, and presets", SAVED, plain_table_html(PRESETS, "narrow"), "gray")

    def page(title_sub, left, right, num):
        return ('<div class="page"><header><h1>GFAM <span>v{ver}</span> cheat sheet</h1><div class="sub">{sub}</div></header>'
                '<div class="cols"><div class="col">{l}</div><div class="col">{r}</div></div>'
                '<footer><span>{repo} &middot; independent hobby project, not affiliated with Nintendo (see NOTICE.md)</span>'
                '<span>page {num}</span></footer></div>').format(
                    ver=VERSION, sub=title_sub, l="".join(left), r="".join(right), repo=REPO, num=num)

    pg1 = page("Drum synth for the Game Boy Advance &middot; gestures, SYNTH, SEQ, FX",
               [sec_global, sec_fx], [sec_synth, sec_seq, sec_cond], "1 / 2")
    pg2 = page("KICK, PATCH, PATT, what is saved",
               [sec_kick, sec_step, sec_saved], [sec_patch, sec_patt], "2 / 2")
    return ('<!DOCTYPE html><html lang="en"><head><meta charset="utf-8"><title>GFAM v' + VERSION + ' cheat sheet</title>'
            '<style>' + CSS + '</style></head><body>' + pg1 + pg2 + '</body></html>')


# ---------------------------------------------------------------------------------------------------------
# Markdown
# ---------------------------------------------------------------------------------------------------------
def md_table(rows, head=("Button", "Action"), keys=True):
    out = ["| %s | %s |" % head, "| --- | --- |"]
    for k, v in rows:
        out.append("| %s | %s |" % (("`%s`" % k) if keys else "**%s**" % k, v.replace("|", "\\|")))
    return "\n".join(out)


def build_md():
    s = []
    s.append("# GFAM v%s: controls\n" % VERSION)
    s.append("Every control of the app, tab by tab. The same list is on the two-page [cheat sheet](GFAM-cheatsheet.pdf) "
             "(HTML: [GFAM-cheatsheet.html](GFAM-cheatsheet.html), built by `tools/make_cheatsheet.py`, which also writes this file).\n")
    s.append("The GBA has these buttons: **D-pad** (← → ↑ ↓), **A**, **B**, **L**, **R**, **START**, **SELECT**.\n")
    s.append("## Global gestures\n\n" + GLOBAL_NOTE.replace("On the tabs", "These work the same on all 6 tabs "
             "(**SYNTH · PATCH · SEQ · KICK · PATT · FX**). On the tabs", 1) + "\n")
    s.append(md_table(GLOBAL) + "\n")
    s.append("### RND (SELECT + START) by tab\n\n" + md_table(RND, ("Tab", "What it randomizes"), keys=False) + "\n")
    s.append("## SYNTH\n\n" + SYNTH_LEAD + "\n\n" + md_table(SYNTH) + "\n\n" + SYNTH_NOTE + "\n")
    s.append("## PATCH\n\n" + PATCH_LEAD + "\n\n" + md_table(PATCH) + "\n\n**Inputs**\n\n" +
             md_table(PATCH_INPUTS, ("Input", "What it modulates"), keys=False) + "\n\n**Sources**\n\n" +
             md_table(PATCH_SOURCES, ("Source", "What it is"), keys=False) + "\n\n" + PATCH_NOTE + "\n")
    s.append("## SEQ\n\n" + SEQ_LEAD + "\n\n" + md_table(SEQ) + "\n\n" + SEQ_NOTE + "\n")
    s.append("## KICK\n\n" + KICK_LEAD + "\n\n**On a pad**\n\n" + md_table(KICK_PADS) +
             "\n\n**On the kick knobs**\n\n" + md_table(KICK_KNOBS) + "\n\n" + KICK_KNOBS_NOTE + "\n")
    s.append("### STEP EDIT (p-locks)\n\n" + STEP_LEAD + "\n\n" + md_table(STEP) + "\n")
    s.append("## PATT: patterns and song\n\n" + PATT_LEAD + "\n\n" + md_table(PATT) + "\n")
    s.append("## FX\n\n" + FX_LEAD + "\n\n" + md_table(FX) + "\n\n" + FX_NOTE + "\n")
    s.append("## Trig conditions\n\n" + COND_LEAD + "\n\n" + md_table(CONDITIONS, ("N", "Conditions"), keys=False) + "\n")
    s.append("## What is saved, and presets\n\n" + SAVED + "\n\n" + md_table(PRESETS, ("Kind", "Presets"), keys=False) + "\n")
    s.append("## Leaving the app\n\nA GBA program cannot close the emulator. RESTART APP (in OPTIONS) does a soft reset; "
             "to go back to your launcher use the console's or emulator's own menu button.\n")
    return "\n".join(s)


def find_chrome():
    for n in ("google-chrome", "chromium", "chromium-browser", "chrome"):
        p = shutil.which(n)
        if p:
            return p
    return None


def main():
    os.makedirs(DOCS, exist_ok=True)
    html_path = os.path.join(DOCS, "GFAM-cheatsheet.html")
    open(html_path, "w", encoding="utf-8").write(build_html())
    open(os.path.join(DOCS, "CONTROLS.md"), "w", encoding="utf-8").write(build_md())
    print("wrote docs/GFAM-cheatsheet.html and docs/CONTROLS.md")

    chrome = find_chrome()
    if not chrome:
        print("no Chrome/Chromium found: PDF and previews skipped")
        return
    pdf = os.path.join(DOCS, "GFAM-cheatsheet.pdf")
    subprocess.check_call([chrome, "--headless=new", "--no-sandbox", "--disable-gpu", "--no-pdf-header-footer",
                           "--print-to-pdf=" + pdf, "file://" + html_path],
                          stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print("wrote docs/GFAM-cheatsheet.pdf")
    if shutil.which("pdftoppm"):
        subprocess.check_call(["pdftoppm", "-r", "80", "-png", pdf, os.path.join(DOCS, "cheatsheet")])
        for f in sorted(os.listdir(DOCS)):          # pdftoppm names them cheatsheet-1.png (or -01.png)
            m = re.match(r"cheatsheet-0*(\d+)\.png$", f)
            if m and f != "cheatsheet-%s.png" % m.group(1):
                os.replace(os.path.join(DOCS, f), os.path.join(DOCS, "cheatsheet-%s.png" % m.group(1)))
        print("wrote docs/cheatsheet-N.png")


if __name__ == "__main__":
    sys.exit(main())

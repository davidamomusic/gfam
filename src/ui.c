/* GFAM user interface: 6 tabs (SYNTH, PATCH, SEQ, KICK, PATT, FX). Every tab is one screen laid out like a
 * hardware panel: rows of big knobs (8 per row, in groups of 4), titles with lines, no help text.
 * LEFT/RIGHT walks over the cells of the tab, UP/DOWN turns the knob under the cursor.
 * START+UP/DOWN picks the pattern from anywhere (it starts at the next bar while running).
 * All help lives in the window that a SELECT tap opens. */
#include <stdint.h>
#include "synth.h"
#include "ui.h"
#include "tiles.h"

#ifndef GFAM_VERSION
#define GFAM_VERSION "0.0"
#endif

enum { TAB_SYNTH, TAB_PATCH, TAB_SEQ, TAB_KICK, TAB_PATT, TAB_FX, NTABS };

typedef struct { uint8_t id; const char *lab; } Cell;

static const Cell SYN[23] = {
    { P_V1W, "WAV" }, { P_V1F, "FRQ" }, { P_V1E, "EG" }, { P_L1, "LVL" },
    { P_V2W, "WAV" }, { P_V2F, "FRQ" }, { P_V2E, "EG" }, { P_L2, "LVL" },
    { P_SYNC, "SYN" }, { P_SMOD, "SEQ" }, { P_FM, "FM" }, { P_LN, "NSE" },
    { P_FMODE, "MOD" }, { P_CUT, "CUT" }, { P_RES, "RES" }, { P_FEG, "EG" },
    { P_VDEC, "VCO" }, { P_FDEC, "VCF" }, { P_ADEC, "VCA" }, { P_AATK, "ATK" },
    { P_VOL, "VOL" }, { P_TEMPO, "BPM" }, { P_NMOD, "NMD" } };
static const char *const SYN_TITLE[6] = { "VCO 1", "VCO 2", "MOD", "FILTER", "ENVELOPES", "OUTPUT" };

static const Cell KSND[8] = {
    { P_KTUN, "TUN" }, { P_KSWP, "SWP" }, { P_KPDC, "PDC" }, { P_KADC, "ADC" },
    { P_KCLK, "CLK" }, { P_KDRV, "DRV" }, { P_KWAV, "WAV" }, { P_KLEV, "LVL" } };

static const Cell FXC[16] = {
    { P_FCMP, "ON" }, { P_FCMPM, "MIX" }, { P_FCAM, "AMT" },
    { P_FSTU, "ON" }, { P_FSTUM, "MIX" }, { P_FSTR, "SLC" },
    { P_FDLY, "ON" }, { P_FDLYM, "MIX" }, { P_FDLT, "TIM" }, { P_FDFB, "FBK" },
    { P_FREV, "ON" }, { P_FREVM, "MIX" }, { P_FRSZ, "SIZ" }, { P_FRTN, "TON" },
    { P_FLIM, "ON" }, { P_FLTH, "THR" } };
static const uint8_t FX_ROW[16] = { 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2 };
static const uint8_t FX_COL[16] = { 0, 1, 2, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1 };

static const uint8_t NCELL[NTABS] = { 23, 16, 16, 25, 0, 16 };
static const char *const TAB_NAME[NTABS] = { "SYNTH", "PATCH", "SEQ", "KICK", "PATT", "FX" };
static const uint8_t TAB_X[NTABS] = { 1, 7, 13, 17, 22, 27 };

static int tab;
static int kedit, kstep;                 /* KICK tab: step edit (p-locks + trig condition) of step kstep */
/* sound names are kept per pattern (the sound itself lives in the pattern) */
static uint8_t pre_i[NPAT], kpre_i[NPAT], pre_r[NPAT], kpre_r[NPAT];
static uint8_t clip_pi, clip_ki, clip_pr, clip_kr;
#define preset   pre_i[synth_kplay()]
#define kpreset  kpre_i[synth_kplay()]
#define randomized pre_r[synth_kplay()]
#define krandom  kpre_r[synth_kplay()]
static int curs[NTABS];                 /* cell index (PATT: dy * 8 + dx) */
#define cursor curs[tab]
static unsigned prev, held_now;
static int cnt[4], rep[4];
static int dirty = 1;
static unsigned last_sig = ~0u;
static uint32_t ticks;
static int armed;
static int bpm_mode, sel_used, start_used, a_used;
static int menu, menu_sel, help_page;   /* menu: 0 closed, 1 options, 2 help */

void app_restart(void) __attribute__((weak));

void ui_init(void)
{
    tab = 0; kedit = 0; kstep = 0;
    for (int i = 0; i < NPAT; i++) pre_i[i] = kpre_i[i] = pre_r[i] = kpre_r[i] = 0;
    for (int i = 0; i < NTABS; i++) curs[i] = 0;
    prev = 0; held_now = 0; armed = 0; ticks = 0;
    bpm_mode = 0; sel_used = 0; start_used = 0; a_used = 0;
    menu = 0; menu_sel = 0; help_page = 0;
    for (int i = 0; i < 4; i++) cnt[i] = rep[i] = 0;
}

/* ---------- cells ---------- */
static const Cell *cell_of(int t, int c)
{
    switch (t) {
    case TAB_SYNTH: return &SYN[c];
    case TAB_KICK:  return (c >= 16 && c < 24) ? &KSND[c - 16] : 0;
    case TAB_FX:    return &FXC[c];
    default:        return 0;
    }
}

static int cell_id(int t, int c)                  /* parameter under a cell, -1 for pads / patterns */
{
    if (t == TAB_SEQ) return c < 8 ? P_PIT0 + c : P_VEL0 + (c - 8);
    if (t == TAB_PATCH) return c < 8 ? P_PBS0 + c : P_PBA0 + (c - 8);
    const Cell *k = cell_of(t, c);
    return k ? k->id : -1;
}

static void cell_pos(int t, int c, int *x, int *y)  /* top-left tile of the cell's block (title row of a knob) */
{
    switch (t) {
    case TAB_SYNTH: { int g = c >> 2; *x = 2 + ((g & 1) ? 13 : 0) + 3 * (c & 3); *y = 1 + 6 * (g >> 1); break; }
    case TAB_SEQ:   { int s = c & 7; *x = 2 + 3 * s + (s >= 4); *y = c < 8 ? 1 : 7; break; }
    case TAB_KICK:
        if (c < 16) { int s = c & 7; *x = 2 + 3 * s + (s >= 4); *y = c < 8 ? 2 : 6; }
        else if (c < 24) { int s = c - 16; *x = 2 + 3 * s + (s >= 4); *y = 11; }
        else { *x = 8; *y = 17; }
        break;
    default:        *x = 2 + 3 * FX_COL[c] + (FX_COL[c] >= 4); *y = 1 + 6 * FX_ROW[c]; break;
    }
}

static const uint8_t GSTART_SYN[6] = { 0, 4, 8, 12, 16, 20 };
static const uint8_t GSTART_SEQ[2] = { 0, 8 };   /* also the PATCH tab: sources, amounts */
static const uint8_t GSTART_KICK[3] = { 0, 8, 16 };
static const uint8_t GSTART_FX[5] = { 0, 3, 6, 10, 14 };

static void jump_group(int dir)
{
    const uint8_t *g; int n;
    switch (tab) {
    case TAB_SYNTH: g = GSTART_SYN; n = 6; break;
    case TAB_SEQ: case TAB_PATCH: g = GSTART_SEQ; n = 2; break;
    case TAB_KICK:  g = GSTART_KICK; n = 3; break;
    case TAB_FX:    g = GSTART_FX; n = 5; break;
    default: return;
    }
    int s = 0;
    for (int i = 0; i < n; i++) if (cursor >= g[i]) s = i;
    s += dir;
    if (s < 0) s = n - 1;
    if (s >= n) s = 0;
    cursor = g[s];
}

static void tempo_change(int d)
{
    int t = P[P_TEMPO] + d;
    P[P_TEMPO] = (int16_t)(t < 0 ? 0 : (t > 255 ? 255 : t));
}

static int cur_or_cue(void) { int q = synth_kqueued(); return q >= 0 ? q : synth_kplay(); }

static void pick_pattern(int p)                   /* manual pattern change leaves song mode */
{
    if (synth_song_on()) synth_song_off();
    synth_queue_pattern(p);
}

/* PATT cursor helpers: cursor = dy * 8 + dx. dy 0..3 patterns (8 per row), dy 4 commands
   (dx 0..3 COPY PASTE CLEAR RAND, dx 4 PLAY), dy 5..8 song slots (8 per row) */
static int patt_dxmax(int dy) { return dy == 4 ? 4 : 7; }

static void patt_move(int k)
{
    int dy = cursor >> 3, dx = cursor & 7;
    if (k == 0) { if (dx < patt_dxmax(dy)) dx++; else { dx = 0; dy = dy < 8 ? dy + 1 : 0; } }   /* runs on to the next row */
    else if (k == 1) { if (dx > 0) dx--; else { dy = dy ? dy - 1 : 8; dx = patt_dxmax(dy); } }
    else if (k == 2 && dy > 0) dy--;
    else if (k == 3 && dy < 8) dy++;
    if (dx > patt_dxmax(dy)) dx = patt_dxmax(dy);
    cursor = dy * 8 + dx;
}

static int song_slot(void)                        /* song slot under the PATT cursor, -1 if none */
{
    int dy = cursor >> 3, dx = cursor & 7;
    return dy >= 5 ? (dy - 5) * 8 + dx : -1;
}

static void cond_cycle(uint8_t *c, int dir)
{
    int v = *c + dir;
    *c = (uint8_t)(v < 0 ? NCOND - 1 : (v >= NCOND ? 0 : v));
}

static void dpad_action(int k, int alt)           /* k: 0 right, 1 left, 2 up, 3 down */
{
    dirty = 1;
    if (bpm_mode && !alt) {
        if (k == 0) tempo_change(10);
        else if (k == 1) tempo_change(-10);
        else if (k == 2) tempo_change(1);
        else tempo_change(-1);
        return;
    }
    if ((held_now & KEY_START) && k >= 2) {          /* START + UP/DOWN: pattern */
        int n = cur_or_cue() + (k == 2 ? 1 : -1);
        pick_pattern(n < 0 ? NPAT - 1 : (n >= NPAT ? 0 : n));
        start_used = 1;
        return;
    }
    if (alt && k < 2) { if (!(tab == TAB_KICK && kedit)) jump_group(k == 0 ? 1 : -1); return; }
    if (alt && k >= 2) {                                        /* SELECT + UP/DOWN: trig condition of the step under the cursor */
        if (tab == TAB_KICK && !kedit && cursor < 16) { cond_cycle(&KCND[synth_kplay()][cursor], k == 2 ? 1 : -1); return; }
        if (tab == TAB_SEQ) { cond_cycle(&SCND[synth_kplay()][cursor & 7], k == 2 ? 1 : -1); return; }
    }
    int fine = 0;                                               /* A + UP/DOWN: fine value (A then does nothing on release) */
    if ((held_now & KEY_A) && k >= 2 && tab != TAB_PATT && !(tab == TAB_KICK && !kedit && cursor < 16)) { fine = 1; a_used = 1; }

    if (tab == TAB_PATT) {
        int s = song_slot();
        if ((held_now & KEY_A) && k >= 2 && s >= 0) {   /* A held + UP/DOWN: change the slot */
            int v = SONG[s] + (k == 2 ? 1 : -1);
            SONG[s] = (uint8_t)(v < 0 ? NPAT : (v > NPAT ? 0 : v));
            a_used = 1;
            return;
        }
        patt_move(k);
        return;
    }
    if (tab == TAB_KICK && kedit) {                  /* step edit: knobs 16..23 and the condition 24 */
        if (k == 0) cursor = cursor >= 24 ? 16 : cursor + 1;
        else if (k == 1) cursor = cursor <= 16 ? 24 : cursor - 1;
        else if (cursor == 24) cond_cycle(&KCND[synth_kplay()][kstep], k == 2 ? 1 : -1);
        else {
            int i = cursor - 16, id = P_KTUN + i, sv = P[id];
            P[id] = (int16_t)klock_get(kstep, i);    /* edit the step's value, then put the global one back */
            param_change(id, k == 2 ? 1 : -1, fine);
            klock_set(kstep, i, P[id]);
            P[id] = (int16_t)sv;
        }
        return;
    }
    int n = tab == TAB_KICK ? 24 : NCELL[tab], c = cursor;
    if (tab == TAB_KICK && c < 16 && k >= 2) { cursor = c ^ 8; return; }   /* pads: UP/DOWN switch row */
    if (k == 0) cursor = (c + 1 >= n) ? 0 : c + 1;
    else if (k == 1) cursor = c ? c - 1 : n - 1;
    else {
        int id = cell_id(tab, c);
        if (id >= 0) {
            int old = P[id];
            param_change(id, k == 2 ? 1 : -1, fine);
            if (tab == TAB_PATCH && id < P_PBA0 && old == 0 && P[id] != 0 && P[P_PBA0 + id - P_PBS0] == 0)
                P[P_PBA0 + id - P_PBS0] = 50;                  /* a new cable starts at half depth */
        }
    }
}

/* ---------- menu / help ---------- */
static const char *const HELP[][16] = {
 { "BASICS",
   "L/R SHOULDER: CHANGE TAB",
   "START: RUN / STOP",
   "START+UP/DOWN: PATTERN",
   " (STARTS AT THE NEXT BAR)",
   "SELECT TAP: THIS MENU",
   "SELECT+B: BPM EDIT MODE",
   "SELECT+START: RANDOMIZE",
   "SELECT+L/R: SOUND PRESET",
   "A+UP/DOWN: FINE VALUE",
   "SELECT+LEFT/RIGHT: NEXT",
   " GROUP OF KNOBS",
   ":BPM EDIT MODE",
   "UP/DOWN +-1  LEFT/RIGHT +-10", 0 },
 { "SYNTH TAB",
   "LEFT/RIGHT: PICK A KNOB",
   "UP/DOWN: TURN IT",
   "A+UP/DOWN: FINE VALUE",
   "A: PLAY STEP (ON RELEASE)",
   "SELECT+L/R: SOUND PRESET",
   "SELECT+START: RANDOM SOUND",
   ":ROWS",
   "VCO1 AND VCO2",
   "MOD (SYNC SEQ FM NOISE)",
   "AND THE FILTER",
   "ENVELOPES AND OUTPUT", 0 },
 { "SEQ TAB",
   "8 PITCH KNOBS (SEMITONES)",
   "8 VELOCITY KNOBS (0 = REST)",
   "LEFT/RIGHT: PICK A STEP",
   "UP/DOWN: TURN THE KNOB",
   "A: PLAY STEP (ON RELEASE)",
   "A+UP/DOWN: FINE VALUE",
   "SELECT+LEFT/RIGHT: JUMP",
   "BETWEEN THE ROWS",
   "SELECT+START: RANDOM STEPS",
   "THE LAMPS SHOW THE STEP",
   "THAT IS PLAYING",
   "SELECT+UP/DOWN: CONDITION",
   "(ORANGE NUMBER = HAS ONE)", 0 },
 { "PATCH TAB: CABLES",
   "8 INPUTS: VC1 VC2 = PITCH,",
   "CUT RES FM VCA, DEC = ALL",
   "DECAY TIMES, NSE = LEVEL",
   "TOP ROW: PICK A SOURCE",
   "(UP/DOWN), BOTTOM: AMOUNT",
   "A: UNPLUG THE CABLE",
   "SELECT+START: RANDOM CABLES",
   ":SOURCES",
   "SEQ PITCH, VEL VELOCITY,",
   "VCO/VCF/VCA ENVELOPES, NSE",
   "NOISE, KCK KICK ENVELOPE,",
   "RND NEW VALUE EVERY STEP",
   "(DEC ONLY HEARS SEQ VEL RND)", 0 },
 { "KICK TAB",
   "16 PADS: OFF / TRIG / ACCENT",
   "LEFT/RIGHT: MOVE (ON TO THE",
   "KNOBS AFTER PAD 16)",
   "UP/DOWN ON A PAD: OTHER ROW",
   "A: TRIG ON/OFF",
   "SELECT+A: ACCENT",
   "SELECT+UP/DOWN: CONDITION",
   "B ON A PAD: STEP EDIT",
   "SELECT+START: RANDOM PATTERN",
   ":SOUND KNOBS",
   "UP/DOWN TURN, A+UP/DOWN FINE",
   "A (RELEASE) OR B: HEAR IT",
   "SELECT+L/R: KICK PRESET",
   "SELECT+START: RANDOM SOUND", 0 },
 { "KICK: STEP EDIT",
   "B ON A PAD OPENS STEP EDIT:",
   "THE KNOBS BECOME THAT STEP'S",
   "LOCKS. TURN ONE TO LOCK IT",
   "(ORANGE). DIM = GLOBAL SOUND",
   "LEFT/RIGHT: KNOBS + EVERY",
   "UP/DOWN ON TRIG: CONDITION",
   "BAR X OF N: 1:1 EVERY BAR,",
   "2:4 = 2ND BAR OF EACH 4.",
   "BARS COUNT FROM RUN OR A",
   "PATTERN CHANGE",
   "A: HEAR THE STEP",
   "SELECT+A: UNLOCK / RESET",
   "B: BACK TO THE PADS", 0 },
 { "PATT: PATTERNS",
   "A PATTERN HOLDS THE SYNTH",
   "AND KICK STEPS AND BOTH",
   "SOUNDS (BPM IS GLOBAL)",
   "ARROWS: MOVE",
   "A ON A PATTERN: PLAY IT",
   "(NEXT BAR IF RUNNING)",
   "COPY/PASTE/CLEAR/RAND ACT",
   "ON THE CURRENT PATTERN",
   "BRIGHT NUMBERS HAVE NOTES",
   "GREEN = CURRENT",
   "ORANGE = QUEUED NEXT",
   "SELECT+START: RANDOM ALL", 0 },
 { "PATT: SONG",
   "32 SLOTS PLAYED IN ORDER,",
   "ONE BAR EACH, EMPTY SLOTS",
   "ARE SKIPPED",
   "A ON A SLOT: PUT THE",
   "CURRENT PATTERN THERE",
   "HOLD A + UP/DOWN: CHANGE",
   "THE NUMBER (-- = EMPTY)",
   "A ON PLAY (RIGHT OF RAND):",
   "START / STOP THE SONG.",
   "START ALSO STOPS IT.",
   "PICKING A PATTERN BY HAND",
   "LEAVES THE SONG", 0 },
 { "FX TAB",
   "ON = LAMP, MIX = AMOUNT",
   "A OR UP/DOWN: SWITCH LAMP",
   "COMPRESSOR: AMT = AMOUNT",
   "STUTTER: SLC = SLICE",
   "DELAY: TIM TIME, FBK FEEDBACK",
   "REVERB: SIZ SIZE, TON TONE",
   "LIMITER: THR = THRESHOLD",
   " 0 TO -10 DB",
   "THE LIMITER IS ALWAYS LAST",
   "CHAIN: CMP STU DLY REV LIM", 0 },
 { "ABOUT",
   "GFAM V" GFAM_VERSION,
   "DRUM SYNTH FOR THE GBA",
   "PATTERNS AND SONG ARE NOT",
   "SAVED WHEN YOU POWER OFF",
   "A GBA ROM CANNOT CLOSE THE",
   "EMULATOR. RESTART APP",
   "RELOADS THE PROGRAM. TO QUIT",
   "USE THE MIYOO MENU BUTTON", 0 },
};
#define NHELP ((int)(sizeof HELP / sizeof HELP[0]))

static void menu_update(unsigned pressed)
{
    dirty = 1;
    if (menu == 2) {
        if (pressed & (KEY_RIGHT | KEY_R)) help_page = help_page + 1 >= NHELP ? 0 : help_page + 1;
        if (pressed & (KEY_LEFT | KEY_L))  help_page = help_page ? help_page - 1 : NHELP - 1;
        if (pressed & (KEY_B | KEY_START | KEY_A)) menu = 1;
        return;
    }
    if (pressed & KEY_UP)   menu_sel = menu_sel ? menu_sel - 1 : 2;
    if (pressed & KEY_DOWN) menu_sel = menu_sel < 2 ? menu_sel + 1 : 0;
    if (pressed & (KEY_B | KEY_START)) menu = 0;
    if (pressed & KEY_A) {
        if (menu_sel == 0) menu = 0;
        else if (menu_sel == 1) { menu = 2; help_page = 0; }
        else if (app_restart) app_restart();
    }
}

/* ---------- input ---------- */
static void patt_press_a(void)                    /* A pressed on the PATT tab */
{
    int dy = cursor >> 3, dx = cursor & 7;
    if (dy < 4) {
        pick_pattern(dy * 8 + dx);
    } else if (dy == 4 && dx < 4) {
        if (dx == 0) { pattern_copy(); clip_pi = preset; clip_ki = kpreset; clip_pr = randomized; clip_kr = krandom; }
        else if (dx == 1) { pattern_paste(); preset = clip_pi; kpreset = clip_ki; randomized = clip_pr; krandom = clip_kr; }
        else if (dx == 2) pattern_clear();
        else { pattern_random(ticks); randomized = 1; krandom = 1; }
    } else if (dy == 4) {
        pattern_flush();
        synth_song_start();
    }
}

void ui_update(unsigned held)
{
    if (!armed) {
        if (held == 0) armed = 1;
        prev = held;
        return;
    }
    static const unsigned dk[4] = { KEY_RIGHT, KEY_LEFT, KEY_UP, KEY_DOWN };
    ticks += 0x9E3779B1u + held;
    unsigned pressed = held & ~prev;
    unsigned released = prev & ~held;
    prev = held;
    held_now = held;
    int alt = (held & KEY_SELECT) != 0;

    if (menu) { menu_update(pressed); return; }

    if (pressed & KEY_SELECT) sel_used = (held & (KEY_START | KEY_B)) ? 1 : 0;
    if (alt && (pressed & ~KEY_SELECT)) sel_used = 1;
    if ((released & KEY_SELECT) && !sel_used) { menu = 1; menu_sel = 0; bpm_mode = 0; dirty = 1; return; }   /* SELECT tap: options menu */

    if (((pressed & KEY_B) && alt) || ((pressed & KEY_SELECT) && (held & KEY_B))) {   /* SELECT+B: BPM edit mode on/off */
        bpm_mode ^= 1; dirty = 1; return;
    }

    if (pressed & KEY_START) start_used = alt ? 1 : 0;
    if (pressed & KEY_A) a_used = 0;

    for (int k = 0; k < 4; k++) {
        if (!(held & dk[k])) { cnt[k] = rep[k] = 0; continue; }
        if (pressed & dk[k]) { cnt[k] = rep[k] = 0; dpad_action(k, alt); continue; }
        if (++cnt[k] >= (rep[k] ? 3 : 16)) { cnt[k] = 0; rep[k] = 1; dpad_action(k, alt); }
    }

    if (pressed & (KEY_L | KEY_R)) {
        dirty = 1;
        int dir = (pressed & KEY_R) ? 1 : -1;
        if (alt) {
            if (tab == TAB_KICK && kedit) {
                /* presets are for the whole pattern: not while editing a step */
            } else if (tab == TAB_KICK) {
                int n = kick_preset_count();
                kpreset += dir;
                if (kpreset < 0) kpreset = n - 1;
                if (kpreset >= n) kpreset = 0;
                kick_preset_load(kpreset); krandom = 0;
            } else if (tab == TAB_SYNTH || tab == TAB_SEQ) {
                int n = preset_count();
                preset += dir;
                if (preset < 0) preset = n - 1;
                if (preset >= n) preset = 0;
                preset_load_sound(preset); randomized = 0;
            }
        } else {
            if (kedit) { kedit = 0; curs[TAB_KICK] = kstep; }
            tab += dir;
            if (tab < 0) tab = NTABS - 1;
            if (tab >= NTABS) tab = 0;
        }
    }
    if (pressed & KEY_START) {
        dirty = 1;
        if (alt) {                                      /* SELECT+START: randomize what the tab shows */
            if (tab == TAB_SYNTH) { preset_random_part(ticks, 1); randomized = 1; }
            else if (tab == TAB_SEQ) preset_random_part(ticks, 0);
            else if (tab == TAB_PATCH) patch_random(ticks);
            else if (tab == TAB_KICK) {
                if (kedit) { }
                else if (cursor < 16) kick_random_pattern(synth_kplay(), ticks);
                else { kick_random_sound(ticks); krandom = 1; }
            } else if (tab == TAB_PATT) pattern_random(ticks);
        }
    }
    if ((released & KEY_START) && !start_used) { synth_set_run(!synth_running()); dirty = 1; }

    if (tab == TAB_SYNTH || tab == TAB_SEQ) {
        if ((released & KEY_A) && !a_used) synth_trigger();   /* on release: A+UP/DOWN is the fine value */
    } else if (tab == TAB_PATCH) {
        if ((released & KEY_A) && !a_used) { int d = cursor & 7; P[P_PBS0 + d] = 0; P[P_PBA0 + d] = 0; dirty = 1; }   /* A: unplug the cable */
    } else if (tab == TAB_KICK) {
        if (kedit) {
            if ((pressed & KEY_A) && alt) {             /* SELECT+A: unlock the knob / condition back to every bar */
                if (cursor == 24) KCND[synth_kplay()][kstep] = 0; else klock_clear(kstep, cursor - 16);
                a_used = 1; dirty = 1;
            }
            if ((released & KEY_A) && !a_used) synth_ktrigger_step(kstep);   /* A: hear this step with its locks */
            if (pressed & KEY_B) { kedit = 0; cursor = kstep; dirty = 1; }
        } else if (cursor < 16) {
            uint8_t *c = &KPAT[synth_kplay()][cursor];
            if (pressed & KEY_A) { *c = alt ? ((*c == 2) ? 1 : 2) : (*c ? 0 : 1); dirty = 1; }        /* A: trig on/off, SELECT+A: accent */
            if (pressed & KEY_B) { kedit = 1; kstep = cursor; cursor = 16; dirty = 1; }   /* B: step edit */
        } else if ((pressed & KEY_B) || ((released & KEY_A) && !a_used)) synth_ktrigger();
    } else if (tab == TAB_PATT) {
        if (pressed & KEY_A) { patt_press_a(); dirty = 1; }
        if ((released & KEY_A) && !a_used) {            /* A tap on a song slot: stamp the current pattern */
            int s = song_slot();
            if (s >= 0) { SONG[s] = (uint8_t)(cur_or_cue() + 1); dirty = 1; }
        }
    } else if (tab == TAB_FX) {
        int id = cell_id(tab, cursor);
        if ((released & KEY_A) && !a_used && PDEF[id].type == T_ENUM && PDEF[id].max == 1) { param_change(id, 1, 0); dirty = 1; }
    }
    if (dirty) pattern_flush();
}

int ui_changed(void)
{
    unsigned sig = (unsigned)synth_kstep() | (unsigned)synth_kplay() << 4 |
                   (unsigned)(synth_kqueued() + 1) << 8 | (unsigned)(synth_song_pos() + 1) << 13 |
                   (unsigned)synth_step() << 19 | (unsigned)synth_running() << 22;
    if (dirty || sig != last_sig) {
        dirty = 0; last_sig = sig;
        return 1;
    }
    return 0;
}

/* ---------- drawing helpers ---------- */
static void put(uint16_t *m, int x, int y, const char *s, int bank)
{
    for (; *s && x < 30; s++, x++) {
        int c = *s;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = 32;
        m[y * 32 + x] = (uint16_t)((c - 32) | (bank << 12));
    }
}

static void putinv(uint16_t *m, int x, int y, const char *s, int bank)   /* dark text on a coloured chip */
{
    for (; *s && x < 30; s++, x++) {
        int c = *s;
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c < 32 || c > 95) c = 32;
        m[y * 32 + x] = (uint16_t)((T_INV0 + c - 32) | (bank << 12));
    }
}

static void tile(uint16_t *m, int x, int y, int t, int bank)
{
    m[y * 32 + x] = (uint16_t)(t | (bank << 12));
}

static int slen(const char *s) { int n = 0; while (s[n]) n++; return n; }

static void bigput(uint16_t *m, int x, int y, const char *s, int bank)   /* 2x2-tile glyphs: " +-0123456789" */
{
    for (; *s && x < 29; s++, x += 2) {
        int c = *s, g = 0;
        if (c == '+') g = 1;
        else if (c == '-') g = 2;
        else if (c >= '0' && c <= '9') g = 3 + c - '0';
        int t = T_BIG0 + g * 4;
        tile(m, x, y, t, bank); tile(m, x + 1, y, t + 1, bank);
        tile(m, x, y + 1, t + 2, bank); tile(m, x + 1, y + 1, t + 3, bank);
    }
}

static void num2(int k, char *n)
{
    int t = 0;
    while (k >= 10) { k -= 10; t++; }
    n[0] = '0' + t; n[1] = '0' + k; n[2] = 0;
}

static void num3s(int v, char *o)
{
    int h = 0, t = 0;
    while (v >= 100) { v -= 100; h++; }
    while (v >= 10) { v -= 10; t++; }
    o[0] = '0' + h; o[1] = '0' + t; o[2] = '0' + v; o[3] = 0;
}

/* section title: centred text with a thin line on both sides, between columns xs..xe */
static void title(uint16_t *m, int xs, int xe, int y, const char *s, int bank)
{
    int n = slen(s), w = xe - xs + 1;
    int tx = xs + ((w - n) >> 1);
    put(m, tx, y, s, bank);
    for (int x = xs; x < tx - 1; x++) tile(m, x, y, T_HL, 0);
    for (int x = tx + n + 1; x <= xe; x++) tile(m, x, y, T_HL, 0);
}

static void icon3(uint16_t *m, int x, int y, int base, int bank)        /* 3x3-tile widget */
{
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++)
            tile(m, x + c, y + r, base + r * 3 + c, bank);
}

static int widget_base(int id)
{
    int v = P[id];
    if (PDEF[id].type == T_ENUM) {
        switch (id) {
        case P_V1W: case P_V2W:     return T_WAVE0 + (v & 7) * 9;
        case P_KWAV: case P_SMOD:   return T_SEL3 + (v > 2 ? 2 : v) * 9;
        case P_FSTR: case P_FDLT:   return T_SEL4 + (v & 3) * 9;
        default:                    return T_LAMP0 + (v ? 9 : 0);
        }
    }
    return T_KNOB0 + ((param_norm(id) * 15 + 32) >> 6) * 9;
}

/* one knob: label above, 3x3 widget below. (x, y) = title row of its block */
static int same(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

static void label(uint16_t *m, int x, int y, const char *lab, int bank)   /* compact label, 3 tiles wide */
{
    for (int i = 0; i < NLAB; i++)
        if (same(LAB_NAME[i], lab)) {
            for (int c = 0; c < 3; c++) tile(m, x + c, y, T_LAB0 + 3 * i + c, bank);
            return;
        }
    put(m, x, y, lab, bank);
}

static void draw_knob(uint16_t *m, int x, int y, int id, const char *lab, int sel, int playing, int alt)
{
    int bank = sel ? 1 : (playing ? 2 : 0);
    label(m, x, y + 1, lab, sel ? 1 : (alt ? 0 : 4));
    icon3(m, x, y + 2, widget_base(id), bank);
}

static void draw_status(uint16_t *m)
{
    char t[4], n[3];
    if (tab == TAB_KICK) put(m, 1, 19, krandom ? "RANDOM" : kick_preset_name(kpreset), 4);
    else if (tab == TAB_SYNTH || tab == TAB_SEQ || tab == TAB_PATCH) put(m, 1, 19, randomized ? "RANDOM" : preset_name(preset), 4);
    put(m, 10, 19, "P", 5); num2(synth_kplay() + 1, n); put(m, 11, 19, n, synth_song_on() ? 2 : 3);
    if (synth_kqueued() >= 0) { put(m, 13, 19, ">", 6); num2(synth_kqueued() + 1, n); put(m, 14, 19, n, 6); }
    else if (synth_song_on()) put(m, 13, 19, "SONG", 2);
    num3s(40 + P[P_TEMPO], t);
    put(m, 18, 19, t, bpm_mode ? 1 : 0); put(m, 21, 19, "BPM", bpm_mode ? 1 : 5);
    if (synth_running()) put(m, 26, 19, "RUN", 2); else put(m, 25, 19, "STOP", 5);
}

/* ---------- tabs ---------- */
static void draw_knob_tab(uint16_t *m, int t, int ncell)
{
    for (int c = 0; c < ncell; c++) {
        int x, y;
        cell_pos(t, c, &x, &y);
        const Cell *k = cell_of(t, c);
        draw_knob(m, x, y, k->id, k->lab, cursor == c, 0, (c & 1));
    }
}

static void draw_synth(uint16_t *m)
{
    for (int g = 0; g < 6; g++) {
        int x = 2 + ((g & 1) ? 13 : 0), y = 1 + 6 * (g >> 1);
        title(m, x, x + 11, y, SYN_TITLE[g], 3);
    }
    draw_knob_tab(m, TAB_SYNTH, 23);
}

static char *numstr(char *o, int v) { if (v >= 10) { *o++ = '1'; v -= 10; } *o++ = (char)('0' + v); return o; }

static void cond_str(int c, char *o, int longf)       /* "1:4" or "BAR 1 OF 4" / "EVERY BAR" */
{
    int x = COND_X[c], n = COND_N[c];
    if (longf) {
        if (c == 0) { const char *e = "EVERY BAR"; while (*e) *o++ = *e++; *o = 0; return; }
        const char *b = "BAR "; while (*b) *o++ = *b++;
        o = numstr(o, x);
        b = " OF "; while (*b) *o++ = *b++;
        o = numstr(o, n);
    } else { o = numstr(o, x); *o++ = ':'; o = numstr(o, n); }
    *o = 0;
}

static void chip(uint16_t *m, int x, int y, const char *s, int bank);

static void draw_cond(uint16_t *m, int cv, int sel)    /* trig condition row (y = 17) */
{
    char t[16];
    cond_str(cv, t, 1);
    put(m, 2, 17, "TRIG", sel ? 1 : 4);
    if (sel) chip(m, 7, 17, t, 1); else if (cv) chip(m, 7, 17, t, 6); else put(m, 7, 17, t, 5);
}

static void draw_seq(uint16_t *m)
{
    title(m, 2, 26, 1, "PITCH", 3);
    title(m, 2, 26, 7, "VELOCITY", 3);
    int step = synth_step(), run = synth_running();
    for (int i = 0; i < 16; i++) {
        int x, y, s = i & 7;
        cell_pos(TAB_SEQ, i, &x, &y);
        int id = cell_id(TAB_SEQ, i);
        char n[2]; n[0] = '1' + s; n[1] = 0;
        int playing = run && s == step;
        int sel = cursor == i;
        label(m, x, y + 1, n, sel ? 1 : (playing ? 2 : (SCND[synth_kplay()][s] ? 6 : 4)));
        icon3(m, x, y + 2, widget_base(id), sel ? 1 : (playing ? 2 : 0));
    }
    for (int s = 0; s < 8; s++) {                       /* play-head lamps under the pitch row */
        int x, y;
        cell_pos(TAB_SEQ, s, &x, &y);
        int on = (s == step);
        tile(m, x + 1, 6, on ? T_LED_ON : T_LED_OFF, on ? (run ? 2 : 6) : 0);
    }
    /* big read-out of the selected knob */
    char tmp[4], buf[16], nn[3];
    int id = cell_id(TAB_SEQ, cursor), s = (cursor & 7) + 1;
    param_short(id, tmp);
    int a = 0, b = 2;
    while (a < 3 && tmp[a] == ' ') a++;
    while (b > a && tmp[b] == ' ') b--;
    int n = 0;
    for (int i = a; i <= b; i++) buf[n++] = tmp[i];
    buf[n] = 0;
    (void)s;
    put(m, 2, 13, cursor < 8 ? "PITCH STEP" : "VELOCITY STEP", 4);
    num2((cursor & 7) + 1, nn); put(m, cursor < 8 ? 13 : 16, 13, nn + 1, 1);
    bigput(m, 2, 14, buf, 1);
    put(m, 2 + 2 * n + 1, 15, cursor < 8 ? "SEMITONES" : (P[id] == 0 ? "REST" : ""), 5);
    draw_cond(m, SCND[synth_kplay()][cursor & 7], 0);
}

/* PATT: chips (inverted text) are filled boxes, plain text = unused / empty */
static void chip(uint16_t *m, int x, int y, const char *s, int bank) { putinv(m, x, y, s, bank); }

static void draw_kick(uint16_t *m)
{
    int run = synth_running(), kc = synth_kstep(), pl = synth_kplay();
    title(m, 2, 26, 1, "KICK PATTERN", 3);
    for (int i = 0; i < 16; i++) {
        int x, y;
        cell_pos(TAB_KICK, i, &x, &y);
        int v = KPAT[pl][i], sel = (cursor == i) || (kedit && i == kstep), cur = run && i == kc;
        icon3(m, x, y, T_PAD0 + v * 9, sel ? 1 : (cur ? 2 : (v == 2 ? 6 : 0)));
        char n[3]; num2(i + 1, n);
        int ex = kstep_extras(pl, i);                   /* orange number = condition or locks on this step */
        put(m, x, y + 3, n, sel ? 1 : (cur ? 2 : (ex ? 6 : ((i & 3) == 0 ? 3 : 5))));
    }
    if (kedit) {
        char t[16] = "STEP  ";
        num2(kstep + 1, t + 5);
        { int n = 0; while (t[n]) n++; t[n++] = ' '; t[n++] = 'L'; t[n++] = 'O'; t[n++] = 'C'; t[n++] = 'K'; t[n++] = 'S'; t[n] = 0; }
        title(m, 2, 26, 11, t, 6);
    } else title(m, 2, 26, 11, "KICK SOUND", 3);
    for (int c = 16; c < 24; c++) {
        int x, y;
        cell_pos(TAB_KICK, c, &x, &y);
        const Cell *k = cell_of(TAB_KICK, c);
        if (!kedit) { draw_knob(m, x, y, k->id, k->lab, cursor == c, 0, (c & 1)); continue; }
        int sel = cursor == c, lk = klock_has(kstep, c - 16);   /* locked knobs glow orange, inherited ones are dim */
        label(m, x, y + 1, k->lab, sel ? 1 : (lk ? 6 : 5));
        icon3(m, x, y + 2, widget_base(k->id), sel ? 1 : (lk ? 6 : 5));
    }
    /* trig condition: fires on bar x of every N bars */
    if (kedit || cursor < 16) draw_cond(m, KCND[pl][kedit ? kstep : cursor], kedit && cursor == 24);
}

static void draw_patch(uint16_t *m)
{
    static const char *const DN[8] = { "VC1", "VC2", "CUT", "RES", "FM", "VCA", "DEC", "NSE" };
    title(m, 2, 26, 1, "CV INPUTS", 3);
    title(m, 2, 26, 8, "AMOUNT", 3);
    for (int s = 0; s < 8; s++) {
        int x = 2 + 3 * s + (s >= 4);
        int src = P[P_PBS0 + s], on = src != 0;
        int ss = cursor == s, sa = cursor == 8 + s;
        char t[4];
        label(m, x, 2, DN[s], (ss || sa) ? 1 : 4);
        icon3(m, x, 3, T_LAMP0 + (on ? 9 : 0), ss ? 1 : (on ? 2 : 5));           /* the jack: lit when a cable is plugged */
        param_short(P_PBS0 + s, t);
        label(m, x, 6, t, ss ? 1 : (on ? 2 : 5));
        icon3(m, x, 9, widget_base(P_PBA0 + s), sa ? 1 : (on ? 0 : 5));
    }
    title(m, 2, 26, 13, "SOURCES", 3);
    put(m, 2, 14, "SEQ PITCH    VEL VELOCITY", 5);
    put(m, 2, 15, "VCO VCF VCA  ENVELOPES", 5);
    put(m, 2, 16, "NSE NOISE    KCK KICK EG", 5);
    put(m, 2, 17, "RND NEW RANDOM EVERY STEP", 5);
}

static void draw_fx(uint16_t *m)
{
    static const char *const T[5] = { "COMPRESSOR", "STUTTER", "DELAY", "REVERB", "LIMITER" };
    title(m, 2, 13, 1, T[0], 3);  title(m, 15, 26, 1, T[1], 3);
    title(m, 2, 13, 7, T[2], 3);  title(m, 15, 26, 7, T[3], 3);
    title(m, 2, 13, 13, T[4], 3);
    draw_knob_tab(m, TAB_FX, 16);
    static const char L[5] = { 'C', 'S', 'D', 'R', 'L' };
    static const int8_t ids[5] = { P_FCMP, P_FSTU, P_FDLY, P_FREV, P_FLIM };
    title(m, 15, 26, 13, "CHAIN", 3);
    for (int i = 0; i < 5; i++) {
        char s[2]; s[0] = L[i]; s[1] = 0;
        put(m, 16 + 2 * i, 15, s, P[ids[i]] ? 2 : 5);
        if (i < 4) put(m, 17 + 2 * i, 15, ">", 5);
    }
}


static void draw_patt(uint16_t *m)
{
    int pl = synth_kplay(), qu = synth_kqueued();
    int cdy = cursor >> 3, cdx = cursor & 7;
    title(m, 2, 27, 1, "PATTERNS", 3);
    for (int p = 0; p < NPAT; p++) {
        int x = 2 + 3 * (p & 7), y = 3 + 2 * (p >> 3);
        char n[4]; n[0] = ' '; num2(p + 1, n + 1); n[3] = 0;
        int sel = (cdy == (p >> 3) && cdx == (p & 7));
        int used = pattern_used(p);
        if (sel) chip(m, x, y, n, 1);
        else if (p == pl) chip(m, x, y, n, 2);
        else if (p == qu) chip(m, x, y, n, 6);
        else put(m, x, y, n, used ? 0 : 5);
    }
    static const char *const cmd[4] = { " COPY ", " PASTE", " CLEAR", " RAND " };
    for (int r = 0; r < 4; r++) {
        int sel = (cdy == 4 && cdx == r);
        if (sel) chip(m, 2 + 6 * r, 11, cmd[r], 1); else put(m, 2 + 6 * r, 11, cmd[r], 4);
    }

    int run = synth_running(), sp = synth_song_pos(), on = synth_song_on();
    {
        const char *s = on ? "STOP" : "PLAY";
        int sel = cdy == 4 && cdx == 4;
        if (sel) chip(m, 26, 11, s, 1); else if (on) chip(m, 26, 11, s, 2); else put(m, 26, 11, s, 4);
    }
    title(m, 2, 27, 13, "SONG", 3);
    for (int i = 0; i < NSONG; i++) {
        int x = 2 + 3 * (i & 7), y = 14 + (i >> 3);
        int sel = (cdy == 5 + (i >> 3) && cdx == (i & 7));
        int cur = on && run && i == sp;
        char n[4]; n[0] = ' ';
        if (SONG[i]) num2(SONG[i], n + 1); else { n[1] = '-'; n[2] = '-'; }
        n[3] = 0;
        if (sel) chip(m, x, y, n, 1);
        else if (cur) chip(m, x, y, n, 2);
        else put(m, x, y, n, SONG[i] ? 0 : 5);
    }
}

/* ---------- screens ---------- */
static void draw_menu(uint16_t *m)
{
    static const char *const items[3] = { "RESUME", "HELP", "RESTART APP" };
    putinv(m, 1, 0, " OPTIONS ", 0);
    put(m, 21, 0, "GFAM V" GFAM_VERSION, 3);
    for (int i = 0; i < 3; i++) {
        put(m, 6, 4 + 2 * i, items[i], menu_sel == i ? 1 : 0);
        if (menu_sel == i) put(m, 4, 4 + 2 * i, ">", 1);
    }
    title(m, 1, 28, 11, "ABOUT", 3);
    put(m, 1, 12, "HELP EXPLAINS EVERY TAB AND", 4);
    put(m, 1, 13, "BUTTON. RESTART RELOADS THE", 4);
    put(m, 1, 14, "APP; TO QUIT USE THE MIYOO", 4);
    put(m, 1, 15, "MENU BUTTON.", 4);
    put(m, 1, 19, "UP/DOWN:PICK  A:OK  B:BACK", 5);
}

static void draw_help(uint16_t *m)
{
    const char *const *pg = HELP[help_page];
    char n[4];
    putinv(m, 1, 0, " HELP ", 0);
    put(m, 8, 0, pg[0], 3);
    num2(help_page + 1, n); put(m, 25, 0, n, 5);
    put(m, 27, 0, "/", 5);
    num2(NHELP, n); put(m, 28, 0, n, 5);
    for (int i = 1; pg[i]; i++) {
        const char *s = pg[i];
        if (s[0] == ':') put(m, 1, 1 + i + 1, s + 1, 3); else put(m, 1, 1 + i + 1, s, 0);
    }
    put(m, 1, 19, "LEFT/RIGHT:PAGE  B:BACK", 5);
}

void ui_draw(uint16_t *m)
{
    for (int i = 0; i < 1024; i++) m[i] = 0;
    if (menu == 1) { draw_menu(m); return; }
    if (menu == 2) { draw_help(m); return; }

    /* tab bar */
    for (int t = 0; t < NTABS; t++) {
        if (t == tab) {
            int x = TAB_X[t] - 1, e = TAB_X[t] + slen(TAB_NAME[t]);
            for (; x <= e; x++) putinv(m, x, 0, " ", 0);
            putinv(m, TAB_X[t], 0, TAB_NAME[t], 0);
        } else put(m, TAB_X[t], 0, TAB_NAME[t], 4);
    }

    int16_t sw[NKICKP];                                 /* step edit: draw the step's values, then restore the globals */
    if (tab == TAB_KICK && kedit)
        for (int i = 0; i < NKICKP; i++) { sw[i] = P[P_KTUN + i]; P[P_KTUN + i] = (int16_t)klock_get(kstep, i); }

    switch (tab) {
    case TAB_SYNTH: draw_synth(m); break;
    case TAB_PATCH: draw_patch(m); break;
    case TAB_SEQ:   draw_seq(m);   break;
    case TAB_KICK:  draw_kick(m);  break;
    case TAB_PATT:  draw_patt(m);  break;
    default:        draw_fx(m);    break;
    }

    /* detail line (full name and value of the selected cell) */
    if (bpm_mode) {
        put(m, 1, 18, "BPM EDIT", 1);
        put(m, 10, 18, "UP/DN:+-1  <>:+-10", 5);
    } else {
        char lng[32];
        int id = cell_id(tab, cursor);
        if (id >= 0 && tab != TAB_SEQ) {
            param_long(id, lng);
            put(m, 1, 18, lng, 1);
            if (tab == TAB_KICK && kedit && klock_has(kstep, cursor - 16)) put(m, 26, 18, "LOCK", 6);
        } else if (tab == TAB_KICK) {
            int pl = synth_kplay();
            if (kedit) {                                 /* cursor on the condition */
                char t[16]; cond_str(KCND[pl][kstep], t, 1);
                put(m, 1, 18, "KICK FIRES", 1); put(m, 12, 18, t, 1);
            } else {
                int v = KPAT[pl][cursor];
                char n[3]; num2(cursor + 1, n);
                put(m, 1, 18, "KICK STEP", 1); put(m, 11, 18, n, 1);
                put(m, 14, 18, v == 0 ? "OFF" : (v == 1 ? "TRIG" : "ACCENT"), 1);
                if (KCND[pl][cursor]) { char t[16]; cond_str(KCND[pl][cursor], t, 0); put(m, 21, 18, t, 6); }
                if (KLMASK[pl][cursor]) put(m, 26, 18, "LOCK", 6);
            }
        } else if (tab == TAB_PATT) {
            int dy = cursor >> 3, dx = cursor & 7;
            char n[3];
            if (dy < 4) {
                int p = dy * 8 + dx;
                put(m, 1, 18, "PATTERN", 1); num2(p + 1, n); put(m, 9, 18, n, 1);
                put(m, 12, 18, p == synth_kplay() ? "PLAYING" : (p == synth_kqueued() ? "NEXT" : ""), 1);
            } else if (dy == 4 && dx < 4) {
                static const char *const d[4] = { "COPY THE CURRENT PATTERN", "PASTE INTO THE CURRENT ONE",
                                                  "CLEAR STEPS, LOCKS, KEEP SOUND", "RANDOM STEPS AND SOUNDS" };
                put(m, 1, 18, d[dx], 1);
            } else if (dy == 4) put(m, 1, 18, synth_song_on() ? "STOP THE SONG" : "PLAY THE SONG", 1);
            else {
                int s = (dy - 5) * 8 + dx;
                put(m, 1, 18, "SLOT", 1); num2(s + 1, n); put(m, 6, 18, n, 1);
                if (SONG[s]) { put(m, 9, 18, "PATTERN", 1); num2(SONG[s], n); put(m, 17, 18, n, 1); }
                else put(m, 9, 18, "EMPTY", 1);
            }
        }
    }
    draw_status(m);
    if (tab == TAB_KICK && kedit)
        for (int i = 0; i < NKICKP; i++) P[P_KTUN + i] = sw[i];
}

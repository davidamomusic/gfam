/* Parameter definitions, formatting, editing and factory presets (control side). */
#include <stdint.h>
#include "synth.h"

int16_t P[NPARAM];

#define NUMP(l, n)          { l, n, T_NUM, 0, 255, 1, 8 }
#define BIPP(l, n)          { l, n, T_BIP, -99, 99, 1, 8 }
#define NOTEP(l, n)         { l, n, T_NOTE, 0, 1919, 16, 1 }
#define ENUMP(l, n, mx)     { l, n, T_ENUM, 0, mx, 1, 1 }
#define PBS                 { "SRC", "PATCH SOURCE", T_ENUM, 0, 8, 1, 1 }
#define PBA                 { "AMT", "PATCH AMOUNT", T_BIP, -99, 99, 1, 10 }
#define PITS(l)             { l, "STEP PITCH", T_SEMI, -768, 768, 16, 1 }
#define VELS(l)             { l, "STEP VELOCITY", T_VEL, 0, 127, 1, 8 }

const ParamDef PDEF[NPARAM] = {
    PITS("ST1"), PITS("ST2"), PITS("ST3"), PITS("ST4"),
    PITS("ST5"), PITS("ST6"), PITS("ST7"), PITS("ST8"),
    VELS("ST1"), VELS("ST2"), VELS("ST3"), VELS("ST4"),
    VELS("ST5"), VELS("ST6"), VELS("ST7"), VELS("ST8"),
    NOTEP("V1F", "VCO1 FREQ"), ENUMP("V1W", "VCO1 WAVE", 7), BIPP("V1E", "VCO1 EG AMT"),
    NOTEP("V2F", "VCO2 FREQ"), ENUMP("V2W", "VCO2 WAVE", 7), BIPP("V2E", "VCO2 EG AMT"),
    ENUMP("SYN", "HARD SYNC", 1), ENUMP("SEQ", "SEQ PITCH MOD", 2),
    NUMP("V1L", "VCO1 LEVEL"), NUMP("V2L", "VCO2 LEVEL"), NUMP("NSE", "NOISE LEVEL"),
    NUMP("FM ", "1>2 FM AMT"), NUMP("VOL", "VOLUME"),
    { "BPM", "TEMPO", T_BPM, 0, 255, 1, 8 },
    ENUMP("MOD", "FILTER MODE", 1), NUMP("CUT", "CUTOFF"), NUMP("RES", "RESONANCE"),
    BIPP("EG ", "VCF EG AMT"), NUMP("NMD", "NOISE>VCF MOD"),
    NUMP("VCO", "VCO DECAY"), NUMP("VCF", "VCF DECAY"), NUMP("VCA", "VCA DECAY"),
    ENUMP("ATK", "VCA ATTACK", 1),
    /* kick sound */
    { "TUN", "KICK TUNE", T_NOTE, 192, 768, 16, 1 },
    NUMP("SWP", "KICK SWEEP"), NUMP("PDC", "KICK PITCH DEC"), NUMP("ADC", "KICK AMP DEC"),
    NUMP("CLK", "KICK CLICK"), NUMP("DRV", "KICK DRIVE"),
    ENUMP("WAV", "KICK WAVE", 2), NUMP("LVL", "KICK LEVEL"),
    /* patch bay: sources, then amounts (text is built in param_long) */
    PBS, PBS, PBS, PBS, PBS, PBS, PBS, PBS,
    PBA, PBA, PBA, PBA, PBA, PBA, PBA, PBA,
    /* effects: on/off + mix, limiter on/off */
    ENUMP("CMP", "COMPRESSOR", 1), NUMP("MIX", "COMPRESSOR MIX"),
    ENUMP("STU", "STUTTER", 1),    NUMP("MIX", "STUTTER MIX"),
    ENUMP("DLY", "DELAY", 1),      NUMP("MIX", "DELAY MIX"),
    ENUMP("REV", "REVERB", 1),     NUMP("MIX", "REVERB MIX"),
    ENUMP("LIM", "LIMITER (LAST)", 1),
    /* effects tuning */
    NUMP("CAM", "COMP AMOUNT"), ENUMP("STR", "STUTTER SLICE", 3), ENUMP("DLT", "DELAY TIME", 3),
    NUMP("DFB", "DELAY FEEDBACK"), NUMP("RSZ", "REVERB SIZE"), NUMP("RTN", "REVERB TONE"),
    { "THR", "LIMITER THR", T_DB, 0, 100, 1, 10 },
};

/* pages 6 (KICK SEQ), 8 (PATTERNS), 9 (SONG) are custom grids: no parameters, COUNT = cells */
const uint8_t PAGE_FIRST[NPAGES] = { 0, 8, 16, 24, 30, 35, P_KTUN, P_KTUN, P_KTUN, P_KTUN, P_FCMP, P_FCAM };
const uint8_t PAGE_COUNT[NPAGES] = { 8, 8, 8, 6, 5, 4, 16, 8, 16, 32, 9, 6 };
const char *const PAGE_NAME[NPAGES] = {
    "SEQ PITCH", "SEQ VELOCITY", "OSCILLATORS", "MIXER AND FM", "FILTER", "ENVELOPES",
    "KICK SEQ", "KICK SOUND", "PATTERNS", "SONG MODE", "EFFECTS", "FX TUNE"
};

/* ---------- small text helpers (no division available) ---------- */
static void num3(int v, char *o)             /* right-aligned 0..999 */
{
    int h = 0, t = 0;
    while (v >= 100) { v -= 100; h++; }
    while (v >= 10)  { v -= 10;  t++; }
    o[0] = h ? '0' + h : ' ';
    o[1] = (h || t) ? '0' + t : ' ';
    o[2] = '0' + v;
}

static void signed2(int v, char *o)          /* +NN / -NN / " 00" */
{
    int neg = v < 0, t = 0;
    if (neg) v = -v;
    while (v >= 10) { v -= 10; t++; }
    if (!t && !v) { o[0] = ' '; o[1] = '0'; o[2] = '0'; return; }
    o[0] = neg ? '-' : '+';
    o[1] = '0' + t;
    o[2] = '0' + v;
}

static const char NOTE_NAMES[12][2] = {
    {'C',' '}, {'C','#'}, {'D',' '}, {'D','#'}, {'E',' '}, {'F',' '},
    {'F','#'}, {'G',' '}, {'G','#'}, {'A',' '}, {'A','#'}, {'B',' '}
};

static void note3(int f, char *o)            /* f in 1/16 semitone */
{
    int n = f >> 4;
    int oct = (n * 683) >> 13;               /* n / 12 for 0..119 */
    int s = n - oct * 12;
    o[0] = NOTE_NAMES[s][0];
    o[1] = NOTE_NAMES[s][1];
    o[2] = oct ? '0' + (oct - 1) : '-';
}

static const char *enum_text(int id, int v)
{
    switch (id) {
    case P_V1W: case P_V2W: {
        static const char *const W[8] = { "TRI", "SQR", "SAW", "PLS", "SIN", "ORG", "FLD", "HOL" };
        return W[v & 7];
    }
    case P_KWAV:            return v == 0 ? "SIN" : (v == 1 ? "TRI" : "FLD");
    case P_PBS0 ... P_PBS0 + 7: {
        static const char *const S[9] = { "OFF", "SEQ", "VEL", "VCO", "VCF", "VCA", "NSE", "KCK", "RND" };
        return S[v > 8 ? 8 : v];
    }
    case P_SYNC:            return v ? "ON " : "OFF";
    case P_SMOD:            return v == 0 ? "OFF" : (v == 1 ? "V12" : "VC2");
    case P_FMODE:           return v ? "HP " : "LP ";
    case P_AATK:            return v ? "SLW" : "FST";
    case P_FSTR: { static const char *const T[4] = { " 16", " 32", " 64", "128" }; return T[v & 3]; }
    case P_FDLT: { static const char *const T[4] = { "1/8", "D/8", "1/4", "D/4" }; return T[v & 3]; }
    default:                return v == 0 ? "OFF" : "ON ";
    }
}

void param_short(int id, char out[4])
{
    int v = P[id];
    out[3] = 0;
    switch (PDEF[id].type) {
    case T_NUM: case T_VEL: num3(v, out); break;
    case T_BPM:  num3(40 + v, out); break;
    case T_BIP:  signed2(v, out); break;
    case T_SEMI: signed2(v / 16, out); break;
    case T_NOTE: note3(v, out); break;
    case T_DB: { int w = (v * 205) >> 11; out[0] = '-'; out[1] = w >= 10 ? '1' : '0' + w; out[2] = w >= 10 ? '0' : ' '; break; }
    default: { const char *s = enum_text(id, v); out[0] = s[0]; out[1] = s[1]; out[2] = s[2]; }
    }
}

static char *put(char *o, const char *s) { while (*s) *o++ = *s++; return o; }

static char *cents(char *o, int frac16)      /* 1/16 st -> "+NNC" */
{
    int c = (frac16 * 25) >> 2, t = 0;
    while (c >= 10) { c -= 10; t++; }
    *o++ = '+'; *o++ = '0' + t; *o++ = '0' + c; *o++ = 'C';
    return o;
}

static const char *const PB_DEST[8] = { "VCO1 PITCH", "VCO2 PITCH", "CUTOFF", "RESONANCE", "FM AMOUNT", "VCA LEVEL", "DECAY TIMES", "NOISE LEVEL" };
static const char *const PB_SRC[9] = { "OFF", "SEQ PITCH", "VELOCITY", "VCO EG", "VCF EG", "VCA EG", "NOISE", "KICK EG", "RANDOM" };

void param_long(int id, char *out)
{
    char *o = out, tmp[4];
    int v = P[id];
    if (id >= P_PBS0 && id < P_PBEND) {              /* patch bay: "DEST < SOURCE" / "DEST AMT +NN" */
        int d = (id - P_PBS0) & 7;
        o = put(o, PB_DEST[d]);
        if (id < P_PBA0) { o = put(o, " < "); o = put(o, PB_SRC[v > 8 ? 8 : v]); }
        else { o = put(o, " AMT "); signed2(v, tmp); o = put(o, tmp); }
        *o = 0;
        return;
    }
    if (id < P_V1F) {                         /* step parameters */
        o = put(o, "STEP ");
        *o++ = '1' + (id & 7);
        o = put(o, id < P_VEL0 ? " PITCH " : " VELOCITY ");
    } else {
        o = put(o, PDEF[id].name);
        *o++ = ' ';
    }
    param_short(id, tmp);
    if (PDEF[id].type == T_DB) {                  /* -N.N DB */
        int w = (v * 205) >> 11;
        *o++ = '-';
        if (w >= 10) { *o++ = '1'; *o++ = '0'; } else *o++ = '0' + w;
        *o++ = '.'; *o++ = '0' + (v - w * 10);
        o = put(o, " DB");
    } else if (PDEF[id].type == T_SEMI) {
        int a = v < 0 ? -v : v;
        signed2(v / 16, tmp);
        o = put(o, tmp); o = put(o, "ST");
        if (a & 15) { *o++ = ' '; o = cents(o, a & 15); }
    } else {
        o = put(o, tmp);
        if (PDEF[id].type == T_NOTE && (v & 15)) { *o++ = ' '; o = cents(o, v & 15); }
        if (PDEF[id].type == T_BPM) o = put(o, " BPM");
    }
    *o = 0;
}

int param_norm(int id)
{
    int v = P[id], n;
    switch (PDEF[id].type) {
    case T_NUM: case T_BPM: n = v >> 2; break;
    case T_BIP:  n = ((v + 99) * 165) >> 9; break;
    case T_NOTE: n = ((v - PDEF[id].min) * 64) / (PDEF[id].max - PDEF[id].min); break;
    case T_SEMI: n = ((v + 768) * 43) >> 10; break;
    case T_VEL:  n = v >> 1; break;
    default:     n = (v * 64) / PDEF[id].max; break;
    }
    return n < 0 ? 0 : (n > 64 ? 64 : n);
}

void param_change(int id, int dir, int alt)
{
    const ParamDef *d = &PDEF[id];
    int v = P[id];
    if (d->type == T_ENUM) {
        v += dir;
        if (v > d->max) v = d->min;
        if (v < d->min) v = d->max;
    } else {
        v += dir * (alt ? d->alt : d->step);
        if (v < d->min) v = d->min;
        if (v > d->max) v = d->max;
    }
    P[id] = (int16_t)v;
}

/* ---------- factory presets ---------- */
#define ST(x) ((x) * 16)
#define N(m)  ((m) * 16)

typedef struct { const char *name; int16_t v[P_NMAIN]; } Preset;

static const Preset PRESETS[] = {
 { "808 KICK", {
   ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),
   127,0,0,70,120,0,50,0,
   N(28),0,60,  N(28),0,45,  0,0,
   230,90,12, 0, 220, 80,
   0,190,20,0,0,
   70,100,120, 0 } },
 { "SEQ BASS", {
   ST(0),ST(0),ST(12),ST(0),ST(7),ST(0),ST(5),ST(3),
   127,90,110,70,127,80,100,60,
   N(36),1,0,  N(36)+3,1,0,  0,1,
   200,170,0, 0, 200, 90,
   0,70,130,55,0,
   100,90,100, 0 } },
 { "HATS", {
   ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),
   100,50,127,50,100,50,127,80,
   N(78),1,0,  N(85),1,0,  0,0,
   70,70,200, 0, 200, 110,
   1,185,40,0,0,
   50,60,45, 0 } },
 { "FM TOMS", {
   ST(0),ST(5),ST(7),ST(12),ST(0),ST(-5),ST(-7),ST(3),
   127,100,110,90,127,100,110,80,
   N(48),0,35,  N(55),0,50,  0,2,
   60,210,10, 110, 200, 70,
   0,230,40,0,0,
   90,100,125, 0 } },
 { "MUSICBOX", {
   ST(0),ST(4),ST(7),ST(12),ST(7),ST(4),ST(2),ST(-3),
   110,80,90,100,85,75,95,70,
   N(72),0,0,  N(84),0,0,  0,1,
   190,70,0, 50, 190, 60,
   0,255,0,0,0,
   80,80,150, 0 } },
 { "ZAP SYNC", {
   ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),ST(0),
   127,0,100,0,127,70,0,90,
   N(40),1,-70,  N(52),1,-80,  1,0,
   80,200,30, 0, 190, 100,
   0,180,90,40,0,
   100,90,130, 0 } },
};

int preset_count(void) { return (int)(sizeof(PRESETS) / sizeof(PRESETS[0])); }
const char *preset_name(int i) { return PRESETS[i].name; }

void patch_clear(void)
{
    for (int k = P_PBS0; k < P_PBEND; k++) P[k] = 0;
}

void preset_load_sound(int i)
{
    for (int k = P_V1F; k < P_NMAIN; k++) P[k] = PRESETS[i].v[k];
    patch_clear();                                    /* a preset comes with an empty patch bay */
}

void preset_load(int i)
{
    for (int k = 0; k < P_NMAIN; k++) P[k] = PRESETS[i].v[k];
}

/* ---------- random preset generator ----------
 * Not fully random: ranges are chosen so the result always makes sound
 * (audible oscillator, filter open enough, first step never a rest). */
static uint32_t rs;
static unsigned rnd(unsigned n)                 /* 0..n-1, no division */
{
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
    return (unsigned)(((rs >> 8) * n) >> 24);
}
static int rrange(int lo, int hi) { return lo + (int)rnd(hi - lo + 1); }
static int chance(int pct) { return (int)rnd(100) < pct; }

void preset_random(uint32_t seed)
{
    static const int8_t SCALE[8] = { 0, 3, 5, 7, 10, 12, 15, 19 };  /* minor pentatonic-ish */
    rs = seed * 2654435761u + 0x9E3779B9u;
    if (!rs) rs = 1;
    for (int i = 0; i < 6; i++) rnd(2);         /* warm up */

    int oct = chance(30);
    for (int i = 0; i < 8; i++) {
        int n = SCALE[rnd(oct ? 8 : 5)];
        if (chance(15)) n -= 12;
        P[P_PIT0 + i] = (int16_t)(n * 16);
        P[P_VEL0 + i] = (int16_t)((i == 0 || !chance(25)) ? rrange(70, 127) : 0);
    }
    P[P_PIT0] = 0;

    P[P_V1F] = (int16_t)(rrange(24, 55) * 16);
    P[P_V1W] = (int16_t)rnd(8);
    P[P_V1E] = (int16_t)(chance(50) ? 0 : rrange(-40, 70));
    static const int8_t IV[6] = { 0, 0, 7, 12, -12, 5 };
    P[P_V2F] = (int16_t)(P[P_V1F] + IV[rnd(6)] * 16 + (chance(70) ? rrange(-10, 10) : 0));
    if (P[P_V2F] < 0) P[P_V2F] = 0;
    P[P_V2W] = (int16_t)rnd(8);
    P[P_V2E] = (int16_t)(chance(50) ? 0 : rrange(-40, 70));
    P[P_SYNC] = (int16_t)chance(25);
    P[P_SMOD] = (int16_t)(chance(15) ? 0 : 1 + rnd(2));

    P[P_L1] = (int16_t)rrange(150, 255);
    P[P_L2] = (int16_t)(chance(25) ? 0 : rrange(80, 230));
    P[P_LN] = (int16_t)(chance(60) ? 0 : rrange(20, 140));
    P[P_FM]  = (int16_t)(chance(55) ? 0 : rrange(20, 130));
    P[P_VOL] = 200;
    P[P_TEMPO] = (int16_t)rrange(30, 140);       /* 70..180 BPM */

    P[P_FMODE] = (int16_t)chance(15);
    if (P[P_FMODE]) {                            /* high-pass would eat low notes: move up two octaves */
        if (P[P_V1F] < 48 * 16) { P[P_V1F] += 24 * 16; P[P_V2F] += 24 * 16; }
    }
    P[P_CUT] = (int16_t)(P[P_FMODE] ? rrange(30, 110) : rrange(125, 240));
    P[P_RES] = (int16_t)rrange(0, 170);
    P[P_FEG] = (int16_t)(chance(80) ? rrange(0, 99) : rrange(-25, 0));
    P[P_NMOD] = (int16_t)(chance(70) ? 0 : rrange(10, 90));

    P[P_VDEC] = (int16_t)rrange(20, 170);
    P[P_FDEC] = (int16_t)rrange(40, 200);
    P[P_ADEC] = (int16_t)rrange(70, 200);
    P[P_AATK] = (int16_t)chance(8);
}

/* ---------- kick presets: 8 sound parameters (ids P_KTUN..P_FX0-1) ---------- */
typedef struct { const char *name; int16_t v[NKICKP]; } KPreset;

static const KPreset KPRESETS[] = {
 { "808 SUB",   { N(26), 90, 45, 135, 20,  30, 0, 130 } },
 { "909 PUNCH", { N(33),150, 40, 105, 90,  90, 1, 120 } },
 { "TECHNO",    { N(30),120, 55, 120, 60, 140, 2, 110 } },
 { "HARD",      { N(28),200, 70, 150,100, 230, 0, 100 } },
 { "DEEP SUB",  { N(24), 30, 30, 150,  0,   0, 0, 140 } },
 { "ZAP KICK",  { N(36),255, 90,  80,120,  60, 2, 110 } },
};

/* ---------- kick patterns and song ---------- */
int16_t PSEQ[NPAT][16] PAT_RAM;
int16_t PSND[NPAT][NSND] PAT_RAM;
uint8_t KPAT[NPAT][16] PAT_RAM;
uint8_t SONG[NSONG] PAT_RAM;
uint8_t KCND[NPAT][16] PAT_RAM;
uint8_t SCND[NPAT][8] PAT_RAM;
/* trig conditions: fire on bar X of every N bars */
const uint8_t COND_X[NCOND] = { 1, 1, 2, 1, 2, 3, 1, 2, 3, 4, 1, 4, 8, 1, 8, 16 };
const uint8_t COND_N[NCOND] = { 1, 2, 2, 3, 3, 3, 4, 4, 4, 4, 8, 8, 8, 16, 16, 16 };
uint8_t KLMASK[NPAT][16] PAT_RAM;
int16_t KLVAL[NPAT][16][NKICKP] PAT_RAM;

static const uint8_t KPAT_INIT[][16] = {
 { 1,0,0,0, 1,0,0,0, 1,0,0,0, 1,0,0,0 },      /*  1 four on the floor   */
 { 2,0,0,0, 1,0,0,0, 2,0,0,0, 1,0,0,1 },      /*  2 punch               */
 { 2,0,0,0, 1,0,0,0, 2,0,0,0, 1,0,0,0 },      /*  3 techno              */
 { 2,0,0,1, 0,0,1,0, 2,0,0,1, 0,0,1,0 },      /*  4 hard                */
 { 2,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,1,0 },      /*  5 deep                */
 { 1,0,1,0, 2,0,0,1, 1,0,1,0, 2,0,0,0 },      /*  6 zap                 */
 { 2,0,0,0, 0,0,0,0, 1,0,0,0, 0,0,0,0 },      /*  7 half time           */
 { 2,0,0,1, 0,0,1,0, 0,0,2,0, 0,1,0,0 },      /*  8 broken              */
 { 2,0,0,0, 1,0,0,0, 2,0,0,0, 1,0,1,0 },      /*  9 techno + ghost      */
 { 2,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0 },      /* 10 one hit per bar     */
};

void patterns_init(void)
{
    for (int p = 0; p < NPAT; p++)
        for (int i = 0; i < 16; i++) {
            KPAT[p][i] = p < (int)(sizeof KPAT_INIT / sizeof KPAT_INIT[0]) ? KPAT_INIT[p][i] : 0;
            PSEQ[p][i] = p == 0 ? P[i] : 0;           /* pattern 1 keeps the loaded steps, the rest start silent */
        }
    for (int p = 0; p < NPAT; p++)                    /* every pattern starts with the loaded synth + kick sound */
        for (int k = 0; k < NSND; k++) PSND[p][k] = P[P_V1F + k];
    for (int i = 0; i < NSONG; i++) SONG[i] = 0;
    for (int p = 0; p < NPAT; p++)
        for (int i = 0; i < 16; i++) {
            KCND[p][i] = 0; KLMASK[p][i] = 0;
            for (int k = 0; k < NKICKP; k++) KLVAL[p][i][k] = 0;
        }
    for (int p = 0; p < NPAT; p++)
        for (int i = 0; i < 8; i++) SCND[p][i] = 0;
}

/* ---- kick step extras: trig condition + parameter locks (current pattern) ---- */
static void step_wipe(int p, int i)
{
    KCND[p][i] = 0; KLMASK[p][i] = 0;
    for (int k = 0; k < NKICKP; k++) KLVAL[p][i][k] = 0;
}
int klock_has(int step, int i) { return (KLMASK[synth_kplay()][step] >> i) & 1; }
int klock_get(int step, int i)
{
    int p = synth_kplay();
    return ((KLMASK[p][step] >> i) & 1) ? KLVAL[p][step][i] : P[P_KTUN + i];
}
void klock_set(int step, int i, int v)
{
    int p = synth_kplay();
    KLVAL[p][step][i] = (int16_t)v;
    KLMASK[p][step] = (uint8_t)(KLMASK[p][step] | (1 << i));
}
void klock_clear(int step, int i)
{
    int p = synth_kplay();
    KLMASK[p][step] = (uint8_t)(KLMASK[p][step] & ~(1 << i));
}
int kstep_extras(int p, int step) { return KCND[p][step] != 0 || KLMASK[p][step] != 0; }

static int global_snd(int id) { return id == P_TEMPO || id == P_VOL; }   /* not part of a pattern */

void pattern_flush(void)
{
    int p = synth_kplay();
    for (int i = 0; i < 16; i++) PSEQ[p][i] = P[i];
    for (int k = 0; k < NSND; k++) if (!global_snd(P_V1F + k)) PSND[p][k] = P[P_V1F + k];
}
void pattern_load(int p)
{
    for (int i = 0; i < 16; i++) P[i] = PSEQ[p][i];
    for (int k = 0; k < NSND; k++) if (!global_snd(P_V1F + k)) P[P_V1F + k] = PSND[p][k];
}

static int16_t clip_s[16];
static int16_t clip_n[NSND];
static uint8_t clip_k[16], clip_c[16], clip_m[16], clip_sc[8];
static int16_t clip_v[16][NKICKP];
static int clip_full;

void pattern_copy(void)
{
    int p = synth_kplay();
    pattern_flush();
    for (int i = 0; i < 16; i++) { clip_s[i] = PSEQ[p][i]; clip_k[i] = KPAT[p][i]; }
    for (int k = 0; k < NSND; k++) clip_n[k] = PSND[p][k];
    for (int i = 0; i < 16; i++) {
        clip_c[i] = KCND[p][i]; clip_m[i] = KLMASK[p][i];
        for (int k = 0; k < NKICKP; k++) clip_v[i][k] = KLVAL[p][i][k];
    }
    for (int i = 0; i < 8; i++) clip_sc[i] = SCND[p][i];
    clip_full = 1;
}

void pattern_paste(void)
{
    int p = synth_kplay();
    if (!clip_full) return;
    for (int i = 0; i < 16; i++) { PSEQ[p][i] = clip_s[i]; KPAT[p][i] = clip_k[i]; }
    for (int k = 0; k < NSND; k++) PSND[p][k] = clip_n[k];
    for (int i = 0; i < 8; i++) SCND[p][i] = clip_sc[i];
    for (int i = 0; i < 16; i++) {
        KCND[p][i] = clip_c[i]; KLMASK[p][i] = clip_m[i];
        for (int k = 0; k < NKICKP; k++) KLVAL[p][i][k] = clip_v[i][k];
    }
    pattern_load(p);
}

void pattern_clear(void)
{
    int p = synth_kplay();
    pattern_flush();                                  /* keep the sound as it is now */
    for (int i = 0; i < 16; i++) { PSEQ[p][i] = 0; KPAT[p][i] = 0; step_wipe(p, i); }
    for (int i = 0; i < 8; i++) SCND[p][i] = 0;
    pattern_load(p);
}

int pattern_used(int p)
{
    if (p == synth_kplay()) pattern_flush();
    for (int i = 0; i < 8; i++) if (PSEQ[p][8 + i] > 0) return 1;
    for (int i = 0; i < 16; i++) if (KPAT[p][i]) return 1;
    return 0;
}

int kick_preset_count(void) { return (int)(sizeof KPRESETS / sizeof KPRESETS[0]); }
const char *kick_preset_name(int i) { return KPRESETS[i].name; }
void kick_preset_load(int i)
{
    for (int k = 0; k < NKICKP; k++) P[P_KTUN + k] = KPRESETS[i].v[k];
}

void kick_random_sound(uint32_t seed)
{
    rs = seed * 2654435761u + 0x7F4A7C15u;
    if (!rs) rs = 1;
    for (int i = 0; i < 6; i++) rnd(2);
    P[P_KTUN] = (int16_t)(rrange(22, 38) * 16);
    P[P_KSWP] = (int16_t)rrange(30, 240);
    P[P_KPDC] = (int16_t)rrange(25, 95);
    P[P_KADC] = (int16_t)rrange(70, 150);
    P[P_KCLK] = (int16_t)(chance(40) ? rrange(0, 20) : rrange(30, 130));
    P[P_KDRV] = (int16_t)(chance(40) ? 0 : rrange(30, 210));
    P[P_KWAV] = (int16_t)rnd(3);
    P[P_KLEV] = (int16_t)rrange(90, 130);
}

void kick_random_pattern(int pat, uint32_t seed)
{
    rs = seed * 2654435761u + 0x1B873593u;
    if (!rs) rs = 1;
    for (int i = 0; i < 6; i++) rnd(2);
    int style = rnd(3);
    for (int i = 0; i < 16; i++) { KPAT[pat][i] = 0; step_wipe(pat, i); }       /* 0 four-on-floor + extras, 1 broken, 2 sparse */
    for (int i = 0; i < 16; i++) {
        int on;
        if (style == 0)      on = (i & 3) == 0 ? 1 : chance(12);
        else if (style == 1) on = (i == 0 || i == 10) ? 1 : chance(30);
        else                 on = (i == 0) ? 1 : ((i & 7) == 0 ? chance(60) : chance(15));
        if (on && i > 0 && KPAT[pat][i - 1]) on = 0;   /* no two kicks in a row */
        KPAT[pat][i] = (uint8_t)(on ? (chance(25) ? 2 : 1) : 0);
    }
    KPAT[pat][0] = 2;
}

void preset_random_part(uint32_t seed, int sound)
{
    int16_t keep[P_NMAIN];
    for (int k = 0; k < P_NMAIN; k++) keep[k] = P[k];
    preset_random(seed);
    for (int k = 0; k < P_NMAIN; k++)
        if ((k >= P_V1F) != (sound != 0)) P[k] = keep[k];
    P[P_TEMPO] = keep[P_TEMPO];                       /* randomizing never touches the BPM or the master volume */
    P[P_VOL] = keep[P_VOL];
    if (!sound) { int p = synth_kplay(); for (int i = 0; i < 8; i++) SCND[p][i] = 0; }   /* new steps, new conditions */
}

void pattern_random(uint32_t seed)
{
    int16_t tempo = P[P_TEMPO], vol = P[P_VOL];
    int p = synth_kplay();
    preset_random(seed);                              /* steps + synth sound; BPM and volume are kept */
    P[P_TEMPO] = tempo; P[P_VOL] = vol;
    kick_random_sound(seed ^ 0x2c1b3c6du);
    kick_random_pattern(p, seed ^ 0x5bd1e995u);
    for (int i = 0; i < 8; i++) SCND[p][i] = 0;
    pattern_flush();
}

void patch_random(uint32_t seed)
{
    rs = seed * 2654435761u + 0x3C6EF372u;
    if (!rs) rs = 1;
    for (int i = 0; i < 6; i++) rnd(2);
    patch_clear();
    int n = 2 + (int)rnd(3);                          /* 2..4 cables */
    for (int c = 0; c < n; c++) {
        int d = (int)rnd(NPATCH);
        P[P_PBS0 + d] = (int16_t)(1 + rnd(8));
        int a = rrange(20, 80);
        P[P_PBA0 + d] = (int16_t)(chance(70) ? a : -a);
    }
}

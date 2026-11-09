/* GFAM: percussion synth + kick sequencer core. Compiled in ARM mode and run from IWRAM.
 * Everything is integer / fixed-point (the ARM7TDMI has no FPU and no divide).
 *
 * To save CPU the slow-moving things (envelopes, pitch, filter cutoff, step sequencer) are updated
 * once per GROUP of 4 samples; only oscillators, filter and amplifiers run per sample. The result is
 * a block of samples in "Q8" units (128 = 32768) that fx.c turns into the final 8-bit output. */
#include <stdint.h>
#include "synth.h"
#include "fx.h"

#ifdef HOST
#define IWRAM_DATA
#define IWRAM_TEXT
#else
#define IWRAM_DATA __attribute__((section(".iwram.data")))
#define IWRAM_TEXT __attribute__((section(".iwram.text")))
#endif

#include "tables.h"

typedef struct { int32_t e, peak, coef, atk; } Env;

static struct {
    uint32_t ph1, ph2, rng;
    int32_t  y1, y2, y3, y4, last;
    int32_t  aei;                  /* VCA level, ramped inside a group */
    Env      ve, fe, ae;           /* VCO / VCF / VCA envelopes (one step per group) */
    int32_t  count, sp;            /* samples to next step, current step pitch */
    int      running, first, cur, trig;
    /* kick voice */
    uint32_t kph;
    Env      kpe, kae, kce;        /* pitch / amplitude / click envelopes */
    int32_t  knz, kaei;            /* previous noise sample; kick level ramped inside a group */
    int      kcur, ktrig, kplay, kqueue, song, spos;
    int32_t  rv;                   /* RANDOM patch source: new value every step */
    uint32_t rv_s;
    int      kcyc, kc3;            /* bars since start / pattern change: mod 16 and mod 3 (trig conditions) */
    int32_t  kv_tun, kv_swp, kv_clk, kv_drv, kv_lev, kv_wav;    /* kick voice parameters latched at the trigger */
} S;

static int32_t mixbuf[FRAME_SAMPLES];

static inline int32_t clampi(int32_t v, int32_t lo, int32_t hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline int32_t env_step(Env *v)
{
    if (v->atk) {
        v->e += v->atk;
        if (v->e >= v->peak) { v->e = v->peak; v->atk = 0; }
    } else {
        v->e = (int32_t)(((int64_t)v->e * v->coef) >> 31);
    }
    return v->e;
}

static inline int32_t tri(uint32_t ph)
{
    int32_t x = (int32_t)(ph >> 16);
    if (x >= 32768) x = 65535 - x;
    return x * 2 - 32768;
}

static inline int32_t sqr(uint32_t ph) { return (ph >> 31) ? 24000 : -24000; }

/* table waves: linear interpolation between 256 entries (table has 257 so idx+1 never wraps) */
static inline int32_t tabw(const int16_t *t, uint32_t ph)
{
    uint32_t i = ph >> 24;
    int32_t a = t[i], b = t[i + 1];
    return a + (((b - a) * (int32_t)((ph >> 16) & 255)) >> 8);
}

/* VCO waveforms: 0 TRI 1 SQR 2 SAW 3 PLS(25%) 4 SIN 5 ORG 6 FLD 7 HOL */
static inline int32_t wave(int w, uint32_t ph)
{
    if (w >= 4) return tabw(&WAVE_TAB[(w - 4) * 257], ph);
    if (w == 0) return tri(ph);
    if (w == 1) return sqr(ph);
    if (w == 2) return (int32_t)(ph >> 16) - 32768;
    return (ph >> 30) ? -9000 : 27000;
}

#define MULQ(a, g) ((int32_t)(((int64_t)(a) * (int64_t)(g)) >> 16))

/* Fire all three envelopes for one step, scaled by that step's velocity. */
#ifdef HOST
int host_trig_n;                                  /* test hook: synth steps that fired */
#endif
static void IWRAM_TEXT do_trigger(int step)
{
    int v = P[P_VEL0 + step];
    S.sp = P[P_PIT0 + step];
    if (v <= 0) return;
#ifdef HOST
    host_trig_n++;
#endif
    int32_t peak = (int32_t)(v * v) << 16;            /* ~2^30 at v=127 */
    S.ve.peak = S.fe.peak = S.ae.peak = peak;
    int dd = 0;                                       /* patch bay "decay" input: sources known at the trigger */
    {
        int s = P[P_PBS0 + 6];
        int32_t sv = s == 1 ? S.sp * 21 : (s == 2 ? v << 7 : (s == 8 ? S.rv : 0));
        dd = (int)((sv * P[P_PBA0 + 6]) >> 14);
    }
    S.ve.coef = DEC_COEF[clampi(P[P_VDEC] + dd, 0, 255)];
    S.fe.coef = DEC_COEF[clampi(P[P_FDEC] + dd, 0, 255)];
    S.ae.coef = DEC_COEF[clampi(P[P_ADEC] + dd, 0, 255)];
    S.ve.atk = S.fe.atk = peak >> 2;                  /* ~1 ms attack (4 groups) */
    S.ae.atk = P[P_AATK] ? (peak >> 9) : (peak >> 2); /* ~110 ms or ~1 ms */
}

/* Fire the kick voice. accent = 1 plays louder. The phase is only reset when the previous kick has
   faded out, so retriggering a long tail never makes a discontinuity (click). */
static inline int kval(int pat, int step, int i)       /* kick knob i for a step: its lock, else the global value */
{
    if (step >= 0 && ((KLMASK[pat][step] >> i) & 1)) return KLVAL[pat][step][i];
    return P[P_KTUN + i];
}

#ifdef HOST
int host_ktrig_n, host_kv_last[6];               /* test hooks: triggers fired and the latched voice parameters */
#endif
static void IWRAM_TEXT do_ktrigger(int accent, int pat, int step)
{
    S.kv_tun = (kval(pat, step, 0) * 1365) >> 10;
    S.kv_swp = kval(pat, step, 1);
    S.kv_clk = kval(pat, step, 4);
    S.kv_drv = kval(pat, step, 5);
    S.kv_wav = kval(pat, step, 6);
    S.kv_lev = kval(pat, step, 7);
#ifdef HOST
    host_ktrig_n++;
    host_kv_last[0] = S.kv_tun; host_kv_last[1] = S.kv_swp; host_kv_last[2] = S.kv_clk;
    host_kv_last[3] = S.kv_drv; host_kv_last[4] = S.kv_wav; host_kv_last[5] = S.kv_lev;
#endif
    if (S.kae.e < (1 << 24)) S.kph = 0;
    S.kpe.e = 1 << 30; S.kpe.atk = 0; S.kpe.coef = DEC_COEF[kval(pat, step, 2)];
    S.kae.e = accent ? (1 << 30) : (int32_t)(((int64_t)1 << 30) * 3 / 4);
    S.kae.atk = 0; S.kae.coef = DEC_COEF[kval(pat, step, 3)];
    S.kce.e = 1 << 30; S.kce.atk = 0; S.kce.coef = KCLK_COEF;
}

static void IWRAM_TEXT set_kplay(int p)
{
    if (p == S.kplay) return;
    pattern_flush();                                  /* keep edits of the pattern we leave */
    S.kplay = p;
    S.kcyc = 0; S.kc3 = 0;                            /* trig conditions count from the new pattern's first bar */
    pattern_load(p);
}

static int IWRAM_TEXT next_slot(int i)                /* next song slot (after i) holding a pattern, or -1 */
{
    for (int k = 1; k <= NSONG; k++) {
        int j = (i + k) % NSONG;
        if (SONG[j]) return j;
    }
    return -1;
}

/* first step of a bar: song advance, or the queued pattern */
static int IWRAM_TEXT cond_ok(int c)                  /* trig condition "bar x of N" (bars counted from 0) */
{
    int x = COND_X[c] - 1;
    switch (COND_N[c]) {
    case 1:  return 1;
    case 2:  return (S.kcyc & 1) == x;
    case 3:  return S.kc3 == x;
    case 4:  return (S.kcyc & 3) == x;
    case 8:  return (S.kcyc & 7) == x;
    default: return S.kcyc == x;
    }
}

static void IWRAM_TEXT bar_start(int first)
{
    if (!first) { S.kcyc = (S.kcyc + 1) & 15; S.kc3 = S.kc3 == 2 ? 0 : S.kc3 + 1; }
    if (S.song) {
        int j = first ? next_slot(NSONG - 1) : next_slot(S.spos);
        if (j >= 0) { S.spos = j; set_kplay(SONG[j] - 1); }
        S.kqueue = -1;
    } else if (S.kqueue >= 0) {
        set_kplay(S.kqueue); S.kqueue = -1;
    }
}

static void IWRAM_TEXT tick(void)
{
    int f = S.first;
    if (f) S.first = 0;
    else { S.cur = (S.cur + 1) & 7; S.kcur = (S.kcur + 1) & 15; }
    if (S.kcur == 0) bar_start(f);
    S.rv_s ^= S.rv_s << 13; S.rv_s ^= S.rv_s >> 17; S.rv_s ^= S.rv_s << 5;
    S.rv = (int32_t)(S.rv_s >> 17) - 16384;           /* RANDOM source, +-16384 */
    if (cond_ok(SCND[S.kplay][S.cur])) do_trigger(S.cur);
    else S.sp = P[P_PIT0 + S.cur];
    int k = KPAT[S.kplay][S.kcur];
    if (k && cond_ok(KCND[S.kplay][S.kcur])) do_ktrigger(k == 2, S.kplay, S.kcur);
    fx_step_sync();
}

void synth_init(void)
{
    S.rng = 0x1234ABCDu; S.rv_s = 0x9E3779B9u; S.rv = 0;
    S.cur = 0; S.first = 1; S.running = 0; S.count = 0; S.trig = 0;
    S.kcyc = 0; S.kc3 = 0; S.kcur = 0; S.ktrig = 0; S.kplay = 0; S.kqueue = -1; S.song = 0; S.spos = -1; S.kph = 0; S.knz = 0; S.kaei = 0; S.aei = 0;
    S.kpe.e = S.kae.e = S.kce.e = 0; S.kpe.atk = S.kae.atk = S.kce.atk = 0;
    S.sp = P[P_PIT0];
    fx_init();
}

void synth_set_run(int on)
{
    if (on && !S.running) {
        S.running = 1; S.first = 1; S.count = 0; S.kcur = S.cur; S.kcyc = 0; S.kc3 = 0;
        if (S.song) { S.cur = 0; S.kcur = 0; S.spos = -1; }
    } else if (!on) { S.running = 0; S.first = 1; S.song = 0; S.spos = -1; }
}

int synth_running(void) { return S.running; }
int synth_step(void)    { return S.cur; }
int synth_kstep(void)   { return S.kcur; }
int synth_kplay(void)   { return S.kplay; }
int synth_kqueued(void) { return S.kqueue; }
int synth_song_on(void) { return S.song; }
void synth_song_off(void) { S.song = 0; S.spos = -1; }
int synth_song_pos(void) { return S.spos; }
void synth_queue_pattern(int p)
{
    if (S.running) S.kqueue = (p == S.kplay) ? -1 : p;
    else { set_kplay(p); S.kqueue = -1; }
}
void synth_song_start(void)
{
    if (S.song) { synth_set_run(0); return; }
    if (next_slot(NSONG - 1) < 0) return;             /* empty song */
    synth_set_run(0);
    S.song = 1;
    synth_set_run(1);
}
void synth_ktrigger(void) { S.ktrig = 1; }
void synth_ktrigger_step(int step) { S.ktrig = 2 + step; }
void synth_trigger(void) { S.trig = 1; }
void synth_advance(void) { S.cur = (S.cur + 1) & 7; S.kcur = S.cur; S.sp = P[P_PIT0 + S.cur]; }

void IWRAM_TEXT synth_render(int8_t *out, int n)
{
    if (S.trig) { S.trig = 0; do_trigger(S.cur); }
    if (S.ktrig) {
        int t = S.ktrig; S.ktrig = 0;
        if (t == 1) do_ktrigger(1, S.kplay, -1);
        else do_ktrigger(KPAT[S.kplay][t - 2] == 2, S.kplay, t - 2);     /* audition one step with its locks */
    }

    const int32_t l1 = P[P_L1], l2 = P[P_L2], ln = P[P_LN], fm = P[P_FM];
    const int32_t vol = P[P_VOL], cut = P[P_CUT], res = P[P_RES] << 4;
    const int32_t nmod = P[P_NMOD];
    const int w1 = P[P_V1W], w2 = P[P_V2W], sync = P[P_SYNC], smod = P[P_SMOD];
    const int hp = P[P_FMODE];
    const int32_t base1 = (P[P_V1F] * 1365) >> 10;    /* 1/16 st -> 1/256 oct */
    const int32_t base2 = (P[P_V2F] * 1365) >> 10;
    const int32_t e1 = (P[P_V1E] * 83) >> 6;          /* -99..99 -> ~-128..128 */
    const int32_t e2 = (P[P_V2E] * 83) >> 6;
    const int32_t feg = (P[P_FEG] * 83) >> 6;
    const int32_t steplen = STEP_LEN[P[P_TEMPO]];

    uint32_t ph1 = S.ph1, ph2 = S.ph2, rng = S.rng;
    int32_t y1 = S.y1, y2 = S.y2, y3 = S.y3, y4 = S.y4, last = S.last;
    int32_t knz = S.knz, aei = S.aei, kaei = S.kaei;
    int32_t nz = knz;

    fx_set_step(steplen);

    for (int g = 0; g < n; g += 4) {
        /* ---------- control rate: once per 4 samples ---------- */
        if (S.running) {
            S.count -= 4;
            if (S.count <= 0) { tick(); S.count += steplen; }
        }
        int32_t ve = env_step(&S.ve);
        int32_t fe = env_step(&S.fe);
        int32_t ae = env_step(&S.ae);
        /* patch bay: sources scaled to +-16384, then amount -99..99 per input */
        int32_t sv[9];
        sv[0] = 0; sv[1] = S.sp * 21; sv[2] = P[P_VEL0 + S.cur] << 7;
        sv[3] = ve >> 16; sv[4] = fe >> 16; sv[5] = ae >> 16;
        sv[6] = nz >> 1; sv[7] = S.kae.e >> 16; sv[8] = S.rv;
        int32_t pm[8];
        for (int d = 0; d < 8; d++) { int s = P[P_PBS0 + d]; pm[d] = s ? sv[s] * P[P_PBA0 + d] : 0; }
        ae = (ae >> 12) * clampi(4096 + (pm[5] >> 9), 0, 7168);   /* VCA input: gain 0 .. 1.75 */
        const int32_t resg = clampi(res + (pm[3] >> 10), 0, 4080);
        const int32_t fmg = clampi(fm + (pm[4] >> 13), 0, 255);
        const int32_t lng = clampi(ln + (pm[7] >> 13), 0, 255);
        int32_t dae = (ae - aei) >> 2;                  /* VCA ramps linearly across the group */

        int32_t sq = (S.sp * 1365) >> 10;
        int32_t eg = ve >> 16;                          /* 0..16384 */
        int32_t p1 = clampi(base1 + ((eg * e1) >> 11) + (smod == 1 ? sq : 0) + (pm[0] >> 12), 0, 2559);
        int32_t p2 = clampi(base2 + ((eg * e2) >> 11) + (smod ? sq : 0) + (pm[1] >> 12), 0, 2559);
        uint32_t inc1 = EXP2_TAB[p1 & 255] << (p1 >> 8);
        uint32_t inc2 = EXP2_TAB[p2 & 255] << (p2 >> 8);
        const int32_t fm_d = (int32_t)(inc2 >> 1);

        int32_t idx = cut + (((fe >> 16) * feg) >> 13) + ((nz * nmod) >> 17) + (pm[2] >> 13);
        const int32_t gc = CUT_G[clampi(idx, 0, 255)];

        /* kick voice control */
        const int32_t ktun = S.kv_tun, kswp = S.kv_swp, kclk = S.kv_clk;
        const int32_t kdrv = S.kv_drv, klev = S.kv_lev, kwav = S.kv_wav;
        const int32_t kgain = 256 + kdrv * 6;
        const int kon = S.kae.e > (1 << 22);
        uint32_t kinc = 0;
        int32_t kcl = 0, kdae = 0;
        if (kon) {
            int32_t kp = ktun + (int32_t)((((int64_t)(env_step(&S.kpe) >> 16)) * kswp) >> 12);
            kp = clampi(kp, 0, 2559);
            kinc = EXP2_TAB[kp & 255] << (kp >> 8);
            kdae = (env_step(&S.kae) - kaei) >> 2;
            if (S.kce.e > (1 << 18)) kcl = env_step(&S.kce) >> 16;   /* click lasts a few ms */
        } else {
            kaei = 0;
        }

        /* ---------- audio rate ---------- */
        for (int j = 0; j < 4; j++) {
            aei += dae;

            /* VCO1 */
            uint32_t n1 = ph1 + inc1;
            int wrap1 = n1 < ph1;
            ph1 = n1;
            int32_t o1 = wave(w1, ph1);

            /* VCO2: linear FM from VCO1 (proportional to its own pitch) */
            int32_t d = (int32_t)(((int64_t)fm_d * o1) >> 14);
            d = (int32_t)(((int64_t)d * fmg) >> 6);
            ph2 += inc2 + (uint32_t)d;
            if (sync && wrap1) ph2 = 0;
            int32_t o2 = wave(w2, ph2);

            /* white noise (xorshift32) */
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            nz = (int16_t)(rng >> 16);

            int32_t mix = (o1 * l1 + o2 * l2 + nz * lng) >> 8;
            mix = clampi(mix, -32767, 32767);

            /* 4-pole ladder (LP) or 4 cascaded one-pole HP, with resonance */
            int32_t u = (mix << 8) - (int32_t)(((int64_t)last * resg) >> 10);
            u = clampi(u, -8388352, 8388352);
            y1 += MULQ(u - y1, gc);
            if (!hp) {
                y2 += MULQ(y1 - y2, gc);
                y3 += MULQ(y2 - y3, gc);
                y4 += MULQ(y3 - y4, gc);
                last = y4;
            } else {
                int32_t h = u - y1;
                y2 += MULQ(h - y2, gc); h -= y2;
                y3 += MULQ(h - y3, gc); h -= y3;
                y4 += MULQ(h - y4, gc); h -= y4;
                last = h;
            }

            /* VCA */
            int32_t a = (int32_t)(((int64_t)last * aei) >> 30);
            int32_t pre = a >> 8;                       /* +-32767 */

            if (kon) {                                  /* kick voice (bypasses VCF/VCA) */
                kaei += kdae;
                S.kph += kinc;
                int32_t kw = kwav == 0 ? tabw(&WAVE_TAB[0], S.kph)
                           : (kwav == 1 ? tri(S.kph + 0x40000000u) : tabw(&WAVE_TAB[2 * 257], S.kph));
                if (kcl) kw += ((((nz - knz) * kcl) >> 15) * kclk) >> 9;    /* HP noise burst */
                int32_t kk = (kw * (kaei >> 15)) >> 15;
                if (kdrv) {                             /* drive: gain then 1.5x - 0.5x^3 soft clip */
                    kk = clampi((kk * kgain) >> 8, -32767, 32767);
                    int32_t k2 = (kk * kk) >> 15;
                    kk += (kk - ((k2 * kk) >> 15)) >> 1;
                }
                pre += (kk * klev) >> 8;
            }
            knz = nz;
            mixbuf[g + j] = (pre * vol) >> 7;           /* Q8 domain, x2 make-up gain included */
        }
    }

    S.ph1 = ph1; S.ph2 = ph2; S.rng = rng;
    S.y1 = y1; S.y2 = y2; S.y3 = y3; S.y4 = y4; S.last = last;
    S.knz = knz; S.aei = aei; S.kaei = kaei;

    fx_process(mixbuf, out, n);
}

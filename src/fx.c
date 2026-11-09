/* Effects chain (ARM mode, IWRAM). Every device has ON/OFF and (except the limiter) a MIX amount.
 * Big buffers (delay, stutter) live in EWRAM; the small reverb buffers live in IWRAM. */
#include <stdint.h>
#include "synth.h"
#include "fx.h"

#ifdef HOST
#define IWRAM_DATA
#define IWRAM_TEXT
#define EWRAM_DATA
#else
#define IWRAM_DATA __attribute__((section(".iwram.data")))
#define IWRAM_TEXT __attribute__((section(".iwram.text")))
#define EWRAM_DATA __attribute__((section(".ewram")))
#endif

#pragma GCC diagnostic ignored "-Wunused-variable"
#include "tables.h"


#define DLY_LEN  32768
#define DLY_MASK (DLY_LEN - 1)
#define STU_LEN  8192

static int16_t dly[DLY_LEN] EWRAM_DATA;
static int16_t stu[STU_LEN] EWRAM_DATA;

/* reverb: 4 combs + 2 all-passes, run at half the sample rate */
#define C0 367
#define C1 391
#define C2 421
#define C3 449
#define A0 181
#define A1 139
static int32_t cb0[C0], cb1[C1], cb2[C2], cb3[C3], ap0[A0], ap1[A1];

static struct {
    int32_t step;                                   /* sequencer step length (samples)    */
    int32_t cenv, cg;                               /* compressor envelope / gain (Q12)   */
    int     dwp, dprime; int32_t dfl;               /* delay write ptr, samples written since enabled, fb low-pass */
    int     srec, swi, sri, slen, ssc, sreq;        /* stutter state                      */
    int     p0, p1, p2, p3, q0, q1;                 /* reverb pointers                    */
    int32_t l0, l1, l2, l3, rlast;                  /* reverb damping states, output hold */
    int32_t lg;                                     /* limiter gain (Q12)                 */
    uint8_t was[4];                                 /* previous ON state: cmp stu dly rev */
} F;

static void clear32(int32_t *p, int n) { for (int i = 0; i < n; i++) p[i] = 0; }

void fx_init(void)
{
    F.cenv = 0; F.cg = 4096; F.dfl = 0;
    F.srec = 1; F.swi = 0; F.sri = 0; F.slen = 1; F.ssc = 0; F.sreq = 0;
    F.dwp = 0; F.dprime = 0; F.lg = 4096; F.rlast = 0;
    F.was[0] = F.was[1] = F.was[2] = F.was[3] = 0;
    F.step = 4000;
}

void fx_defaults(void)
{
    P[P_FCMP] = 0; P[P_FCMPM] = 160; P[P_FCAM] = 140;
    P[P_FSTU] = 0; P[P_FSTUM] = 255; P[P_FSTR] = 1;
    P[P_FDLY] = 0; P[P_FDLYM] = 110; P[P_FDLT] = 1; P[P_FDFB] = 120;
    P[P_FREV] = 0; P[P_FREVM] = 90;  P[P_FRSZ] = 150; P[P_FRTN] = 128;
    P[P_FLIM] = 1; P[P_FLTH] = 6;                  /* -0.6 dB = the old fixed ceiling */
#ifdef FX_MASK                                    /* profiling builds: bit0 cmp, bit1 stutter, bit2 delay, bit3 reverb */
    P[P_FCMP] = FX_MASK & 1; P[P_FSTU] = (FX_MASK >> 1) & 1; P[P_FDLY] = (FX_MASK >> 2) & 1; P[P_FREV] = (FX_MASK >> 3) & 1;
#endif
}

void fx_set_step(int32_t steplen) { F.step = steplen; }
void fx_step_sync(void) { F.sreq = 1; }

static inline int32_t clamp16(int32_t v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : v); }

/* ---- compressor: feed-forward peak detector, 4:1 above the threshold, auto make-up gain ---- */
static void IWRAM_TEXT comp(int32_t *x, int n, int mix)
{
    const int32_t thr = 65535 - P[P_FCAM] * 240;           /* 65535 .. 4335 */
    const int32_t mk = 4096 + ((65535 - thr) >> 5);
    int32_t cenv = F.cenv, cg = F.cg;
    for (int g = 0; g < n; g += 4) {
        int32_t pk = 0;
        for (int j = 0; j < 4; j++) { int32_t a = x[g + j]; a = a < 0 ? -a : a; if (a > pk) pk = a; }
        if (pk > 65535) pk = 65535;
        if (pk > cenv) cenv += (pk - cenv) >> 1; else cenv -= (cenv - pk) >> 7;
        int32_t target = 4096;
        if (cenv > thr) {
            int32_t t = thr + ((cenv - thr) >> 2);
            target = (t * RECIP[cenv >> 8]) >> 16;      /* t <= cenv, so t*RECIP <= ~2^28 */
            if (target > 4096) target = 4096;
        }
        int32_t ng = target < cg ? cg - ((cg - target) >> 1) : cg + ((target - cg) >> 7);
        int32_t dg = (ng - cg) >> 2;
        for (int j = 0; j < 4; j++) {
            cg += dg;
            int32_t v = x[g + j];
            int32_t w = (v * cg) >> 12;             /* |v| < 2^18, cg <= 2^12: fits in 32 bits */
            w = (w * mk) >> 12;
            x[g + j] = v + (((w - v) * mix) >> 8);
        }
        cg = ng;
    }
    F.cenv = cenv; F.cg = cg;
}

/* ---- stutter: record the start of every 2nd step, then loop that slice ---- */
static void IWRAM_TEXT stutter(int32_t *x, int n, int mix, int first)
{
    int slen = F.step >> P[P_FSTR];
    if (slen < 16) slen = 16;
    if (slen > STU_LEN) slen = STU_LEN;
    if (first) { F.srec = 1; F.swi = 0; F.ssc = 0; }
    int rec = F.srec, wi = F.swi, ri = F.sri, pl = F.slen, req = F.sreq, sc = F.ssc;
    for (int i = 0; i < n; i++) {
        if (req) {
            req = 0;
            if (!(sc++ & 1)) { rec = 1; wi = 0; }
        }
        int32_t v = x[i], w;
        if (rec) {
            int32_t h = v >> 1;
            stu[wi++] = (int16_t)(h > 32767 ? 32767 : (h < -32768 ? -32768 : h));
            w = v;
            if (wi >= slen) { rec = 0; ri = 0; pl = slen; }
        } else {
            w = (int32_t)stu[ri] << 1;
            if (++ri >= pl) ri = 0;
        }
        x[i] = v + (((w - v) * mix) >> 8);
    }
    F.srec = rec; F.swi = wi; F.sri = ri; F.slen = pl; F.sreq = req; F.ssc = sc;
}

/* ---- delay: tempo-synced, filtered feedback ---- */
static void IWRAM_TEXT delay(int32_t *x, int n, int mix)
{
    static const uint8_t K[4] = { 2, 3, 4, 6 };
    int32_t d = F.step * K[P[P_FDLT]];
    if (d > DLY_MASK) d = DLY_MASK;
    if (d < 1) d = 1;
    const int32_t fb = P[P_FDFB] > 230 ? 230 : P[P_FDFB];
    int wp = F.dwp, primed = F.dprime;
    int32_t fl = F.dfl;
    for (int i = 0; i < n; i++) {
        /* old contents of the ring are never played: only samples written since the delay was enabled count */
        int32_t r = primed >= d ? dly[(wp - d) & DLY_MASK] : 0;
        if (primed < DLY_LEN) primed++;
        fl += (r - fl) >> 1;
        dly[wp] = (int16_t)clamp16((x[i] >> 1) + ((fl * fb) >> 8));
        wp = (wp + 1) & DLY_MASK;
        x[i] += (r * 2 * mix) >> 8;
    }
    F.dwp = wp; F.dfl = fl; F.dprime = primed;
}

/* ---- reverb: small Freeverb-style room at half rate (two output samples per pass) ---- */
#define COMB(buf, len, ptr, lp)                                     \
    { int32_t y = buf[ptr];                                         \
      lp += ((y - lp) * damp) >> 8;                                 \
      buf[ptr] = in + ((lp * fb) >> 8);                             \
      if (++ptr >= len) ptr = 0;                                    \
      sum += y; }

static void IWRAM_TEXT reverb(int32_t *x, int n, int mix)
{
    const int32_t fb = 150 + ((P[P_FRSZ] * 88) >> 8);         /* 150..238 -> 0.59..0.93 */
    const int32_t damp = 256 - ((P[P_FRTN] * 200) >> 8);      /* more TONE = brighter   */
    int p0 = F.p0, p1 = F.p1, p2 = F.p2, p3 = F.p3, q0 = F.q0, q1 = F.q1;
    int32_t l0 = F.l0, l1 = F.l1, l2 = F.l2, l3 = F.l3, last = F.rlast;
    for (int i = 0; i < n; i += 2) {
        int32_t a = x[i], b = x[i + 1];
        int32_t in = (a + b) >> 5, sum = 0;
        COMB(cb0, C0, p0, l0) COMB(cb1, C1, p1, l1) COMB(cb2, C2, p2, l2) COMB(cb3, C3, p3, l3)
        int32_t s = sum >> 1, vv;
        vv = ap0[q0]; ap0[q0] = s + (vv >> 1); s = vv - s; if (++q0 >= A0) q0 = 0;
        vv = ap1[q1]; ap1[q1] = s + (vv >> 1); s = vv - s; if (++q1 >= A1) q1 = 0;
        x[i]     = a + (((last + s) * mix) >> 8);              /* half-rate output, linearly interpolated */
        x[i + 1] = b + ((s * mix) >> 7);
        last = s;
    }
    F.p0 = p0; F.p1 = p1; F.p2 = p2; F.p3 = p3; F.q0 = q0; F.q1 = q1;
    F.l0 = l0; F.l1 = l1; F.l2 = l2; F.l3 = l3; F.rlast = last;
}

/* ---- limiter (last device): instant attack, ~100 ms release, ceiling LIM_CEIL; OFF = hard clip ---- */
/* ceiling for threshold d (tenths of dB below full scale, 0..100): 127 * 10^(-d/200) in Q8, linear between whole dB */
static const uint16_t DB_Q15[12] = { 32768, 29205, 26029, 23197, 20674, 18426, 16423, 14636, 13044, 11626, 10361, 10361 };
static int32_t IWRAM_TEXT lim_ceil(int d)
{
    if (d < 0) d = 0;
    if (d > 100) d = 100;
    int q = (d * 205) >> 11, r = d - q * 10;
    int g = DB_Q15[q] - (((int)(DB_Q15[q] - DB_Q15[q + 1]) * r * 205) >> 11);   /* r / 10 without a division */
    return (int32_t)(((127 << 8) * g) >> 15);
}

static void IWRAM_TEXT limiter(const int32_t *x, int8_t *out, int n, int on)
{
    const int32_t LIM_CEIL = lim_ceil(P[P_FLTH]);
    int32_t lg = F.lg;
    for (int i = 0; i < n; i++) {
        int32_t s = x[i];
        if (on) {
            int32_t y = (s * lg) >> 12, m = y < 0 ? -y : y;
            while (m > LIM_CEIL) {
                lg -= (m > 2 * LIM_CEIL) ? (lg >> 2) : (lg >> 4) + 1;
                y = (s * lg) >> 12; m = y < 0 ? -y : y;
            }
            if (lg < 4096) {
                lg += ((4096 - lg) >> 11) + 1;
                if (lg > 4096) lg = 4096;
            }
            s = y;
        }
        s >>= 8;
        out[i] = (int8_t)(s > 127 ? 127 : (s < -128 ? -128 : s));
    }
    F.lg = lg;
}

void IWRAM_TEXT fx_process(int32_t *x, int8_t *out, int n)
{
    int on_c = P[P_FCMP], on_s = P[P_FSTU], on_d = P[P_FDLY], on_r = P[P_FREV];

    /* a device that was off starts from silence when it is switched on */
    if (on_d && !F.was[2]) { F.dwp = 0; F.dprime = 0; F.dfl = 0; }
    if (on_r && !F.was[3]) {
        clear32(cb0, C0); clear32(cb1, C1); clear32(cb2, C2); clear32(cb3, C3);
        clear32(ap0, A0); clear32(ap1, A1);
        F.p0 = F.p1 = F.p2 = F.p3 = F.q0 = F.q1 = 0;
        F.l0 = F.l1 = F.l2 = F.l3 = 0; F.rlast = 0;
    }
    if (on_c && !F.was[0]) { F.cenv = 0; F.cg = 4096; }
    int first_s = on_s && !F.was[1];
    F.was[0] = on_c; F.was[1] = on_s; F.was[2] = on_d; F.was[3] = on_r;

    if (on_c) comp(x, n, P[P_FCMPM]);
    if (on_s) stutter(x, n, P[P_FSTUM], first_s);
    if (on_d) delay(x, n, P[P_FDLYM]);
    if (on_r) reverb(x, n, P[P_FREVM]);
    limiter(x, out, n, P[P_FLIM]);
}

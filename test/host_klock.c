/* Kick p-locks and trig conditions: engine (latched voice, 1:N bars) and UI editing. */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/synth.h"
#include "../src/ui.h"
extern int host_ktrig_n, host_kv_last[6], host_trig_n;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int8_t buf[FRAME_SAMPLES];
static void setup(void)
{
    preset_load(0); kick_preset_load(0); patterns_init(); fx_defaults();
    P[P_TEMPO] = 255; synth_init(); ui_init(); ui_update(0);
}
/* runs n bars; fired[b] = number of kick triggers in bar b; tun[b] = tune latched at the last trigger */
static int last_ks = -1;
static void bars(int n, int *fired, int *tun)
{
    int b = -1, guard = 0;
    while (b < n && guard++ < 400000) {
        int before = host_ktrig_n;
        synth_render(buf, FRAME_SAMPLES);
        int ks = synth_kstep();
        if (ks == 0 && last_ks != 0) {            /* a new bar began somewhere in this buffer */
            b++;
            if (b >= n) break;
            fired[b] = 0;
        }
        if (b >= 0 && b < n) { fired[b] += host_ktrig_n - before; tun[b] = host_kv_last[0]; }
        last_ks = ks;
    }
}
static void press(unsigned k) { ui_update(k); ui_update(0); }
static void sel(unsigned k) { ui_update(KEY_SELECT); ui_update(KEY_SELECT | k); ui_update(0); }
static void hold_a(unsigned k) { ui_update(KEY_A); ui_update(KEY_A | k); ui_update(KEY_A); ui_update(0); }

int main(void)
{
    int fired[40], tun[40];

    /* ---- conditions: one kick on step 1 with each condition ---- */
    for (int c = 0; c < NCOND; c++) {
        setup();
        for (int i = 0; i < 16; i++) KPAT[0][i] = 0;
        KPAT[0][0] = 1; KCND[0][0] = (uint8_t)c;
        synth_set_run(1); last_ks = -1;
        bars(36, fired, tun);
        for (int b = 0; b < 33; b++) {
            int want = (b % COND_N[c]) == COND_X[c] - 1;
            CHECK(fired[b] == want, "cond %d bar %d fired %d want %d", c, b, fired[b], want);
        }
    }

    /* ---- the counter starts again when the pattern changes and on a restart ---- */
    setup();
    for (int i = 0; i < 16; i++) { KPAT[0][i] = 0; KPAT[1][i] = 0; }
    KPAT[0][0] = 1; KPAT[1][0] = 1; KCND[1][0] = 1;      /* pattern 2: every 2 bars */
    synth_set_run(1); last_ks = -1;
    bars(3, fired, tun);                                 /* bar 0 is pattern 1; ask pattern 2 mid-run */
    synth_queue_pattern(1);
    bars(5, fired, tun);
    CHECK(synth_kplay() == 1, "pattern 2 is playing");
    CHECK(fired[0] + fired[1] >= 1, "first bars after the change fire once (%d %d %d %d)", fired[0], fired[1], fired[2], fired[3]);
    CHECK(fired[1] == 0 || fired[0] == 0, "1:2 does not fire twice in a row");
    synth_set_run(0); synth_set_run(1); last_ks = -1;
    bars(4, fired, tun);
    CHECK(fired[0] == 1 && fired[1] == 0 && fired[2] == 1 && fired[3] == 0, "restart counts from bar 1 (%d %d %d %d)", fired[0], fired[1], fired[2], fired[3]);

    /* ---- p-locks reach the voice only on their step ---- */
    setup();
    for (int i = 0; i < 16; i++) KPAT[0][i] = 0;
    KPAT[0][0] = 1; KPAT[0][8] = 1;
    P[P_KTUN] = 26 * 16;
    klock_set(8, 0, 40 * 16);                            /* step 9: other tune */
    synth_set_run(1); last_ks = -1;
    int seen0 = -1, seen8 = -1, guard = 0;
    while ((seen0 < 0 || seen8 < 0) && guard++ < 100000) {
        int before = host_ktrig_n;
        synth_render(buf, FRAME_SAMPLES);
        if (host_ktrig_n != before) {
            if (synth_kstep() == 0) seen0 = host_kv_last[0]; else if (synth_kstep() == 8) seen8 = host_kv_last[0];
        }
    }
    CHECK(seen0 == (26 * 16 * 1365) >> 10, "step 1 uses the global tune (%d)", seen0);
    CHECK(seen8 == (40 * 16 * 1365) >> 10, "step 9 uses its lock (%d)", seen8);
    CHECK(klock_get(0, 0) == 26 * 16 && klock_get(8, 0) == 40 * 16, "klock_get");
    klock_clear(8, 0);
    CHECK(!klock_has(8, 0) && klock_get(8, 0) == 26 * 16, "unlock returns to the global value");

    /* ---- pattern edit helpers carry / drop the extras ---- */
    setup();
    KPAT[0][3] = 1; KCND[0][3] = 3; klock_set(3, 5, 99); klock_set(3, 6, 2);
    pattern_copy();
    synth_queue_pattern(7);
    pattern_paste();
    CHECK(KCND[7][3] == 3 && KLMASK[7][3] == ((1 << 5) | (1 << 6)) && KLVAL[7][3][5] == 99 && KLVAL[7][3][6] == 2, "paste carries cond and locks");
    CHECK(kstep_extras(7, 3) && !kstep_extras(7, 4), "kstep_extras");
    pattern_clear();
    CHECK(KCND[7][3] == 0 && KLMASK[7][3] == 0, "clear drops them");
    KCND[7][3] = 2; klock_set(3, 1, 7);
    kick_random_pattern(7, 5);
    CHECK(KCND[7][3] == 0 && KLMASK[7][3] == 0, "a new random kick pattern starts without them");
    CHECK(KCND[0][3] == 3 && KLMASK[0][3] != 0, "other patterns keep theirs");

    /* ---- editing from the buttons ---- */
    setup();
    press(KEY_R); press(KEY_R); press(KEY_R);            /* PATCH, SEQ, KICK tab; cursor on pad 1 */
    press(KEY_RIGHT); press(KEY_RIGHT);                  /* pad 3 */
    int trig0 = KPAT[0][2];
    sel(KEY_UP); sel(KEY_UP);                            /* condition 2:2 */
    CHECK(KCND[0][2] == 2, "SELECT+UP/DOWN on a pad sets the condition (%d)", KCND[0][2]);
    CHECK(KPAT[0][2] == trig0, "and does not toggle the trig");
    sel(KEY_DOWN);
    CHECK(KCND[0][2] == 1, "and back");
    press(KEY_A);
    CHECK(KPAT[0][2] == (trig0 ? 0 : 1), "a plain A tap toggles the trig");
    press(KEY_A);
    sel(KEY_A);
    CHECK(KPAT[0][2] == 2, "SELECT+A makes an accent");
    int g_tun = P[P_KTUN], g_drv = P[P_KDRV];
    press(KEY_B);                                        /* step edit */
    press(KEY_UP); press(KEY_UP);                        /* tune +2 semitones, locked */
    CHECK(klock_has(2, 0) && KLVAL[0][2][0] == g_tun + 32, "UP in step edit locks the knob (%d vs %d)", KLVAL[0][2][0], g_tun + 32);
    CHECK(P[P_KTUN] == g_tun, "the global tune is untouched");
    press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_RIGHT);   /* DRV */
    press(KEY_UP);
    CHECK(klock_has(2, 5) && KLVAL[0][2][5] == g_drv + 1, "drive lock (%d)", KLVAL[0][2][5]);
    CHECK(P[P_KDRV] == g_drv, "global drive untouched");
    sel(KEY_A);                                          /* unlock it */
    CHECK(!klock_has(2, 5), "SELECT+A unlocks");
    for (int i = 0; i < 3; i++) press(KEY_RIGHT);        /* ... LVL, COND */
    press(KEY_UP);
    CHECK(KCND[0][2] == 2, "UP on the condition cell (%d)", KCND[0][2]);
    sel(KEY_A);
    CHECK(KCND[0][2] == 0, "SELECT+A resets the condition");
    press(KEY_B);                                        /* leave */
    press(KEY_UP);                                       /* back on the pads: UP switches row */
    CHECK(KCND[0][2] == 0, "normal mode again");

    /* ---- SEQ tab: condition on a synth step ---- */
    press(KEY_L);                                        /* SEQ */
    press(KEY_RIGHT);                                    /* step 2 pitch */
    sel(KEY_UP); sel(KEY_UP); sel(KEY_UP);
    CHECK(SCND[0][1] == 3, "SELECT+UP/DOWN on a SEQ step (%d)", SCND[0][1]);
    int trigs = host_trig_n;
    press(KEY_A);
    synth_render(buf, FRAME_SAMPLES);
    CHECK(host_trig_n == trigs + 1, "A on SEQ plays the step when released");
    {   /* A+UP/DOWN = fine value on SEQ: changes the knob, does not play the step */
        int id = P_PIT0 + 1, v0 = P[id];
        trigs = host_trig_n;
        hold_a(KEY_UP);
        synth_render(buf, FRAME_SAMPLES);
        CHECK(P[id] != v0 && host_trig_n == trigs, "A+UP on SEQ is a value change and does not play (%d->%d)", v0, P[id]);
        v0 = P[id];
        sel(KEY_DOWN);
        CHECK(P[id] == v0 && SCND[0][1] == 2, "SELECT+DOWN leaves the knob alone (cond %d)", SCND[0][1]);
    }
    press(KEY_R);
    synth_set_run(0);

    /* the step's voice is what the audition plays */
    synth_ktrigger_step(2);
    host_ktrig_n = 0;
    synth_render(buf, FRAME_SAMPLES);
    CHECK(host_ktrig_n == 1 && host_kv_last[0] == ((g_tun + 32) * 1365) >> 10, "audition uses the locks (%d)", host_kv_last[0]);

    {   /* SYNTH tab: A tap plays on release; A+UP/DOWN is the fine value and plays nothing */
        setup();
        int t0 = host_trig_n, v0;
        ui_update(KEY_A); synth_render(buf, FRAME_SAMPLES);
        ui_update(0);     synth_render(buf, FRAME_SAMPLES);
        CHECK(host_trig_n == t0 + 1, "SYNTH: A tap plays once (%d)", host_trig_n - t0);
        press(KEY_RIGHT);                                    /* cell 1 = VCO1 pitch */
        int id = P_V1F; v0 = P[id]; t0 = host_trig_n;
        hold_a(KEY_UP); synth_render(buf, FRAME_SAMPLES);
        int dfine = P[id] - v0; v0 = P[id];
        press(KEY_UP);
        int dnorm = P[id] - v0; v0 = P[id];
        sel(KEY_UP);
        CHECK(dfine != 0 && dnorm != 0 && dfine != dnorm && host_trig_n == t0, "SYNTH: fine %d vs normal %d, no play", dfine, dnorm);
        CHECK(P[id] - v0 == dnorm, "SYNTH: SELECT+UP is a normal step (%d)", P[id] - v0);
    }
    {   /* PATT: RIGHT at pattern 8 runs on to the next row, LEFT at the row start goes back */
        setup();
        for (int i = 0; i < 4; i++) press(KEY_R);            /* PATT, cursor on pattern 1 */
        for (int i = 0; i < 8; i++) press(KEY_RIGHT);
        press(KEY_A); synth_render(buf, FRAME_SAMPLES);
        CHECK(synth_kplay() == 8, "RIGHT passes pattern 8 on to 9 (%d)", synth_kplay());
        press(KEY_LEFT); press(KEY_A); synth_render(buf, FRAME_SAMPLES);
        CHECK(synth_kplay() == 7, "LEFT at the row start returns to pattern 8 (%d)", synth_kplay());
    }
    printf(fails ? "klock tests: %d FAILED\n" : "klock tests: all ok\n", fails);
    return fails != 0;
}

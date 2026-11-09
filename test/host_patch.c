/* Patch bay + synth step conditions: engine behaviour and UI. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../src/synth.h"
#include "../src/ui.h"
extern int host_trig_n;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int8_t buf[FRAME_SAMPLES];
static void setup(void)
{
    preset_load(0); kick_preset_load(0); patch_clear(); patterns_init(); fx_defaults();
    P[P_TEMPO] = 255; synth_init(); ui_init(); ui_update(0);
}
static void press(unsigned k) { ui_update(k); ui_update(0); }

/* energy of n buffers of the synth (kick silenced) */
static double energy(int n, int skip)
{
    double e = 0;
    for (int f = 0; f < n + skip; f++) {
        synth_render(buf, FRAME_SAMPLES);
        if (f >= skip) for (int i = 0; i < FRAME_SAMPLES; i++) e += (double)buf[i] * buf[i];
    }
    return e;
}
static void quiet_kick(void) { for (int i = 0; i < 16; i++) KPAT[0][i] = 0; }

int main(void)
{
    /* ---- synth step conditions ---- */
    for (int c = 0; c < NCOND; c++) {
        setup(); quiet_kick();
        for (int i = 0; i < 8; i++) P[P_VEL0 + i] = 0;
        P[P_VEL0] = 100; SCND[0][0] = (uint8_t)c;
        synth_set_run(1);
        int per_bar[40] = { 0 }, bar = -1, last_ks = -1, guard = 0;
        while (bar < 34 && guard++ < 400000) {
            int before = host_trig_n;
            synth_render(buf, FRAME_SAMPLES);
            int ks = synth_kstep();
            if (ks < last_ks) bar++;
            if (bar >= 0 && bar < 40) per_bar[bar] += host_trig_n - before;
            last_ks = ks;
        }
        for (int b = 0; b < 33; b++) {                    /* bar index b is bar counter b + 1 (the first bar was counted as -1) */
            int fire = ((b + 1) % COND_N[c]) == COND_X[c] - 1;
            CHECK(per_bar[b] == (fire ? 2 : 0), "synth cond %d bar %d: %d triggers", c, b, per_bar[b]);
        }
    }

    /* ---- VCA input: velocity opens the VCA ---- */
    setup(); quiet_kick();
    for (int i = 0; i < 8; i++) { P[P_VEL0 + i] = 60; }
    P[P_ADEC] = 120;
    synth_set_run(1);
    double e0 = energy(80, 10);
    setup(); quiet_kick();
    for (int i = 0; i < 8; i++) { P[P_VEL0 + i] = 60; }
    P[P_ADEC] = 120;
    P[P_PBS0 + 5] = 2; P[P_PBA0 + 5] = 99;               /* VEL -> VCA */
    synth_set_run(1);
    double e1 = energy(80, 10);
    CHECK(e1 > e0 * 1.4, "VEL>VCA raises the level (%g vs %g)", e1, e0);
    setup(); quiet_kick();
    for (int i = 0; i < 8; i++) { P[P_VEL0 + i] = 60; }
    P[P_ADEC] = 120;
    P[P_PBS0 + 5] = 2; P[P_PBA0 + 5] = -99;              /* negative: closes it */
    synth_set_run(1);
    double e2 = energy(80, 10);
    CHECK(e2 < e0 * 0.6, "negative amount lowers it (%g vs %g)", e2, e0);

    /* ---- decay input: velocity lengthens the decays at the trigger ---- */
    setup(); quiet_kick();
    for (int i = 0; i < 8; i++) { P[P_VEL0 + i] = 0; }
    P[P_VEL0] = 100; P[P_ADEC] = 90; P[P_VDEC] = 90; P[P_FDEC] = 90;
    synth_set_run(1);
    double d0 = energy(12, 2);
    setup(); quiet_kick();
    for (int i = 0; i < 8; i++) { P[P_VEL0 + i] = 0; }
    P[P_VEL0] = 100; P[P_ADEC] = 90; P[P_VDEC] = 90; P[P_FDEC] = 90;
    P[P_PBS0 + 6] = 2; P[P_PBA0 + 6] = 99;
    synth_set_run(1);
    double d1 = energy(12, 2);
    CHECK(d1 > d0 * 1.3, "VEL>DEC lengthens the note (%g vs %g)", d1, d0);

    /* ---- every cable at once, random patches: bounded output ---- */
    int bad = 0;
    for (uint32_t sd = 1; sd < 120; sd++) {
        setup();
        preset_random(sd * 7u); P[P_TEMPO] = 200; kick_random_sound(sd);
        patch_random(sd);
        int n = 0; for (int d = 0; d < 8; d++) n += P[P_PBS0 + d] != 0;
        CHECK(n >= 1 && n <= 4, "random cables %d", n);
        synth_set_run(1);
        int pk = 0;
        for (int f = 0; f < 250; f++) {
            synth_render(buf, FRAME_SAMPLES);
            for (int i = 0; i < FRAME_SAMPLES; i++) { int v = abs(buf[i]); if (v > pk) pk = v; }
        }
        if (pk > 127) bad++;
    }
    CHECK(!bad, "%d random patches broke the ceiling", bad);
    /* all sources into all inputs, maximum depth */
    for (int s = 1; s <= 8; s++) {
        setup();
        for (int d = 0; d < 8; d++) { P[P_PBS0 + d] = (int16_t)s; P[P_PBA0 + d] = (d & 1) ? -99 : 99; }
        synth_set_run(1);
        int pk = 0;
        for (int f = 0; f < 400; f++) { synth_render(buf, FRAME_SAMPLES); for (int i = 0; i < FRAME_SAMPLES; i++) { int v = abs(buf[i]); if (v > pk) pk = v; } }
        CHECK(pk <= 127, "source %d everywhere: peak %d", s, pk);
    }

    /* ---- cables belong to the pattern; a preset empties them ---- */
    setup();
    P[P_PBS0 + 2] = 4; P[P_PBA0 + 2] = -40;
    pattern_flush();
    synth_queue_pattern(5);
    CHECK(P[P_PBS0 + 2] == 0, "another pattern has its own (empty) bay");
    P[P_PBS0 + 1] = 8; P[P_PBA0 + 1] = 30;
    synth_queue_pattern(0);
    CHECK(P[P_PBS0 + 2] == 4 && P[P_PBA0 + 2] == -40 && P[P_PBS0 + 1] == 0, "pattern 1 cables restored");
    synth_queue_pattern(5);
    CHECK(P[P_PBS0 + 1] == 8 && P[P_PBA0 + 1] == 30, "pattern 6 cables restored");
    pattern_copy(); synth_queue_pattern(9); pattern_paste();
    CHECK(P[P_PBS0 + 1] == 8, "paste carries the cables");
    preset_load_sound(2);
    CHECK(P[P_PBS0 + 1] == 0 && P[P_PBA0 + 1] == 0, "a preset empties the patch bay");

    /* ---- UI: PATCH tab ---- */
    setup();
    press(KEY_R);                                        /* PATCH */
    press(KEY_UP);                                       /* source of VC1: OFF -> SEQ */
    CHECK(P[P_PBS0] == 1 && P[P_PBA0] == 50, "new cable starts at half depth (%d %d)", P[P_PBS0], P[P_PBA0]);
    press(KEY_UP);
    CHECK(P[P_PBS0] == 2 && P[P_PBA0] == 50, "next source keeps the amount");
    for (int i = 0; i < 8; i++) press(KEY_RIGHT);        /* amount of VC1 */
    press(KEY_DOWN); press(KEY_DOWN);
    CHECK(P[P_PBA0] == 48, "amount (%d)", P[P_PBA0]);
    press(KEY_A);
    CHECK(P[P_PBS0] == 0 && P[P_PBA0] == 0, "A unplugs");
    ui_update(KEY_SELECT); ui_update(KEY_SELECT | KEY_START); ui_update(KEY_SELECT); ui_update(0);
    int n = 0; for (int d = 0; d < 8; d++) n += P[P_PBS0 + d] != 0;
    CHECK(n >= 1, "SELECT+START plugs random cables (%d)", n);

    printf(fails ? "patch tests: %d FAILED\n" : "patch tests: all ok\n", fails);
    return fails != 0;
}

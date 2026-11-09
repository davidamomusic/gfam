/* Pattern queue, per-pattern steps, copy/paste and song mode logic. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "../src/synth.h"
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int8_t buf[FRAME_SAMPLES];
static int last_ks = -1;                  /* carried across calls so one bar start counts once */
/* run until n bars have started; records kplay / song slot at each bar start */
static int bars(int n, int *kp, int *sp, int *qu)
{
    int got = 0, guard = 0;
    while (got < n && guard++ < 200000) {
        synth_render(buf, FRAME_SAMPLES);
        int ks = synth_kstep();
        if (ks == 0 && last_ks != 0) { kp[got] = synth_kplay(); sp[got] = synth_song_pos(); qu[got] = synth_kqueued(); got++; }
        last_ks = ks;
    }
    return got;
}
static void setup(void)
{
    preset_load(0); kick_preset_load(0); patterns_init(); fx_defaults();
    P[P_TEMPO] = 255;                                   /* fastest */
    synth_init(); last_ks = -1;
}
int main(void)
{
    int kp[24], sp[24], qu[24];

    /* ---- queue ---- */
    setup();
    synth_queue_pattern(3);
    CHECK(synth_kplay() == 3 && synth_kqueued() == -1, "stopped queue applies at once");
    synth_set_run(1);
    bars(1, kp, sp, qu);
    CHECK(kp[0] == 3, "bar1 pattern %d", kp[0]);
    synth_render(buf, FRAME_SAMPLES);
    synth_queue_pattern(5);
    CHECK(synth_kplay() == 3 && synth_kqueued() == 5, "running queue waits");
    bars(2, kp, sp, qu);
    CHECK(kp[0] == 5 && qu[0] == -1 && kp[1] == 5, "queued pattern %d,%d q%d", kp[0], kp[1], qu[0]);
    synth_queue_pattern(5);
    CHECK(synth_kqueued() == -1, "re-queue same pattern");

    /* ---- synth steps live in the pattern ---- */
    setup();
    P[P_VEL0 + 2] = 99; P[P_PIT0 + 2] = 5 * 16;         /* edit pattern 1 */
    pattern_flush();
    CHECK(PSEQ[0][8 + 2] == 99, "flush stores steps");
    synth_queue_pattern(1);                              /* stopped: instant, pattern 2 is empty */
    CHECK(P[P_VEL0 + 2] == 0 && P[P_PIT0 + 2] == 0, "empty pattern clears live steps (%d)", P[P_VEL0 + 2]);
    P[P_VEL0 + 4] = 77;                                  /* edit pattern 2 without an explicit flush */
    synth_queue_pattern(0);                              /* switching flushes the one we leave */
    CHECK(PSEQ[1][8 + 4] == 77, "switch keeps edits of the old pattern");
    CHECK(P[P_VEL0 + 2] == 99 && P[P_PIT0 + 2] == 80, "pattern 1 restored (%d %d)", P[P_VEL0 + 2], P[P_PIT0 + 2]);
    CHECK(pattern_used(0) && pattern_used(1), "used patterns");
    CHECK(!pattern_used(12), "unused pattern");

    /* ---- copy / paste / clear on the current pattern ---- */
    pattern_copy();
    synth_queue_pattern(12);
    pattern_paste();
    CHECK(P[P_VEL0 + 2] == 99 && KPAT[12][0] == KPAT[0][0] && KPAT[12][4] == KPAT[0][4], "paste");
    pattern_clear();
    CHECK(P[P_VEL0 + 2] == 0 && KPAT[12][0] == 0 && !pattern_used(12), "clear");
    pattern_random(1234u);
    CHECK(pattern_used(12) && P[P_VEL0] > 0, "random beat");

    /* ---- song ---- */
    setup();
    SONG[1] = 1; SONG[2] = 2; SONG[3] = 2; SONG[5] = 4;  /* slot 0 empty, slot 4 empty */
    synth_song_start();
    CHECK(synth_song_on(), "song on");
    int n = bars(8, kp, sp, qu);
    CHECK(n == 8, "bars %d", n);
    static const int exp_k[8] = { 0, 1, 1, 3, 0, 1, 1, 3 };
    static const int exp_s[8] = { 1, 2, 3, 5, 1, 2, 3, 5 };
    for (int i = 0; i < 8; i++)
        CHECK(kp[i] == exp_k[i] && sp[i] == exp_s[i], "song bar %d: pat %d slot %d", i, kp[i], sp[i]);
    synth_song_start();                                  /* start/stop toggle */
    CHECK(!synth_song_on() && !synth_running(), "song stops");
    synth_song_start();
    last_ks = -1;
    bars(1, kp, sp, qu);
    CHECK(sp[0] == 1 && kp[0] == 0, "restart from the first filled slot (%d %d)", sp[0], kp[0]);
    synth_set_run(0);
    CHECK(!synth_song_on(), "START stops the song");

    /* hand picking a pattern leaves the song but keeps playing */
    synth_song_start(); last_ks = -1;
    bars(2, kp, sp, qu);
    synth_song_off();
    synth_queue_pattern(7);
    bars(3, kp, sp, qu);
    CHECK(kp[0] == 7 && kp[1] == 7 && kp[2] == 7 && synth_running(), "song off, pattern stays (%d)", kp[0]);

    /* empty song does nothing */
    setup();
    synth_song_start();
    CHECK(!synth_song_on() && !synth_running(), "empty song");

    /* song steps switch the live synth steps too */
    setup();
    PSEQ[0][8] = 0; PSEQ[3][8] = 111;
    SONG[0] = 4; SONG[1] = 1;
    synth_song_start(); last_ks = -1;
    bars(1, kp, sp, qu);
    CHECK(kp[0] == 3 && P[P_VEL0] == 111, "song loads pattern steps (%d)", P[P_VEL0]);
    bars(1, kp, sp, qu);
    CHECK(kp[0] == 0 && P[P_VEL0] != 111, "next slot loads its own steps");

    /* all song slots (NSONG), patterns above 16 too */
    setup();
    for (int i = 0; i < NSONG; i++) SONG[i] = (uint8_t)(1 + ((i * 3) % NPAT));
    synth_song_start(); last_ks = -1;
    { int a[NSONG + 3], b[NSONG + 3], c[NSONG + 3]; int got = bars(NSONG + 2, a, b, c);
      CHECK(got == NSONG + 2 && a[NSONG - 1] == ((NSONG - 1) * 3) % NPAT && b[NSONG - 1] == NSONG - 1 && a[NSONG] == 0 && b[NSONG] == 0,
            "full song wraps (%d %d)", a[NSONG - 1], b[NSONG]); }
    CHECK(NPAT == 32 && NSONG == 32, "32 patterns, 32 song slots");

    /* ---- sound lives in the pattern too; BPM and volume stay global ---- */
    setup();
    P[P_TEMPO] = 100; P[P_VOL] = 180;
    P[P_CUT] = 77; P[P_KTUN] = 400; P[P_KDRV] = 9;       /* sound of pattern 1 */
    pattern_flush();
    synth_queue_pattern(1);                               /* stopped: switch at once */
    CHECK(P[P_CUT] == 190 && P[P_KTUN] == 26 * 16, "other patterns keep their own (initial) sound (%d %d)", P[P_CUT], P[P_KTUN]);
    P[P_CUT] = 150; P[P_KTUN] = 300; P[P_KDRV] = 200; P[P_TEMPO] = 120; P[P_VOL] = 90;   /* sound of pattern 2 */
    synth_queue_pattern(0);
    CHECK(P[P_CUT] == 77 && P[P_KTUN] == 400 && P[P_KDRV] == 9, "pattern 1 sound restored (%d %d %d)", P[P_CUT], P[P_KTUN], P[P_KDRV]);
    CHECK(P[P_TEMPO] == 120 && P[P_VOL] == 90, "BPM and volume are global (%d %d)", P[P_TEMPO], P[P_VOL]);
    synth_queue_pattern(1);
    CHECK(P[P_CUT] == 150 && P[P_KTUN] == 300 && P[P_KDRV] == 200, "pattern 2 sound restored");
    /* sound change while running comes in at the bar start, the song carries it along */
    synth_queue_pattern(0);
    SONG[0] = 2; SONG[1] = 1;
    synth_song_start(); synth_set_run(1); last_ks = -1;
    synth_set_run(0); synth_song_start(); last_ks = -1;
    bars(1, kp, sp, qu);
    CHECK(kp[0] == 1 && P[P_CUT] == 150, "song bar 1 uses pattern 2 sound (%d)", P[P_CUT]);
    bars(1, kp, sp, qu);
    CHECK(kp[0] == 0 && P[P_CUT] == 77, "song bar 2 uses pattern 1 sound (%d)", P[P_CUT]);
    synth_set_run(0);
    /* copy / paste carry the sound, clear keeps it */
    pattern_copy();
    synth_queue_pattern(5);
    pattern_paste();
    CHECK(P[P_CUT] == 77 && P[P_KTUN] == 400, "paste carries the sound");
    P[P_CUT] = 5;
    pattern_clear();
    CHECK(P[P_CUT] == 5 && !pattern_used(5), "clear keeps the sound");
    /* random never touches BPM / volume */
    for (uint32_t sd = 1; sd < 200; sd++) {
        P[P_TEMPO] = 77; P[P_VOL] = 123;
        preset_random_part(sd, 1);  CHECK(P[P_TEMPO] == 77 && P[P_VOL] == 123, "random sound moved BPM (seed %u)", sd);
        preset_random_part(sd, 0);  CHECK(P[P_TEMPO] == 77 && P[P_VOL] == 123, "random steps moved BPM");
        pattern_random(sd);         CHECK(P[P_TEMPO] == 77 && P[P_VOL] == 123, "random pattern moved BPM");
        kick_random_sound(sd);      CHECK(P[P_TEMPO] == 77, "random kick");
    }

    printf(fails ? "pattern tests: %d FAILED\n" : "pattern tests: all ok\n", fails);
    return fails != 0;
}

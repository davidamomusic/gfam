/* Dumps the screens used in the README and docs as raw 32x32 u16 tilemaps (one file per screen).
 * It drives the real UI code with key presses, exactly as a player would, so the screens are what the
 * GBA draws. tools/make_screens.py builds this, runs it and turns the tilemaps into PNG files.
 *   usage: host_screens OUTDIR */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/synth.h"
#include "../src/ui.h"

static const char *outdir = "build/screens";

static void shot(const char *name)
{
    static uint16_t map[1024];
    char path[512];
    ui_draw(map);
    snprintf(path, sizeof path, "%s/%s.bin", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fwrite(map, 2, 1024, f);
    fclose(f);
}

static void press(unsigned k) { ui_update(k); ui_update(0); }
static void press_n(unsigned k, int n) { for (int i = 0; i < n; i++) press(k); }
static void sel(unsigned k) { ui_update(KEY_SELECT); ui_update(KEY_SELECT | k); ui_update(KEY_SELECT); ui_update(0); }
static void sel_n(unsigned k, int n) { for (int i = 0; i < n; i++) sel(k); }
static void hold_a(unsigned k) { ui_update(KEY_A); ui_update(KEY_A | k); ui_update(KEY_A); ui_update(0); }
static void start_combo(unsigned k) { ui_update(KEY_START); ui_update(KEY_START | k); ui_update(KEY_START); ui_update(0); }

static void run_for(int samples)                  /* let the sequencer play */
{
    static int8_t buf[FRAME_SAMPLES];
    for (int n = 0; n < samples; n += FRAME_SAMPLES) synth_render(buf, FRAME_SAMPLES);
    ui_update(0);
}

static void goto_tab(int t)                       /* 0 SYNTH 1 PATCH 2 SEQ 3 KICK 4 PATT 5 FX, from SYNTH */
{
    press_n(KEY_R, t);
}

int main(int argc, char **argv)
{
    if (argc > 1) outdir = argv[1];

    preset_load(0); kick_preset_load(0); patterns_init(); fx_defaults();
    synth_init();
    ui_init();
    ui_update(0);

    /* ---- SYNTH ---- */
    shot("synth-1");                              /* 808 KICK, cursor on VCO 1 WAVE */
    sel(KEY_R);                                   /* SEQ BASS */
    press_n(KEY_RIGHT, 13); press_n(KEY_UP, 4);   /* FILTER CUT */
    press(KEY_START); run_for(5200);
    shot("synth-2");
    sel_n(KEY_R, 4);                              /* ZAP SYNC (hard sync lamp on) */
    press_n(KEY_LEFT, 5);                         /* MOD group */
    run_for(2600);
    shot("synth-3");
    press(KEY_START);                             /* stop */

    /* ---- PATCH ---- */
    goto_tab(1);
    shot("patch-1");                              /* empty patch bay */
    press_n(KEY_UP, 2);                           /* VC1 <- VEL */
    press_n(KEY_RIGHT, 4); press_n(KEY_UP, 3);    /* FM <- VCO envelope */
    press_n(KEY_RIGHT, 2); press_n(KEY_UP, 6);    /* DEC <- NSE */
    sel(KEY_RIGHT);                               /* amounts row */
    press_n(KEY_DOWN, 3);
    shot("patch-2");

    /* ---- SEQ ---- */
    press(KEY_R);
    preset_load_sound(1);
    press(KEY_START); run_for(4200);
    press_n(KEY_RIGHT, 2); press_n(KEY_UP, 6);
    sel(KEY_UP); sel(KEY_UP); sel(KEY_UP);        /* a trig condition on step 3 */
    shot("seq-1");
    press_n(KEY_RIGHT, 3); sel_n(KEY_UP, 7);      /* step 6, another condition */
    press_n(KEY_UP, 4);
    run_for(5600);
    shot("seq-2");

    /* ---- KICK ---- */
    press(KEY_R);
    run_for(2400);
    shot("kick-1");                               /* pads, kick playing */
    press_n(KEY_RIGHT, 16);                       /* on to the kick sound knobs */
    press_n(KEY_UP, 3);
    run_for(3000);
    shot("kick-2");
    press_n(KEY_LEFT, 5);                         /* back on a pad */
    press(KEY_B);                                 /* STEP EDIT */
    press_n(KEY_UP, 3);
    press_n(KEY_RIGHT, 2); press_n(KEY_DOWN, 3);
    shot("kick-3");
    press_n(KEY_RIGHT, 6);                        /* the condition cell */
    press_n(KEY_UP, 2);
    shot("kick-4");
    press(KEY_B);

    /* ---- PATT ---- */
    press(KEY_R);
    press_n(KEY_RIGHT, 4);
    shot("patt-1");                               /* the 32 patterns; the first ten hold the factory beats */
    press_n(KEY_LEFT, 4);
    press_n(KEY_DOWN, 5);                         /* the song slots */
    press(KEY_A);                                 /* slot 1 <- pattern 01 */
    start_combo(KEY_UP); press(KEY_RIGHT); press(KEY_A);
    start_combo(KEY_UP); press(KEY_RIGHT); press(KEY_A);
    start_combo(KEY_DOWN); press(KEY_RIGHT); press(KEY_A);
    press(KEY_UP); press_n(KEY_RIGHT, 1);         /* on PLAY */
    press(KEY_A);                                 /* play the song */
    run_for(3000);
    shot("patt-2");

    /* ---- FX ---- */
    press(KEY_R);
    press(KEY_A);                                 /* compressor on */
    press_n(KEY_RIGHT, 6); press(KEY_A);          /* delay on */
    press_n(KEY_RIGHT, 4); press(KEY_A);          /* reverb on */
    press_n(KEY_RIGHT, 5); press_n(KEY_UP, 3);    /* limiter threshold (the limiter is on by default) */
    shot("fx-1");

    /* ---- BPM edit, options, help ---- */
    press(KEY_R);                                 /* back to SYNTH */
    sel(KEY_B);                                   /* SELECT+B: BPM edit mode */
    press_n(KEY_UP, 9); press(KEY_RIGHT);
    shot("bpm-1");
    sel(KEY_B);                                   /* leave BPM edit */
    press(KEY_SELECT);                            /* SELECT tap: options */
    shot("menu-1");
    press(KEY_DOWN); press(KEY_A);                /* HELP */
    for (int i = 0; i < 10; i++) {
        char name[32];
        snprintf(name, sizeof name, "help-%d", i + 1);
        shot(name);
        press(KEY_RIGHT);
    }
    return 0;
}

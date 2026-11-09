/* Button-gesture test (white box: includes ui.c to see its static state).
 *   SELECT tap     -> OPTIONS menu
 *   SELECT + B     -> BPM edit mode on/off (either press order)
 *   B on SYNTH/SEQ -> nothing (no manual step advance)
 * build: gcc -DHOST -O2 -o build/host_buttons test/host_buttons.c src/synth.c src/params.c src/fx.c -lm */
#include <stdio.h>
#include "../src/ui.c"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void frame(unsigned k) { ui_update(k); }
static void tap(unsigned k) { frame(k); frame(0); }
static void combo(unsigned hold, unsigned k) { frame(hold); frame(hold | k); frame(hold); frame(0); }   /* hold first, then k */

static void reset(void)
{
    preset_load(1); kick_preset_load(0); patterns_init();
    synth_init();
    ui_init();
    frame(0);                                  /* arms the input */
}

static void goto_tab(int t) { while (tab != t) tap(KEY_R); }

int main(void)
{
    /* ---- SELECT tap opens OPTIONS, does not touch BPM mode ---- */
    reset();
    tap(KEY_SELECT);
    CHECK(menu == 1);
    CHECK(bpm_mode == 0);
    tap(KEY_B);                                /* B closes the menu */
    CHECK(menu == 0);

    /* menu still navigates as before: DOWN, A -> help; B -> back; RESUME closes */
    tap(KEY_SELECT); tap(KEY_DOWN); tap(KEY_A);
    CHECK(menu == 2);
    tap(KEY_B);
    CHECK(menu == 1);
    tap(KEY_UP); tap(KEY_A);                   /* RESUME */
    CHECK(menu == 0);

    /* ---- SELECT+B toggles BPM edit mode, never opens the menu ---- */
    reset();
    combo(KEY_SELECT, KEY_B);
    CHECK(bpm_mode == 1);
    CHECK(menu == 0);
    combo(KEY_SELECT, KEY_B);
    CHECK(bpm_mode == 0);
    CHECK(menu == 0);

    /* other press order: B held first, then SELECT */
    reset();
    frame(KEY_B); frame(KEY_B | KEY_SELECT); frame(KEY_B); frame(0);
    CHECK(bpm_mode == 1);
    CHECK(menu == 0);

    /* ---- BPM edit mode works: UP/DOWN +-1, LEFT/RIGHT +-10 ---- */
    reset();
    combo(KEY_SELECT, KEY_B);
    CHECK(bpm_mode == 1);
    int t0 = P[P_TEMPO];
    tap(KEY_UP);    CHECK(P[P_TEMPO] == t0 + 1);
    tap(KEY_DOWN);  CHECK(P[P_TEMPO] == t0);
    tap(KEY_RIGHT); CHECK(P[P_TEMPO] == t0 + 10);
    tap(KEY_LEFT);  CHECK(P[P_TEMPO] == t0);
    combo(KEY_SELECT, KEY_B);
    CHECK(bpm_mode == 0);
    tap(KEY_UP);                               /* normal again: UP turns a knob, not the tempo */
    CHECK(P[P_TEMPO] == t0);

    /* opening the menu leaves BPM mode */
    reset();
    combo(KEY_SELECT, KEY_B);
    tap(KEY_SELECT);
    CHECK(menu == 1);
    CHECK(bpm_mode == 0);

    /* ---- B no longer advances the step on SYNTH and SEQ (stopped) ---- */
    reset();
    CHECK(tab == TAB_SYNTH);
    CHECK(!synth_running());
    int s0 = synth_step();
    tap(KEY_B); tap(KEY_B); tap(KEY_B);
    CHECK(synth_step() == s0);
    goto_tab(TAB_SEQ);
    tap(KEY_B); tap(KEY_B);
    CHECK(synth_step() == s0);

    /* ---- KICK keeps its B gestures ---- */
    reset();
    goto_tab(TAB_KICK);
    CHECK(kedit == 0);
    tap(KEY_B);                                /* B on a pad: step edit */
    CHECK(kedit == 1);
    tap(KEY_B);                                /* B again: back to the pads */
    CHECK(kedit == 0);
    combo(KEY_SELECT, KEY_B);                  /* SELECT+B on KICK: BPM mode only, no step edit */
    CHECK(bpm_mode == 1);
    CHECK(kedit == 0);

    /* ---- other SELECT combos do not open the menu or toggle BPM mode ---- */
    reset();
    combo(KEY_SELECT, KEY_START);              /* random */
    CHECK(menu == 0 && bpm_mode == 0);
    combo(KEY_SELECT, KEY_R);                  /* sound preset */
    CHECK(menu == 0 && bpm_mode == 0);
    combo(KEY_SELECT, KEY_RIGHT);
    CHECK(menu == 0 && bpm_mode == 0);
    combo(KEY_SELECT, KEY_UP);
    CHECK(menu == 0 && bpm_mode == 0);
    CHECK(tab == TAB_SYNTH);                   /* SELECT+R is a preset, not a tab change */

    /* START tap still runs / stops */
    reset();
    tap(KEY_START); CHECK(synth_running());
    tap(KEY_START); CHECK(!synth_running());

    printf("%d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}

/* Dumps the UI tilemap for several pages/states to build/ui_N.bin (32x32 u16 each). */
#include <stdio.h>
#include <stdint.h>
#include "../src/synth.h"
#include "../src/ui.h"

static void dump(const char *path)
{
    static uint16_t map[1024];
    ui_draw(map);
    FILE *f = fopen(path, "wb");
    fwrite(map, 2, 1024, f);
    fclose(f);
}

static void press(unsigned k) { ui_update(k); ui_update(0); }

static void sel(unsigned k) { ui_update(KEY_SELECT); ui_update(KEY_SELECT | k); ui_update(0); }
static void start_combo(unsigned k) { ui_update(KEY_START); ui_update(KEY_START | k); ui_update(KEY_START); ui_update(0); }

static void hold_a(unsigned k) { ui_update(KEY_A); ui_update(KEY_A | k); ui_update(KEY_A); ui_update(0); }

int main(void)
{
    preset_load(1); kick_preset_load(0); patterns_init();
    synth_init();
    ui_init();
    ui_update(0);
    dump("build/ui_0.bin");                       /* SYNTH, stopped                 */
    press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_UP); press(KEY_UP);
    sel(KEY_RIGHT); sel(KEY_RIGHT); press(KEY_UP);
    dump("build/ui_1.bin");                       /* third row                      */
    press(KEY_R);                                 /* PATCH */
    dump("build/ui_2.bin");                       /* empty patch bay                */
    press(KEY_UP); press(KEY_UP);                 /* VC1 <- VEL */
    press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_RIGHT);
    press(KEY_UP); press(KEY_UP); press(KEY_UP);  /* RES <- VCA EG */
    press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_UP); press(KEY_UP); press(KEY_UP); press(KEY_UP); press(KEY_UP); press(KEY_UP);
    sel(KEY_RIGHT); press(KEY_DOWN); press(KEY_DOWN); press(KEY_DOWN);
    dump("build/ui_3.bin");                       /* cables plugged, amount selected */
    press(KEY_R);
    synth_set_run(1);
    hold_a(KEY_UP); hold_a(KEY_UP);
    press(KEY_RIGHT); press(KEY_RIGHT); hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP);
    dump("build/ui_4.bin");                       /* SEQ with step conditions       */
    press(KEY_R);
    press(KEY_RIGHT); press(KEY_RIGHT);
    hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP); hold_a(KEY_UP);
    dump("build/ui_5.bin");                       /* KICK pads, condition 2:3        */
    press(KEY_B);
    press(KEY_UP); press(KEY_UP); press(KEY_UP);
    press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_DOWN); press(KEY_DOWN); press(KEY_DOWN);
    dump("build/ui_6.bin");                       /* step edit                      */
    for (int i = 0; i < 6; i++) press(KEY_RIGHT);
    press(KEY_UP); press(KEY_UP);
    dump("build/ui_7.bin");                       /* step edit, condition cell      */
    press(KEY_B);
    press(KEY_R);
    start_combo(KEY_UP); start_combo(KEY_UP);
    dump("build/ui_8.bin");                       /* PATT                           */
    press(KEY_R);
    for (int i = 0; i < 15; i++) press(KEY_RIGHT);
    press(KEY_UP); press(KEY_UP); press(KEY_UP);
    dump("build/ui_9.bin");                       /* FX                             */
    press(KEY_SELECT);                            /* SELECT tap opens the options window */
    dump("build/ui_10.bin");                      /* options window                 */
    press(KEY_DOWN); press(KEY_A);
    for (int i = 0; i < 4; i++) press(KEY_RIGHT);
    dump("build/ui_11.bin");                      /* help: KICK                     */
    press(KEY_LEFT); press(KEY_LEFT);
    dump("build/ui_12.bin");                      /* help: SEQ                      */
    press(KEY_RIGHT); press(KEY_RIGHT); press(KEY_RIGHT);
    dump("build/ui_13.bin");                      /* help: PATCH                    */
    printf("ok\n");
    return 0;
}

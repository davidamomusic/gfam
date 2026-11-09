/* Effects test: renders a single kick hit with each device alone and with everything on. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/synth.h"
#define RATE (16777216 / TIMER_PERIOD)
static int8_t out[RATE * 3];
static void render(const char *name, int dly, int rev, int stu, int cmp, int lim)
{
    preset_load(0); kick_preset_load(0); patterns_init(); fx_defaults();
    for (int i = 0; i < 8; i++) P[P_VEL0 + i] = 0;
    P[P_VEL0] = 127;                        /* one synth hit per bar of 8 steps */
    for (int i = 0; i < 16; i++) KPAT[0][i] = (i == 0 || i == 5 || i == 8) ? 1 : 0;
    P[P_TEMPO] = 80;
    P[P_FDLY] = dly; P[P_FREV] = rev; P[P_FSTU] = stu; P[P_FCMP] = cmp; P[P_FLIM] = lim;
    P[P_FDLYM] = 200; P[P_FREVM] = 255; P[P_FSTUM] = 255; P[P_FCMPM] = 255; P[P_FCAM] = 200;
    synth_init(); synth_set_run(1);
    int total = (int)(sizeof out / FRAME_SAMPLES) * FRAME_SAMPLES;
    for (int i = 0; i < total; i += FRAME_SAMPLES) synth_render(out + i, FRAME_SAMPLES);
    int pk = 0; double e = 0;
    for (int i = 0; i < total; i++) { int v = abs(out[i]); if (v > pk) pk = v; e += out[i] * out[i]; }
    char path[64]; snprintf(path, sizeof path, "build/fx_%s.raw", name);
    FILE *f = fopen(path, "wb"); fwrite(out, 1, total, f); fclose(f);
    printf("%-8s peak %3d rms %.3f\n", name, pk, __builtin_sqrt(e / total) / 128);
}
int main(void)
{
    render("dry", 0, 0, 0, 0, 1);
    render("comp", 0, 0, 0, 1, 1);
    render("stut", 0, 0, 1, 0, 1);
    render("delay", 1, 0, 0, 0, 1);
    render("reverb", 0, 1, 0, 0, 1);
    render("all", 1, 1, 1, 1, 1);
    render("all_nolim", 1, 1, 1, 1, 0);
    return 0;
}

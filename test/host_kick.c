/* Renders every kick preset on its own (main sequencer silenced) and every VCO wave. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/synth.h"
#define RATE (16777216 / TIMER_PERIOD)
static void write_raw(const char *path, const int8_t *s, int n) { FILE *f = fopen(path, "wb"); fwrite(s, 1, n, f); fclose(f); }
int main(void)
{
    static int8_t buf[FRAME_SAMPLES];
    int frames = (RATE * 4) / FRAME_SAMPLES;
    int8_t *all = malloc(frames * FRAME_SAMPLES);
    for (int k = 0; k < kick_preset_count(); k++) {
        preset_load(0);
        for (int i = 0; i < 8; i++) P[P_VEL0 + i] = 0;
        P[P_VOL] = 200; P[P_TEMPO] = 80;                 /* 120 BPM */
        kick_preset_load(k); patterns_init();
        fx_defaults(); synth_init(); synth_set_run(1);
        for (int f = 0; f < frames; f++) synth_render(all + f * FRAME_SAMPLES, FRAME_SAMPLES);
        char path[64]; snprintf(path, sizeof path, "build/kick_%d.raw", k);
        write_raw(path, all, frames * FRAME_SAMPLES);
        int pk = 0; for (int i = 0; i < frames * FRAME_SAMPLES; i++) if (abs(all[i]) > pk) pk = abs(all[i]);
        printf("kick %d %-9s peak %d\n", k, kick_preset_name(k), pk);
    }
    /* each VCO wave, one steady note, written raw */
    for (int w = 0; w < 8; w++) {
        preset_load(0);
        for (int i = 0; i < 8; i++) { P[P_VEL0 + i] = 127; P[P_PIT0 + i] = 0; }
        for (int i = 0; i < 16; i++) KPAT[0][i] = 0;
        P[P_V1W] = w; P[P_V1F] = 48 * 16; P[P_V1E] = 0; P[P_L1] = 255; P[P_L2] = 0; P[P_LN] = 0;
        P[P_CUT] = 255; P[P_RES] = 0; P[P_FEG] = 0; P[P_ADEC] = 200; P[P_VOL] = 200; P[P_TEMPO] = 0;
        synth_init(); synth_set_run(1);
        for (int f = 0; f < 20; f++) synth_render(all + f * FRAME_SAMPLES, FRAME_SAMPLES);
        char path[64]; snprintf(path, sizeof path, "build/wave_%d.raw", w);
        write_raw(path, all, 20 * FRAME_SAMPLES);
    }
    return 0;
}

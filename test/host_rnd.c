/* Stress test: 300 random presets must be audible and never exceed the limiter ceiling. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "../src/synth.h"
int main(void)
{
    int bad_quiet = 0, bad_peak = 0, N = 300;
    double minr = 9, maxr = 0; int maxpk = 0;
    static int8_t buf[FRAME_SAMPLES];
    for (int k = 0; k < N; k++) {
        preset_random(k * 7919u + 12345u);
        kick_preset_load(k % kick_preset_count()); patterns_init();
        if (k & 1) { kick_random_sound(k * 31u + 7u); kick_random_pattern(0, k * 17u + 3u); }
        fx_defaults(); synth_init(); synth_set_run(1);
        double e = 0; int pk = 0, n = 0;
        for (int fr = 0; fr < 130; fr++) {           /* ~2.2 s */
            synth_render(buf, FRAME_SAMPLES);
            if (fr < 4) continue;
            for (int i = 0; i < FRAME_SAMPLES; i++) { int v = buf[i]; e += v * v; n++; if (abs(v) > pk) pk = abs(v); }
        }
        double rms = sqrt(e / n) / 128.0;
        if (rms < 0.01) bad_quiet++;
        if (pk > 118) bad_peak++;
        if (rms < minr) minr = rms; if (rms > maxr) maxr = rms; if (pk > maxpk) maxpk = pk;
    }
    printf("random presets: %d  quiet(<0.01 rms): %d  over ceiling: %d  rms %.3f..%.3f  max peak %d\n",
           N, bad_quiet, bad_peak, minr, maxr, maxpk);
    /* factory presets */
    for (int p = 0; p < preset_count(); p++) {
        preset_load(p); synth_init(); synth_set_run(1);
        int pk = 0; double e = 0; int n = 0;
        for (int fr = 0; fr < 130; fr++) { synth_render(buf, FRAME_SAMPLES);
            for (int i = 0; i < FRAME_SAMPLES; i++) { int v = buf[i]; e += v*v; n++; if (abs(v) > pk) pk = abs(v); } }
        printf("  %-9s peak %3d rms %.3f\n", preset_name(p), pk, sqrt(e/n)/128.0);
    }
    return 0;
}

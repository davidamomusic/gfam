/* Stress: random effect settings (all devices, extreme feedback/size) must stay bounded by the limiter. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "../src/synth.h"
int main(void)
{
    static int8_t buf[FRAME_SAMPLES];
    int over = 0, silent = 0, N = 120; int maxpk = 0;
    srand(7);
    for (int k = 0; k < N; k++) {
        preset_random(k * 977u + 5u); kick_preset_load(k % kick_preset_count()); patterns_init(); fx_defaults();
        P[P_FCMP] = rand() & 1; P[P_FSTU] = rand() & 1; P[P_FDLY] = rand() & 1; P[P_FREV] = rand() & 1;
        P[P_FCMPM] = rand() & 255; P[P_FSTUM] = rand() & 255; P[P_FDLYM] = rand() & 255; P[P_FREVM] = rand() & 255;
        P[P_FCAM] = rand() & 255; P[P_FSTR] = rand() & 3; P[P_FDLT] = rand() & 3;
        P[P_FDFB] = (k & 1) ? 255 : rand() & 255; P[P_FRSZ] = (k & 2) ? 255 : rand() & 255; P[P_FRTN] = rand() & 255;
        P[P_TEMPO] = rand() % 256;
        P[P_FLTH] = (k % 5 == 0) ? 0 : (k % 5 == 1) ? 100 : rand() % 101;      /* limiter threshold 0 .. -10 dB */
        double ceil_ = 127.0 * pow(10.0, -P[P_FLTH] / 200.0);
        synth_init(); synth_set_run(1);
        long e = 0; int pk = 0;
        for (int f = 0; f < 900; f++) {
            if (f == 450) { P[P_FDLY] ^= 1; P[P_FREV] ^= 1; P[P_FSTU] ^= 1; }     /* toggling mid-run */
            synth_render(buf, FRAME_SAMPLES);
            for (int i = 0; i < FRAME_SAMPLES; i++) { int v = abs(buf[i]); if (v > pk) pk = v; e += v * v; }
        }
        if (pk > (int)ceil_ + 2) over++;                         /* negative-side floor and the dB interpolation add up to 2 LSB */
        if (e < 1000) silent++;
        if (pk > maxpk) maxpk = pk;
    }
    printf("fx stress: %d runs, over ceiling %d, silent %d, max peak %d\n", N, over, silent, maxpk);
    return over || silent;
}

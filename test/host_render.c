/* Host-side harness: renders each preset with the exact same DSP code the GBA runs. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/synth.h"

#define FRAME FRAME_SAMPLES
#define RATE (16777216 / TIMER_PERIOD)

static void write_wav(const char *path, const int8_t *s, int n)
{
    FILE *f = fopen(path, "wb");
    uint32_t dsz = n * 2, rsz = 36 + dsz, sr = RATE, br = RATE * 2;
    uint16_t fmt = 1, ch = 1, al = 2, bps = 16;
    uint32_t sixteen = 16;
    fwrite("RIFF", 1, 4, f); fwrite(&rsz, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    fwrite(&sixteen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
    fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f); fwrite(&al, 2, 1, f); fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dsz, 4, 1, f);
    for (int i = 0; i < n; i++) { int16_t v = (int16_t)(s[i] * 256); fwrite(&v, 2, 1, f); }
    fclose(f);
}

int main(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 4;
    int frames = (RATE * secs) / FRAME;
    int8_t *buf = malloc(frames * FRAME);
    for (int p = 0; p < preset_count(); p++) {
        preset_load(p);
        synth_init();
        synth_set_run(1);
        for (int fr = 0; fr < frames; fr++) synth_render(buf + fr * FRAME, FRAME);
        char path[128];
        snprintf(path, sizeof path, "build/preset_%d.wav", p);
        write_wav(path, buf, frames * FRAME);
        printf("preset %d %-9s -> %s\n", p, preset_name(p), path);
    }
    /* text formatting smoke test */
    char s[8], l[40];
    for (int id = 0; id < NPARAM; id += 5) {
        preset_load(1);
        param_short(id, s); param_long(id, l);
        printf("  id %2d  [%s]  \"%s\"  norm=%d\n", id, s, l, param_norm(id));
    }
    free(buf);
    return 0;
}

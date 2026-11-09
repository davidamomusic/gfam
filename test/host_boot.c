/* Boot-time behaviour checks: (1) engine output while stopped, (2) spurious input at power-on. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../src/synth.h"
#include "../src/ui.h"

static int peak_after(int frames)
{
    static int8_t buf[FRAME_SAMPLES];
    int pk = 0;
    for (int f = 0; f < frames; f++) {
        synth_render(buf, FRAME_SAMPLES);
        for (int i = 0; i < FRAME_SAMPLES; i++) { int a = abs(buf[i]); if (a > pk) pk = a; }
    }
    return pk;
}

int main(void)
{
    /* 1. clean boot, nothing pressed, 5 s of frames */
    preset_load(0); synth_init(); ui_init();
    for (int f = 0; f < 300; f++) ui_update(0);
    printf("clean boot : running=%d  peak=%d\n", synth_running(), peak_after(300));

    /* 2. emulator reports "all buttons held" for the first frames, then releases */
    preset_load(0); synth_init(); ui_init();
    ui_update(0x3FF); ui_update(0x3FF); ui_update(0);
    printf("all-held   : running=%d  peak=%d\n", synth_running(), peak_after(300));

    /* 3. START already held when the ROM launches (launcher button still down) */
    preset_load(0); synth_init(); ui_init();
    ui_update(KEY_START); ui_update(KEY_START); ui_update(0);
    printf("START held : running=%d  peak=%d\n", synth_running(), peak_after(300));
    return 0;
}

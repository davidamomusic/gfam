/* GBA hardware glue: video (Mode 0 text BG), Direct Sound A via DMA1 + Timer 0, input. */
#include <stdint.h>
#include "synth.h"
#include "ui.h"
#include "gfx.h"
#include "audio.h"

#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

#define DISPCNT     REG16(0x04000000)
#define VCOUNT      REG16(0x04000006)
#define BG0CNT      REG16(0x04000008)
#define SOUNDCNT_L  REG16(0x04000080)
#define SOUNDCNT_H  REG16(0x04000082)
#define SOUNDCNT_X  REG16(0x04000084)
#define FIFO_A      0x040000A0u
#define DMA1SAD     REG32(0x040000BC)
#define DMA1DAD     REG32(0x040000C0)
#define DMA1CNT_L   REG16(0x040000C4)
#define DMA1CNT_H   REG16(0x040000C6)
#define TM0CNT_L    REG16(0x04000100)
#define TM0CNT_H    REG16(0x04000102)
#define KEYINPUT    REG16(0x04000130)
#define WAITCNT     REG16(0x04000204)

#define PAL_RAM     ((volatile uint16_t *)0x05000000)
#define VRAM_TILES  ((volatile uint32_t *)0x06000000)
#define VRAM_MAP    ((volatile uint32_t *)0x0600F800)   /* screenblock 31 */

static uint16_t shadow[1024] __attribute__((aligned(4)));

static void video_init(void)
{
    DISPCNT = 0x0080;                                   /* forced blank */
    for (int i = 0; i < NPAL_ENTRIES; i++) PAL_RAM[i] = GFX_PAL[i];
    for (int i = 0; i < NTILES * 8; i++) VRAM_TILES[i] = GFX_TILES[i];
    BG0CNT = 0x1F00;                                    /* char block 0, screen block 31, 4bpp */
    DISPCNT = 0x0100;                                   /* Mode 0, BG0 on */
}

#ifdef PROFILE
volatile uint32_t prof_max, prof_render, prof_ui;   /* worst buffer render / worst UI step, in units of 64 CPU cycles */
static inline void prof_start(void) { REG16(0x0400010A) = 0; REG16(0x04000108) = 0; REG16(0x0400010A) = 0x81; }
static inline uint32_t prof_stop(void) { return REG16(0x04000108); }
#endif

/* Leave the app: stop sound/IRQs and ask the BIOS for a soft reset (jumps back to the ROM entry).
   A GBA program cannot close the emulator, so on the Miyoo this is a restart. */
void app_restart(void)
{
    audio_stop();
    __asm__ volatile("swi 0x00" ::: "memory");
    for (;;) { }
}

int main(void)
{
    WAITCNT = 0x4317;                                   /* ROM 3/1 wait states + prefetch */
    video_init();
    preset_load(0);
    kick_preset_load(0);
    patterns_init();
    fx_defaults();
    synth_init();
    ui_init();

    audio_init();

    uint32_t seen = audio_vframes();
    for (;;) {
        /* 1. keep the audio queue full (highest priority) */
        int8_t *b;
        while ((b = audio_free_buffer()) != 0) {
#ifdef PROFILE
            prof_start();
#endif
            synth_render(b, FRAME_SAMPLES);
#ifdef PROFILE
            { uint32_t t = prof_stop(); if (t > prof_render) prof_render = t; }
#endif
            audio_commit();
        }

        /* 2. once per video frame: buttons + screen */
        uint32_t now = audio_vframes();
        if (now != seen) {
            seen = now;
#ifdef PROFILE
            prof_start();
#endif
            ui_update(~KEYINPUT & 0x3FF);
            if (ui_changed()) {
                ui_draw(shadow);
                const uint32_t *s = (const uint32_t *)shadow;
                for (int i = 0; i < 512; i++) VRAM_MAP[i] = s[i];
            }
#ifdef PROFILE
            { uint32_t t = prof_stop(); if (t > prof_ui) prof_ui = t; }
#endif
        }
    }
}

/* Sound hardware glue (ARM mode: the IRQ handler must be ARM code). */
#include <stdint.h>
#include "synth.h"
#include "audio.h"

#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))
#define SOUNDCNT_X  REG16(0x04000084)
#define SOUNDCNT_L  REG16(0x04000080)
#define SOUNDCNT_H  REG16(0x04000082)
#define DMA1SAD     REG32(0x040000BC)
#define DMA1DAD     REG32(0x040000C0)
#define DMA1CNT     REG32(0x040000C4)      /* count (low 16) | control << 16 */
#define TM0CNT_L    REG16(0x04000100)
#define TM0CNT_H    REG16(0x04000102)
#define TM1CNT_L    REG16(0x04000104)
#define TM1CNT_H    REG16(0x04000106)
#define REG_IE      REG16(0x04000200)
#define REG_IF      REG16(0x04000202)
#define REG_IME     REG16(0x04000208)
#define REG_DISPSTAT REG16(0x04000004)
#define IRQ_VECTOR  REG32(0x03007FFC)

#ifdef HOST
#define IWRAM_TEXT
#else
#define IWRAM_TEXT __attribute__((section(".iwram.text")))
#endif

#define NBUF 4
static int8_t bufs[NBUF][FRAME_SAMPLES] __attribute__((aligned(4)));
static int8_t silence[FRAME_SAMPLES] __attribute__((aligned(4)));
static volatile uint32_t played;       /* sequence number of the buffer being played (written by the IRQ) */
static volatile uint32_t produced;              /* sequence number of the next buffer to render (written by main) */
static volatile uint32_t vframes;      /* video frames since boot (VBlank IRQ) */
volatile uint32_t audio_underruns;

/* Called by the BIOS IRQ dispatcher.
 * Timer 1 overflow: the buffer in the FIFO has been fully sent -> start the next one (or silence if
 * the main loop has not finished rendering it).  VBlank: just count frames. */
static void IWRAM_TEXT irq_handler(void)
{
    uint16_t f = REG_IF;
    if (f & 0x0010) {
        DMA1CNT = 0;                                   /* stop */
        if (produced - played > 1) {
            played = played + 1;
            DMA1SAD = (uint32_t)bufs[played & (NBUF - 1)];
        } else {
            DMA1SAD = (uint32_t)silence;
            audio_underruns++;
        }
        DMA1CNT = 0xB6400000u;                         /* enable | FIFO timing | repeat | 32 bit | dest fixed */
    }
    if (f & 0x0001) vframes++;
    REG_IF = f;                                        /* acknowledge */
}

void audio_init(void)
{
    played = 0; produced = 1; vframes = 0;             /* buffer 0 (silence) is "playing" */

    REG_IME = 0;
    IRQ_VECTOR = (uint32_t)irq_handler;

    SOUNDCNT_X = 0x0080;                               /* master enable */
    SOUNDCNT_L = 0;
    SOUNDCNT_H = 0x8B06;                               /* A 100%, A -> L+R, timer 0, reset both FIFOs */
    DMA1CNT = 0;
    DMA1SAD = (uint32_t)bufs[0];
    DMA1DAD = 0x040000A0;
    DMA1CNT = 0xB6400000u;

    TM0CNT_H = 0;
    TM1CNT_H = 0;
    TM0CNT_L = (uint16_t)(65536 - TIMER_PERIOD);
    TM1CNT_L = (uint16_t)(65536 - FRAME_SAMPLES);
    REG_DISPSTAT |= 0x0008;                            /* VBlank IRQ */
    REG_IF = 0xFFFF;
    REG_IE |= 0x0011;                                  /* timer 1 + VBlank */
    REG_IME = 1;
    TM1CNT_H = 0x00C4;                                 /* enable | cascade | IRQ */
    TM0CNT_H = 0x0080;                                 /* enable, 1 CPU cycle per tick */
}

void audio_stop(void)
{
    REG_IME = 0;
    DMA1CNT = 0;
    TM0CNT_H = 0; TM1CNT_H = 0;
    SOUNDCNT_X = 0;
    REG_IE = 0;
    REG_IF = 0xFFFF;
}

/* The next buffer that may be rendered, or 0 when the queue is full. */
int8_t *audio_free_buffer(void)
{
    if (produced - played >= NBUF) return 0;
    return bufs[produced & (NBUF - 1)];
}

void audio_commit(void) { produced++; }

uint32_t audio_vframes(void) { return vframes; }

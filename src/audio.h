#pragma once
#include <stdint.h>
/* Direct Sound A output. Timer 0 free-runs at the sample rate and is never touched again; Timer 1
 * (cascade) overflows once per buffer and its IRQ re-arms DMA1 on the next buffer of a 4-buffer queue.
 * (Same scheme as the reference ROM that works on the Miyoo.) The main loop keeps the queue full, so a
 * slow iteration (screen redraw) does not interrupt the sound. */
void audio_init(void);
void audio_stop(void);                         /* silence everything (used before a soft reset) */
int8_t *audio_free_buffer(void);               /* next buffer to render, or 0 if the queue is full */
void audio_commit(void);                       /* call after filling the buffer returned above */
uint32_t audio_vframes(void);                  /* video frames elapsed (VBlank IRQ) */
extern volatile uint32_t audio_underruns;

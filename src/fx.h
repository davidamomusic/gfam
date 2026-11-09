#pragma once
#include <stdint.h>
/* Effects chain. Order: compressor -> stutter -> delay -> reverb -> limiter (always last).
 * Input is a block of samples in Q8 units (128 = 32768); output is the final signed 8-bit stream. */
void fx_init(void);
void fx_set_step(int32_t steplen);                       /* sequencer step length in samples */
void fx_step_sync(void);                                 /* called on every sequencer step    */
void fx_process(int32_t *x, int8_t *out, int n);         /* n is a multiple of 4              */

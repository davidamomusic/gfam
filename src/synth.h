#pragma once
#include <stdint.h>

#define TIMER_PERIOD  924     /* CPU cycles per sample -> 18157 Hz                       */
#define FRAME_SAMPLES 160     /* samples per DMA buffer: multiple of 16 (DMA bursts) and of 4 (control group) */

/* ---- parameter ids (order = preset table order) ---- */
enum {
    P_PIT0 = 0,          /* 0..7   step pitch, 1/16 semitone units, -768..768 */
    P_VEL0 = 8,          /* 8..15  step velocity 0..127 (0 = rest)            */
    P_V1F = 16,          /* VCO1 pitch, 1/16 semitone units, 0..1919          */
    P_V1W, P_V1E,        /* wave 0=TRI 1=SQR ; EG amount -99..99              */
    P_V2F, P_V2W, P_V2E,
    P_SYNC,              /* 0 off, 1 on                                       */
    P_SMOD,              /* 0 off, 1 VCO1&2, 2 VCO2                           */
    P_L1, P_L2, P_LN,    /* mixer levels 0..255                               */
    P_FM,                /* 1>2 FM amount 0..255                              */
    P_VOL,               /* master volume 0..255                              */
    P_TEMPO,             /* 0..255 -> 40..295 BPM                             */
    P_FMODE,             /* 0 LP, 1 HP                                        */
    P_CUT, P_RES,        /* 0..255                                            */
    P_FEG,               /* VCF EG amount -99..99                             */
    P_NMOD,              /* noise -> VCF mod 0..255                           */
    P_VDEC, P_FDEC, P_ADEC,  /* decay times 0..255                            */
    P_AATK,              /* VCA attack 0=FAST 1=SLOW                          */
    P_NMAIN,             /* = 39: parameters stored in the main presets       */
    P_KTUN = P_NMAIN,    /* kick body pitch (1/16 semitone units)             */
    P_KSWP,              /* pitch sweep depth 0..255                          */
    P_KPDC,              /* pitch decay 0..255                                */
    P_KADC,              /* amplitude decay 0..255                            */
    P_KCLK,              /* click / noise transient 0..255                    */
    P_KDRV,              /* drive 0..255                                      */
    P_KWAV,              /* 0 sine, 1 triangle, 2 folded sine                 */
    P_KLEV,              /* kick level 0..255                                 */
    /* patch bay: 8 inputs, each with a source (0 off, 1 SEQ, 2 VEL, 3 VCO EG, 4 VCF EG, 5 VCA EG, 6 NOISE,
       7 KICK EG, 8 RANDOM) and an amount -99..99. Inputs: VCO1, VCO2, CUTOFF, RESONANCE, FM, VCA, DECAYS, NOISE */
    P_PBS0,              /* ..P_PBS0+7: sources */
    P_PBA0 = P_PBS0 + 8, /* ..P_PBA0+7: amounts */
    P_PBEND = P_PBA0 + 8,
    P_FX0 = P_PBEND,     /* effects (global, not stored in presets) */
    P_FCMP = P_FX0,      /* compressor on/off, mix                            */
    P_FCMPM,
    P_FSTU, P_FSTUM,     /* stutter                                           */
    P_FDLY, P_FDLYM,     /* delay                                             */
    P_FREV, P_FREVM,     /* reverb                                            */
    P_FLIM,              /* limiter on/off (always the last device)           */
    P_FCAM,              /* compressor amount (lower threshold)               */
    P_FSTR,              /* stutter slice 0..3 = 1/16 .. 1/128 note           */
    P_FDLT,              /* delay time 0..3 = 1/8, dotted 1/8, 1/4, dotted 1/4 */
    P_FDFB,              /* delay feedback 0..255                             */
    P_FRSZ,              /* reverb size / decay                               */
    P_FRTN,              /* reverb tone (damping)                             */
    P_FLTH,              /* limiter threshold 0..100 = 0.0 .. -10.0 dB        */
    NPARAM
};
#define NKICKP (P_PBS0 - P_KTUN)
#define NSND   (P_FX0 - P_V1F)      /* sound parameters stored per pattern (synth + kick; BPM and VOLUME stay global) */

typedef struct {
    const char *label;   /* 3 chars, column label */
    const char *name;    /* long name              */
    uint8_t type;
    int16_t min, max, step, alt;
} ParamDef;

enum { T_NUM, T_BIP, T_NOTE, T_SEMI, T_VEL, T_BPM, T_ENUM, T_DB };

#define NPAGES 12
#define PAGE_KSEQ  6      /* kick step page (custom 16-step grid)   */
#define PAGE_KSND  7      /* kick sound page                         */
#define PAGE_PAT   8      /* pattern bank (custom 4x4 grid)          */
#define PAGE_SONG  9      /* song mode (custom 4x8 grid)             */
#define PAGE_FX    10     /* effects on/off + mix (9 columns)        */
#define PAGE_FXT   11     /* effects tuning                          */

/* ---- kick patterns and song ---- */
#define NPAT  32          /* patterns of 16 steps: 0 off, 1 trig, 2 accent  */
#define NSONG 32          /* song slots: 0 = empty (skipped), 1..NPAT = pattern */
#define NCOND 16          /* trig conditions "bar x of N": 0 = every bar, then 1:2 2:2 1:3 2:3 3:3 1:4 2:4 3:4 4:4 1:8 4:8 8:8 1:16 8:16 16:16 */
extern const uint8_t COND_X[NCOND], COND_N[NCOND];
#define NPATCH 8
#ifdef HOST
#define PAT_RAM
#else
#define PAT_RAM __attribute__((section(".ewram")))     /* pattern data lives in EWRAM (IWRAM is nearly full) */
#endif
/* A pattern holds everything that makes it sound: the 8 synth steps (pitch + velocity, stored in
   PSEQ as 16 values laid out like P[P_PIT0..P_VEL0+7]), the 16 kick steps (KPAT) and the synth + kick
   sound (PSND, same order as P[P_V1F..P_FX0-1]; BPM and VOLUME are global and not stored).
   The live copy is P[]; the pattern being played is synth_kplay(). */
extern int16_t PSND[NPAT][NSND];
extern int16_t PSEQ[NPAT][16];
extern uint8_t KPAT[NPAT][16];
extern uint8_t SONG[NSONG];
/* kick step extras (per pattern, per step): trig condition and parameter locks of the 8 kick sound knobs */
extern uint8_t KCND[NPAT][16];
extern uint8_t SCND[NPAT][8];                    /* trig condition of each synth step */
extern uint8_t KLMASK[NPAT][16];                 /* bit i set = kick parameter P_KTUN+i is locked on this step */
extern int16_t KLVAL[NPAT][16][NKICKP];
int  klock_has(int step, int i);                 /* current pattern: is knob i locked on this step */
int  klock_get(int step, int i);                 /* value that step plays (lock or global) */
void klock_set(int step, int i, int v);
void klock_clear(int step, int i);
int  kstep_extras(int p, int step);              /* 1 if the step has a condition or any lock */
void patterns_init(void);                        /* factory patterns, empty song; pattern 1 takes the live synth steps */
void pattern_flush(void);                        /* P[0..15] -> PSEQ[current]  */
void pattern_load(int p);                        /* PSEQ[p] -> P[0..15]        */
void pattern_copy(void);                         /* current pattern -> clipboard */
void pattern_paste(void);                        /* clipboard -> current pattern */
void pattern_clear(void);                        /* clears the steps (sound stays) */
void pattern_random(uint32_t seed);              /* random pattern: steps, kick steps and both sounds (BPM untouched) */
int  pattern_used(int p);                        /* 1 if it holds any trig or note */
extern const ParamDef PDEF[NPARAM];
extern const uint8_t PAGE_FIRST[NPAGES];
extern const uint8_t PAGE_COUNT[NPAGES];
extern const char *const PAGE_NAME[NPAGES];

/* ---- params.c (control side, runs from ROM) ---- */
extern int16_t P[NPARAM];
void param_change(int id, int dir, int alt);
void param_short(int id, char out[4]);           /* 3-char value for the column */
void param_long(int id, char *out);              /* detailed value, <= 24 chars */
int  param_norm(int id);                         /* 0..64 bar height            */
int  preset_count(void);
void preset_load_sound(int idx);                 /* sound parameters only, steps untouched */
void preset_random_part(uint32_t seed, int sound); /* sound = 1: sound only, 0: steps only (clears the step conditions) */
void patch_random(uint32_t seed);                /* a few random cables in the patch bay */
void patch_clear(void);
const char *preset_name(int idx);
void preset_load(int idx);
void preset_random(uint32_t seed);               /* fill the main P[] with a fresh random patch */
int  kick_preset_count(void);
const char *kick_preset_name(int idx);
void kick_preset_load(int idx);                  /* loads the kick sound only */
void kick_random_sound(uint32_t seed);
void kick_random_pattern(int pat, uint32_t seed);   /* fills pattern pat */

/* ---- synth.c (audio side, runs from IWRAM) ---- */
void synth_init(void);
void synth_render(int8_t *out, int n);
void synth_set_run(int on);
int  synth_running(void);
void synth_trigger(void);        /* play current step once                  */
void synth_advance(void);        /* move to next step without playing       */
int  synth_step(void);           /* current step 0..7                       */
int  synth_kstep(void);          /* current kick step 0..15                 */
void synth_ktrigger(void);       /* audition the kick                       */
void synth_ktrigger_step(int step);   /* audition a step of the current pattern with its locks */
void synth_queue_pattern(int p);   /* play pattern p (next bar if running, now if stopped) */
int  synth_kplay(void);          /* pattern being played                    */
int  synth_kqueued(void);        /* pending pattern or -1                   */
void synth_song_off(void);       /* leave song mode but keep playing the current pattern */
void synth_song_start(void);     /* play the song from its first slot (or stop it if it is playing) */
int  synth_song_on(void);        /* song is playing                         */
int  synth_song_pos(void);       /* current song slot, -1 = not started     */
void fx_defaults(void);          /* effects start state (all off, limiter on) */

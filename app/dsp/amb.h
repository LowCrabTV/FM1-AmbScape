/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument: a generative ambient garden. Four layers draw on one pool of notes (a scale:
 * the root plus up to eleven more pitch classes) and one chord that drifts slowly through it:
 *
 *   PAD     four breathing voices: each swells in, hangs and fades on its own cycle (unequal lengths,
 *           as the loops of Music for Airports), taking its note of the chord as it starts, so the
 *           chord forms and dissolves. Two colours, by piece: a filtered Rhodes (FM, blooming brighter
 *           as it swells) or Voices (saws through two formants, as "2/1"); then a soft low-pass and an
 *           ensemble chorus. The chord moves every DRIFT (20 s .. 5 min): voices on shared
 *           notes hold, the others breathe in anew.
 *           Or the SLATE: up to four chords in order, each for a set number of bars, in place of Drift.
 *   BELLS   soft two-operator FM bells played by four tape loops of unequal length (13, 17, 23, 29
 *           quarter notes), each holding a short phrase that shifts against the others, each its own
 *           shade of bell. EVOLVE changes a note now and then; DENSITY sets how many notes a loop holds.
 *   BASS    a sine and its sub-octave on the chord's root (now and then its fifth), gliding.
 *   PULSE   a soft thump, a closed hat and an open hat, each on its own Euclidean pattern (16, 12 and 7
 *           sixteenths): polyrhythms. HITS sets how many; the patterns turn and change around it.
 *
 * Over it all, the ARC: a slow tide, minutes long, between sparse (fewer bells, a darker vowel, a quieter
 * pulse, more reverb) and full; MOTION sets how far it goes. And rests: with EVOLVE, the pulse
 * sits out a few bars, the bass a chord, a bell loop a pass.
 * Then, after Discreet Music's chain, an Echoplex (160 ms, fed back) and a tape loop (to ~5.6 s, two
 * heads, each repeat darker), and Dattorro's plate (from FoMni), fed generously: the space is most of
 * what is heard.
 * FREEZE holds the chord and lets the plate ring on forever.
 *
 * Threads: en_render() runs in the audio interrupt. Everything else posts commands, which the render
 * drains at the start of a block; the state below marked (UI) is written by the render and only read
 * elsewhere. Float only, no libm: fastmath.h. */
#pragma once
#include <stdint.h>

#define EN_SR 44100.0f
#define EN_NLOOP 4                     /* bell tape loops */
#define EN_LOOP_MAX 29
#define EN_NDRUM 3                     /* pulse: thump, closed hat, open hat */
#define EN_NPAD 4                      /* pad voices */

/* modes: pools relative to the root (bit i: root + i semitones) */
enum { MODE_IONIAN, MODE_DORIAN, MODE_PHRYGIAN, MODE_LYDIAN, MODE_MIXOLYDIAN, MODE_AEOLIAN, MODE_PENT_MAJ,
       MODE_PENT_MIN, EN_NMODES };
extern const char *const EN_MODE_NAME[EN_NMODES];
extern const uint16_t EN_MODE_POOL[EN_NMODES];
extern const char *const EN_NOTE_NAME[12];
extern const uint8_t EN_LOOP_LEN[EN_NLOOP];
extern const uint8_t EN_DRUM_N[EN_NDRUM];
#define EN_NSLOT 4                     /* the slate's steps */
#define EN_NBARS 10
#define EN_NRBARS 12
#define EN_NBCOLOR 6
#define EN_NSHAPE 6                    /* a chord's shapes: seventh, triad, sus2, sus4, add9, sixth. A slot's value: 0 empty,
                                        * else 1 + degree * EN_NSHAPE + shape */
#define EN_NARP 8                      /* the arp's styles, Off first */
#define EN_NARATE 4
#define EN_NAPOLY 5
extern const uint8_t EN_SLATE_BARS[EN_NBARS];

/* parameters: value ranges in en_param_info() */
enum {
    P_PAD, P_BELLS, P_BASS, P_PULSE,                  /* layer levels */
    P_DENSITY, P_EVOLVE, P_DRIFT, P_HITS,             /* motion */
    P_ECHO, P_ETIME, P_REVERB, P_SIZE,                /* space */
    P_BRIGHT, P_SWELL, P_BTONE, P_MOTION,             /* tone */
    P_TUNE, P_MIDI, P_TEMPO, P_ROOT, P_MODE, P_BOCT,  /* setup, panel encoders, bell octave */
    P_PADTONE,                                        /* the pad's colour, set by the piece: Rhodes, Voices, Strings, Noir */
    P_BASSOCT, P_GLIDE, P_FIFTH,                      /* bass: its octave, its glide, how often it leans on the fifth */
    P_SWING, P_HATS,                                  /* pulse: swing, the hats' colour */
    P_SLOT1, P_SLOT2, P_SLOT3, P_SLOT4,               /* the slate: each step's chord, 0 empty, n the scale's degree n */
    P_LEN1, P_LEN2, P_LEN3, P_LEN4,                   /* and how long it lasts: an index into EN_SLATE_BARS */
    P_ARP, P_ARATE, P_ARANGE,                         /* the bells' arpeggio: its style (0 off), rate, octaves */
    P_PADN, P_BASSN,                                  /* how many notes: the pad's chord, 2..4; the bass's, 0 held */
    P_APOLY,                                          /* the arp against the grid: Off, 3:4, 5:4, 3:2, 7:4 */
    P_SPARK, P_SPLEN, P_SPOFT,                        /* Sparkle: how much at its peak (0 off), each wave's length, the gap between */
    P_RSLOT1, P_RSLOT2, P_RSLOT3, P_RSLOT4,           /* the root rotator's steps: 0 empty, n = pitch class n - 1 */
    P_RLEN1, P_RLEN2, P_RLEN3, P_RLEN4,               /* and how long each lasts: an index into EN_ROOT_BARS */
    P_BCOLOR,                                         /* the bells' colour: Bell, Soft, Wood, Strings, Piano; Lead, a solo synth */
    P_SWOOSH, P_SWLEN, P_SWOFT, P_SWTONE,             /* Swoosh: how loud (0 off), each pass's length, the gap between, white to brown */
    P_SEED,                                           /* the garden's dice: 0 Wild, or a number, the same garden each PLAY */
    P_DEAL, P_DCOUNT, P_DSPICE, P_DFORM,              /* the chord dealer: Off or a number, 2-4 chords, Spice, Form */
    P_RDEAL, P_RDCOUNT, P_RDSPICE, P_RDFORM,          /* the key dealer: Off or a number, 2-4 keys, Spice, Form */
    P_DREAM, P_DRANGE, P_DCAP,                        /* Dream: 0 Off, 1 Wild, 2-1000 the numbers 1-999; Range; Spice cap */
    P_RHITS0, P_RHITS1, P_RHITS2,                     /* the Rings: thump, closed hat, open hat: 0 Auto, else 1 + hits */
    P_RTURN0, P_RTURN1, P_RTURN2, P_RMOVES,           /* each ring's turn (steps); Moves: 0 Follow (Evolve), 1 Still */
    P_SONG,                                           /* Song: 1 the garden in sections, as songs go (the default); 0 free */
    P_PSPREAD, P_BPLUCK,                              /* the pad's Spread: Close, Open, Wide, Vast; the bass's Pluck, 0 hum .. 100 */
    P_CRICKET, P_PEEP, P_WATER, P_THUNDER,            /* the Night: crickets, peepers, water (creek, rain, waves), thunder; 0 off */
    P_NPARAMS
};
typedef struct {
    const char *name;
    int16_t lo, hi, def;
} en_param_t;
const en_param_t *en_param_info(int i);
void en_param_text(int i, int v, char *buf);   /* the value as the screen shows it (10 bytes) */

/* commands (main loop -> render) */
void en_init(void);
void en_set(int param, int value);
void en_pool(uint32_t pool);                   /* the scale, relative to the root (bit 0 always set) */
void en_play(int on);                          /* the garden grows / rests (the tails ring on) */
void en_next(void);                            /* the next chord: on the next bar (now while resting) */
void en_freeze(int on);
void en_seed(uint32_t s);                      /* stir some entropy into the generators */
void en_panic(void);                           /* everything quiet, now */
void en_regrow(void);                          /* a new piece: fresh loops, the home chord */
void en_restart(int what);                     /* from the top at the next bar: 1 the chords' steps, 2 the keys' */
/* MIDI clock in (as FoMni): ticks drive the 1/16 grid (6 a step) while they keep coming */
enum { EN_CLK_TICK, EN_CLK_START, EN_CLK_CONTINUE, EN_CLK_STOP };
void en_clock(int msg);

/* the render (audio ISR): n stereo frames, 24-bit in int32; gain Q12 (the MASTER pot) */
void en_render(int32_t *out_lr, uint32_t n, uint32_t gain_q12);

/* MIDI out, from the render: the app sends it (USB); status, data 1, data 2 */
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2);

/* state for the screen (UI) */
extern volatile uint8_t en_playing, en_frozen, en_ext;
extern volatile uint8_t en_chord[EN_NPAD];     /* the chord's notes (MIDI), pad voices' targets */
extern volatile uint8_t en_chord_root;         /* the chord's root, MIDI pitch class 0..11 */
extern volatile uint8_t en_chord_n;            /* its distinct tones: 3 or 4 */
extern volatile float en_drift;                /* 0..1: how far to the next chord */
extern volatile float en_pad_level, en_bass_level;
extern volatile uint8_t en_loop_pos[EN_NLOOP];
extern volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];   /* -1 rest, else the slot's offset */
extern volatile float en_loop_glow[EN_NLOOP];  /* 0..1: a bell just rang from this loop */
extern volatile uint8_t en_drum_pos[EN_NDRUM];
extern volatile uint32_t en_drum_pat[EN_NDRUM];/* bit i: a hit on step i */
extern volatile float en_drum_glow[EN_NDRUM];
extern volatile uint32_t en_bars;              /* sixteenths played since start (for tests) */
extern volatile uint8_t en_root;               /* the root playing: the rotator's, or Root */
extern volatile float en_spark;                /* 0..1: where the Sparkle's wave is */
/* the dealers (pure): from a number, a progression for the slate's four steps (slot values, EN_SLATE_BARS
 * indices) or a journey for the rotator's (root values, its bars' indices). Steps not dealt are emptied. The
 * chords' one returns the bars they come to; the keys' one takes them, each key lasting whole passes of them */
#define EN_NDFORM 9                    /* Even, Home, Turn: each chord about 12, 20 or 32 s, at the piece's tempo */
#define EN_NRFORM 4                    /* Even, Slow, Home, Lift */
int en_deal_chords(uint32_t pool, int num, int count, int spice, int form, int tempo, int16_t *slot, int16_t *len);
void en_deal_roots(int home, int num, int count, int spice, int form, int cycle, int min_bars, int16_t *rslot,
                   int16_t *rlen);
int en_deal_cycle(int count, int form, int tempo);   /* the bars a dealt progression comes to, at this tempo */
int en_ring_auto(int d, int hits, int hats);   /* a ring's hits on Auto: from Hits and Hats, as the pulse plays them */
extern volatile uint16_t en_seed_now;          /* the number the garden growing now grew from, 1..999 */
extern volatile float en_arc;                  /* the arc, -1 sparse .. +1 full (times Motion) */
extern volatile uint8_t en_pulse_rest;         /* the pulse is sitting a few bars out */
extern volatile uint8_t en_pad_note[EN_NPAD];  /* the note each voice sings now (MIDI) */
extern volatile float en_pad_amp[EN_NPAD];     /* and how loud */

/* the pure parts, for the tests and the screen */
uint32_t en_euclid(int k, int n, int rot);     /* Bjorklund's pattern, as a bitmask (bit 0 = step 0) */
int en_scale(uint32_t pool, int *steps);       /* the pool's pitch classes (relative), ascending; count */
/* the chord on scale degree deg: its tones (MIDI pitch classes, absolute), 3 or 4 of them; count */
int en_chord_tones(uint32_t pool, int root, int deg, int *pcs);
/* and in a shape: the root first, the fifth third (as en_chord_tones), each tone of the scale; count */
int en_chord_shape(uint32_t pool, int root, int deg, int shape, int *pcs);
/* voice leading: new pad notes for the tones, each voice moving as little as possible, all in lo..hi */
void en_voice_lead(const int *from, const int *pcs, int ntones, int *to, int lo, int hi);
const char *en_chord_name(const int *pcs, int n, char *buf);   /* "Em7", "Dsus4", "C" ... */

/* the pieces: recipes for every parameter (Tune and MIDI out included, but they are the player's) */
#define EN_NPIECES 8
const char *en_piece_name(int i);
void en_piece(int i, int16_t *par, uint16_t *pool);

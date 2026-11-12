/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument (amb.h). One compilation unit, -O2. */
#include "amb.h"
#include "fastmath.h"

#if defined EN_HOST || defined PLAT_HOST
#define EN_POOL
#else
#define EN_POOL __attribute__((section(".pool")))
#endif
#define BARRIER() __asm__ volatile("" ::: "memory")

const char *const EN_NOTE_NAME[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
const char *const EN_MODE_NAME[EN_NMODES] = {"Ionian", "Dorian", "Phrygian", "Lydian", "Mixolyd.", "Aeolian",
                                             "Pent maj", "Pent min"};
#define B(a, b, c, d, e, f, g) (1u << (a) | 1u << (b) | 1u << (c) | 1u << (d) | 1u << (e) | 1u << (f) | 1u << (g))
const uint16_t EN_MODE_POOL[EN_NMODES] = {
    B(0, 2, 4, 5, 7, 9, 11), B(0, 2, 3, 5, 7, 9, 10), B(0, 1, 3, 5, 7, 8, 10), B(0, 2, 4, 6, 7, 9, 11),
    B(0, 2, 4, 5, 7, 9, 10), B(0, 2, 3, 5, 7, 8, 10), B(0, 2, 4, 7, 9, 9, 9),  B(0, 3, 5, 7, 10, 10, 10)};
#undef B
const uint8_t EN_LOOP_LEN[EN_NLOOP] = {13, 17, 23, 29};
const uint8_t EN_DRUM_N[EN_NDRUM] = {16, 12, 7};
const uint8_t EN_SLATE_BARS[EN_NBARS] = {1, 2, 3, 4, 6, 8, 12, 16, 24, 32};
static const uint8_t EN_ROOT_BARS[EN_NRBARS] = {1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128};

static const en_param_t PARAMS[P_NPARAMS] = {
    {"Pad", 0, 100, 60},      {"Lead", 0, 100, 50},    {"Bass", 0, 100, 45},     {"Pulse", 0, 100, 40},
    {"Density", 0, 100, 40},  {"Evolve", 0, 100, 35},   {"Drift", 0, 100, 25},    {"Hits", 0, 100, 35},
    {"Echo", 0, 100, 45},     {"Echo time", 0, 5, 3},   {"Reverb", 0, 100, 65},   {"Size", 0, 100, 80},
    {"Bright", 0, 100, 40},   {"Swell", 0, 100, 60},    {"Lead tone", 0, 100, 50}, {"Motion", 0, 100, 50},
    {"Tune", -50, 50, 0},     {"MIDI out", 0, 1, 1},    {"Tempo", 30, 120, 72},   {"Root", 0, 11, 2},
    {"Mode", 0, EN_NMODES - 1, 1}, {"Lead oct", -2, 2, 0}, {"Pad tone", 0, 3, 0},
    {"Bass oct", -1, 1, 0},   {"Glide", 0, 100, 20},    {"Fifth", 0, 100, 35},
    {"Swing", 0, 100, 0},     {"Hats", 0, 100, 50},
    {"Chord 1", 0, 12 * EN_NSHAPE, 0}, {"Chord 2", 0, 12 * EN_NSHAPE, 0},
    {"Chord 3", 0, 12 * EN_NSHAPE, 0}, {"Chord 4", 0, 12 * EN_NSHAPE, 0},
    {"Bars 1", 0, EN_NBARS - 1, 3}, {"Bars 2", 0, EN_NBARS - 1, 3}, {"Bars 3", 0, EN_NBARS - 1, 3},
    {"Bars 4", 0, EN_NBARS - 1, 3},
    {"Arp", 0, EN_NARP - 1, 0}, {"Arp rate", 0, EN_NARATE - 1, 2}, {"Arp range", 1, 3, 2},
    {"Pad notes", 2, 4, 4},   {"Bass notes", -1, 100, 0},
    {"Arp poly", 0, EN_NAPOLY - 1, 0},
    {"Sparkle", 0, 100, 0},   {"Spk length", 0, 100, 56}, {"Spk often", 0, 100, 44},
    {"Root 1", 0, 12, 0},     {"Root 2", 0, 12, 0},     {"Root 3", 0, 12, 0},     {"Root 4", 0, 12, 0},
    {"Root bars 1", 0, EN_NRBARS - 1, 3}, {"Root bars 2", 0, EN_NRBARS - 1, 3}, {"Root bars 3", 0, EN_NRBARS - 1, 3},
    {"Root bars 4", 0, EN_NRBARS - 1, 3},
    {"Lead colour", 0, EN_NBCOLOR - 1, 0},
    {"Swoosh", 0, 100, 0},    {"Swsh length", 0, 100, 50}, {"Swsh often", 0, 100, 30}, {"Swsh tone", 0, 100, 50},
    {"Seed", 0, 999, 0},
    {"Deal", 0, 999, 0},     {"Deal count", 2, 4, 4}, {"Spice", 0, 100, 30}, {"Form", 0, EN_NDFORM - 1, 1},
    {"Key deal", 0, 999, 0}, {"Key count", 2, 4, 3}, {"Key spice", 0, 100, 30}, {"Key form", 0, EN_NRFORM - 1, 0},
    {"Dream", 0, 1000, 0},   {"Range", 0, 100, 30},   {"Spice cap", 0, 100, 70},
    {"Thump hits", 0, 17, 0}, {"Closed hits", 0, 13, 0}, {"Open hits", 0, 8, 0},
    {"Thump turn", 0, 15, 0}, {"Closed turn", 0, 11, 2}, {"Open turn", 0, 6, 3}, {"Moves", 0, 1, 0},
    {"Song", 0, 1, 1},
    {"Spread", 0, 3, 0},     {"Pluck", 0, 100, 15},
    {"Crickets", 0, 100, 0}, {"Peepers", 0, 100, 0}, {"Water", 0, 100, 0}, {"Thunder", 0, 100, 0},
};
const en_param_t *en_param_info(int i) { return (i >= 0 && i < P_NPARAMS) ? &PARAMS[i] : &PARAMS[0]; }

/* the pieces: eight recipes for the whole garden (the FM-1's PRESETS), named for night drives (Nightscape FM;
 * as AMB-1 they were Garden, Departures, Voices, Afternoon, Night Bus, Shoreline, Glass, Tides). Tune and
 * MIDI out are taken from here by neither: they belong to the player, not the piece */
/*                     Pad Bel Bas Pul  Den Evo Dri Hit  Ech ET Rev Siz  Bri Swe Ton Mot  Tun MI Tem Rt Md Oc  PT  BO Gli Fif  Swi Hat
 * (PT: the pad's colour, 0 Rhodes, 1 Voices; BO: the bass's octave). Hat 0 in each: for work, sleep and meditation,
 * no hats unless turned up, and the pulse a sparse kick (Hits 45 at most: two a bar) */
static const struct {
    const char *name;
    int8_t v[P_NPARAMS];
} PIECES[EN_NPIECES] = {
    {"Blue Hour",  {60, 50, 45, 40, 40, 35, 25, 35, 45, 3, 65, 80, 40, 60, 50, 50, 0, 1, 72, 2, 1, 0, 0, 0, 20, 35, 0, 0}},
    {"Last Light", {35, 50, 0, 0, 35, 30, 40, 0, 30, 3, 70, 85, 20, 55, 5, 40, 0, 1, 60, 1, 0, -1, 0, 0, 30, 25, 0, 0}},
    {"Pines",      {85, 0, 25, 0, 30, 30, 50, 0, 20, 5, 80, 90, 67, 70, 30, 55, 0, 1, 56, 5, 5, 0, 1, -1, 45, 30, 0, 0}},
    {"Ember Line", {50, 50, 35, 25, 55, 45, 20, 30, 50, 2, 60, 75, 60, 50, 60, 55, 0, 1, 84, 7, 3, 0, 0, 0, 15, 40, 15, 0}},
    {"Chevrons",   {45, 50, 55, 60, 45, 50, 15, 45, 55, 1, 55, 70, 35, 40, 75, 70, 0, 1, 96, 9, 5, 0, 0, 0, 10, 45, 30, 0}},
    {"Afterglow",  {40, 50, 30, 0, 30, 40, 40, 0, 85, 4, 55, 95, 30, 80, 0, 85, 0, 1, 64, 4, 6, 0, 0, 0, 40, 20, 0, 0}},
    {"Headlights", {35, 50, 30, 20, 60, 55, 25, 25, 45, 2, 65, 80, 55, 45, 90, 50, 0, 1, 76, 3, 7, 1, 0, 1, 10, 30, 10, 0}},
    {"Gravel",     {55, 50, 40, 35, 45, 75, 10, 40, 50, 3, 65, 85, 60, 60, 55, 95, 0, 1, 70, 10, 4, 0, 1, 0, 25, 55, 10, 0}},
};

const char *en_piece_name(int i) { return PIECES[(unsigned)i < EN_NPIECES ? i : 0].name; }
void en_piece(int i, int16_t *par, uint16_t *pool)
{
    int p;
    i = (unsigned)i < EN_NPIECES ? i : 0;
    for (p = 0; p < P_NPARAMS; p++)
        par[p] = p < P_SLOT1 ? PIECES[i].v[p] : PARAMS[p].def;   /* the slate is the player's: empty in every recipe */
    *pool = EN_MODE_POOL[PIECES[i].v[P_MODE]];
}

static const char *const ARP_NAME[EN_NARP] = {"Off", "Up", "Down", "Up/Dn", "Dn/Up", "Random", "Converge", "Diverge"};
static const char *const ARATE_NAME[EN_NARATE] = {"1/4", "1/8.", "1/8", "1/16"};
static const uint8_t ARATE_STEPS[EN_NARATE] = {4, 3, 2, 1};      /* sixteenths a step */
/* Poly: so many notes in the time of so many Rate steps */
static const char *const APOLY_NAME[EN_NAPOLY] = {"Off", "3:4", "5:4", "3:2", "7:4"};
static const uint8_t APOLY_NUM[EN_NAPOLY] = {1, 3, 5, 3, 7}, APOLY_DEN[EN_NAPOLY] = {1, 4, 4, 2, 4};
static const float ARATE_DEC[EN_NARATE] = {1.0f, 0.8f, 0.6f, 0.4f};   /* the faster, the shorter the bell */
static const char *const BCOLOR_NAME[EN_NBCOLOR] = {"Bell", "Soft", "Wood", "Strings", "Piano", "Synth"};
static const char *const PADTONE_NAME[4] = {"Rhodes", "Voices", "Strings", "Noir"};
static const char *const SPREAD_NAME[4] = {"Close", "Open", "Wide", "Vast"};
static const char *const ETIME_NAME[6] = {"1/4.", "1/2", "1/2.", "1 bar", "2 bars", "4 bars"};
static const float ETIME_BEATS[6] = {1.5f, 2.0f, 3.0f, 4.0f, 8.0f, 16.0f};

static char *itoa_s(int v, char *b)
{
    char t[8];
    int n = 0, neg = v < 0;
    if (neg)
        v = -v;
    do {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    } while (v && n < 6);
    if (neg)
        *b++ = '-';
    while (n)
        *b++ = t[--n];
    *b = 0;
    return b;
}

static float splen_secs(int v) { return 5.0f * fm_exp2f((float)v * 0.03585f); }     /* 5 s .. 1 min */
static float spoft_secs(int v) { return 10.0f * fm_exp2f((float)v * 0.04907f); }    /* 10 s .. 5 min */
static float swlen_secs(int v) { return 2.0f * fm_exp2f((float)v * 0.03322f); }     /* 2 s .. 20 s */
static float drift_secs(int v) { return 20.0f * fm_exp2f((float)v * 0.039069f); }   /* 20 s .. 5 min */

static const char *const DFORM_NAME[EN_NDFORM] = {"Even 12s", "Even 20s", "Even 32s", "Home 12s", "Home 20s",
                                                  "Home 32s", "Turn 12s", "Turn 20s", "Turn 32s"};
static const char *const RFORM_NAME[EN_NRFORM] = {"Even", "Slow", "Home", "Lift"};
void en_param_text(int i, int v, char *b)
{
    static const char *const ONOFF[2] = {"Off", "On"};
    const char *s = 0;
    if (i == P_ETIME)
        s = ETIME_NAME[(unsigned)v < 6u ? v : 0];
    else if (i == P_MIDI)
        s = ONOFF[v ? 1 : 0];
    else if (i == P_PADTONE)
        s = PADTONE_NAME[(unsigned)v < 4u ? v : 0];
    else if (i == P_MODE)
        s = EN_MODE_NAME[(unsigned)v < EN_NMODES ? v : 0];
    else if (i == P_ROOT)
        s = EN_NOTE_NAME[(unsigned)v < 12u ? v : 0];
    else if (i == P_ARP)
        s = ARP_NAME[(unsigned)v < EN_NARP ? v : 0];
    else if (i == P_ARATE)
        s = ARATE_NAME[(unsigned)v < EN_NARATE ? v : 0];
    else if (i == P_APOLY)
        s = APOLY_NAME[(unsigned)v < EN_NAPOLY ? v : 0];
    else if (i == P_BCOLOR)
        s = BCOLOR_NAME[(unsigned)v < EN_NBCOLOR ? v : 0];
    else if (i == P_DFORM)
        s = DFORM_NAME[(unsigned)v < EN_NDFORM ? v : 0];
    else if (i == P_RDFORM)
        s = RFORM_NAME[(unsigned)v < EN_NRFORM ? v : 0];
    else if ((i == P_DEAL || i == P_RDEAL) && !v)
        s = "Off";
    else if (i == P_DREAM && v < 2)
        s = v ? "Wild" : "Off";
    else if (i >= P_RHITS0 && i <= P_RHITS2 && !v)
        s = "Auto";
    else if (i == P_RMOVES)
        s = v ? "Still" : "Follow";
    else if (i == P_SONG)
        s = v ? "On" : "Off";
    else if (i == P_PSPREAD)
        s = SPREAD_NAME[(unsigned)v < 4u ? v : 0];
    else if (i == P_WATER)                       /* where on the water's journey */
        s = !v ? "Off" : v < 21 ? "Creek" : v < 43 ? "Drizzle" : v < 60 ? "Rain" : v < 80 ? "Downpour" : "Waves";
    else if ((i == P_CRICKET || i == P_PEEP || i == P_THUNDER) && !v)
        s = "Off";
    if (s) {
        int k = 0;
        while (s[k] && k < 9) {
            b[k] = s[k];
            k++;
        }
        b[k] = 0;
        return;
    }
    if (i >= P_RHITS0 && i <= P_RHITS2) {        /* a count: "5 hits" */
        b = itoa_s(v - 1, b);
        s = v == 2 ? " hit" : " hits";
        while (*s)
            *b++ = *s++;
        *b = 0;
        return;
    }
    if (i >= P_RTURN0 && i <= P_RTURN2) {        /* "Turn 3" */
        s = "Turn ";
        while (*s)
            *b++ = *s++;
        itoa_s(v, b);
        return;
    }
    if (i == P_DREAM) {                          /* a number: shown from 1 */
        itoa_s(v - 1, b);
        return;
    }
    if (i == P_DCOUNT || i == P_RDCOUNT) {
        b = itoa_s(v, b);
        s = i == P_DCOUNT ? " chords" : " keys";
        while (*s)
            *b++ = *s++;
        *b = 0;
        return;
    }
    if (((i >= P_SLOT1 && i < P_SLOT1 + EN_NSLOT) || (i >= P_RSLOT1 && i < P_RSLOT1 + EN_NSLOT)) && !v) {
        b[0] = 'O', b[1] = 'f', b[2] = 'f', b[3] = 0;
        return;
    }
    if (i >= P_RSLOT1 && i < P_RSLOT1 + EN_NSLOT) {
        s = EN_NOTE_NAME[(v - 1) % 12];
        while (*s)
            *b++ = *s++;
        *b = 0;
        return;
    }
    if (i >= P_RLEN1 && i < P_RLEN1 + EN_NSLOT) {
        int bars = EN_ROOT_BARS[(unsigned)v < EN_NRBARS ? v : 0];
        b = itoa_s(bars, b);
        s = bars == 1 ? " bar" : " bars";
        while (*s)
            *b++ = *s++;
        *b = 0;
        return;
    }
    if (i >= P_LEN1 && i < P_LEN1 + EN_NSLOT) {
        int bars = EN_SLATE_BARS[(unsigned)v < EN_NBARS ? v : 0];
        b = itoa_s(bars, b);
        s = bars == 1 ? " bar" : " bars";
        while (*s)
            *b++ = *s++;
        *b = 0;
        return;
    }
    if (i == P_BASSN && v <= 0) {               /* Held; below it, Pedal: the key's home under every chord */
        const char *s2 = v ? "Pedal" : "Held";
        while ((*b++ = *s2++))
            ;
        return;
    }
    if (i == P_ARANGE) {
        b = itoa_s(v, b);
        s = " oct";
        while (*s)
            *b++ = *s++;
        *b = 0;
        return;
    }
    if (i == P_DRIFT || i == P_SPLEN || i == P_SPOFT || i == P_SWLEN || i == P_SWOFT) {
        int sec = (int)((i == P_DRIFT ? drift_secs(v) : i == P_SPLEN ? splen_secs(v) : i == P_SWLEN ? swlen_secs(v) : spoft_secs(v)) + 0.5f);
        if (sec < 60) {
            b = itoa_s(sec, b);
            *b++ = 's';
            *b = 0;
        } else {
            b = itoa_s(sec / 60, b);
            *b++ = ':';
            *b++ = (char)('0' + sec % 60 / 10);
            *b++ = (char)('0' + sec % 10);
            *b = 0;
        }
        return;
    }
    if (i == P_SWING) {                          /* as a swing percentage: 50% (straight) .. 75% */
        b = itoa_s(50 + v / 4, b);
        *b++ = '%';
        *b = 0;
        return;
    }
    if ((i == P_TUNE || i == P_BOCT || i == P_BASSOCT) && v > 0)
        *b++ = '+';
    itoa_s(v, b);
}

/* ------------------------------------------------------- the pure parts --- */
uint32_t en_euclid(int k, int n, int rot)
{
    /* Bjorklund's algorithm (as Toussaint's "The Euclidean Algorithm Generates Traditional Musical
     * Rhythms"): k groups "x" and n - k groups ".", the remainders folded onto the heads until at most
     * one remains. Groups are bit strings, LSB first: (bits, length). */
    uint32_t gb[32], m = 0;
    uint8_t gl[32];
    int ng, heads, tails, i, pos = 0;
    if (n <= 0 || n > 32 || k <= 0)
        return 0;
    if (k > n)
        k = n;
    for (i = 0; i < n; i++) {
        gb[i] = i < k ? 1u : 0u;
        gl[i] = 1;
    }
    ng = n;
    heads = k;
    tails = n - k;
    while (tails > 1 && heads > 0) {
        int pairs = heads < tails ? heads : tails;
        for (i = 0; i < pairs; i++) {           /* a tail onto each head */
            gb[i] |= gb[ng - pairs + i] << gl[i];
            gl[i] = (uint8_t)(gl[i] + gl[ng - pairs + i]);
        }
        if (heads > tails) {                    /* the unpaired heads become the new tails */
            tails = heads - pairs;
            heads = pairs;
        } else {
            tails -= pairs;
            heads = pairs;
        }
        ng = heads + tails;
    }
    for (i = 0; i < ng; i++) {
        uint32_t j;
        for (j = 0; j < gl[i]; j++)
            if (gb[i] >> j & 1u)
                m |= 1u << (((pos + (int)j + rot) % n + n) % n);
        pos += gl[i];
    }
    return m;
}

int en_scale(uint32_t pool, int *steps)
{
    int i, m = 0;
    pool |= 1u;
    for (i = 0; i < 12; i++)
        if (pool >> i & 1u)
            steps[m++] = i;
    return m;
}

int en_chord_tones(uint32_t pool, int root, int deg, int *pcs)
{
    static const uint8_t OFF[7] = {0, 2, 4, 6, 1, 3, 5};   /* stacked thirds, then what's left */
    int st[12], m = en_scale(pool, st), want, n = 0, i, j;
    want = m >= 6 ? 4 : 3;
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 7 && n < want; i++) {
        int pc = (root + st[(deg + OFF[i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    return n;
}

/* the shapes, in scale steps above the root. Every tone of the scale, so nothing can clash; the fifth stays third */
static const int8_t SHAPE_OFF[EN_NSHAPE][4] = {
    {0, 2, 4, 6}, {0, 2, 4, -1}, {0, 1, 4, -1}, {0, 3, 4, -1}, {0, 2, 4, 1}, {0, 2, 4, 5}};
static const int8_t SHAPE_COLOUR[EN_NSHAPE] = {-1, -1, 1, 3, 1, 5};   /* the step that makes it, among the passing ones */

int en_chord_shape(uint32_t pool, int root, int deg, int shape, int *pcs)
{
    int st[12], m, n = 0, i, j;
    if (shape <= 0 || shape >= EN_NSHAPE)
        return en_chord_tones(pool, root, deg, pcs);
    m = en_scale(pool, st);
    deg = ((deg % m) + m) % m;
    for (i = 0; i < 4 && SHAPE_OFF[shape][i] >= 0; i++) {
        int pc = (root + st[(deg + SHAPE_OFF[shape][i]) % m]) % 12, dup = 0;
        for (j = 0; j < n; j++)
            dup |= pcs[j] == pc;
        if (!dup)
            pcs[n++] = pc;
    }
    if (n < 3) {                                /* a short scale: the plain chord's notes */
        int t[4], k = en_chord_tones(pool, root, deg, t);
        for (i = 0; i < k && n < 3; i++) {
            int dup = 0;
            for (j = 0; j < n; j++)
                dup |= pcs[j] == t[i];
            if (!dup)
                pcs[n++] = t[i];
        }
    }
    return n;
}

static int nearest(int from, int pc, int lo, int hi)    /* the note of class pc nearest from, in lo..hi */
{
    int n = from + ((pc - from) % 12 + 12) % 12;          /* at or above */
    if (n - from > 6)
        n -= 12;
    while (n < lo)
        n += 12;
    while (n > hi)
        n -= 12;
    return n;
}

void en_voice_lead(const int *from, const int *pcs, int nt, int *to, int lo, int hi)
{
    static const uint8_t PERM[24][4] = {
        {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 2, 3, 1}, {0, 3, 1, 2}, {0, 3, 2, 1},
        {1, 0, 2, 3}, {1, 0, 3, 2}, {1, 2, 0, 3}, {1, 2, 3, 0}, {1, 3, 0, 2}, {1, 3, 2, 0},
        {2, 0, 1, 3}, {2, 0, 3, 1}, {2, 1, 0, 3}, {2, 1, 3, 0}, {2, 3, 0, 1}, {2, 3, 1, 0},
        {3, 0, 1, 2}, {3, 0, 2, 1}, {3, 1, 0, 2}, {3, 1, 2, 0}, {3, 2, 0, 1}, {3, 2, 1, 0}};
    int t[4], p, v, best = 1 << 30;
    for (v = 0; v < 4; v++)
        t[v] = pcs[v < nt ? v : 0];                        /* three tones: the root doubled */
    for (p = 0; p < 24; p++) {
        int c = 0, n[4], a, b;
        for (v = 0; v < 4; v++) {
            n[v] = nearest(from[v], t[PERM[p][v]], lo, hi);
            c += n[v] > from[v] ? n[v] - from[v] : from[v] - n[v];
        }
        for (a = 0; a < 4; a++)                            /* no two voices on one note */
            for (b = a + 1; b < 4; b++)
                if (n[a] == n[b])
                    c += 40;
        if (c < best) {
            best = c;
            for (v = 0; v < 4; v++)
                to[v] = n[v];
        }
    }
}

const char *en_chord_name(const int *pcs, int n, char *buf)
{
    uint32_t iv = 0;
    int i, k = 0, third, fifth;
    const char *q = "", *r = EN_NOTE_NAME[pcs[0] % 12];
    for (i = 1; i < n; i++)
        iv |= 1u << ((pcs[i] - pcs[0] + 12) % 12);
    third = iv >> 4 & 1u ? 4 : iv >> 3 & 1u ? 3 : 0;
    fifth = iv >> 7 & 1u ? 7 : iv >> 6 & 1u ? 6 : iv >> 8 & 1u ? 8 : 0;
    if (third == 4) {
        q = fifth == 8 ? "aug" : iv >> 11 & 1u ? "M7" : iv >> 10 & 1u ? "7" : iv >> 9 & 1u ? "6"
            : iv >> 2 & 1u ? "add9" : "";
    } else if (third == 3) {
        if (fifth == 6)
            q = iv >> 10 & 1u ? "m7b5" : iv >> 9 & 1u ? "dim7" : iv >> 2 & 1u ? "dimadd9" : "dim";
        else
            q = iv >> 10 & 1u ? "m7" : iv >> 11 & 1u ? "mM7" : iv >> 9 & 1u ? "m6" : iv >> 2 & 1u ? "madd9" : "m";
    } else {
        q = iv >> 5 & 1u ? (iv >> 10 & 1u ? "7sus4" : "sus4") : iv >> 2 & 1u ? "sus2" : "5";
    }
    while (*r)
        buf[k++] = *r++;
    while (*q)
        buf[k++] = *q++;
    buf[k] = 0;
    return buf;
}

/* ------------------------------------------------------------ the dealers --- */
/* A number deals the same each time: its own dice, apart from the garden's. The guardrails hold at every Spice:
 * home first; no chord twice; the last leads home; a diminished chord only with Spice, and never the longest. The
 * cycle comes to 8, 16 or 32 bars, as phrases do. Spice: low, the near, strong moves in their old order (home, then
 * the chords that lead away, then those that lead back); on up, the weaker moves and the colours too. */
static uint32_t deal_rng = 1u;
static float deal_rand(void)
{
    uint32_t x = deal_rng;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    deal_rng = x;
    return (float)(x >> 8) * (1.0f / 16777216.0f);
}
static void deal_seed(uint32_t num, uint32_t salt)
{
    uint32_t x = num * 2654435761u ^ salt;
    int i;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    deal_rng = x ? x : 1u;
    for (i = 0; i < 4; i++)
        deal_rand();
}
static int deal_pick(const float *w, int n)    /* an index by weight; -1: none */
{
    float s = 0.0f, r;
    int i, last = -1;
    for (i = 0; i < n; i++)
        if (w[i] > 0.0f)
            s += w[i], last = i;
    if (last < 0)
        return -1;
    r = deal_rand() * s;
    for (i = 0; i < n; i++)
        if (w[i] > 0.0f && (r -= w[i]) < 0.0f)
            return i;
    return last;
}
static int bars_index(const uint8_t *table, int n, int bars)
{
    int i;
    for (i = 0; i < n; i++)
        if (table[i] == bars)
            return i;
    return -1;
}

/* the chords' Form: each chord's share, in units. The pace (12, 20 or 32 s a chord, on average) and the tempo set the
 * cycle: 4, 8, 16, 32 or 64 bars, as phrases go */
static const uint8_t DFORM_UNITS[3][3][4] = {
    {{1, 1}, {1, 1, 2}, {1, 1, 1, 1}},          /* Even (three: the last held, to round the phrase) */
    {{3, 1}, {2, 1, 1}, {4, 2, 1, 1}},          /* Home: the first longest */
    {{3, 1}, {3, 3, 2}, {2, 2, 3, 1}},          /* Turn: the last short, to turn back */
};

static float chord_move(const int *st, int m, const uint8_t *dim, int d, int e, float sp)
{
    static const int8_t FUNC[7] = {0, 1, 0, 1, 2, 0, 2};   /* home, leading away, leading back (I ii iii IV V vi vii) */
    const int iv = (st[e] - st[d] + 12) % 12;
    float w;
    if (iv == 5 || iv == 7)
        w = 5.0f;                               /* by a fourth or a fifth: the strongest */
    else if (iv == 3 || iv == 4 || iv == 8 || iv == 9)
        w = 3.5f;                               /* by a third: two notes kept */
    else if (iv == 6)
        w = sp >= 0.7f ? 0.8f : 0.0f;           /* the tritone, only with Spice */
    else
        w = 2.5f;                               /* by a step */
    if (w <= 0.0f)
        return 0.0f;
    if (m == 7) {
        const int f = FUNC[d], g = FUNC[e];
        if ((f == 0 && g == 1) || (f == 1 && g == 2) || (f == 2 && g == 0))
            w *= 1.0f + 1.5f * (1.0f - sp);     /* the old order, strongest when plain */
        if (e == 2)
            w *= sp < 0.3f ? 0.05f : 0.35f + 0.65f * sp;   /* the mediant: a colour of Spice */
    }
    if (dim[e] && sp < 0.5f)
        return 0.0f;
    return w * (1.0f - 0.6f * sp) + 3.0f * 0.6f * sp;     /* Spice evens the odds */
}

int en_deal_cycle(int count, int form, int tempo)   /* the phrase nearest the pace's seconds, at this tempo */
{
    static const float PACE[3] = {12.0f, 20.0f, 32.0f};
    const int n = count < 2 ? 2 : count > 4 ? 4 : count, f = form < 0 ? 0 : form >= EN_NDFORM ? EN_NDFORM - 1 : form;
    const float want = (float)n * PACE[f % 3] * (float)(tempo < 30 ? 30 : tempo) / 240.0f;
    int total = 4;
    while (total < 64 && (float)total * 1.41421f < want)
        total *= 2;
    return total;
}

int en_deal_chords(uint32_t pool, int num, int count, int spice, int form, int tempo, int16_t *slot, int16_t *len)
{
    int st[12], m = en_scale(pool, st), n, i, e, seq[4], bars[4], total, longest = 0, attempt, ok = 0, shape, f;
    uint8_t dim[12], used[12];
    float w[12], sp = (float)(spice < 0 ? 0 : spice > 100 ? 100 : spice) * 0.01f;
    n = count < 2 ? 2 : count > 4 ? 4 : count;
    if (n > m)
        n = m;
    f = form < 0 ? 0 : form >= EN_NDFORM ? EN_NDFORM - 1 : form;
    total = en_deal_cycle(n, f, tempo);
    {
        const uint8_t *u = DFORM_UNITS[f / 3][n - 2];
        int sum = 0, good = 1;
        for (i = 0; i < n; i++)
            sum += u[i];
        for (i = 0; i < n; i++) {
            bars[i] = total * u[i] / sum;
            good &= total * u[i] % sum == 0 && bars_index(EN_SLATE_BARS, EN_NBARS, bars[i]) >= 0;
        }
        if (!good)                              /* (two, the first longest, over 32: 24 bars is too long a step) */
            for (i = 0; i < n; i++)
                bars[i] = total / n;
    }
    for (i = 0; i < n; i++)
        longest = bars[i] > longest ? bars[i] : longest;
    for (e = 0; e < m; e++) {
        int pcs[4];
        en_chord_tones(pool, 0, e, pcs);
        dim[e] = (uint8_t)(m >= 5 && (pcs[2] - pcs[0] + 12) % 12 == 6);
    }
    for (attempt = 0; attempt < 64 && !ok; attempt++) {
        deal_seed((uint32_t)num, 0xC40D5u + (uint32_t)attempt * 0x9E3779B9u);
        for (e = 0; e < m; e++)
            used[e] = 0;
        seq[0] = 0;
        used[0] = 1;
        ok = 1;
        for (i = 1; i < n && ok; i++) {
            for (e = 0; e < m; e++) {
                w[e] = used[e] ? 0.0f : chord_move(st, m, dim, seq[i - 1], e, sp);
                if (dim[e] && bars[i] >= longest)
                    w[e] = 0.0f;
                if (i == n - 1)
                    w[e] *= chord_move(st, m, dim, e, 0, sp);   /* the last: a good way home */
            }
            if ((e = deal_pick(w, m)) < 0)
                ok = 0;
            else
                seq[i] = e, used[e] = 1;
        }
    }
    if (!ok)                                    /* (never, with seven notes: the plainest, step by step) */
        for (i = 1; i < n; i++)
            seq[i] = i;
    for (i = 0; i < EN_NSLOT; i++) {
        if (i >= n) {
            slot[i] = 0;
            len[i] = 3;
            continue;
        }
        if (dim[seq[i]]) {
            shape = deal_rand() < 0.5f ? 0 : 1;
        } else {
            float col = sp < 0.35f ? 0.0f : sp < 0.7f ? 0.3f : 0.55f;
            if (!i)
                col *= 0.5f;
            shape = deal_rand() < col ? 2 + (int)(deal_rand() * 3.999f) : deal_rand() < 0.5f ? 0 : 1;
        }
        slot[i] = (int16_t)(1 + seq[i] * EN_NSHAPE + shape);
        len[i] = (int16_t)bars_index(EN_SLATE_BARS, EN_NBARS, bars[i]);
    }
    for (i = 0, e = 0; i < n; i++)
        e += bars[i];
    return e;
}

/* the keys: how near by the notes two keys share. A fourth or fifth, six of seven; a whole step, five; a minor
 * third, four; a major third, three; a half step or the tritone, two. Spice opens them in that order (fifths alone
 * never come home by other keys, so the whole step is there from the start, seldom) */
static float key_move(int iv, float sp)
{
    if (iv == 5 || iv == 7)
        return 6.0f;
    if (iv == 2)                                /* the lift; and down a step: plain, the way round home again */
        return sp >= 0.25f ? 3.0f : 1.0f;
    if (iv == 10)
        return sp >= 0.25f ? 1.5f : 1.0f;
    if (iv == 3 || iv == 9)
        return sp >= 0.35f ? 2.5f : 0.0f;
    if (iv == 4 || iv == 8)
        return sp >= 0.6f ? 2.5f : 0.0f;
    if (iv == 1 || iv == 11 || iv == 6)
        return sp >= 0.85f ? 1.0f : 0.0f;
    return 0.0f;
}
static int key_far(int iv) { return iv == 4 || iv == 8 || iv == 1 || iv == 11 || iv == 6; }

void en_deal_roots(int home, int num, int count, int spice, int form, int cycle, int min_bars, int16_t *rslot,
                   int16_t *rlen)
{
    int n, i, iv, k[4], pass[4], attempt, ok = 0, last, lift;
    float w[12], sp = (float)(spice < 0 ? 0 : spice > 100 ? 100 : spice) * 0.01f;
    n = count < 2 ? 2 : count > 4 ? 4 : count;
    lift = form == 3;
    last = lift ? n - 2 : n - 1;                /* Lift: the last key a whole step above the one before */
    if (cycle <= 0)
        cycle = 8;
    for (attempt = 0; attempt < 64 && !ok; attempt++) {
        deal_seed((uint32_t)num, 0x7E75u + (uint32_t)attempt * 0x9E3779B9u);
        k[0] = 0;
        ok = 1;
        for (i = 1; i <= last && ok; i++) {
            for (iv = 0; iv < 12; iv++) {
                int to = (k[i - 1] + iv) % 12, j, dup = !to;
                for (j = 1; j < i; j++)
                    dup |= k[j] == to;
                w[iv] = dup ? 0.0f : key_move(iv, sp);
                if (sp < 0.85f && i >= 2 && key_far((k[i - 1] - k[i - 2] + 12) % 12) && key_far(iv))
                    w[iv] = 0.0f;               /* never two far moves running */
                if (i == n - 1 && !lift) {      /* the last: home again, as near */
                    const int back = (12 - to) % 12;
                    w[iv] *= key_move(back, sp) > 0.0f ? 1.0f : 0.0f;
                    if (sp < 0.85f && key_far(back) && (key_far(iv) || key_far(k[1])))
                        w[iv] = 0.0f;           /* (round the loop too: home, then away again) */
                }
            }
            if ((iv = deal_pick(w, 12)) < 0)
                ok = 0;
            else
                k[i] = (k[i - 1] + iv) % 12;
        }
        if (ok && lift) {
            int j, to = (k[n - 2] + 2) % 12;
            ok = to != 0;
            for (j = 1; j < n - 1; j++)
                ok &= k[j] != to;
            k[n - 1] = to;
        }
    }
    if (!ok)                                    /* (only at the plainest, with four keys: round the fifths) */
        for (i = 1; i < n; i++)
            k[i] = lift && i == n - 1 ? (k[i - 1] + 2) % 12 : (k[i - 1] + 7) % 12;
    for (i = 0; i < n; i++) {                   /* (Song: each key at least a section) */
        const int base = min_bars > cycle ? (min_bars + cycle - 1) / cycle : 1;
        pass[i] = base * (form == 1 ? 2 : form == 2 && !i ? 2 : lift && !i ? 3 : 1);
    }
    for (i = 0; i < EN_NSLOT; i++) {
        int b;
        if (i >= n) {
            rslot[i] = 0;
            rlen[i] = 3;
            continue;
        }
        rslot[i] = (int16_t)((home + k[i]) % 12 + 1);
        while (pass[i] > 1 && bars_index(EN_ROOT_BARS, EN_NRBARS, pass[i] * cycle) < 0)
            pass[i]--;
        b = bars_index(EN_ROOT_BARS, EN_NRBARS, pass[i] * cycle);
        if (b < 0) {                            /* chords that come to an odd length: the nearest above, or the longest */
            for (b = 0; b < EN_NRBARS - 1 && EN_ROOT_BARS[b] < cycle; b++)
                ;
        }
        rlen[i] = (int16_t)b;
    }
}

/* ------------------------------------------------------------ commands --- */
enum { C_SET, C_POOL, C_PLAY, C_NEXT, C_FREEZE, C_SEED, C_PANIC, C_CLOCK, C_REGROW, C_RESTART };
#define QN 128u
typedef char qn_holds_a_whole_piece[P_NPARAMS + 1 + 4 <= (int)QN ? 1 : -1];   /* project_apply: every param and the pool, with room */
static uint32_t q[QN];
static volatile uint32_t q_w, q_r;
static void post(uint32_t c, int a, int b)
{
    uint32_t w = q_w;
    if (w - q_r >= QN)
        return;                                   /* full: dropped (never, at the UI's rate) */
    q[w % QN] = c << 24 | ((uint32_t)a & 0xFFu) << 16 | ((uint32_t)b & 0xFFFFu);
    BARRIER();
    q_w = w + 1;
}
void en_set(int p, int v) { post(C_SET, p, v); }
void en_pool(uint32_t pool) { post(C_POOL, 0, (int)(pool & 0xFFFu)); }
void en_play(int on) { post(C_PLAY, on, 0); }
void en_next(void) { post(C_NEXT, 0, 0); }
void en_freeze(int on) { post(C_FREEZE, on, 0); }
void en_seed(uint32_t s) { post(C_SEED, 0, (int)(s & 0xFFFFu)); }
void en_panic(void) { post(C_PANIC, 0, 0); }
void en_clock(int msg) { post(C_CLOCK, msg, 0); }
void en_regrow(void) { post(C_REGROW, 0, 0); }
void en_restart(int what) { post(C_RESTART, what, 0); }

/* -------------------------------------------------------------- state --- */
volatile uint8_t en_playing, en_frozen, en_ext;
volatile uint8_t en_chord[EN_NPAD], en_chord_root, en_chord_n;
volatile float en_drift, en_pad_level, en_bass_level;
volatile uint8_t en_loop_pos[EN_NLOOP];
volatile int8_t en_loop_note[EN_NLOOP][EN_LOOP_MAX];
volatile float en_loop_glow[EN_NLOOP];
volatile uint8_t en_drum_pos[EN_NDRUM];
volatile uint32_t en_drum_pat[EN_NDRUM];
volatile float en_drum_glow[EN_NDRUM];
volatile uint32_t en_bars;
volatile float en_arc;
volatile float en_spark;
volatile uint16_t en_seed_now;
volatile uint8_t en_root;
volatile uint8_t en_pulse_rest;
volatile uint8_t en_pad_note[EN_NPAD];
volatile float en_pad_amp[EN_NPAD];

static int16_t par[P_NPARAMS];
static uint32_t pool = 0xAADu;                  /* Ionian until told */
static int nsteps, steps[12];                   /* the scale */
static int cur_root;                            /* the root playing: Root, or the rotator's step */
static int deg, shape;                          /* the chord's scale degree, and its shape */
static int chord_pcs[4], chord_nt;
static int pad_pcs[4], pad_nt;                 /* the tones the pad sings of it (Pad notes) */
static float tunefac = 1.0f;
static uint32_t rng = 0x9E3779B9u;

static uint32_t rnd(void)                       /* Marsaglia's xorshift (Xorshift RNGs, 2003): shifts 13, 17, 5 */
{
    uint32_t x = rng;
    x = x ^ (x << 13);
    x = x ^ (x >> 17);
    rng = x ^ (x << 5);
    return rng;
}
static float frand(void) { return (float)(rnd() >> 8) * (1.0f / 16777216.0f); }
static int irand(int n) { return n > 0 ? (int)((rnd() >> 8) % (uint32_t)n) : 0; }
/* the sound's own dice (the pad's breaths, the slow modulators, the echo's heads), apart from the garden's: so a
 * Seed grows the same garden, whenever they happen to roll */
static uint32_t trng = 0x6D2B79F5u;
static float tfrand(void)
{
    uint32_t x = trng;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    trng = x;
    return (float)(x >> 8) * (1.0f / 16777216.0f);
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2)
{
    if (par[P_MIDI])
        en_midi_out(st, d1, d2);
}

static float mtof(float n) { return 440.0f * tunefac * fm_exp2f((n - 69.0f) * (1.0f / 12.0f)); }

/* a sine table: 1024 points a turn, plus one */
#define SN 1024
static float sint[SN + 1];
static inline float sine(float ph)              /* ph in turns, any value >= 0 */
{
    float x = (ph - (float)(int)ph) * (float)SN;
    int i = (int)x;
    float f = x - (float)i;
    return sint[i] + (sint[i + 1] - sint[i]) * f;
}

/* ------------------------------------------------- slow random motion --- */
#define NMOD 9                                  /* 0..5 the pads', 6..8 the echo's */
#define NMOD_PAD 6
static struct {
    float v, tgt, rate;
    uint32_t left;                              /* samples until a new target */
} mod[NMOD];

static void mods_step(uint32_t n)
{
    int i;
    for (i = 0; i < NMOD; i++) {
        if (mod[i].left <= n) {
            float secs = 7.0f + 33.0f * tfrand();
            mod[i].tgt = tfrand() * 2.0f - 1.0f;
            mod[i].left = (uint32_t)(secs * EN_SR);
            mod[i].rate = (float)n / (secs * EN_SR * 0.5f);
        } else {
            mod[i].left -= n;
        }
        mod[i].v += (mod[i].tgt - mod[i].v) * fm_minf(mod[i].rate, 1.0f);
    }
}
static float motion;                            /* 0..1: Motion */

/* ---------------------------------------------------------------- pad --- */
/* Breathing voices, as the sung tape loops of Music for Airports' "2/1": each voice swells in, hangs
 * and fades on its own cycle (unequal lengths, set by Swell), taking its note of the chord as it
 * starts, so the chord forms and dissolves rather than sitting there. The timbre: two saws a few
 * cents apart through two formant band-passes, a vowel that Bright moves from "oo" through "ah" to
 * "eh", with a little of the first formant's low-pass for body. */
typedef struct {
    float ph[2], inc[2];
    float note, target;                         /* MIDI: the note of this breath, the next one's */
    float t, rate, peak, amp;                   /* t 0..1 through the breath, per 32 samples */
    float ic[4];                                /* the two filters' states */
    float c1[4], c2[4];                         /* their coefficients: a1, a2, a3, k */
    float vib, pan;
    float hf;                                   /* a handoff's fade: 1 normal */
    uint8_t midi_note, hand;                    /* hand: 1 fading out of the old key, 2 into the new */
    uint8_t voices;                             /* this breath's colour: 0 Rhodes, 1 Voices, 2 Strings, 3 Noir */
} pad_t;
static pad_t pad[EN_NPAD];
static float pad_gate, pad_gain, pad_vol, bright, swell;
static const float PAD_PAN[EN_NPAD] = {-0.55f, -0.18f, 0.18f, 0.55f};
static const float BREATH[EN_NPAD] = {1.0f, 1.21f, 1.43f, 1.67f};    /* unequal cycles, as 2/1's 12..20 s loops */
static const float START_T[EN_NPAD] = {0.0f, 0.9f, 0.82f, 0.95f};    /* the first breaths overlap */
static const float VOWEL[4][2] = {{320.0f, 800.0f}, {480.0f, 900.0f}, {730.0f, 1090.0f}, {530.0f, 1840.0f}};
static float arcm;                              /* the slow arc, -1 sparse .. +1 full, times Motion */

static float breath(float t)                    /* rise, hang, fall, a short rest */
{
    if (t < 0.4f)
        return 0.5f - 0.5f * fm_cosf(FM_PI * t * 2.5f);
    if (t < 0.55f)
        return 1.0f;
    if (t < 0.9f)
        return 0.5f + 0.5f * fm_cosf(FM_PI * (t - 0.55f) * (1.0f / 0.35f));
    return 0.0f;
}

static void svf_coefs(float *c, float f, float k)
{
    float g = fm_tanf(FM_PI * fm_clampf(f, 40.0f, 16000.0f) * (1.0f / EN_SR));
    c[0] = 1.0f / (1.0f + g * (g + k));
    c[1] = g * c[0];
    c[2] = g * c[1];
    c[3] = k;
}

static void midi(uint32_t st, uint32_t d1, uint32_t d2);
static void pad_breathe(pad_t *v, int i)        /* a new breath: its length, its strength, its note */
{
    float secs = (4.0f + 0.12f * swell) * BREATH[i] * (0.85f + 0.3f * tfrand());
    v->rate = 32.0f / (secs * EN_SR);
    v->peak = 0.6f + 0.4f * tfrand();
    if (en_playing && v->midi_note)
        midi(0x80, v->midi_note, 0);
    v->note = v->target;
    v->voices = (uint8_t)par[P_PADTONE];
    v->midi_note = (uint8_t)v->target;
    if (en_playing)
        midi(0x90, v->midi_note, (uint32_t)(50.0f + 40.0f * v->peak));
}

/* a new key (piece, root, scale): the voices still singing the old one fade out in about a second and
 * breathe again on the new chord; a voice at rest just takes its new note. Chord drift doesn't do
 * this: there the old notes are meant to hang over */
#define HAND_STEP (32.0f / (1.0f * EN_SR))
static uint8_t pad_snap;                       /* the slate or the rotator moved it: the pad goes on the bar, not a breath later */
static void pad_handoff(void)
{
    int i;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        if (v->amp > 0.002f && ((float)v->midi_note != v->target || v->voices != par[P_PADTONE]))
            v->hand = pad_snap ? 3 : 1;
        else if (!v->hand)
            v->note = v->target;
    }
}

/* a chord change: a voice singing a note the new chord doesn't have fades out in about a second and
 * breathes in on its new note; a voice on a shared note holds it to the end of its breath. When the slate or the
 * rotator moves it (the bass there on the bar), a quarter of a second, and in at the top of its breath */
static void pad_leave(void)
{
    int i, k;
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        int pc = v->midi_note % 12, shared = 0;
        for (k = 0; k < pad_nt; k++)
            shared |= pad_pcs[k] == pc;
        if (v->amp > 0.002f && !shared && !v->hand)
            v->hand = pad_snap ? 3 : 1;
        else if (v->amp <= 0.002f && !v->hand)
            v->note = v->target;
    }
}

static void pad_coefs(pad_t *v, int i)          /* every 32 samples: the breath, pitch, the vowel */
{
    float f, vw, f1, f2;
    int a;
    if (v->hand == 1 || v->hand == 3) {         /* out of the old key (3: the snap, in a quarter of a second) */
        v->hf -= HAND_STEP * (v->hand == 3 ? 4.0f : 1.0f);
        if (v->hf <= 0.0f) {
            v->hf = 0.0f;
            v->t = v->hand == 3 ? 0.4f : 0.15f; /* a breath already rising (the snap: at its top), on the new chord */
            v->hand = v->hand == 3 ? 4 : 2;
            pad_breathe(v, i);
        }
    } else if (v->hand == 2 || v->hand == 4) {  /* into the new */
        v->hf += HAND_STEP * (v->hand == 4 ? 3.0f : 0.6f);
        if (v->hf >= 1.0f) {
            v->hf = 1.0f;
            v->hand = 0;
        }
    }
    v->t += v->rate;
    if (v->t >= 1.0f) {
        v->t -= 1.0f;
        pad_breathe(v, i);
    }
    v->vib += (4.6f + 0.4f * (float)i) * 32.0f / EN_SR;
    if (v->vib >= 1.0f)
        v->vib -= 1.0f;
    f = mtof(v->note + 0.08f * motion * mod[i].v);                          /* a slow drift, as tape */
    if (v->voices >= 2) {                       /* Strings: a section's bow; Noir: a CS-80's brass, blooming */
        const int noir = v->voices == 3;
        const float br = breath(v->t), b01 = (float)bright * 0.01f;
        const float det = noir ? 0.0042f : 0.0035f;   /* apart: 7 cents, 6 */
        float fv = f * (1.0f + 0.0578f * sine(v->vib) * (noir ? 0.07f + 0.06f * br : 0.045f)), fc;
        v->inc[0] = fv * (1.0f - det) * (1.0f / EN_SR);
        v->inc[1] = fv * (1.0f + det) * (1.0f / EN_SR);
        fc = noir ? f * (1.2f + (2.0f + 10.0f * b01) * br) : f * (3.0f + 9.0f * b01) * (1.0f + 0.25f * arcm);
        svf_coefs(v->c1, fc, noir ? 0.5f : 1.1f);
        svf_coefs(v->c2, fc * 1.4f, 1.3f);
        return;
    }
    v->inc[0] = f * 0.99856f * (1.0f / EN_SR);  /* -2.5 and +2.5 cents */
    v->inc[1] = f * 1.00144f * (1.0f / EN_SR);
    vw = fm_clampf((float)bright * 0.01f + 0.25f * arcm + 0.2f * motion * mod[(i + 2) % NMOD_PAD].v, 0.0f, 1.0f) * 2.999f;
    a = (int)vw;
    vw -= (float)a;
    f1 = VOWEL[a][0] + (VOWEL[a + 1][0] - VOWEL[a][0]) * vw;
    f2 = VOWEL[a][1] + (VOWEL[a + 1][1] - VOWEL[a][1]) * vw;
    svf_coefs(v->c1, f1 * (1.0f + 0.03f * (float)i), 0.45f);
    svf_coefs(v->c2, f2 * (1.0f - 0.02f * (float)i), 0.3f);
}

static inline float blep_saw(float t, float dt)
{
    float y = 2.0f * t - 1.0f;
    if (t < dt) {
        float x = t / dt;
        y -= x + x - x * x - 1.0f;
    } else if (t > 1.0f - dt) {
        float x = (t - 1.0f) / dt;
        y -= x * x + x + x + 1.0f;
    }
    return y;
}

/* ------------------------------------------------------------------ bass --- */
static struct {
    float ph, inc, note, target, amp;
    float env, lvl, edec;                       /* Bass notes: each its own ring (env falls by edec; lvl follows it,
                                                 * so no clicks); held, both stay at 1 */
    int32_t wait;                               /* swing: samples until a note that waits (pend) */
    uint8_t midi_note, rest, pend;              /* rest: sitting this chord out */
} bass;
static float bass_vol, glide_k = 1.0f / (0.25f * EN_SR);      /* glide_k: 1 / the glide's time in samples */
static float bass_att = 0.006f;                 /* its attack, by Pluck: ~4 ms plucked .. ~40 ms humming */

/* ----------------------------------------------------------------- bells --- */
#define NBELL 6
typedef struct {
    float cph, mph, cinc, minc, amp, adec, idx, idec, att, arate, gl, gr;
    int32_t wait;
    uint8_t on, note, loop, mvel;               /* mvel: its MIDI note waits with it (the arp, off the grid) */
} bell_t;
static bell_t bell[NBELL];
static uint8_t bell_late;                       /* the next bell's MIDI note waits with it */
static float bell_vol, btone, bell_dec = 1.0f;   /* bell_dec: the next bell's ring, times this */
static int8_t loop_note[EN_NLOOP][EN_LOOP_MAX];
static uint8_t loop_vel[EN_NLOOP][EN_LOOP_MAX], loop_pos[EN_NLOOP];
static const uint8_t LOOP_BASE[EN_NLOOP] = {72, 67, 79, 62};     /* each loop's register */
static const float LOOP_PAN[EN_NLOOP] = {-0.6f, 0.35f, 0.7f, -0.25f};
static const float RATIO[5] = {1.0f, 2.0f, 3.0f, 3.5f, 4.77f};

/* ----------------------------------------------------------------- pulse --- */
typedef struct {
    float ph, inc, inc_end, amp, adec, idx, x1, x2, att, mph[4], pend;
    int32_t wait;                               /* swing: samples until a hit that waits (pend: its velocity) */
    uint8_t on, note;
} drum_t;
static drum_t drum[EN_NDRUM];
static uint8_t drum_pos[EN_NDRUM];
static uint32_t drum_pat[EN_NDRUM];
static float pulse_vol, hat_f = 0.9f, hat_open = 0.42f, hat_closed = 0.06f, hat_choke = 0.02f;
static float hat_lv = 1.0f, hat_hp = 1.0f, hat_bp = 0.0f;
static uint8_t hat_roll;
static float pulse_left;                         /* to the next MIDI clock pulse */
static uint32_t noise = 22222u;
static const uint8_t DRUM_KMAX[EN_NDRUM] = {5, 7, 3};
static const uint8_t DRUM_GM[EN_NDRUM] = {36, 42, 46};          /* kick, closed hat, open hat */
/* the hats' metal: four square waves at the 808's inharmonic ratios (Hz / EN_SR) */
static const float HAT_INC[4] = {304.4f / EN_SR, 369.6f / EN_SR, 522.7f / EN_SR, 800.0f / EN_SR};

/* ------------------------------------------------------------------ space --- */
static float echo_send, rev_send, echo_len, echo_len_t, echo_fb = 0.55f;
static float rev_in_gain = 1.0f;                /* falls to 0 while frozen */

/* -------------------------------------------------------------- harmony --- */
static void scale_set(void)
{
    nsteps = en_scale(pool, steps);
}

/* a loop's note over a chord in a shape: its third gives way to a sus's note; its seventh to the shape's own */
static int shape_note(int off)
{
    static const int8_t SEVENTH[EN_NSHAPE] = {6, 4, 4, 4, 1, 5};
    int r = off % nsteps;
    if (!shape)
        return off;
    if (r == 2 && (shape == 2 || shape == 3))
        return off - r + SHAPE_COLOUR[shape];
    if (r == 6 && nsteps > 6)
        return off - r + SEVENTH[shape];
    return off;
}

static int bell_note(int l, int off)            /* a loop slot's note, from the chord's degree */
{
    int d = deg + shape_note(off), oct = d / nsteps, s = steps[d % nsteps], base, root = cur_root, n;
    base = LOOP_BASE[l] - ((LOOP_BASE[l] - root) % 12 + 12) % 12;   /* the root at or below the register */
    n = base + s + 12 * oct + 12 * par[P_BOCT];
    while (n > 103)
        n -= 12;
    while (n < 36)
        n += 12;
    return n;
}

/* Pad notes: all of the chord; or its root, third and fifth; or its root and fifth, open across the voices. Under
 * four, the arc moves it by one as each chord comes */
static void pad_tones(void)
{
    int i, n = par[P_PADN];
    if (n < 4)
        n = (int)fm_clampf((float)(n + (arcm > 0.4f) - (arcm < -0.4f)), 2.0f, 4.0f);
    pad_nt = chord_nt;
    for (i = 0; i < chord_nt; i++)
        pad_pcs[i] = chord_pcs[i];
    if (chord_nt >= 3 && n == 3) {
        pad_nt = 3;
    } else if (chord_nt >= 3 && n == 2) {
        pad_pcs[1] = chord_pcs[2];
        pad_pcs[2] = chord_pcs[0];
        pad_pcs[3] = chord_pcs[2];
        pad_nt = 4;
    }
}

static void pad_retarget(void)                  /* the chord's tones to the four voices */
{
    int from[4], to[4], all[4], i;
    chord_nt = en_chord_shape(pool, cur_root, deg, shape, chord_pcs);
    pad_tones();
    for (i = 0; i < EN_NPAD; i++)
        from[i] = (int)(pad[i].target + 0.5f);
    en_voice_lead(from, pad_pcs, pad_nt, to, 48, 77);
    if (par[P_PADN] < 4)                        /* the screen names the whole chord, whatever the pad leaves out */
        en_voice_lead(from, chord_pcs, chord_nt, all, 48, 77);
    if (par[P_PSPREAD] > 0) {                   /* Spread: the root an octave down at the bottom; Wide, the top voice an
                                                 * octave up; Vast, the next one too: the open voicing ambient keeps */
        int r = -1, k, up;
        for (i = 0; i < EN_NPAD; i++)
            if (to[i] % 12 == chord_pcs[0] && (r < 0 || to[i] < to[r]))
                r = i;
        if (r >= 0 && to[r] >= 48)
            to[r] -= 12;
        for (up = 1; up < par[P_PSPREAD]; up++) {
            int hi = -1;
            for (k = 0; k < EN_NPAD; k++)
                if (k != r && to[k] <= 79 && (hi < 0 || to[k] > to[hi]))
                    hi = k;
            if (hi >= 0)
                to[hi] += 12;
        }
    }
    for (i = 0; i < EN_NPAD; i++) {             /* each voice takes its note at its next breath */
        pad[i].target = (float)to[i];
        en_chord[i] = (uint8_t)(par[P_PADN] < 4 ? all[i] : to[i]);
    }
    en_chord_root = (uint8_t)chord_pcs[0];
    en_chord_n = (uint8_t)chord_nt;
}

static void bass_to(int pc)
{
    int n = 36 + pc % 12 + 12 * par[P_BASSOCT];     /* C2..B2, an octave down or up */
    bass.target = (float)n;
    if (par[P_BASSN] > 0)
        return;                                 /* stepping: each hit sends its own note (once, not twice) */
    if (en_playing && bass.midi_note != n) {
        midi(0x82, bass.midi_note, 0);
        midi(0x92, (uint32_t)n, 80);
    }
    bass.midi_note = (uint8_t)n;
}

static float drift_left, drift_total, evolve;   /* evolve: Evolve, 0..1 */
static uint8_t change_pending, fifth_done;

static void drift_reset(void)
{
    drift_total = drift_secs(par[P_DRIFT]) * (0.7f + 0.6f * frand()) * EN_SR;
    drift_left = drift_total;
    fifth_done = 0;
}

static int bass_home(void)                      /* the bass's note: the chord's root, or with Pedal the key's */
{
    return par[P_BASSN] < 0 ? cur_root : chord_pcs[0];
}

static void chord_to(int d)                     /* the chord on degree d: the pad, the bass */
{
    deg = d;
    pad_retarget();
    pad_leave();
    bass_to(bass_home());
    /* Evolve: the held bass sometimes sits a chord out, more often when the arc is sparse; with Notes, it keeps time */
    bass.rest = par[P_BASSN] == 0 && frand() < evolve * 0.3f + 0.25f * fm_maxf(-arcm, 0.0f);   /* (a pedal stays) */
    drift_reset();
}

static void chord_change(void)                  /* to another degree: the near, strong moves first */
{
    static const float W7[7] = {4.0f, 2.0f, 1.0f, 3.0f, 3.0f, 3.0f, 0.4f};
    float w[12], sum = 0.0f, r;
    int i;
    for (i = 0; i < nsteps; i++) {
        w[i] = i == deg ? 0.0f : nsteps == 7 ? W7[i] : i == 0 ? 3.0f : 1.5f;
        sum += w[i];
    }
    r = frand() * sum;
    for (i = 0; i < nsteps; i++) {
        r -= w[i];
        if (r <= 0.0f && w[i] > 0.0f)
            break;
    }
    if (i >= nsteps)
        i = deg == 0 ? (nsteps > 1 ? 1 : 0) : 0;
    shape = 0;
    chord_to(i);
}

/* ------------------------------------------------------------- the slate --- */
/* Up to four chords in order, each for its bars, in place of Drift while any slot holds one. It counts bars on
 * the grid, so it follows MIDI clock; Freeze holds it; EDIT goes to the next step on the next bar. A step's
 * degree wraps if the scale has since lost notes, as en_chord_tones does. */
#define SLATE_NONE 0xFFu
static uint8_t slate_on, slate_i = SLATE_NONE;  /* any slot filled; the step playing */
static uint16_t slate_left, slate_total;        /* bars left on this step, and its length */

static void slate_next(void)                    /* to the next filled slot after this one */
{
    int k, i = slate_i == SLATE_NONE ? EN_NSLOT - 1 : slate_i;
    for (k = 0; k < EN_NSLOT; k++) {
        i = (i + 1) % EN_NSLOT;
        if (par[P_SLOT1 + i]) {
            slate_i = (uint8_t)i;
            slate_total = slate_left = EN_SLATE_BARS[par[P_LEN1 + i]];
            shape = (par[P_SLOT1 + i] - 1) % EN_NSHAPE;
            pad_snap = 1;
            chord_to((par[P_SLOT1 + i] - 1) / EN_NSHAPE % nsteps);
            pad_snap = 0;
            return;
        }
    }
}

static void slate_bar(void)                     /* a bar line, the garden not frozen */
{
    if (slate_i == SLATE_NONE || change_pending || --slate_left == 0) {
        change_pending = 0;
        slate_next();
    }
}

static void slate_check(void)                   /* a slot changed: the slate on, or off and Drift back */
{
    int i, on = 0;
    for (i = 0; i < EN_NSLOT; i++)
        on |= par[P_SLOT1 + i] != 0;
    if (slate_on && !on) {
        slate_i = SLATE_NONE;
        drift_reset();
    }
    slate_on = (uint8_t)on;
}

/* ----------------------------------------------------- the root rotator --- */
/* Up to four roots in order, each for its bars, in place of Root while any step holds one. The scale moves with it,
 * so the chords do. A step's end starts the chords again from their first step (or, drifting, the home chord): the
 * sections line up. On the grid and the bar, so it follows MIDI clock; Freeze holds it. */
static uint8_t root_on, root_i = SLATE_NONE, root_stepped;   /* stepped: a step began (the song's sections) */
static int song_bars(void);
static uint16_t root_left;

static int root_bar(void)                       /* a bar line, the garden not frozen: 1 if a new step began */
{
    int k, i = root_i == SLATE_NONE ? EN_NSLOT - 1 : root_i;
    if (root_i != SLATE_NONE && --root_left)
        return 0;
    for (k = 0; k < EN_NSLOT; k++) {
        i = (i + 1) % EN_NSLOT;
        if (par[P_RSLOT1 + i]) {
            root_i = (uint8_t)i;
            root_left = EN_ROOT_BARS[par[P_RLEN1 + i]];
            if (par[P_SONG] && root_left < song_bars())
                root_left = (uint16_t)song_bars();   /* Song: a key lasts at least a section */
            root_stepped = 1;
            if ((par[P_RSLOT1 + i] - 1) % 12 == cur_root)
                return 0;                       /* the same root: nothing to start again */
            cur_root = (par[P_RSLOT1 + i] - 1) % 12;
            en_root = (uint8_t)cur_root;
            change_pending = 0;
            if (slate_on) {
                slate_i = SLATE_NONE;
                slate_next();
            } else {
                shape = 0;
                chord_to(0);
            }
            pad_snap = 1;
            pad_handoff();                      /* the voices cross over, as for any new key: on the bar */
            pad_snap = 0;
            return 1;
        }
    }
    return 0;
}

static void root_check(void)                    /* a step changed: the rotator on, or off and Root back */
{
    int i, on = 0;
    for (i = 0; i < EN_NSLOT; i++)
        on |= par[P_RSLOT1 + i] != 0;
    if (root_on && !on) {
        root_i = SLATE_NONE;
        cur_root = par[P_ROOT];
        en_root = (uint8_t)cur_root;
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            bass_to(bass_home());
        }
    }
    root_on = (uint8_t)on;
}

/* ------------------------------------------------------------- the loops --- */
/* Density's other half: how often a new note is a passing note. None to 20, as AMB-1 always was from 60 */
static float passing(void) { return 0.28f * fm_clampf(((float)par[P_DENSITY] - 20.0f) * (1.0f / 40.0f), 0.0f, 1.0f); }

static int rand_offset(void)                    /* scale steps above the chord's degree */
{
    if (frand() >= passing()) {                 /* a chord tone, sometimes an octave up */
        int o = shape ? SHAPE_OFF[shape][irand(shape == 1 || shape == 2 || shape == 3 ? 3 : 4)] : 2 * irand(chord_nt);
        return o + (frand() < 0.3f ? nsteps : 0);
    }
    return 1 + 2 * irand(3);                    /* a passing note: the 2nd, 4th or 6th */
}

static int loop_count(int l)
{
    int i, c = 0;
    for (i = 0; i < EN_LOOP_LEN[l]; i++)
        c += loop_note[l][i] >= 0;
    return c;
}

static int loop_target(void)                    /* 0..5 notes: Density, swelled and thinned by the arc */
{
    float d = (float)par[P_DENSITY] * 0.05f * (1.0f + 0.8f * arcm);
    return (int)(fm_clampf(d, 0.0f, 5.0f) + frand());
}
static uint8_t loop_rest[EN_NLOOP];             /* this pass: silent (Evolve) */

static void loop_add(int l)
{
    int len = EN_LOOP_LEN[l], i, s;
    for (i = 0; i < 8; i++) {
        s = irand(len);
        if (loop_note[l][s] < 0) {
            loop_note[l][s] = (int8_t)rand_offset();
            loop_vel[l][s] = (uint8_t)(50 + irand(50));
            return;
        }
    }
}

static int loop_pick(int l)                     /* a random sounding slot, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0)
            return s;
    return -1;
}

static int loop_pick_passing(int l)             /* a random slot on a passing note, -1 none */
{
    int len = EN_LOOP_LEN[l], i, s = irand(len);
    for (i = 0; i < len; i++, s = (s + 1) % len)
        if (loop_note[l][s] >= 0 && loop_note[l][s] % nsteps & 1 && loop_note[l][s] % nsteps != SHAPE_COLOUR[shape])
            return s;
    return -1;
}

static void loop_publish(int l)
{
    int i;
    for (i = 0; i < EN_LOOP_MAX; i++)
        en_loop_note[l][i] = i < EN_LOOP_LEN[l] ? loop_note[l][i] : -1;
}

static void loop_regrow(int l)
{
    int i, n = loop_target();
    for (i = 0; i < EN_LOOP_MAX; i++)
        loop_note[l][i] = -1;
    for (i = 0; i < n; i++)
        loop_add(l);
    loop_publish(l);
}

/* ------------------------------------------------------------- the song --- */
/* Song (on by default): the garden in sections, as songs go. A section lasts whole passes of the chords, about two and
 * a half minutes (with keys, each key's step is one; home the first). What changes the piece's character waits for a
 * section's end: through a section the bells' loops keep their motif (what Density adds back comes from it first); at
 * its end the motif is developed (more with Evolve) for somewhere new, or a kind of section already heard gets its own
 * back, so a return is a return (A B A C A B ...). The arc rises and falls with the sections, not on its own clock.
 * The rings and the arp turn, and the bass's figure changes, only between sections; the pulse may take its breath in a
 * section's last two bars. Off: the garden as it always was, each part on its own clock. */
#define SONG_KINDS 5
static int8_t song_note[SONG_KINDS][EN_NLOOP][EN_LOOP_MAX];
static uint8_t song_vel[SONG_KINDS][EN_NLOOP][EN_LOOP_MAX];
static uint8_t song_has[SONG_KINDS], song_kind, song_edge;   /* edge: this bar began a section */
static uint16_t song_sec, song_left;
static const float SONG_ARC[6] = {-0.5f, 0.1f, 0.6f, -0.1f, 0.8f, -0.3f};   /* open low, build, peak, settle, peak, ease */
static void arc_aim(float t);
static void drums_evolve(void);

static int song_bars(void)                       /* a section: whole passes of the chords, about 150 s */
{
    const float bar_s = 240.0f / (float)(par[P_TEMPO] < 30 ? 30 : par[P_TEMPO]);
    int cycle = 0, i, n;
    for (i = 0; i < EN_NSLOT; i++)
        if (par[P_SLOT1 + i])
            cycle += EN_SLATE_BARS[par[P_LEN1 + i]];
    if (!cycle)
        cycle = 8;
    n = (int)(150.0f / (bar_s * (float)cycle) + 0.5f);
    if (n < 1)
        n = 1;
    while (n > 1 && n * cycle > 256)
        n--;
    return n * cycle;
}
static void song_keep(int k)
{
    int l, s;
    for (l = 0; l < EN_NLOOP; l++)
        for (s = 0; s < EN_LOOP_MAX; s++)
            song_note[k][l][s] = loop_note[l][s], song_vel[k][l][s] = loop_vel[l][s];
    song_has[k] = 1;
}
static void song_mutate(int l)                   /* a note of the motif changed, or moved */
{
    int s = loop_pick(l);
    if (s < 0)
        return;
    if (frand() < 0.5f) {
        loop_note[l][s] = (int8_t)rand_offset();
    } else {
        int d = irand(EN_LOOP_LEN[l]);
        if (loop_note[l][d] < 0) {
            loop_note[l][d] = loop_note[l][s];
            loop_vel[l][d] = loop_vel[l][s];
            loop_note[l][s] = -1;
        }
    }
}
static void loop_add_song(int l)                 /* Density adds: the section's motif first */
{
    const int len = EN_LOOP_LEN[l];
    int i, s = irand(len);
    if (par[P_SONG] && song_has[song_kind])
        for (i = 0; i < len; i++, s = (s + 1) % len)
            if (loop_note[l][s] < 0 && song_note[song_kind][l][s] >= 0) {
                loop_note[l][s] = song_note[song_kind][l][s];
                loop_vel[l][s] = song_vel[song_kind][l][s];
                return;
            }
    loop_add(l);
}
static void song_enter(int kind)
{
    int l, s, m;
    if (song_has[kind]) {                        /* a return: its own motif again, a touch changed with Evolve */
        for (l = 0; l < EN_NLOOP; l++) {
            for (s = 0; s < EN_LOOP_MAX; s++)
                loop_note[l][s] = song_note[kind][l][s], loop_vel[l][s] = song_vel[kind][l][s];
            if (frand() < evolve)
                song_mutate(l);
        }
    } else {                                     /* somewhere new: the motif developed, more with Evolve */
        for (l = 0; l < EN_NLOOP; l++)
            for (m = 1 + (int)(evolve * 3.0f); m > 0; m--)
                song_mutate(l);
        song_keep(kind);
    }
    for (l = 0; l < EN_NLOOP; l++)
        loop_publish(l);
    song_kind = (uint8_t)kind;
    arc_aim(SONG_ARC[song_sec % 6]);
    if (frand() < evolve)                        /* the rings may turn, between sections */
        drums_evolve();
    song_sec++;
    song_edge = 1;
}
static void song_reset(void)                     /* from the start: what grows now is the first section's */
{
    int k;
    for (k = 0; k < SONG_KINDS; k++)
        song_has[k] = 0;
    song_keep(0);
    song_kind = 0;
    song_left = (uint16_t)song_bars();
    song_sec = 0;
    arc_aim(SONG_ARC[0]);
    song_sec = 1;
    root_stepped = 0;
}
static void song_bar(void)                       /* a bar line, Song on */
{
    song_edge = 0;
    if (root_on) {                               /* with keys: each key's step a section */
        if (root_stepped) {
            root_stepped = 0;
            if (song_sec > 1 || root_i != 0)
                song_enter(root_i);
        }
        return;
    }
    if (song_left && --song_left)
        return;
    song_left = (uint16_t)song_bars();
    song_enter(song_sec % 2u ? 1 + (int)(song_sec / 2u) % 2 : 0);   /* A B A C A B ... */
}
static int song_ending(void) { return root_on ? root_left == 2 : song_left == 2; }

static void loop_pass(int l)                    /* the end of a pass: grow, prune, evolve */
{
    int c = loop_count(l), t = loop_target(), s;
    for (; c < t; c++)                          /* Density: to its count by the end of a pass */
        loop_add_song(l);
    for (; c > t; c--) {                        /* fewer: the passing notes go first */
        if ((s = loop_pick_passing(l)) < 0 && (s = loop_pick(l)) < 0)
            break;
        loop_note[l][s] = -1;
    }
    if (passing() <= 0.0f && (s = loop_pick_passing(l)) >= 0)   /* Density turned low: one a pass to the chord */
        loop_note[l][s] = (int8_t)rand_offset();
    loop_rest[l] = frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    if (!par[P_SONG] && frand() < evolve * 0.7f && (s = loop_pick(l)) >= 0) {   /* (Song: at a section's end) */
        if (frand() < 0.5f) {
            loop_note[l][s] = (int8_t)rand_offset();             /* another note */
        } else {
            int d = irand(EN_LOOP_LEN[l]);                       /* the same note, elsewhere */
            if (loop_note[l][d] < 0) {
                loop_note[l][d] = loop_note[l][s];
                loop_vel[l][d] = loop_vel[l][s];
                loop_note[l][s] = -1;
            }
        }
    }
    loop_publish(l);
}

/* each loop its own bell: index, decay and ratio a little apart, so the four can be told apart */
static const float LOOP_IDX[EN_NLOOP] = {1.0f, 0.55f, 1.5f, 0.8f}, LOOP_DEC[EN_NLOOP] = {1.0f, 1.35f, 0.75f, 1.6f};
static const int8_t LOOP_RAT[EN_NLOOP] = {0, 0, 1, -1};

/* the bells' colours: the same two operators, set differently. ratio: the modulator's (0: Tone's, as the Bell);
 * index: idx0 + idx1 Tone^2; it fades over isec0 + isec1 Tone (0: it holds); ring: the decay's length, times this;
 * arate: the strike (0: Tone's, as the Bell); gain: to sit level with the Bell, measured (long-run RMS through the
 * echo and reverb; Strings 3 dB over, for its slow swell) */
static const struct {
    float ratio, idx0, idx1, isec0, isec1, ring, arate, gain;
} BCOLOR[EN_NBCOLOR] = {
    {0.0f, 0.15f, 6.5f, 0.35f, 2.6f, 1.0f, 0.0f, 1.0f},          /* Bell */
    {1.0f, 0.05f, 1.4f, 1.0f, 2.0f, 1.15f, 0.00025f, 1.1f},      /* Soft: round, no strike */
    {2.0f, 0.55f, 2.0f, 0.0f, 0.0f, 1.7f, 0.0006f, 0.62f},       /* Wood: odd harmonics, hollow, holding */
    {1.0f, 0.7f, 1.6f, 6.0f, 6.0f, 2.2f, 0.00007f, 1.4f},        /* Strings: a slow swell that hangs */
    {1.0f, 1.3f, 3.0f, 0.22f, 0.5f, 0.95f, 0.008f, 1.05f},        /* Piano: a warm strike that mellows */
    {1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.001f, 1.0f},          /* Synth: (not a bell: its own voice, below) */
};

/* Lead: a solo synth. With the bells' colour on Lead, their notes go to it instead: two voices taking turns, so
 * each note starts clean on its own pitch while the one before trails off. Each: two saws a hair apart through a
 * low-pass that opens as it sounds, a vibrato that comes in late, into the echo and the reverb. A note holds a while,
 * or until the next comes, then lets go slowly, darkening as it trails off, unlike a bell. Bell tone is its
 * brightness. */
#define LEAD_COLOUR 5
typedef struct {
    float ph[2], inc, env, vel, vt, s[2], c[4], tgt, k;
    int32_t hold;
    uint8_t on, note;
} lead_v;
static struct {
    lead_v v[2];
    int32_t wait;
    uint8_t cur, pend, pnote, pvel, midi;
} lead;

static void lead_start(int n, float vel)
{
    lead_v *w;
    if (lead.midi)
        midi(0x81, lead.midi, 0);
    lead.v[lead.cur].hold = 0;                  /* the last note lets go */
    lead.cur ^= 1u;
    w = &lead.v[lead.cur];
    w->note = (uint8_t)n;
    w->inc = mtof((float)n) * (1.0f / EN_SR);   /* on its pitch at once: no glide */
    w->k = w->inc;
    w->vel = vel;
    w->on = 1;
    w->hold = (int32_t)(1.4f * EN_SR);
    w->vt = 0.0f;
    lead.midi = (uint8_t)n;
    midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void lead_note(int n, float vel, int32_t wait)
{
    if (lead.pend) {                            /* one waiting already: it goes now */
        lead.pend = 0;
        lead_start(lead.pnote, (float)lead.pvel * (1.0f / 110.0f));
    }
    if (wait > 0) {
        lead.pend = 1;
        lead.pnote = (uint8_t)n;
        lead.pvel = (uint8_t)(vel * 110.0f);
        lead.wait = wait;
    } else {
        lead_start(n, vel);
    }
}

static void bell_voice(int l, int n, float vel, int32_t wait)
{
    int i, best = 0, ri;
    float lo = 9.0f, t60, p;
    bell_t *v;
    const int c = par[P_BCOLOR];
    if (c == LEAD_COLOUR) {                     /* the solo synth takes it */
        lead_note(n, vel, wait);
        en_loop_glow[l] = 1.0f;
        return;
    }
    for (i = 0; i < NBELL; i++) {               /* a free voice, or the quietest */
        float a = bell[i].on ? bell[i].amp : -1.0f;
        if (a < lo) {
            lo = a;
            best = i;
        }
    }
    v = &bell[best];
    if (v->on)
        midi(0x81, v->note, 0);
    v->on = 1;
    v->note = (uint8_t)n;
    v->loop = (uint8_t)l;
    v->cph = v->mph = 0.0f;
    v->cinc = mtof((float)n) * (1.0f / EN_SR);
    ri = par[P_BTONE] * 5 / 101 + LOOP_RAT[l];
    v->minc = v->cinc * (BCOLOR[c].ratio > 0.0f ? BCOLOR[c].ratio : RATIO[ri < 0 ? 0 : ri > 4 ? 4 : ri]);
    v->amp = vel * BCOLOR[c].gain;
    t60 = 4.2f - (float)(n - 60) * 0.06f;       /* the low bells ring longer */
    t60 = fm_clampf(t60, 1.6f, 5.0f) * LOOP_DEC[l] * bell_dec * BCOLOR[c].ring;
    v->adec = fm_expf(-6.9f / (t60 * EN_SR));
    /* Bell tone: 0 a round, nearly pure tone; 100 a long metallic clang (index in radians, as turns) */
    v->idx = (BCOLOR[c].idx0 + BCOLOR[c].idx1 * btone * btone) * LOOP_IDX[l] * (0.6f + 0.4f * vel) * (1.0f / FM_TWO_PI);
    v->idec = BCOLOR[c].isec0 > 0.0f ? fm_expf(-6.9f / ((BCOLOR[c].isec0 + BCOLOR[c].isec1 * btone) * EN_SR)) : 1.0f;
    v->arate = BCOLOR[c].arate > 0.0f ? BCOLOR[c].arate : 0.0004f + 0.0056f * fm_minf(btone * 2.0f, 1.0f);
    v->att = 0.0f;
    p = (LOOP_PAN[l] + 1.0f) * (FM_PI / 4.0f);
    v->gl = fm_cosf(p);
    v->gr = fm_sinf(p);
    v->wait = wait;
    en_loop_glow[l] = 1.0f;
    v->mvel = 0;
    if (bell_late && wait > 0)
        v->mvel = (uint8_t)(vel * 110.0f);
    else
        midi(0x91, (uint32_t)n, (uint32_t)(vel * 110.0f));
}

static void bell_ring(int l, int slot)
{
    int n = bell_note(l, loop_note[l][slot]);
    float vel = (float)loop_vel[l][slot] * 0.01f;
    int32_t wait = irand(2600);                 /* a human hand: up to 60 ms late */
    bell_voice(l, n, vel, wait);
    if (n + 12 <= 103 && frand() < 0.08f + 0.12f * fm_maxf(arcm, 0.0f))
        bell_voice(l, n + 12, vel * 0.4f, wait + 300 + irand(2000));   /* now and then, its octave */
}

/* -------------------------------------------------------------- the arp --- */
/* In place of the loops' notes (they keep turning, silent, for when it's off): the chord's tones over Range, in
 * the Style's order at the Rate. Density picks the steps that sound, of 16, as Hits does the pulse's; the arc
 * thins them and Evolve turns the pattern and rests it. A step that doesn't sound doesn't move the order on.
 * Poly: the steps fall so many in the time of so many Rate steps, against the grid and locked to it. Each note
 * takes the chord as it's struck: chords move on bar lines, and a step in a sixteenth is struck within it. */
static uint32_t sixteenth;
static float swing, step_len;                   /* (the pulse's: below) */
static uint8_t arp_acc, arp_pos, arp_rot, arp_rest, arp_last;   /* arp_acc: to the next step, in 1/num sixteenths */
static uint16_t arp_seq;
static uint32_t arp_pat;

static void arp_reset(void)
{
    arp_acc = arp_pos = arp_rot = arp_rest = 0;
    arp_seq = 0;
}

static int arp_list(int *out)                   /* its notes, ascending, from the chord's root at middle C or above */
{
    int off[4], i, j, o, n = 0, r = 60 + chord_pcs[0] + 12 * par[P_BOCT];
    for (i = 0; i < chord_nt; i++) {
        int x = (chord_pcs[i] - chord_pcs[0] + 12) % 12;
        for (j = i; j > 0 && off[j - 1] > x; j--)
            off[j] = off[j - 1];
        off[j] = x;
    }
    for (o = 0; o < par[P_ARANGE]; o++)
        for (i = 0; i < chord_nt; i++)
            if (r + off[i] + 12 * o <= 103)
                out[n++] = r + off[i] + 12 * o;
    return n;
}

static int arp_index(int n)                     /* where the Style is, of n notes */
{
    const int s = arp_seq, p = n > 1 ? 2 * n - 2 : 1, tri = s % p < n ? s % p : p - s % p;
    int x = s % n, i;
    switch (par[P_ARP]) {
    case 2: return n - 1 - x;                   /* Down */
    case 3: return tri;                         /* Up/Dn: the ends once */
    case 4: return n - 1 - tri;                 /* Dn/Up */
    case 5:                                     /* Random: never the same note twice running */
        i = irand(n);
        return n > 1 && i == arp_last ? (i + 1) % n : i;
    case 6: return x & 1 ? n - 1 - x / 2 : x / 2;   /* Converge: in from both ends */
    case 7:                                     /* Diverge: Converge backwards */
        x = n - 1 - x;
        return x & 1 ? n - 1 - x / 2 : x / 2;
    default: return x;                          /* Up */
    }
}

static void arp_note(int32_t wait)              /* a step of the arp, wait samples from now */
{
    int list[12], n, i, pos;
    if (arp_pos == 0) {                         /* its pattern: Density, swelled and thinned by the arc */
        float k = (float)par[P_DENSITY] * 0.16f * (1.0f + 0.8f * arcm);
        if ((!par[P_SONG] || song_edge) && frand() < evolve * 0.25f)
            arp_rot = (uint8_t)irand(16);
        arp_pat = en_euclid((int)(fm_clampf(k, 0.0f, 16.0f) + frand()), 16, arp_rot);
        arp_rest = frand() < evolve * 0.15f + 0.15f * fm_maxf(-arcm, 0.0f);
    }
    pos = arp_pos;
    arp_pos = (arp_pos + 1u) & 15u;
    if (!(arp_pat >> pos & 1u) || arp_rest || (n = arp_list(list)) == 0)
        return;
    i = arp_index(n);
    arp_last = (uint8_t)i;
    bell_dec = ARATE_DEC[par[P_ARATE]];         /* each step a loop's shade of bell, and its place */
    bell_late = 1;
    bell_voice(arp_seq % EN_NLOOP, list[i], pos == 0 ? 0.9f : 0.55f + 0.25f * frand(), wait + irand(300));
    bell_late = 0;
    bell_dec = 1.0f;
    arp_seq++;
}

static void arp_step(void)                      /* a sixteenth: the steps that fall in it */
{
    const int num = APOLY_NUM[par[P_APOLY]], every = ARATE_STEPS[par[P_ARATE]] * APOLY_DEN[par[P_APOLY]];
    while (arp_acc < num) {
        /* on the grid, Swing moves the off sixteenths; off it, the rhythm is already its own */
        arp_note(par[P_APOLY] ? (int32_t)((float)arp_acc / (float)num * step_len)
                              : sixteenth & 1u ? (int32_t)(swing * step_len) : 0);
        arp_acc = (uint8_t)(arp_acc + every);
    }
    arp_acc = (uint8_t)(arp_acc - num);
}

/* ------------------------------------------------------- the bass's notes --- */
/* Bass notes above 0: a locked speed, 1, 2, 4 or 8 even hits a bar. Nothing else moves it: not the arc, not Evolve.
 * The bar's first is the root; then, as Notes goes up, the fifth, the octave, the third and the seventh, in a
 * figure that Evolve now and then changes (which note, never when). The chord's tones only, so it follows the
 * slate and Drift. */
static uint32_t bass_pat;
static uint8_t bass_i, bass_k;

static void bass_pattern(void)
{
    static const uint16_t EVEN[4] = {0x0001u, 0x0101u, 0x1111u, 0x5555u};
    int i = par[P_BASSN] >= 100 ? 3 : par[P_BASSN] / 25;
    bass_k = (uint8_t)(1u << i);
    bass_pat = EVEN[i];
}

static void bass_sound(int n)
{
    /* it rings until about the next hit: longer the fewer they are */
    const float pl = (float)par[P_BPLUCK] * 0.01f;  /* Pluck: 0 hums on to the next note, 100 rings out as plucked */
    float t60 = fm_clampf(24.0f / (float)bass_k * step_len * (1.0f / EN_SR), 0.3f, 4.0f) * pl + 30.0f * (1.0f - pl);
    if (en_playing) {
        midi(0x82, bass.midi_note, 0);
        midi(0x92, (uint32_t)n, 80);
    }
    bass.midi_note = (uint8_t)n;
    bass.target = (float)n;
    bass.env = 1.0f;
    bass.edec = fm_expf(-6.9f / (t60 * EN_SR));
}

static void bass_step(void)                     /* a sixteenth */
{
    int pos = (int)(sixteenth & 15u), up[5], m = 1, j, r, n;
    int32_t w;
    if (pos == 0) {
        bass_pattern();
        bass_i = 0;
    }
    if (!(bass_pat >> pos & 1u) || bass.rest)
        return;
    r = 36 + chord_pcs[0] % 12 + 12 * par[P_BASSOCT];
    up[0] = 0;                                  /* above the root: its figure's notes, as Notes opens them */
    if (par[P_BASSN] >= 25 && chord_nt >= 3)
        up[m++] = (chord_pcs[2] - chord_pcs[0] + 12) % 12;
    if (par[P_BASSN] >= 50)
        up[m++] = 12;
    if (par[P_BASSN] >= 75) {
        up[m++] = (chord_pcs[1] - chord_pcs[0] + 12) % 12;
        if (chord_nt >= 4)
            up[m++] = (chord_pcs[3] - chord_pcs[0] + 12) % 12;
    }
    j = bass_i % m;
    if (bass_i && (!par[P_SONG] || song_edge) && frand() < evolve * 0.25f)
        j = irand(m);
    n = r + up[j];
    bass_i++;
    w = sixteenth & 1u ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        bass.wait = w;
        bass.pend = (uint8_t)n;
    } else {
        bass_sound(n);
    }
}

/* ---------------------------------------------------------------- sparkle --- */
/* Little flourishes on the bells, in waves: for Length it swells and wanes, then rests for Often, each a little
 * varied. Sparkle sets how much at the peak; the arc leans on it. A run of the scale that ends on a chord tone, a
 * grace note into one, the pad's note high up, and a rising chord when one comes. Quiet, short, sharing the bells'
 * voices (it takes the quietest), of the chord and its scale only. */
static float sp_env;
static uint32_t sp_left = (uint32_t)(1.5f * EN_SR), sp_total;
static uint8_t sp_in, sp_on, sp_busy, sp_shape;
static int sp_deg = -1;

static void spark_env(uint32_t n)               /* the slow part: where the wave is */
{
    if (sp_left > n) {
        sp_left -= n;
    } else if (sp_in) {
        sp_in = 0;
        sp_left = (uint32_t)(spoft_secs(par[P_SPOFT]) * (0.7f + 0.6f * frand()) * EN_SR);
    } else {
        sp_in = 1;
        sp_total = sp_left = (uint32_t)(splen_secs(par[P_SPLEN]) * (0.8f + 0.4f * frand()) * EN_SR);
    }
    if (sp_in) {
        float s = fm_sinf(FM_PI * (1.0f - (float)sp_left / (float)sp_total));
        sp_env = s * s;
    } else {
        sp_env = 0.0f;
    }
    en_spark = sp_env;
}

static int spark_tone(void)                     /* a chord tone, as an offset an octave above the chord's degree */
{
    const int m = shape == 1 || shape == 2 || shape == 3 ? 3 : 4;
    return nsteps + (shape ? SHAPE_OFF[shape][irand(m)] : 2 * irand(chord_nt));
}

static void spark_bell(int l, int n, float vel, float wait)
{
    bell_voice(l, n, vel, (int32_t)(wait * step_len) + 1 + irand(200));
}

static void spark_run(float vel)                /* three to five notes of the scale, ending on a chord tone */
{
    const int n = 3 + irand(3), e = spark_tone(), up = frand() < 0.6f, l = irand(EN_NLOOP);
    int i;
    for (i = 0; i < n; i++) {
        int off = up ? e - (n - 1) + i : e + (n - 1) - i;
        while (off < 0)
            off += nsteps;
        spark_bell(l, bell_note(2, off), vel * (0.65f + 0.35f * (float)i / (float)(n - 1)), (float)i * 0.5f);
    }
    sp_busy = (uint8_t)(n / 2 + 1);
}

static void spark_grace(float vel)              /* the scale's note beside a chord tone, then the tone */
{
    const int e = spark_tone(), l = irand(EN_NLOOP);
    spark_bell(l, bell_note(2, e + (frand() < 0.5f ? -1 : 1)), vel * 0.5f, 0.0f);
    spark_bell(l, bell_note(2, e), vel, 0.24f);
    sp_busy = 1;
}

static void spark_high(float vel)               /* a tone the pad sings, up high: by the garden's dice, not its breath */
{
    int n = 60 + chord_pcs[chord_nt > 0 ? irand(chord_nt) : 0];
    while (n < 79)
        n += 12;
    while (n > 103)
        n -= 12;
    spark_bell(irand(EN_NLOOP), n, vel, 0.0f);
    sp_busy = 2;
}

static void spark_rise(float vel)               /* a new chord: its tones, rising */
{
    int list[12], n = arp_list(list), i, m = n < chord_nt ? n : chord_nt;
    for (i = 0; i < m; i++) {
        int note = list[i] + 12;
        while (note > 103)
            note -= 12;
        spark_bell(i % EN_NLOOP, note, vel * (0.7f + 0.1f * (float)i), (float)i * 0.4f);
    }
    sp_busy = 2;
}

static void spark_tick(void)                    /* a sixteenth */
{
    const int changed = deg != sp_deg || shape != sp_shape;
    const float amt = (float)par[P_SPARK] * 0.01f, vel = (0.3f + 0.45f * sp_env) * (0.6f + 0.4f * amt);
    sp_deg = deg;
    sp_shape = (uint8_t)shape;
    if (sp_busy && !changed) {
        sp_busy--;
        return;
    }
    bell_late = 1;                              /* the MIDI notes with the sound */
    bell_dec = 0.35f;                           /* and short */
    if (changed && sp_env > 0.15f) {
        spark_rise(vel);
    } else if (frand() < amt * sp_env * 0.35f * (1.0f + 0.5f * arcm)) {
        const float r = frand();
        if (r < 0.45f)
            spark_run(vel);
        else if (r < 0.7f)
            spark_grace(vel);
        else
            spark_high(vel);
    }
    bell_late = 0;
    bell_dec = 1.0f;
}

/* ---------------------------------------------------------------- pulse --- */
/* (each ring's turn at home: the Rings' Turn knobs, 0, 2 and 3 in every recipe) */
static int drum_k[EN_NDRUM], drum_kb[EN_NDRUM], drum_rot[EN_NDRUM], pulse_rest;   /* rest: bars left */

static void drum_publish(int d)
{
    drum_pat[d] = en_euclid(drum_k[d], EN_DRUM_N[d], drum_rot[d]);
    en_drum_pat[d] = drum_pat[d];
}

/* a ring's hits on Auto (pure: the knobs ask it too). The thump's from Hits. The hats': how much hat, by Hats: 0 none;
 * up to 50 a share of what Hits gives them; 50 just that; on up to 100, toward a full ring, whatever Hits says */
int en_ring_auto(int d, int hits, int hats)
{
    static const uint8_t FULL[EN_NDRUM] = {0, 10, 5};
    const int home = (hits * DRUM_KMAX[d % EN_NDRUM] + 50) / 100;
    d %= EN_NDRUM;
    if (!d)
        return home;
    if (hats <= 50)
        return (home * hats + 49) / 50;
    return home + ((FULL[d] > home ? FULL[d] - home : 0) * (hats - 50) + 25) / 50;
}

static void drums_pattern(void)                 /* the patterns' home: the Rings' counts and turns, or Auto */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        int k = par[P_RHITS0 + d] ? par[P_RHITS0 + d] - 1 : en_ring_auto(d, par[P_HITS], par[P_HATS]);
        drum_kb[d] = drum_k[d] = k > EN_DRUM_N[d] ? EN_DRUM_N[d] : k;
        drum_rot[d] = par[P_RTURN0 + d] % EN_DRUM_N[d];
        drum_publish(d);
    }
}

static void drums_evolve(void)                  /* a bar: a pattern turns, or gains or loses a hit */
{
    int d = irand(EN_NDRUM), n = EN_DRUM_N[d], r0, k0;
    if (!drum_kb[d])
        return;
    r0 = drum_rot[d], k0 = drum_k[d];
    if (frand() < 0.5f) {
        drum_rot[d] = (drum_rot[d] + (frand() < 0.5f ? 1 : n - 1)) % n;
    } else {
        int k = drum_k[d] + (frand() < 0.5f ? 1 : -1);
        if (k >= drum_kb[d] - 1 && k <= drum_kb[d] + 1 && k >= 1 && k <= n)
            drum_k[d] = k;
    }
    if (par[P_RMOVES]) {                        /* Still: the dice rolled all the same, so the rest goes as it would */
        drum_rot[d] = r0, drum_k[d] = k0;
        return;
    }
    drum_publish(d);
}

static void drum_fire(int d, float vel)
{
    drum_t *v = &drum[d];
    v->on = 1;
    v->amp = vel;
    v->att = 0.0f;
    switch (d) {
    case 0: {                                   /* thump: the chord's root, low, falling into place */
        float f = mtof((float)(36 + chord_pcs[0]));
        v->note = DRUM_GM[0];
        v->inc = f * 2.2f * (1.0f / EN_SR);
        v->inc_end = f * (1.0f / EN_SR);
        v->adec = fm_expf(-6.9f / (0.45f * EN_SR));
        break;
    }
    case 1:                                     /* closed hat: short; it chokes the open one */
        v->note = DRUM_GM[1];
        v->adec = fm_expf(-6.9f / (hat_closed * EN_SR));
        if (vel >= 0.35f || par[P_HATS] <= 50)   /* a ghost note leaves it ringing */
            drum[2].adec = fm_expf(-6.9f / (hat_choke * EN_SR));
        break;
    default:                                    /* open hat: a long, soft "tss" */
        v->note = DRUM_GM[2];
        v->adec = fm_expf(-6.9f / (hat_open * EN_SR));
        break;
    }
    en_drum_glow[d] = 1.0f;
    midi(0x99, v->note, (uint32_t)(vel * 100.0f));
    midi(0x89, v->note, 0);
}

static void drum_hit(int d, float vel, int off)  /* off: an off sixteenth, which Swing delays */
{
    int32_t w = off ? (int32_t)(swing * step_len) : 0;
    if (w > 0) {
        drum[d].wait = w;
        drum[d].pend = vel;
    } else {
        drum_fire(d, vel);
    }
}

static int32_t drums_wait(void)                 /* samples to the next hit that waits; 0: none */
{
    int32_t m = 0;
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (!m || drum[d].wait < m))
            m = drum[d].wait;
    if (bass.wait > 0 && (!m || bass.wait < m))
        m = bass.wait;
    return m;
}

static void drums_wait_step(int32_t k)          /* k samples went by: hits whose wait is over */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        if (drum[d].wait > 0 && (drum[d].wait -= k) <= 0) {
            drum[d].wait = 0;
            drum_fire(d, drum[d].pend);
        }
    if (bass.wait > 0 && (bass.wait -= k) <= 0) {
        bass.wait = 0;
        bass_sound(bass.pend);
    }
}

/* ---------------------------------------------------------------- swoosh --- */
/* Falling stars of noise. For Length a pass swells and wanes on a gentle curve, its filter falling from bright to
 * dark as it crosses the stereo field, into the echo and the reverb; then a rest for Often, each a little varied.
 * Tone goes from white to brown. No pitch, so nothing to clash with. Its own noise and its own dice, so that the rest
 * of the garden goes as it would without it; and nothing is computed between passes. */
static uint32_t sw_left, sw_total, sw_rng;
static uint8_t sw_in;
static int8_t sw_side;
static float sw_want, sw_lv, sw_play, sw_height, sw_width;
static float sw_f, sw_low, sw_band, sw_brown, sw_mix, sw_gl, sw_gr, sw_tl, sw_tr;

static float sw_dice(void)
{
    uint32_t r = sw_rng;
    r ^= r << 13, r ^= r >> 17, r ^= r << 5;
    sw_rng = r;
    return (float)(r >> 8) * (1.0f / 16777216.0f);
}

static void swoosh_env(uint32_t n)              /* the slow part: where the pass is */
{
    const float k = (float)n * (1.0f / EN_SR);
    float a = 0.0f, pan = 0.0f;
    sw_play += ((en_playing ? 1.0f : 0.0f) - sw_play) * fm_minf(k * 0.5f, 1.0f);
    sw_lv += (sw_want - sw_lv) * fm_minf(k * 20.0f, 1.0f);
    if (en_playing && par[P_SWOOSH]) {
        if (sw_left > n) {
            sw_left -= n;
        } else if (sw_in) {
            sw_in = 0;
            sw_left = (uint32_t)(spoft_secs(par[P_SWOFT]) * (0.7f + 0.6f * sw_dice()) * EN_SR);
        } else {
            sw_in = 1;
            sw_total = sw_left = (uint32_t)(swlen_secs(par[P_SWLEN]) * (0.8f + 0.4f * sw_dice()) * EN_SR);
            sw_side = sw_dice() < 0.5f ? -1 : 1;
            sw_height = 0.7f + 0.3f * sw_dice();
            sw_width = 0.6f + 0.35f * sw_dice();
        }
    }
    if (sw_in) {
        const float t = (float)par[P_SWTONE] * 0.01f, p = 1.0f - (float)sw_left / (float)sw_total;
        const float q = p * (1.4f - 0.4f * p);  /* it reaches its height a little before the middle */
        const float s = fm_sinf(FM_PI * q), oct = (1.0f - t) * 2.7f + 2.7f * sw_height * (1.0f - q);
        a = s * s;
        pan = (float)sw_side * sw_width * (2.0f * q - 1.0f);
        sw_f = 2.0f * fm_sinf(FM_PI * 150.0f * fm_exp2f(oct) * (1.0f / EN_SR));   /* 150 Hz .. 6.3 kHz */
        sw_mix = t;
    }
    a *= sw_lv * sw_play;
    pan = 0.5f + 0.5f * pan;
    sw_tl = a * sine((1.0f - pan) * 0.25f);
    sw_tr = a * sine(pan * 0.25f);
}

/* ----------------------------------------------------------------- night --- */
/* The Night page: crickets, spring peepers, water and distant thunder, synthesized: impressions, not recordings.
 * Crickets: a few, each chirping its own few pulses of a high tone, left and right. Peepers: little rising whistles,
 * each frog about once a second, near and far; the knob calls up more of them. Water, one journey: a creek's
 * babble (bands of noise wandering, no pitched bubbles), then rain from a drizzle to a downpour (drops are ticks of
 * noise), then the sea's waves, each crossfading into the next. Thunder: now and then a far roll, slow overlapping
 * swells of deep noise, much of it in the reverb, never a clap; turned up, more often and louder. Each follows PLAY
 * as the music does and sits in its reverb; their own dice, so the garden grows as it would without them; and
 * nothing is computed for what's off. They belong to the player, not the piece (the weather). */
#define NT_NCR 3
#define NT_NPE 8
#define NT_NBL 6
#define NT_NTB 8
#define NT_PULSE 706u                            /* a cricket's pulse, 16 ms; a pulse every 30 ms */
#define NT_STEP 1323u
/* how loud each, at 100 (set by ear and measured against the pad: see the tests) */
#define NT_G_CRICKET 0.24f
#define NT_G_PEEP 0.22f
#define NT_G_DROP 0.5f
#define NT_G_RAIN 0.18f
#define NT_G_CREEK 0.2f
#define NT_G_WAVES 0.9f
#define NT_G_THUNDER 0.9f
static uint32_t nt_rng = 0x1B873593u;
static float nt_play, nt_lv[4], nt_want[4];      /* crickets, peepers, water, thunder: as sounding, as wanted */
static struct { float ph, inc, gl, gr; uint32_t t, period, pt; uint8_t k, np; } nt_cr[NT_NCR];
static struct { float ph, inc0, dinc, gl, gr; uint32_t t, period, dur; } nt_pe[NT_NPE];
static struct { float amp, dec, gl, gr; } nt_bl[NT_NBL];   /* drops: ticks of noise */
static float nt_wx;                              /* where on the water's journey: 0 creek .. 3 waves */
static float nt_lp[4], nt_brown, nt_cf, nt_rain_a, nt_rain_g, nt_low_g, nt_creek_g, nt_tick;
/* the creek's babble: four bands of noise (two a side), each wandering in pitch and swelling and sinking a few
 * dozen times a second, as water does over stones */
static float nt_ck_f[4], nt_ck_ft[4], nt_ck_c[4], nt_ck_g[4], nt_ck_gt[4], nt_ck_lo[4], nt_ck_bd[4];
static float nt_wv_ph, nt_wv_len, nt_wv_pan, nt_wv_e, nt_wv_tl, nt_wv_tr, nt_wv_gl, nt_wv_gr, nt_wv_a, nt_wv_foam;
static float nt_th_t, nt_th_wait, nt_th_pan, nt_th_e, nt_th_g, nt_th_gl, nt_th_gr, nt_th_fl, nt_th_flt;
static float nt_th_lp[6];                         /* each side: two poles round 90 Hz, and the body round 220 */
static int nt_th_v;
static float nt_th_at[NT_NTB], nt_th_amp[NT_NTB], nt_th_rise[NT_NTB], nt_th_fall[NT_NTB];

static float nt_dice(void)
{
    uint32_t r = nt_rng;
    r ^= r << 13, r ^= r >> 17, r ^= r << 5;
    nt_rng = r;
    return (float)(r >> 8) * (1.0f / 16777216.0f);
}

static void nt_pan(float p, float *gl, float *gr)   /* p -1 left .. +1 right, equal power */
{
    p = 0.5f + 0.5f * p;
    *gl = sine((1.0f - p) * 0.25f);
    *gr = sine(p * 0.25f);
}

static void night_thunder_next(float soon)       /* the next roll: in soon seconds or, from 0, by Thunder */
{
    const float v = (float)par[P_THUNDER] * 0.01f;
    nt_th_wait = soon > 0.0f ? soon : (200.0f * fm_exp2f(-3.0f * v)) * (0.5f + nt_dice());   /* 3 min .. 25 s */
    nt_th_t = -1.0f;
}

static void night_thunder_start(void)            /* a roll, far off: up to eight slow swells, overlapping, over 10-20 s */
{
    int i;
    float at = 0.0f;
    nt_th_t = 0.0f;
    nt_th_pan = (nt_dice() * 2.0f - 1.0f) * 0.35f;
    nt_pan(nt_th_pan, &nt_th_gl, &nt_th_gr);
    for (i = 0; i < NT_NTB; i++) {
        const int on = i < 4 || nt_dice() < 0.55f;
        nt_th_at[i] = at;
        nt_th_amp[i] = on ? (i ? 0.3f + 0.7f * nt_dice() : 0.7f + 0.3f * nt_dice()) : 0.0f;
        nt_th_rise[i] = 0.8f + 1.7f * nt_dice();
        nt_th_fall[i] = 2.5f + 3.5f * nt_dice();
        at += 0.3f + 1.1f * nt_dice();
    }
}

static void night_env(uint32_t n)                /* the slow part: levels, who sings, the water's place, the rolls */
{
    const float k = (float)n * (1.0f / EN_SR);
    int i;
    nt_play += ((en_playing ? 1.0f : 0.0f) - nt_play) * fm_minf(k * 0.5f, 1.0f);
    for (i = 0; i < 4; i++)
        nt_lv[i] += (nt_want[i] - nt_lv[i]) * fm_minf(k * 4.0f, 1.0f);
    if (nt_lv[2] > 1e-5f) {                      /* water */
        const float x = nt_wx, rain = fm_clampf((x - 0.4f) * 1.6f, 0.0f, 1.0f) * fm_clampf((2.9f - x) * 1.6f, 0.0f, 1.0f);
        const float heavy = fm_clampf(x - 1.0f, 0.0f, 1.0f), creek = fm_clampf(1.4f - x, 0.0f, 1.0f);
        const float waves = fm_clampf((x - 2.3f) * 1.6f, 0.0f, 1.0f), g = nt_lv[2] * nt_play;
        float rate, e, p;
        nt_cf = fm_sinf(FM_PI * (2600.0f - 1500.0f * heavy) * (1.0f / EN_SR)) * 2.0f;   /* the hiss: from 2.6 kHz, lower when heavy */
        nt_rain_a = nt_cf * 0.5f;
        nt_rain_g = g * (rain * (0.35f + 0.65f * heavy) * 0.9f + creek * 0.12f);   /* the creek's own quiet hiss too */
        nt_low_g = heavy * 0.7f;                 /* a downpour's roar underneath */
        nt_creek_g = g * creek;
        if (nt_creek_g > 1e-5f)
            for (i = 0; i < 4; i++) {            /* the babble: each band wanders and swells, a few dozen times a second */
                static const float MID[2] = {750.0f, 2100.0f};
                if (nt_dice() < k * 22.0f) {
                    nt_ck_ft[i] = MID[i & 1] * fm_exp2f(nt_dice() * 1.3f - 0.65f);
                    nt_ck_gt[i] = nt_dice();
                    nt_ck_gt[i] *= nt_ck_gt[i];
                }
                nt_ck_f[i] += (nt_ck_ft[i] - nt_ck_f[i]) * fm_minf(k * 30.0f, 1.0f);
                nt_ck_g[i] += (nt_ck_gt[i] - nt_ck_g[i]) * fm_minf(k * 45.0f, 1.0f);
                nt_ck_c[i] = 2.0f * fm_sinf(FM_PI * nt_ck_f[i] * (1.0f / EN_SR));
            }
        rate = rain * (5.0f + 45.0f * heavy);    /* drops a second, ticking on the leaves */
        if (nt_dice() < rate * k) {
            for (i = 0; i < NT_NBL && nt_bl[i].amp > 1e-4f; i++)
                ;
            if (i < NT_NBL) {
                nt_bl[i].amp = g * rain * (0.15f + 0.35f * nt_dice());
                nt_bl[i].dec = fm_exp2f(-1.4427f / ((0.0015f + 0.0035f * nt_dice()) * EN_SR));
                nt_pan(nt_dice() * 1.8f - 0.9f, &nt_bl[i].gl, &nt_bl[i].gr);
            }
        }
        if (waves > 0.0f || nt_wv_gl + nt_wv_gr > 1e-6f) {   /* the sea: swell, crash, the wash going back */
            nt_wv_ph += k / nt_wv_len;
            if (nt_wv_ph >= 1.0f) {
                nt_wv_ph -= 1.0f;
                nt_wv_len = 8.0f + 4.0f * nt_dice();
                nt_wv_pan = (nt_dice() * 2.0f - 1.0f) * 0.35f;
            }
            p = nt_wv_ph;
            e = p < 0.4f ? 0.15f + 0.35f * (p * 2.5f) * (p * 2.5f)
                : p < 0.45f ? 0.5f + 10.0f * (p - 0.4f)
                : 0.12f + 0.88f * fm_expf(-(p - 0.45f) * 5.5f);
            nt_wv_e = e;
            nt_wv_a = 1.0f - fm_expf(-2.0f * FM_PI * (180.0f + 2600.0f * e * e) * (1.0f / EN_SR));
            nt_wv_foam = e * e * e;
            nt_pan(nt_wv_pan * (p < 0.45f ? 1.0f : 1.0f - 2.0f * (p - 0.45f)), &nt_wv_tl, &nt_wv_tr);
            nt_wv_tl *= g * waves * e * 1.1f;
            nt_wv_tr *= g * waves * e * 1.1f;
        }
    } else {
        nt_rain_g = nt_creek_g = 0.0f;
        nt_wv_tl = nt_wv_tr = 0.0f;
    }
    if (nt_want[3] > 0.0f && en_playing) {       /* thunder: wait, then a roll */
        if (nt_th_t < 0.0f) {
            nt_th_wait -= k;
            if (nt_th_wait <= 0.0f)
                night_thunder_start();
        }
    }
    if (nt_th_t >= 0.0f) {
        float e = 0.0f, end = 0.0f;
        nt_th_t += k;
        for (i = 0; i < NT_NTB; i++) {
            const float d = nt_th_t - nt_th_at[i];
            if (nt_th_amp[i] <= 0.0f)
                continue;
            if (d > 0.0f)
                e += nt_th_amp[i] * (d < nt_th_rise[i] ? d / nt_th_rise[i] : fm_expf(-(d - nt_th_rise[i]) / nt_th_fall[i]));
            end = fm_maxf(end, nt_th_at[i] + nt_th_rise[i] + 5.0f * nt_th_fall[i]);
        }
        if (nt_dice() < k * 7.0f)                /* the roll's grain: slow, uneven surges */
            nt_th_flt = 0.5f + nt_dice();
        nt_th_fl += (nt_th_flt - nt_th_fl) * fm_minf(k * 5.0f, 1.0f);
        nt_th_e = e * nt_th_fl;
        if (nt_th_t > end)
            night_thunder_next(0.0f);
    } else {
        nt_th_e = 0.0f;
    }
    nt_th_g = nt_lv[3] * nt_play * nt_th_e;
}

static void night_init(void)                     /* the creatures where they sit, all quiet */
{
    static const float CF[NT_NCR] = {4400.0f, 4750.0f, 4150.0f}, CP[NT_NCR] = {-0.65f, 0.7f, 0.1f}, CG[NT_NCR] = {1.0f, 0.8f, 0.55f};
    int i;
    nt_rng = 0x1B873593u;
    nt_play = nt_wx = 0.0f;
    for (i = 0; i < 4; i++)
        nt_lv[i] = nt_want[i] = 0.0f;
    for (i = 0; i < NT_NCR; i++) {
        nt_cr[i].ph = 0.0f;
        nt_cr[i].inc = CF[i] * (1.0f / EN_SR);
        nt_cr[i].np = (uint8_t)(i == 1 ? 3 : 4);
        nt_cr[i].k = nt_cr[i].np;
        nt_cr[i].t = nt_cr[i].pt = 0;
        nt_cr[i].period = (uint32_t)((0.1f + 0.5f * nt_dice()) * EN_SR);
        nt_pan(CP[i], &nt_cr[i].gl, &nt_cr[i].gr);
        nt_cr[i].gl *= CG[i];
        nt_cr[i].gr *= CG[i];
    }
    for (i = 0; i < NT_NPE; i++) {
        const float f = 2550.0f + 450.0f * nt_dice(), g = 0.35f + 0.65f * nt_dice();
        nt_pe[i].ph = 0.0f;
        nt_pe[i].dur = (uint32_t)((0.1f + 0.05f * nt_dice()) * EN_SR);
        nt_pe[i].inc0 = f * (1.0f / EN_SR);
        nt_pe[i].dinc = nt_pe[i].inc0 * 0.2f / (float)nt_pe[i].dur;   /* up about 20% through the peep */
        nt_pe[i].period = (uint32_t)((0.2f + 1.2f * nt_dice()) * EN_SR);
        nt_pe[i].t = nt_pe[i].dur;
        nt_pan(nt_dice() * 1.8f - 0.9f, &nt_pe[i].gl, &nt_pe[i].gr);
        nt_pe[i].gl *= g;
        nt_pe[i].gr *= g;
    }
    for (i = 0; i < NT_NBL; i++)
        nt_bl[i].amp = 0.0f;
    for (i = 0; i < 4; i++) {
        nt_lp[i] = nt_ck_lo[i] = nt_ck_bd[i] = nt_ck_g[i] = nt_ck_gt[i] = 0.0f;
        nt_ck_f[i] = nt_ck_ft[i] = (i & 1) ? 2100.0f : 750.0f;
        nt_ck_c[i] = 2.0f * fm_sinf(FM_PI * nt_ck_f[i] * (1.0f / EN_SR));
    }
    for (i = 0; i < 6; i++)
        nt_th_lp[i] = 0.0f;
    nt_brown = nt_rain_g = nt_creek_g = nt_low_g = nt_tick = 0.0f;
    nt_cf = nt_rain_a = 0.0f;
    nt_wv_ph = 0.0f;
    nt_wv_len = 10.0f;
    nt_wv_pan = nt_wv_e = nt_wv_tl = nt_wv_tr = nt_wv_gl = nt_wv_gr = nt_wv_a = nt_wv_foam = 0.0f;
    nt_th_e = nt_th_g = 0.0f;
    nt_th_fl = nt_th_flt = 1.0f;
    nt_th_gl = nt_th_gr = 0.7f;
    nt_th_t = -1.0f;
    nt_th_wait = 1e9f;
    nt_th_v = 0;
}

/* Hats above 50, a closed hat's sixteenth: ghost notes between the hits; near the top, a roll, a soft hit half a
 * step after this one and into the next */
static void hats_more(float pa)
{
    const float more = (float)(par[P_HATS] - 50) * 0.02f;
    const int hit = drum_pat[1] >> drum_pos[1] & 1u, off = (int)(sixteenth & 1u);
    if (hat_roll) {
        hat_roll = 0;
        if (!hit)
            drum_hit(1, 0.7f * pa, off);
    } else if (!hit) {
        if (frand() < more * 0.6f)
            drum_hit(1, (0.16f + 0.12f * frand()) * pa, off);
    } else if (!off && more > 0.5f && frand() < (more - 0.5f) * 0.3f) {
        drum[1].wait = (int32_t)(0.5f * step_len);
        drum[1].pend = 0.45f * pa;
        hat_roll = 1;
    }
}

/* the grid: one call a sixteenth (MIDI clock: 6 ticks) */
static void tick(void)
{
    int d, l;
    const float pa = fm_clampf(1.0f + 1.3f * arcm, 0.3f, 1.15f);   /* the arc: the pulse fades (never out) */
    if ((sixteenth & 15u) == 0u) {              /* a bar: Evolve's rests and turns */
        if (pulse_rest > 0)
            pulse_rest--;
        else if (par[P_SONG] ? song_ending() && frand() < evolve * 0.5f : frand() < evolve * 0.015f)
            pulse_rest = par[P_SONG] ? 2 : 2 + irand(3);   /* a short breath (Song: before a new section) */
        en_pulse_rest = (uint8_t)(pulse_rest > 0);
        if (!par[P_SONG] && frand() < evolve * 0.25f)   /* (Song: between sections) */
            drums_evolve();
    }
    if ((sixteenth & 15u) == 0u && !en_frozen) {   /* chords move on the bar: first, so what sounds on it is of the new chord */
        if (root_on && root_bar()) {
            /* the new step's first chord is in */
        } else if (slate_on) {
            slate_bar();
        } else if (change_pending) {
            change_pending = 0;
            chord_change();
        }
    }
    if ((sixteenth & 15u) == 0u && !en_frozen && par[P_SONG])
        song_bar();                              /* the song: a section's end, and its next */
    if (par[P_HATS] > 50 && !pulse_rest)
        hats_more(pa);
    for (d = 0; d < EN_NDRUM; d++) {
        if (drum_pat[d] >> drum_pos[d] & 1u && !pulse_rest)
            drum_hit(d, ((drum_pos[d] == 0 ? 0.95f : 0.6f) + 0.25f * frand()) * pa, (int)(sixteenth & 1u));
        en_drum_pos[d] = drum_pos[d];
        if (++drum_pos[d] >= EN_DRUM_N[d])
            drum_pos[d] = 0;
    }
    if (par[P_BASSN] > 0)
        bass_step();
    if (par[P_ARP])
        arp_step();
    if (par[P_SPARK])
        spark_tick();
    if ((sixteenth & 3u) == 0u) {               /* the loops step in quarter notes */
        for (l = 0; l < EN_NLOOP; l++) {
            int s = loop_pos[l];
            if (loop_note[l][s] >= 0 && !loop_rest[l] && !par[P_ARP])
                bell_ring(l, s);
            en_loop_pos[l] = (uint8_t)s;
            if (++loop_pos[l] >= EN_LOOP_LEN[l]) {
                loop_pos[l] = 0;
                loop_pass(l);
            }
        }
    }
    sixteenth++;
    en_bars = sixteenth;
}

/* ------------------------------------------------- reverb: Dattorro's plate --- */
/* J. Dattorro, "Effect Design, Part 1", JAES 1997 (as in FoMni): a predelay, four input diffusers,
 * then a figure-eight tank of two halves, each a modulated allpass, a delay, a damping lowpass, an
 * allpass and a delay, feeding the other half. Stereo out from seven taps a side. The paper's lengths
 * are for 29761 Hz; DS() scales them to 44.1 kHz. */
#define DS(n) (((n) * 14818 + 5000) / 10000)          /* 44100 / 29761 */
#define PRE_N 1323                                   /* 30 ms */
#define EXC DS(16)
enum { L_PRE, L_IN1, L_IN2, L_IN3, L_IN4, L_APL, L_D1L, L_AP2L, L_D2L, L_APR, L_D1R, L_AP2R, L_D2R, L_N };
static const int DL_LEN[L_N] = {PRE_N, DS(142), DS(107), DS(379), DS(277), DS(672) + EXC + 2, DS(4453), DS(1800),
                                DS(3720), DS(908) + EXC + 2, DS(4217), DS(2656), DS(3163)};
#define DL_TOTAL (PRE_N + DS(142) + DS(107) + DS(379) + DS(277) + DS(672) + DS(4453) + DS(1800) + DS(3720) + DS(908) + \
                  DS(4217) + DS(2656) + DS(3163) + 2 * (EXC + 2))
static float pl_buf[DL_TOTAL] EN_POOL;
typedef struct {
    float *b;
    int n, i;
} dl_t;
static dl_t dl[L_N];
static float pl_bw, pl_damp_l, pl_damp_r, pl_decay = 0.5f, pl_decay_t = 0.5f, pl_dd2 = 0.5f, lfo_s, lfo_c = 1.0f;

static inline float tap(const dl_t *d, int k)
{
    int j = d->i - k;
    return d->b[j < 0 ? j + d->n : j];
}
static inline void push(dl_t *d, float x)
{
    d->b[d->i] = x;
    if (++d->i >= d->n)
        d->i = 0;
}
static inline float allpass(dl_t *d, int len, float x, float g)
{
    float z = tap(d, len), v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}
static inline float allpass_mod(dl_t *d, float len, float x, float g)
{
    int k = (int)len;
    float f = len - (float)k, z = tap(d, k) + (tap(d, k + 1) - tap(d, k)) * f, v = x - g * z;
    push(d, fm_flush(v));
    return z + g * v;
}

static inline void reverb(float in, float *ol, float *or_)
{
    float x, a, b, fb_l = tap(&dl[L_D2R], DS(3163)), fb_r = tap(&dl[L_D2L], DS(3720));
    push(&dl[L_PRE], in);
    x = tap(&dl[L_PRE], PRE_N);
    pl_bw += 0.6f * (x - pl_bw);
    x = allpass(&dl[L_IN1], DS(142), pl_bw, 0.75f);
    x = allpass(&dl[L_IN2], DS(107), x, 0.75f);
    x = allpass(&dl[L_IN3], DS(379), x, 0.625f);
    x = allpass(&dl[L_IN4], DS(277), x, 0.625f);
    {
        float s = lfo_s + 0.0001f * lfo_c, c = lfo_c - 0.0001f * s;
        lfo_s = s;
        lfo_c = c;
    }
    a = allpass_mod(&dl[L_APL], (float)DS(672) + (float)EXC * lfo_s, x + pl_decay * fb_l, -0.7f);
    push(&dl[L_D1L], a);
    b = tap(&dl[L_D1L], DS(4453));
    pl_damp_l += 0.45f * (b - pl_damp_l);
    b = allpass(&dl[L_AP2L], DS(1800), pl_damp_l * pl_decay, pl_dd2);
    push(&dl[L_D2L], b);
    a = allpass_mod(&dl[L_APR], (float)DS(908) + (float)EXC * lfo_c, x + pl_decay * fb_r, -0.7f);
    push(&dl[L_D1R], a);
    b = tap(&dl[L_D1R], DS(4217));
    pl_damp_r += 0.45f * (b - pl_damp_r);
    b = allpass(&dl[L_AP2R], DS(2656), pl_damp_r * pl_decay, pl_dd2);
    push(&dl[L_D2R], b);
    *ol = 0.6f * (tap(&dl[L_D1R], DS(266)) + tap(&dl[L_D1R], DS(2974)) - tap(&dl[L_AP2R], DS(1913)) +
                  tap(&dl[L_D2R], DS(1996)) - tap(&dl[L_D1L], DS(1990)) - tap(&dl[L_AP2L], DS(187)) -
                  tap(&dl[L_D2L], DS(1066)));
    *or_ = 0.6f * (tap(&dl[L_D1L], DS(353)) + tap(&dl[L_D1L], DS(3627)) - tap(&dl[L_AP2L], DS(1228)) +
                   tap(&dl[L_D2L], DS(2673)) - tap(&dl[L_D1R], DS(2111)) - tap(&dl[L_AP2R], DS(335)) -
                   tap(&dl[L_D2R], DS(121)));
}

static void plate_init(void)
{
    float *p = pl_buf;
    int k, i;
    for (k = 0; k < L_N; k++) {
        dl[k].b = p;
        dl[k].n = DL_LEN[k];
        dl[k].i = 0;
        p += DL_LEN[k];
    }
    for (i = 0; i < DL_TOTAL; i++)
        pl_buf[i] = 0.0f;
    pl_bw = pl_damp_l = pl_damp_r = 0.0f;
    lfo_s = 0.0f;
    lfo_c = 1.0f;
}

/* ------------------------------------------------- echo: tape and Echoplex --- */
/* After Discreet Music's chain: an Echoplex (a short, smearing echo, ~160 ms, fed back) and a tape loop
 * (long: up to ~5.6 s, synced to the tempo), each repeat a little darker and thinner, with a little
 * wow. The tape runs at a quarter rate (11 kHz, 16-bit, mono): it loses its highs anyway. Two heads:
 * the first plays the loop's length, the second a part of it (3/4 at first). With Motion the echo
 * evolves: each head wanders across the field, the feedback leans now on one head, now on the other
 * (the repeats fall in changing patterns, left and right), and now and then the second head slides to
 * another part of the loop, as a head moved by hand (the repeats bend a little in pitch on the way).
 * The Echoplex has a second tap, for the right. */
#define TAPE_N 61600                             /* 5.59 s at 11.025 kHz (800 B less than 5.62: the pool keeps its 8 KiB of room with the backup) */
#define TAPE_SR 11025.0f
static int16_t tape[TAPE_N] EN_POOL;
static int tape_w, tape_ph;
static float tape_acc, tape_lp, tape_hp, tape_p1, tape_p2, tape_c1, tape_c2;
static const float HEAD2[5] = {0.75f, 0.6667f, 0.625f, 0.5f, 0.375f};   /* the second head's parts */
static float head2 = 0.75f, head2_t = 0.75f, head2_v;   /* its place (of the loop), target, speed */
static float fb_lean;                            /* 0..1: how much of the feedback is the second head's */
static float hg[4] = {1.0f, 0.0f, 0.0f, 1.0f};   /* the heads' pan gains: 1 left, 1 right, 2 left, 2 right */
static uint32_t head_left;                       /* samples until the second head may move */
#define PLEX_N 7100                              /* 160 ms at 44.1 kHz */
#define PLEX_TAP 2700                            /* the right's tap: ~100 ms */
static int16_t plex[PLEX_N];
static int plex_w;
static float plex_lp, plex_lp2;

static inline float tape_read(float d)           /* d quarter-rate samples ago */
{
    int k = (int)d, i0, i1;
    float f = d - (float)k, a, c;
    i0 = tape_w - k;
    i1 = i0 - 1;
    if (i0 < 0)
        i0 += TAPE_N;
    if (i1 < 0)
        i1 += TAPE_N;
    a = (float)tape[i0];
    c = (float)tape[i1];
    return (a + (c - a) * f) * (1.0f / 12000.0f);
}
static inline int16_t s16(float x)
{
    x *= 12000.0f;
    return (int16_t)(x > 32767.0f ? 32767.0f : x < -32767.0f ? -32767.0f : x);
}

static void echo_time(void)
{
    int i = par[P_ETIME];
    float beat, t;
    if (par[P_TEMPO] <= 0)
        return;                                  /* not yet (en_init applies in order) */
    beat = 60.0f / (float)par[P_TEMPO];
    t = ETIME_BEATS[i] * beat;
    while (t * TAPE_SR > (float)(TAPE_N - 8) && i > 0)
        t = ETIME_BEATS[--i] * beat;             /* too long for the tape: the next shorter */
    if (t * TAPE_SR > (float)(TAPE_N - 8))
        t = (float)(TAPE_N - 8) / TAPE_SR;
    echo_len_t = t * TAPE_SR;
    if (echo_len <= 0.0f)
        echo_len = echo_len_t;
}

/* once a chunk: where the heads sit in the field, how the feedback leans, the second head's slide */
static void heads_step(uint32_t n)
{
    float p, want, vmax;
    int i;
    for (i = 0; i < 2; i++) {
        p = fm_clampf((i ? 0.7f : -0.7f) + 0.9f * motion * mod[6 + i].v, -1.0f, 1.0f);
        p = (p + 1.0f) * (FM_PI / 4.0f);
        hg[2 * i] = fm_cosf(p);
        hg[2 * i + 1] = fm_sinf(p);
    }
    fb_lean = 0.25f * motion * (1.0f + mod[8].v);
    if (head_left <= n) {
        head_left = (uint32_t)((20.0f + 40.0f * tfrand()) * EN_SR);
        if (en_playing && !en_frozen && tfrand() < motion)
            head2_t = HEAD2[(int)(tfrand() * 4.999f)];
    } else {
        head_left -= n;
    }
    /* its speed (a part of the loop per tape sample): at most a bend of ~14 cents, easing in and out */
    vmax = 0.008f / fm_maxf(echo_len, 100.0f);
    want = fm_clampf((head2_t - head2) * (1.0f / (2.0f * TAPE_SR)), -vmax, vmax);
    head2_v += (want - head2_v) * fm_minf((float)n * (1.0f / EN_SR), 1.0f);
}

/* one sample of both: the input (a mono send) to the tape's heads (el, er) and the Echoplex (xl, xr) */
static inline void echoes(float in, float *el, float *er, float *xl, float *xr)
{
    float y = (float)plex[plex_w] * (1.0f / 12000.0f);         /* the Echoplex: 160 ms, fed back, darker */
    int t = plex_w - PLEX_TAP;
    plex_lp += 0.35f * (y - plex_lp);
    plex_lp2 += 0.35f * ((float)plex[t < 0 ? t + PLEX_N : t] * (1.0f / 12000.0f) - plex_lp2);
    plex[plex_w] = s16(in + 0.55f * plex_lp);
    if (++plex_w >= PLEX_N)
        plex_w = 0;
    *xl = plex_lp;
    *xr = plex_lp2;
    tape_acc += in + 0.3f * plex_lp;
    if (++tape_ph >= 4) {                        /* the tape, a quarter of the time */
        float d, h1, h2, fb;
        tape_ph = 0;
        echo_len += (echo_len_t - echo_len) * 0.001f;
        head2 += head2_v;
        d = echo_len * (1.0f + 0.003f * motion * mod[5].v);   /* wow */
        if (d > (float)(TAPE_N - 4))
            d = (float)(TAPE_N - 4);
        h1 = tape_read(d);
        h2 = tape_read(d * head2);
        tape_lp += 0.55f * (h1 + fb_lean * (h2 - h1) - tape_lp);   /* each time round: darker, thinner */
        tape_hp += 0.03f * (tape_lp - tape_hp);
        fb = tape_lp - tape_hp;
        if (++tape_w >= TAPE_N)
            tape_w = 0;
        tape[tape_w] = s16(tape_acc * 0.25f + echo_fb * fb);
        tape_acc = 0.0f;
        tape_p1 = tape_c1;
        tape_p2 = tape_c2;
        tape_c1 = h1;
        tape_c2 = h2;
    }
    {   /* between the tape's samples: a line from the last to the next; then to the field */
        float f = (float)tape_ph * 0.25f;
        float h1 = tape_p1 + (tape_c1 - tape_p1) * f, h2 = tape_p2 + (tape_c2 - tape_p2) * f;
        *el = hg[0] * h1 + hg[2] * h2;
        *er = hg[1] * h1 + hg[3] * h2;
    }
}

/* ------------------------------------------------------------ parameters --- */
static int pulse;
static uint32_t ext_age;

static void tempo_set(void) { step_len = EN_SR * 60.0f / ((float)par[P_TEMPO] * 4.0f); }

static void regrow(void);
static void apply(int p, int v)
{
    const en_param_t *pi = &PARAMS[p];
    if (v < pi->lo)
        v = pi->lo;
    if (v > pi->hi)
        v = pi->hi;
    par[p] = (int16_t)v;
    switch (p) {
    case P_PAD: pad_vol = (float)(v * v) * 0.0001f; break;
    case P_BELLS: bell_vol = (float)(v * v) * 0.0001f; break;
    case P_BASS: bass_vol = (float)(v * v) * 0.0001f; break;
    case P_PULSE: pulse_vol = (float)(v * v) * 0.0001f; break;
    case P_HITS:
    case P_RHITS0: case P_RHITS1: case P_RHITS2:
    case P_RTURN0: case P_RTURN1: case P_RTURN2:
    case P_RMOVES:                               /* (Still: back to where they're set) */
        drums_pattern();
        break;
    case P_BASSOCT:
        if (nsteps)
            bass_to(bass.midi_note % 12);
        break;
    case P_SPARK:                                /* turned up: a wave soon, to hear it by */
        if (v && !sp_on) {
            sp_in = 0;
            sp_left = (uint32_t)(1.5f * EN_SR);
        }
        sp_on = v != 0;
        if (!v)
            en_spark = sp_env = 0.0f;
        break;
    case P_SPOFT:                                /* a shorter gap needn't wait out the long one it was in */
        if (!sp_in && (float)sp_left > spoft_secs(v) * EN_SR)
            sp_left = (uint32_t)(spoft_secs(v) * EN_SR);
        break;
    case P_SWOOSH:                               /* 0.36 dB a click: at 1 barely there under a piece, at 100 as loud as it */
        sw_want = v ? fm_exp2f((float)(v - 100) * 0.06f) : 0.0f;
        if (!v) {                                /* turned up again, the first pass soon */
            sw_in = 0;
            sw_left = (uint32_t)(1.5f * EN_SR);
        }
        break;
    case P_CRICKET: nt_want[0] = v ? fm_exp2f((float)(v - 100) * 0.035f) : 0.0f; break;   /* gentler: more join too */
    case P_PEEP: nt_want[1] = v ? fm_exp2f((float)(v - 100) * 0.035f) : 0.0f; break;
    case P_WATER:                                /* a place on the journey, not a level: each has its own */
        nt_want[2] = v ? 1.0f : 0.0f;
        if (v)
            nt_wx = (float)(v - 1) * (3.0f / 99.0f);
        break;
    case P_THUNDER: {                            /* far and seldom to nearer and often; up from 0, a first roll soon */
        const int was = nt_th_v;
        nt_th_v = v;
        nt_want[3] = v ? fm_exp2f((float)(v - 100) * 0.025f) : 0.0f;
        if (v && nt_th_t < 0.0f && (!was || nt_th_wait > 200.0f * fm_exp2f(-0.03f * (float)v)))
            night_thunder_next(was ? 0.0f : 4.0f + 6.0f * nt_dice());
        break;
    }
    case P_SWOFT:                                /* a shorter gap needn't wait out the long one it was in */
        if (!sw_in && (float)sw_left > spoft_secs(v) * EN_SR)
            sw_left = (uint32_t)(spoft_secs(v) * EN_SR);
        break;
    case P_ARATE: case P_APOLY: arp_acc = 0; break;   /* the new rhythm from the next sixteenth */
    case P_BASSN:
        if (v > 0) {
            bass_pattern();
            bass.rest = 0;
        } else {                                 /* back to one long note: the chord's, or the key's (Pedal) */
            bass.wait = 0;
            bass.env = bass.edec = 1.0f;
            if (nsteps)
                bass_to(bass_home());
        }
        break;
    case P_PADN:
        if (nsteps) {
            pad_retarget();
            pad_leave();
        }
        break;
    case P_GLIDE: glide_k = 1.0f / ((0.02f + 0.012f * (float)v) * EN_SR); break;   /* 20 ms .. 1.2 s */
    case P_SWING: swing = (float)v * 0.005f; break;      /* the off sixteenths up to half a step late */
    case P_PSPREAD:
        if (nsteps) {
            pad_retarget();
            pad_handoff();
        }
        break;
    case P_BPLUCK:
        bass_att = 0.0006f + 0.0054f * (float)v * 0.01f;
        break;
    case P_SONG:                                 /* on: the song from here, this the first section */
        if (v)
            song_reset();
        break;
    case P_SEED:                                 /* a number: that garden, now; the one growing kept: nothing */
        if (v && v != en_seed_now)
            regrow();
        break;
    case P_HATS:
        /* how much hat: at once how many (hat_hits), how loud and how bright. Both halves meet at 50 */
        if (v <= 50) {
            const float t = (float)v * 0.02f;
            hat_f = 0.35f + 0.011f * (float)v;
            hat_hp = t;                                   /* dark: the band; a high-pass alone leaves the noise bright */
            hat_bp = 1.0f - t;
            hat_open = 0.12f + 0.0061f * (float)v;
            hat_closed = 0.06f;
            hat_choke = 0.02f;
            hat_lv = fm_exp2f((float)(v - 50) * 0.05f);
        } else {
            hat_f = 0.9f + 0.0030f * (float)(v - 50);
            hat_hp = 1.0f;
            hat_bp = 0.0f;
            hat_open = 0.425f + 0.0055f * (float)(v - 50);
            hat_closed = 0.06f + 0.0006f * (float)(v - 50);
            hat_choke = 0.02f + 0.002f * (float)(v - 50);
            hat_lv = 1.0f + 0.006f * (float)(v - 50);
        }
        drums_pattern();
        break;
    case P_DRIFT:
        if (drift_total > 0.0f) {                /* keep the progress, at the new pace */
            float frac = drift_left / drift_total;
            drift_total = drift_secs(v) * EN_SR;
            drift_left = frac * drift_total;
        }
        break;
    case P_ECHO: echo_send = (float)(v * v) * 0.0001f; break;
    case P_ETIME: echo_time(); break;
    case P_REVERB: rev_send = (float)(v * v) * 0.0001f; break;
    case P_SIZE:
        pl_decay_t = 0.6f + 0.375f * (float)v * 0.01f;     /* to 0.975: a long hall */
        echo_fb = 0.5f + 0.32f * (float)v * 0.01f;
        break;
    case P_BRIGHT: bright = (float)v; break;
    case P_SWELL: swell = (float)v; break;
    case P_EVOLVE: evolve = (float)v * 0.01f; break;
    case P_BTONE: btone = (float)v * 0.01f; break;
    case P_PADTONE: pad_handoff(); break;        /* the voices cross over as they did for a new key */
    case P_MOTION: motion = (float)v * 0.01f; break;
    case P_SLOT1: case P_SLOT2: case P_SLOT3: case P_SLOT4: slate_check(); break;
    case P_BOCT:                                 /* let it be heard now: a bell from the first loop */
        if (en_playing && nsteps) {
            int l, k;
            for (l = 0; l < EN_NLOOP; l++)
                for (k = 0; k < EN_LOOP_LEN[l]; k++)
                    if (loop_note[l][k] >= 0) {
                        bell_ring(l, k);
                        return;
                    }
        }
        break;
    case P_TUNE: tunefac = fm_exp2f((float)v * (1.0f / 1200.0f)); break;
    case P_TEMPO:
        tempo_set();
        echo_time();
        break;
    case P_RSLOT1: case P_RSLOT2: case P_RSLOT3: case P_RSLOT4: root_check(); break;
    case P_ROOT:
        if (root_on)                             /* it's back when the rotator's off */
            break;
        cur_root = v;
        en_root = (uint8_t)v;
        if (nsteps) {
            pad_retarget();
            pad_handoff();
            bass_to(bass_home());
        }
        break;
    case P_MIDI:
        if (!v)
            en_midi_out(0xB0, 123, 0);
        break;
    default: break;
    }
}

static void pool_set(uint32_t p)
{
    pool = (p | 1u) & 0xFFFu;
    scale_set();
    if (deg >= nsteps)
        deg = 0;
    pad_retarget();
    pad_handoff();
    bass_to(bass_home());
}

static uint32_t seed_mix(uint32_t n)            /* a Seed's number to the dice's state */
{
    n = n * 2654435761u ^ 0x5BD1E995u;
    n ^= n >> 15;
    n *= 0x2C1B3C6Du;
    n ^= n >> 12;
    return n ? n : 1u;
}

static void arc_home(void);
static void regrow(void)                        /* a new piece: fresh loops, home chord, patterns home */
{
    int l;
    /* the garden's number: the Seed's, or in Wild a new one, kept where the Seed's knob can find it. From it, the
     * dice; and what they work on, the arc and Sparkle's waves, from their start */
    en_seed_now = (uint16_t)(par[P_SEED] ? par[P_SEED] : 1 + irand(999));
    rng = seed_mix(en_seed_now);
    arc_home();
    sp_left = (uint32_t)(1.5f * EN_SR);
    sp_in = sp_busy = 0;
    sp_env = 0.0f;
    en_spark = 0.0f;
    sp_deg = -1;
    deg = 0;
    shape = 0;
    pad_retarget();
    pad_handoff();
    bass_to(bass_home());
    bass.rest = 0;
    for (l = 0; l < EN_NLOOP; l++) {             /* on the home chord, which they'll begin on */
        loop_regrow(l);
        loop_rest[l] = 0;
    }
    pulse_rest = 0;
    en_pulse_rest = 0;
    drums_pattern();
    drift_reset();
    change_pending = 0;
    slate_i = SLATE_NONE;                        /* from its first step, on the next bar */
    root_i = SLATE_NONE;
    arp_reset();
    song_reset();
}

static void notes_off(void)
{
    int i;
    for (i = 0; i < 4; i++)
        en_midi_out(0xB0 | (uint32_t)i, 123, 0);
    en_midi_out(0xB9, 123, 0);
}

static void quiet(void)
{
    int i;
    for (i = 0; i < NBELL; i++)
        bell[i].on = 0;
    for (i = 0; i < EN_NDRUM; i++)
        drum[i].on = 0, drum[i].wait = 0;
    for (i = 0; i < EN_NPAD; i++)
        pad[i].amp = 0.0f;
    bass.amp = 0.0f;
    notes_off();
}

void en_init(void)
{
    int i;
    for (i = 0; i <= SN; i++)
        sint[i] = fm_sinf((float)i * (FM_TWO_PI / (float)SN));
    plate_init();
    for (i = 0; i < TAPE_N; i++)
        tape[i] = 0;
    for (i = 0; i < PLEX_N; i++)
        plex[i] = 0;
    tape_w = plex_w = tape_ph = 0;
    bass.env = bass.lvl = bass.edec = 1.0f;
    bass.wait = 0;
    root_on = 0;
    root_i = SLATE_NONE;
    head2 = head2_t = 0.75f;
    head2_v = 0.0f;
    echo_len = 0.0f;
    q_r = q_w = 0;
    for (i = 0; i < NMOD; i++) {
        mod[i].v = mod[i].tgt = 0.0f;
        mod[i].left = 0;
    }
    for (i = 0; i < EN_NPAD; i++) {
        pad[i].note = pad[i].target = (float)(50 + 7 * i);
        pad[i].pan = PAD_PAN[i];
        pad[i].amp = 0.0f;
        pad[i].ph[0] = 0.0f;
        pad[i].ph[1] = 0.37f;
        pad[i].vib = 0.3f * (float)i;
        pad[i].midi_note = 0;
        pad[i].hf = 1.0f;
        pad[i].hand = 0;
    }
    arcm = 0.0f;
    pad_gain = 0.0f;
    sw_in = 0;
    sw_left = (uint32_t)(1.5f * EN_SR);
    sw_rng = 0x2545F491u;
    sw_lv = sw_play = sw_gl = sw_gr = sw_tl = sw_tr = sw_low = sw_band = sw_brown = 0.0f;
    night_init();
    for (i = 0; i < P_NPARAMS; i++)
        apply(i, PARAMS[i].def);
    pool_set(EN_MODE_POOL[par[P_MODE]]);
    for (i = 0; i < EN_NPAD; i++)
        pad[i].note = pad[i].target;
    bass.note = bass.target;
    for (i = 0; i < EN_NLOOP; i++) {
        loop_pos[i] = 0;
        loop_regrow(i);
    }
    drums_pattern();
    drift_reset();
    pl_decay = pl_decay_t;
    regrow();                                    /* the first garden has its number too */
}

/* ------------------------------------------------------------ transport --- */
static void clock_pulse(void)
{
    if (pulse == 0)
        tick();
    if (!en_ext)
        midi(0xF8, 0, 0);
    if (++pulse >= 6)
        pulse = 0;
}

static void play_start(int from_top)
{
    int i;
    if (from_top) {
        pulse = 0;
        sixteenth = 0;
        slate_i = SLATE_NONE;
        root_i = SLATE_NONE;
        arp_reset();
        for (i = 0; i < EN_NDRUM; i++)
            drum_pos[i] = 0;
        for (i = 0; i < EN_NLOOP; i++)
            loop_pos[i] = 0;
        song_reset();
    }
    pulse_left = 0.0f;
    en_playing = 1;
    pad_gate = 1.0f;
    for (i = 0; i < EN_NPAD; i++)               /* from silence: the breaths start over, overlapping */
        if (pad[i].amp < 1e-4f) {
            pad[i].t = START_T[i];
            pad[i].midi_note = 0;
            pad_breathe(&pad[i], i);
        }
    if (bass.amp < 1e-4f)
        bass.note = bass.target;
    midi(0x92, bass.midi_note, 80);
    if (!en_ext)
        midi(from_top ? 0xFA : 0xFB, 0, 0);
}

static void play_stop(void)
{
    int d;
    for (d = 0; d < EN_NDRUM; d++)
        drum[d].wait = 0;                        /* a swung hit still waiting stays unplayed */
    bass.wait = 0;
    en_playing = 0;
    pad_gate = 0.0f;
    if (!en_ext)
        midi(0xFC, 0, 0);
    notes_off();
}

static void drain(void)
{
    while (q_r != q_w) {
        uint32_t c = q[q_r % QN];
        int a = (int)(c >> 16 & 0xFFu), b = (int)(int16_t)(c & 0xFFFFu);
        BARRIER();
        q_r++;
        switch (c >> 24) {
        case C_SET:
            if (a < P_NPARAMS)
                apply(a, b);
            break;
        case C_POOL: pool_set((uint32_t)b & 0xFFFu); break;
        case C_PLAY:
            if (a && !en_playing) {
                if (par[P_SEED])                 /* a Seed: its garden again, from the start */
                    regrow();
                play_start(1);
            } else if (!a && en_playing) {
                play_stop();
            }
            break;
        case C_NEXT:
            if (en_playing)
                change_pending = 1;
            else if (slate_on)
                slate_next();
            else
                chord_change();
            break;
        case C_FREEZE: en_frozen = (uint8_t)(a != 0); break;
        case C_SEED:
            rng ^= (uint32_t)(b & 0xFFFF) * 2654435761u;
            if (!rng)
                rng = 1u;
            break;
        case C_CLOCK:
            en_ext = 1;
            ext_age = 0;
            if (a == EN_CLK_TICK) {
                if (en_playing)
                    clock_pulse();
            } else if (a == EN_CLK_START) {
                if (par[P_SEED])
                    regrow();
                play_start(1);
            } else if (a == EN_CLK_CONTINUE) {
                if (!en_playing)
                    play_start(0);
            } else if (en_playing) {
                play_stop();
            }
            break;
        case C_PANIC: quiet(); break;
        case C_REGROW: regrow(); break;
        case C_RESTART:                          /* a deal: its steps from the first, at the next bar */
            if (a & 1)
                slate_i = SLATE_NONE;
            if (a & 2)
                root_i = SLATE_NONE;
            break;
        }
    }
}

/* --------------------------------------------------------------- render --- */
#define BLK 256
static float mixl[BLK], mixr[BLK], rsend[BLK], esend[BLK];

/* the pad's bus: a 12 dB low-pass, darker than it sounds on paper (Brian Eno darkened everything: the
 * Synthi's filter below half, then tape). No chorus: a modulated delay against pure tones beats (on
 * the FM-1's speaker above all), and that read as pulsing. The width comes from the voices' places,
 * the echo's two heads and the plate. */
static float padl[BLK], padr[BLK];
static float plp[4];

static void pad_bus(uint32_t n)
{
    const float fc = fm_clampf(700.0f * fm_exp2f(bright * 0.03f + 0.6f * arcm), 300.0f, 9000.0f);
    const float c = 1.0f - fm_expf(-FM_TWO_PI * fc * (1.0f / EN_SR));
    uint32_t j;
    for (j = 0; j < n; j++) {
        float l, r;
        plp[0] += c * (padl[j] - plp[0]);
        plp[1] += c * (plp[0] - plp[1]);
        plp[2] += c * (padr[j] - plp[2]);
        plp[3] += c * (plp[2] - plp[3]);
        l = plp[1];
        r = plp[3];
        mixl[j] += l * 0.8f;
        mixr[j] += r * 0.8f;
        rsend[j] += (l + r) * 0.6f;
        esend[j] += (l + r) * 0.2f;
    }
    for (j = 0; j < 4; j++)
        plp[j] = fm_flush(plp[j]);
}

#define PADG_STRINGS 0.9f                       /* each colour about as loud as Voices */
#define PADG_NOIR 0.9f
static void render_pad(uint32_t n)
{
    int i;
    float lvl = 0.0f;
    uint32_t j0;
    for (j0 = 0; j0 < n; j0++)
        padl[j0] = padr[j0] = 0.0f;
    const float g = pad_vol * 0.5f;
    pad_gain += (pad_gate - pad_gain) * fm_minf((float)n / ((pad_gate > 0.0f ? 2.0f : 5.0f) * EN_SR), 1.0f);
    for (i = 0; i < EN_NPAD; i++) {
        pad_t *v = &pad[i];
        float p0 = v->ph[0], p1 = v->ph[1], amp = v->amp, s0 = v->ic[0], s1 = v->ic[1], s2 = v->ic[2], s3 = v->ic[3],
              gl, gr, pn, tgt = 0.0f, idx = 0.0f;
        uint32_t j;
        if (amp < 1e-5f && pad_gain < 1e-4f) {
            v->amp = 0.0f;
            continue;
        }
        pn = fm_clampf(v->pan + 0.35f * motion * mod[(i + 3) % NMOD_PAD].v, -1.0f, 1.0f);
        gl = fm_cosf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        gr = fm_sinf((pn + 1.0f) * (FM_PI / 4.0f)) * g;
        for (j = 0; j < n; j++) {
            float x, v1, v2, v3, b1, l1, b2;
            if (!(j & 31u)) {
                float br;
                pad_coefs(v, i);
                br = breath(v->t);
                tgt = br * v->peak * pad_gain * v->hf;
                /* the Rhodes blooms: more overtones as the breath swells (index in turns) */
                idx = (0.04f + 0.1f * bright * 0.01f + 0.1f * br) * (1.0f + 0.4f * arcm);
            }
            if (!v->voices) {                    /* FM, the modulator at the carrier's pitch: a Rhodes */
                float m = sine(p0) * idx;
                x = (sine(p0 + m + 1.0f) + 0.12f * sine(2.0f * p0)) * 1.2f;   /* one carrier: no beating */
                p0 += v->inc[0];
                if (p0 >= 1.0f)
                    p0 -= 1.0f;
                amp += (tgt - amp) * 0.01f;
                x *= amp;
                padl[j] += x * gl;
                padr[j] += x * gr;
                continue;
            }
            if (v->voices >= 2) {                /* Strings, Noir: two saws apart through two low-passes */
                x = blep_saw(p0, v->inc[0]) + blep_saw(p1, v->inc[1]);
                p0 += v->inc[0];
                if (p0 >= 1.0f)
                    p0 -= 1.0f;
                p1 += v->inc[1];
                if (p1 >= 1.0f)
                    p1 -= 1.0f;
                v3 = x - s1;
                v1 = v->c1[0] * s0 + v->c1[1] * v3;
                v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
                s0 = 2.0f * v1 - s0;
                s1 = 2.0f * v2 - s1;
                v3 = v2 - s3;
                v1 = v->c2[0] * s2 + v->c2[1] * v3;
                v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
                s2 = 2.0f * v1 - s2;
                s3 = 2.0f * v2 - s3;
                amp += (tgt - amp) * 0.01f;
                x = v2 * amp * (v->voices == 3 ? PADG_NOIR : PADG_STRINGS);
                padl[j] += x * gl;
                padr[j] += x * gr;
                continue;
            }
            /* as Discreet Music's Synthi: a mellow triangle (a sine's neighbour), a little saw in it */
            x = (p0 < 0.5f ? 4.0f * p0 - 1.0f : 3.0f - 4.0f * p0) * 1.4f + 0.45f * blep_saw(p0, v->inc[0]) + 0.6f * sine(p0);
            p0 += v->inc[0];
            if (p0 >= 1.0f)
                p0 -= 1.0f;
            v3 = x - s1;                         /* the first formant: TPT SVF (Zavalishin), bp and lp */
            v1 = v->c1[0] * s0 + v->c1[1] * v3;
            v2 = s1 + v->c1[1] * s0 + v->c1[2] * v3;
            s0 = 2.0f * v1 - s0;
            s1 = 2.0f * v2 - s1;
            b1 = v1 * v->c1[3];
            l1 = v2;
            v3 = x - s3;                         /* the second */
            v1 = v->c2[0] * s2 + v->c2[1] * v3;
            v2 = s3 + v->c2[1] * s2 + v->c2[2] * v3;
            s2 = 2.0f * v1 - s2;
            s3 = 2.0f * v2 - s3;
            b2 = v1 * v->c2[3];
            amp += (tgt - amp) * 0.01f;
            x = (b1 + 0.5f * b2 + 0.45f * l1) * amp;
            padl[j] += x * gl;                   /* to the pad's bus: smoothed and chorused below */
            padr[j] += x * gr;
        }
        v->ic[0] = fm_flush(s0);
        v->ic[1] = fm_flush(s1);
        v->ic[2] = fm_flush(s2);
        v->ic[3] = fm_flush(s3);
        v->amp = amp;
        v->ph[0] = p0;
        v->ph[1] = p1;
        en_pad_note[i] = (uint8_t)v->note;
        en_pad_amp[i] = amp;
        lvl = fm_maxf(lvl, amp);
    }
    en_pad_level = lvl;
    pad_bus(n);
}

static void render_bass(uint32_t n)
{
    uint32_t j;
    const float g = bass_vol * 0.42f, tgt = en_playing && !bass.rest ? 1.0f : 0.0f,
                rate = tgt > 0.0f ? 1.0f / (2.5f * EN_SR) : 1.0f / (4.0f * EN_SR);
    const float edec = bass.edec;
    float ph = bass.ph, amp = bass.amp, env = bass.env, lvl = bass.lvl, inc;
    if (amp < 1e-5f && !en_playing) {
        bass.amp = 0.0f;
        en_bass_level = 0.0f;
        return;
    }
    /* Glide; with Bass notes, never so slow that a note doesn't arrive */
    bass.note += (bass.target - bass.note) *
                 fm_minf((float)n * (par[P_BASSN] > 0 ? fm_maxf(glide_k, 4.0f / step_len) : glide_k), 1.0f);
    inc = mtof(bass.note) * (1.0f / EN_SR);
    for (j = 0; j < n; j++) {
        float x = sine(ph) * 0.75f + sine(ph * 0.5f + 0.25f) * 0.45f + sine(ph * 2.0f) * 0.12f;
        ph += inc;
        if (ph >= 2.0f)                          /* two turns: the sub-octave's period */
            ph -= 2.0f;
        amp += (tgt - amp) * rate;
        env *= edec;
        lvl += (env - lvl) * bass_att;           /* the attack: ~4 ms plucked, ~40 ms humming */
        x *= amp * lvl * g;
        mixl[j] += x;
        mixr[j] += x;
        rsend[j] += x * 0.08f;
    }
    bass.ph = ph;
    bass.amp = amp;
    bass.env = env;
    bass.lvl = lvl;
    en_bass_level = amp * lvl;
}

static void render_bells(uint32_t n)
{
    int i;
    const float g = bell_vol * 0.62f;
    for (i = 0; i < NBELL; i++) {
        bell_t *v = &bell[i];
        float cph = v->cph, mph = v->mph, amp = v->amp, idx = v->idx, att = v->att;
        const float arate = v->arate;
        uint32_t j = 0;
        if (!v->on)
            continue;
        if (v->wait > 0) {
            if ((uint32_t)v->wait >= n) {
                v->wait -= (int32_t)n;
                continue;
            }
            j = (uint32_t)v->wait;
            v->wait = 0;
        }
        if (v->mvel) {                           /* (its wait may have ended on the last block's edge) */
            midi(0x91, v->note, v->mvel);
            v->mvel = 0;
        }
        for (; j < n; j++) {
            float m = sine(mph) * idx, x;
            float p = cph + m + 4.0f;            /* + 4 turns: positive for the table */
            x = sine(p) * amp * att * g;
            mixl[j] += x * v->gl;
            mixr[j] += x * v->gr;
            rsend[j] += x * 0.7f;
            esend[j] += x * 0.85f;
            cph += v->cinc;
            mph += v->minc;
            if (cph >= 1.0f)
                cph -= 1.0f;
            if (mph >= 1.0f)
                mph -= 1.0f;
            amp *= v->adec;
            idx *= v->idec;
            att += (1.0f - att) * arate;         /* the strike: ~4 ms, or soft as a slowed piano (~50 ms) */
        }
        v->cph = cph;
        v->mph = mph;
        v->amp = amp;
        v->idx = idx;
        v->att = att;
        if (amp < 0.0008f) {
            v->on = 0;
            midi(0x81, v->note, 0);
        }
    }
}

static void render_lead(uint32_t n)
{
    const float g = bell_vol * 0.62f * 0.55f;
    uint32_t j;
    int i;
    if (!lead.v[0].on && !lead.v[1].on && !lead.pend)
        return;
    for (j = 0; j < n; j++) {
        if (lead.pend && --lead.wait <= 0) {
            lead.pend = 0;
            lead_start(lead.pnote, (float)lead.pvel * (1.0f / 110.0f));
        }
        for (i = 0; i < 2; i++) {
            lead_v *w = &lead.v[i];
            float x, v1, v2, v3, y;
            if (!w->on)
                continue;
            if (!(j & 31u)) {
                const float depth = fm_minf(w->vt * (1.0f / (0.6f * EN_SR)), 1.0f) * 0.15f;   /* late, then a semitone's sixth */
                const float f = mtof((float)w->note);
                w->vt += 32.0f;
                w->k = f * (1.0f / EN_SR) * (1.0f + depth * 0.0578f * sine(w->vt * (5.2f / EN_SR)));
                svf_coefs(w->c, f * (1.5f + (1.5f + 6.0f * btone) * w->env), 0.75f);
                if (w->hold > 0) {
                    w->hold -= 32;
                    w->tgt = w->vt < 0.15f * EN_SR ? 1.0f : 0.75f;   /* a swell, then held a little lower */
                } else {
                    w->tgt = 0.0f;
                }
            }
            w->env += (w->tgt - w->env) * (w->tgt > w->env ? 0.004f : w->tgt > 0.0f ? 0.00008f : 0.000025f);   /* let go: ~0.9 s */
            w->inc += (w->k - w->inc) * 0.05f;   /* (only the vibrato's sway: ~0.5 ms) */
            x = blep_saw(w->ph[0], w->inc) + 0.7f * blep_saw(w->ph[1], w->inc * 1.003f);
            w->ph[0] += w->inc;
            if (w->ph[0] >= 1.0f)
                w->ph[0] -= 1.0f;
            w->ph[1] += w->inc * 1.003f;
            if (w->ph[1] >= 1.0f)
                w->ph[1] -= 1.0f;
            v3 = x - w->s[1];                    /* a low-pass: TPT SVF */
            v1 = w->c[0] * w->s[0] + w->c[1] * v3;
            v2 = w->s[1] + w->c[1] * w->s[0] + w->c[2] * v3;
            w->s[0] = 2.0f * v1 - w->s[0];
            w->s[1] = 2.0f * v2 - w->s[1];
            y = v2 * w->env * w->vel * g;
            mixl[j] += y * 0.8f;
            mixr[j] += y * 0.8f;
            rsend[j] += y * 0.8f;
            esend[j] += y;
        }
    }
    for (i = 0; i < 2; i++) {
        lead_v *w = &lead.v[i];
        w->s[0] = fm_flush(w->s[0]);
        w->s[1] = fm_flush(w->s[1]);
        if (w->on && w->hold <= 0 && w->env < 1e-4f) {
            w->on = 0;
            w->env = 0.0f;
        }
    }
    if (lead.midi && !lead.v[0].on && !lead.v[1].on && !lead.pend) {
        midi(0x81, lead.midi, 0);
        lead.midi = 0;
    }
}

/* a hat: the metal and noise through a Chamberlin SVF, dark to bright by Hats; no pitch, little echo */
static void render_hat(drum_t *v, uint32_t n, float g, float pl, float pr)
{
    uint32_t j;
    int k;
    float x1, x2, amp, att;
    if (!v->on)
        return;
    x1 = v->x1, x2 = v->x2, amp = v->amp, att = v->att;
    for (j = 0; j < n; j++) {
        float in = 0.0f, hp, x;
        for (k = 0; k < 4; k++) {
            v->mph[k] += HAT_INC[k];
            if (v->mph[k] >= 1.0f)
                v->mph[k] -= 1.0f;
            in += v->mph[k] < 0.5f ? 0.25f : -0.25f;
        }
        noise = noise * 1664525u + 1013904223u;
        in = 0.5f * in + 0.6f * (float)(int32_t)noise * (1.0f / 2147483648.0f);
        hp = in - x2 - 0.8f * x1;
        x1 += hat_f * hp;
        x2 += hat_f * x1;
        x = (hat_hp * hp + hat_bp * x1) * amp * att * g;
        mixl[j] += x * pl;
        mixr[j] += x * pr;
        rsend[j] += x * 0.2f;
        esend[j] += x * 0.08f;
        amp *= v->adec;
        att += (1.0f - att) * 0.3f;
    }
    v->x1 = fm_flush(x1);
    v->x2 = fm_flush(x2);
    v->amp = amp;
    v->att = att;
    v->on = amp > 0.0005f;
}

static void render_drums(uint32_t n)
{
    uint32_t j;
    const float g = pulse_vol * 1.1f;
    drum_t *v;
    v = &drum[0];                                /* thump: a sine falling to its note */
    if (v->on) {
        float ph = v->ph, inc = v->inc, amp = v->amp, att = v->att;
        for (j = 0; j < n; j++) {
            float x = sine(ph) * amp * att * g;
            mixl[j] += x;
            mixr[j] += x;
            rsend[j] += x * 0.1f;
            ph += inc;
            if (ph >= 1.0f)
                ph -= 1.0f;
            inc += (v->inc_end - inc) * 0.0012f;
            amp *= v->adec;
            att += (1.0f - att) * 0.02f;
        }
        v->ph = ph;
        v->inc = inc;
        v->amp = amp;
        v->att = att;
        v->on = amp > 0.0005f;
    }
    render_hat(&drum[1], n, g * hat_lv * 0.2f, 0.6f, 1.0f);    /* closed: a little right */
    render_hat(&drum[2], n, g * hat_lv * 0.14f, 1.0f, 0.7f);   /* open: a little left */
}

static void render_swoosh(uint32_t n)
{
    uint32_t j;
    const float f = sw_f, w = 1.0f - sw_mix, b = sw_mix * 4.0f, k = 1.0f / (float)n;
    const float dl = (sw_tl - sw_gl) * k, dr = (sw_tr - sw_gr) * k;
    float low = sw_low, band = sw_band, brown = sw_brown, gl = sw_gl, gr = sw_gr;
    uint32_t r = sw_rng;
    if (sw_tl + sw_tr < 1e-6f && gl + gr < 1e-6f) {
        sw_gl = sw_gr = 0.0f;
        return;
    }
    for (j = 0; j < n; j++) {
        float x, m;
        r ^= r << 13, r ^= r >> 17, r ^= r << 5;
        x = (float)(int32_t)r * (1.0f / 2147483648.0f);
        brown += (x - brown) * 0.03f;
        x = x * w + brown * b;
        low += f * band;
        band += f * (x - low - 1.2f * band);
        x = band + 0.5f * low;
        gl += dl;
        gr += dr;
        mixl[j] += x * gl;
        mixr[j] += x * gr;
        m = x * (gl + gr);
        rsend[j] += m * 0.5f;
        esend[j] += m * 0.4f;
    }
    sw_low = low, sw_band = band, sw_brown = brown, sw_gl = gl, sw_gr = gr, sw_rng = r;
}

static void render_night(uint32_t n)
{
    uint32_t j;
    int i;
    float g = nt_lv[0] * nt_play * NT_G_CRICKET;
    if (g > 1e-5f) {                             /* crickets: a few pulses of a high tone, then a rest */
        const int on = 1 + (par[P_CRICKET] > 40) + (par[P_CRICKET] > 75);
        for (i = 0; i < NT_NCR; i++) {
            float ph = nt_cr[i].ph;
            const float inc = nt_cr[i].inc, gl = nt_cr[i].gl * g, gr = nt_cr[i].gr * g;
            for (j = 0; j < n; j++) {
                if (nt_cr[i].k < nt_cr[i].np) {
                    if (nt_cr[i].pt < NT_PULSE) {
                        const float x = sine((float)nt_cr[i].pt * (0.5f / (float)NT_PULSE)) * sine(ph);
                        mixl[j] += x * gl;
                        mixr[j] += x * gr;
                        rsend[j] += x * (gl + gr) * 0.3f;
                    }
                    ph += inc;
                    if (ph >= 1.0f)
                        ph -= 1.0f;
                    if (++nt_cr[i].pt >= NT_STEP) {
                        nt_cr[i].pt = 0;
                        nt_cr[i].k++;
                    }
                }
                if (++nt_cr[i].t >= nt_cr[i].period) {
                    nt_cr[i].t = nt_cr[i].pt = 0;
                    nt_cr[i].k = i < on ? 0 : nt_cr[i].np;
                    nt_cr[i].period = (uint32_t)((0.42f + 0.3f * nt_dice()) * EN_SR);
                }
            }
            nt_cr[i].ph = ph;
        }
    }
    g = nt_lv[1] * nt_play * NT_G_PEEP;
    if (g > 1e-5f) {                             /* peepers: a little whistle bending up, about once a second each */
        const int on = 1 + par[P_PEEP] * (NT_NPE - 1) / 100;
        for (i = 0; i < NT_NPE; i++) {
            float ph = nt_pe[i].ph;
            const float gl = nt_pe[i].gl * g, gr = nt_pe[i].gr * g, inv = 0.5f / (float)nt_pe[i].dur;
            for (j = 0; j < n; j++) {
                if (nt_pe[i].t < nt_pe[i].dur) {
                    const float s = sine((float)nt_pe[i].t * inv), x = s * s * sine(ph);
                    mixl[j] += x * gl;
                    mixr[j] += x * gr;
                    rsend[j] += x * (gl + gr) * 0.35f;
                    ph += nt_pe[i].inc0 + nt_pe[i].dinc * (float)nt_pe[i].t;
                    if (ph >= 1.0f)
                        ph -= 1.0f;
                }
                if (++nt_pe[i].t >= nt_pe[i].period) {
                    if (i >= on) {
                        nt_pe[i].t = nt_pe[i].period;   /* resting */
                    } else {
                        nt_pe[i].t = 0;
                        ph = 0.0f;
                        nt_pe[i].period = (uint32_t)((0.8f + 0.6f * nt_dice()) * EN_SR);
                    }
                }
            }
            nt_pe[i].ph = ph;
        }
    }
    for (i = 0; i < NT_NBL && nt_bl[i].amp <= 1e-4f; i++)
        ;
    if (i < NT_NBL) {                            /* drops: short ticks of bright noise, no pitch */
        static float tick[BLK];
        uint32_t r = nt_rng;
        float hp = nt_tick;
        for (j = 0; j < n; j++) {
            float c;
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            c = (float)(int32_t)r * (1.0f / 2147483648.0f);
            hp += (c - hp) * 0.3f;
            tick[j] = (c - hp) * NT_G_DROP;
        }
        nt_rng = r;
        nt_tick = hp;
        for (; i < NT_NBL; i++) {
            float a = nt_bl[i].amp;
            const float dec = nt_bl[i].dec, gl = nt_bl[i].gl, gr = nt_bl[i].gr;
            if (a <= 1e-4f)
                continue;
            for (j = 0; j < n; j++) {
                const float x = tick[j] * a;
                mixl[j] += x * gl;
                mixr[j] += x * gr;
                rsend[j] += x * 0.4f;
                a *= dec;
            }
            nt_bl[i].amp = a;
        }
    }
    if (nt_rain_g + nt_creek_g > 1e-6f || nt_wv_tl + nt_wv_tr + nt_wv_gl + nt_wv_gr > 1e-6f) {   /* water */
        uint32_t r = nt_rng;
        float l0 = nt_lp[0], l1 = nt_lp[1], w0 = nt_lp[2], w1 = nt_lp[3], br = nt_brown;
        float wgl = nt_wv_gl, wgr = nt_wv_gr;
        const float ra = nt_rain_a, rg = nt_rain_g * NT_G_RAIN, lowg = nt_low_g, cg = nt_creek_g * NT_G_CREEK;
        const float c0 = nt_ck_c[0], c1 = nt_ck_c[1], c2 = nt_ck_c[2], c3 = nt_ck_c[3];
        const float g0 = nt_ck_g[0] * cg, g1 = nt_ck_g[1] * cg, g2 = nt_ck_g[2] * cg, g3 = nt_ck_g[3] * cg;
        const float wa = nt_wv_a, foam = nt_wv_foam, k = 1.0f / (float)n;
        const float dl = (nt_wv_tl * NT_G_WAVES - wgl) * k, dr = (nt_wv_tr * NT_G_WAVES - wgr) * k;
        for (j = 0; j < n; j++) {
            float a, b, yl, yr;
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            a = (float)(int32_t)r * (1.0f / 2147483648.0f);
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            b = (float)(int32_t)r * (1.0f / 2147483648.0f);
            l0 += ra * (a - l0);                 /* rain: the hiss over a cutoff, a downpour's roar under it */
            l1 += ra * (b - l1);
            yl = ((a - l0) + lowg * l0) * rg;
            yr = ((b - l1) + lowg * l1) * rg;
            if (cg > 1e-6f) {                    /* the creek: four wandering bands of the same noise */
                nt_ck_lo[0] += c0 * nt_ck_bd[0];
                nt_ck_bd[0] += c0 * (a - nt_ck_lo[0] - 0.7f * nt_ck_bd[0]);
                nt_ck_lo[1] += c1 * nt_ck_bd[1];
                nt_ck_bd[1] += c1 * (a - nt_ck_lo[1] - 0.7f * nt_ck_bd[1]);
                nt_ck_lo[2] += c2 * nt_ck_bd[2];
                nt_ck_bd[2] += c2 * (b - nt_ck_lo[2] - 0.7f * nt_ck_bd[2]);
                nt_ck_lo[3] += c3 * nt_ck_bd[3];
                nt_ck_bd[3] += c3 * (b - nt_ck_lo[3] - 0.7f * nt_ck_bd[3]);
                yl += nt_ck_bd[0] * g0 + nt_ck_bd[1] * g1;
                yr += nt_ck_bd[2] * g2 + nt_ck_bd[3] * g3;
            }
            br += (a - br) * 0.02f;              /* the sea: deep noise opening with the swell, foam at the crash */
            w0 += wa * (br * 5.0f + a * foam - w0);
            w1 += wa * (br * 5.0f + b * foam - w1);
            wgl += dl;
            wgr += dr;
            yl += w0 * wgl;
            yr += w1 * wgr;
            mixl[j] += yl;
            mixr[j] += yr;
            rsend[j] += (yl + yr) * 0.15f;
        }
        nt_rng = r;
        nt_lp[0] = l0, nt_lp[1] = l1, nt_lp[2] = w0, nt_lp[3] = w1, nt_brown = br;
        nt_wv_gl = wgl, nt_wv_gr = wgr;
    }
    if (nt_th_g > 1e-6f) {                       /* thunder: a far rumble, deep and wide, much of it the reverb's */
        uint32_t r = nt_rng;
        float l1 = nt_th_lp[0], l2 = nt_th_lp[1], lb = nt_th_lp[2], r1 = nt_th_lp[3], r2 = nt_th_lp[4], rb = nt_th_lp[5];
        const float tg = nt_th_g * NT_G_THUNDER, gl = nt_th_gl * tg, gr = nt_th_gr * tg;
        for (j = 0; j < n; j++) {
            float a, b, yl, yr;
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            a = (float)(int32_t)r * (1.0f / 2147483648.0f);
            r ^= r << 13, r ^= r >> 17, r ^= r << 5;
            b = (float)(int32_t)r * (1.0f / 2147483648.0f);
            l1 += (a - l1) * 0.0128f;            /* two poles at 90 Hz: the weight */
            l2 += (l1 - l2) * 0.0128f;
            lb += (a - lb) * 0.031f;             /* round 220 Hz: enough body to be heard on small speakers */
            r1 += (b - r1) * 0.0128f;
            r2 += (r1 - r2) * 0.0128f;
            rb += (b - rb) * 0.031f;
            yl = (l2 * 4.0f + (lb - l1) * 0.35f) * gl;
            yr = (r2 * 4.0f + (rb - r1) * 0.35f) * gr;
            mixl[j] += yl;
            mixr[j] += yr;
            rsend[j] += (yl + yr) * 0.6f;
        }
        nt_rng = r;
        nt_th_lp[0] = l1, nt_th_lp[1] = l2, nt_th_lp[2] = lb, nt_th_lp[3] = r1, nt_th_lp[4] = r2, nt_th_lp[5] = rb;
    }
}

static void render_chunk(int32_t *out, uint32_t n, float gain)
{
    uint32_t j;
    const float rg = rev_send * rev_in_gain * (1.0f - 0.25f * arcm), eg = echo_send;
    for (j = 0; j < n; j++)
        mixl[j] = mixr[j] = rsend[j] = esend[j] = 0.0f;
    render_pad(n);
    render_bass(n);
    render_bells(n);
    render_lead(n);
    render_drums(n);
    render_swoosh(n);
    render_night(n);
    for (j = 0; j < n; j++) {
        float wl, wr, l, r, el, er, xl, xr;
        echoes(esend[j] * eg, &el, &er, &xl, &xr);
        reverb((rsend[j] * rg + (el + er + xl + xr) * 0.35f * rev_send) * 0.8f, &wl, &wr);
        l = (mixl[j] + 0.75f * el + 0.3f * xl + 1.3f * wl) * 0.36f * gain;
        r = (mixr[j] + 0.75f * er + 0.3f * xr + 1.3f * wr) * 0.36f * gain;
        out[2 * j] = (int32_t)(fm_tanhf(l) * 8300000.0f);
        out[2 * j + 1] = (int32_t)(fm_tanhf(r) * 8300000.0f);
    }
}

static float slate_done(void)                   /* 0..1: how far through this step */
{
    if (slate_i == SLATE_NONE || !slate_total)
        return 0.0f;
    return fm_clampf(((float)(slate_total - slate_left) + (float)(sixteenth & 15u) * 0.0625f) / (float)slate_total, 0.0f, 1.0f);
}

/* the arc: a slow tide, minutes long, between sparse (-1) and full (+1); Motion sets how far it goes */
static float arc, arc_tgt;
static uint32_t arc_left;
static void arc_aim(float t)                     /* a section's arc (Song); free again, a new one at once */
{
    arc_tgt = t;
    arc_left = 0;
}

static void arc_home(void)
{
    arc = arc_tgt = arcm = 0.0f;
    en_arc = 0.0f;
    arc_left = 0;
}

static void control(uint32_t n)                 /* once a chunk: the slow things */
{
    int i;
    mods_step(n);
    heads_step(n);
    if (par[P_SPARK] && en_playing)
        spark_env(n);
    swoosh_env(n);
    night_env(n);
    if (par[P_SONG]) {
        /* the sections aim it (song_enter) */
    } else if (arc_left <= n) {
        float r = frand() * 2.0f - 1.0f;
        arc_tgt = r * (0.6f + 0.4f * frand()) + (r > 0.0f ? 0.25f : -0.25f);   /* away from the middle */
        arc_left = (uint32_t)((50.0f + 100.0f * frand()) * EN_SR);
    } else {
        arc_left -= n;
    }
    if (en_playing)
        arc += (fm_clampf(arc_tgt, -1.0f, 1.0f) - arc) * (float)n * (1.0f / (35.0f * EN_SR));
    arcm = arc * motion;
    en_arc = arcm;
    pl_decay += ((en_frozen ? 0.9995f : pl_decay_t) - pl_decay) * 0.002f;
    pl_dd2 = fm_clampf(pl_decay + 0.15f, 0.25f, 0.5f);
    rev_in_gain += ((en_frozen ? 0.0f : 1.0f) - rev_in_gain) * 0.01f;
    if (en_playing && !en_frozen && !change_pending) {
        if (!slate_on)
            drift_left -= (float)n;
        if (!fifth_done && (slate_on ? slate_done() >= 0.5f : drift_left < 0.5f * drift_total)) {
            fifth_done = 1;                      /* halfway: the bass may lean on the fifth */
            if (frand() < (float)par[P_FIFTH] * 0.01f && chord_nt >= 3 && par[P_BASSN] == 0)
                bass_to(chord_pcs[2]);
        }
        if (!slate_on && drift_left <= 0.0f)
            change_pending = 1;
    }
    en_drift = slate_on ? slate_done() : drift_total > 0.0f ? 1.0f - fm_clampf(drift_left / drift_total, 0.0f, 1.0f) : 0.0f;
    for (i = 0; i < EN_NLOOP; i++)
        en_loop_glow[i] *= 0.985f;
    for (i = 0; i < EN_NDRUM; i++)
        en_drum_glow[i] *= 0.95f;
}

void en_render(int32_t *out, uint32_t n, uint32_t gain_q12)
{
    const float gain = (float)gain_q12 * (1.0f / 4096.0f) * 2.0f;
    drain();
    if (en_ext && ++ext_age > 100u)
        en_ext = 0;
    while (n) {
        uint32_t k = n > BLK ? BLK : n;
        int timed = en_playing && !en_ext;
        if (timed) {
            if (pulse_left <= 0.0f) {
                clock_pulse();
                pulse_left += step_len * (1.0f / 6.0f);
            }
            if ((float)k > pulse_left)
                k = (uint32_t)pulse_left + 1u;
        }
        if (k > n)
            k = n;
        {
            int32_t w = drums_wait();            /* a swung hit lands on its sample */
            if (w > 0 && (uint32_t)w < k)
                k = (uint32_t)w;
        }
        if (timed)
            pulse_left -= (float)k;
        control(k);
        render_chunk(out, k, gain);
        drums_wait_step((int32_t)k);
        out += 2 * k;
        n -= k;
    }
}

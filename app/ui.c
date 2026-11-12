/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 UI: the panel and the screen. Part of the unity build (after gfx.c and project.c); the host
 * simulator includes it too.
 *
 *   keys           the scale: each key adds or removes its note from the pool (the root stays)
 *   PLAY           the garden grows / rests        ARP   the next chord, now
 *   REC            FREEZE: the chord holds, the reverb rings on    OCT- / OCT+   the bells' octave
 *   ENV            the pad's colour: Rhodes / Voices (lit: Voices), kept with the piece
 *   SELECT tempo   ALGORITHM mode (loads a pool)   PRESETS the piece (Root is on the Setup page)
 *   HOME SEQ FX SEL GLO   the pages: LAYERS, MOTION, SPACE, TONE, SETUP (EDIT steps through them)
 *   KNOB 1-4       the page's four values
 *   SELECT pressed the Chords page, the slate: KNOB 1-4 each step's chord; SELECT held, each step's bars;
 *                  pressed again, the Roots page: the same, for the root rotator
 *   SAVE           saves (it also saves by itself a few seconds after a change, while resting);
 *                  held 1.5 s: the piece back to its recipe */
#include "fastmath.h"

/* the pages, one a layer (a button each); SELECT steps through them in this order */
enum { V_GARDEN, V_PAD, V_BELLS, V_ARP, V_BASS, V_PULSE, V_SPACE, V_SETUP, V_CHORDS, V_ROOTS, V_DEAL, V_RINGS, V_DREAM, NVIEWS };
static const char *const VIEW_NAME[NVIEWS] = {"Garden", "Pad", "Bells", "Arp", "Bass", "Pulse", "Space", "Setup", "Chords", "Keys", "Deal", "Rings", "Dream"};
static const uint8_t VIEW_BTN[NVIEWS] = {B_HOME, B_ENV, B_ARP, B_ARP, B_LFO, B_SEQ, B_FX, B_GLO, B_SEL, B_SEL, B_SEL, B_SEQ, B_HOME};
/* the four knobs of each page: a parameter, or one of these; and what the knob is called there */
enum { K_LEDS = 100, K_CPU, K_NONE };
static const uint8_t VIEW_KNOB[NVIEWS][4] = {
    {P_TEMPO, P_EVOLVE, P_MOTION, P_DRIFT},
    {P_PAD, P_SWELL, P_BRIGHT, P_PADTONE},
    {P_BELLS, P_DENSITY, P_BTONE, P_BCOLOR},
    {P_ARP, P_ARATE, P_ARANGE, P_APOLY},
    {P_BASS, P_BASSOCT, P_GLIDE, P_FIFTH},
    {P_PULSE, P_HITS, P_SWING, P_HATS},
    {P_ECHO, P_ETIME, P_REVERB, P_SIZE},
    {P_ROOT, P_TUNE, P_MIDI, K_LEDS},
    {P_SLOT1, P_SLOT2, P_SLOT3, P_SLOT4},
    {P_RSLOT1, P_RSLOT2, P_RSLOT3, P_RSLOT4},
    {P_DEAL, P_DCOUNT, P_DSPICE, P_DFORM},
    {P_RHITS0, P_RHITS1, P_RHITS2, P_RMOVES},
    {P_DREAM, P_DRANGE, P_DCAP, P_SEED},
};
static const char *const KNOB_LABEL[NVIEWS][4] = {
    {"Tempo", "Evolve", "Motion", "Drift"},
    {"Level", "Swell", "Bright", "Colour"},
    {"Level", "Density", "Tone", "Colour"},
    {"Arp", "Rate", "Range", "Poly"},
    {"Level", "Octave", "Glide", "Fifth"},
    {"Level", "Hits", "Swing", "Hats"},
    {"Echo", "Echo time", "Reverb", "Size"},
    {"Root", "Tune", "MIDI out", "Lights"},
    {"Chord 1", "Chord 2", "Chord 3", "Chord 4"},
    {"Key 1", "Key 2", "Key 3", "Key 4"},
    {"Deal", "Chords", "Spice", "Form"},
    {"Thump", "Closed", "Open", "Moves"},
    {"Dream", "Range", "Spice cap", "Seed"},
};
/* SELECT held: a page's other knobs */
static const uint8_t VIEW_KNOB2[NVIEWS][4] = {
    {P_SPARK, P_SPLEN, P_SPOFT, P_SONG},
    {K_NONE, P_PADN, P_PSPREAD, K_NONE},
    {P_BOCT, K_NONE, K_NONE, K_NONE},
    {K_NONE, K_NONE, K_NONE, K_NONE},
    {K_NONE, P_BASSN, P_BPLUCK, K_NONE},
    {K_NONE, K_NONE, K_NONE, K_NONE},
    {P_SWOOSH, P_SWLEN, P_SWOFT, P_SWTONE},
    {K_NONE, K_NONE, K_NONE, K_NONE},
    {P_LEN1, P_LEN2, P_LEN3, P_LEN4},
    {P_RLEN1, P_RLEN2, P_RLEN3, P_RLEN4},
    {P_RDEAL, P_RDCOUNT, P_RDSPICE, P_RDFORM},
    {P_RTURN0, P_RTURN1, P_RTURN2, K_NONE},
    {K_NONE, K_NONE, K_NONE, K_NONE},
};
static const char *const KNOB_LABEL2[NVIEWS][4] = {
    {"Sparkle", "Length", "Often", "Song"},
    {"", "Notes", "Spread", ""},
    {"Octave", "", "", ""},
    {"", "", "", ""},
    {"", "Notes", "Pluck", ""},
    {"", "", "", ""},
    {"Swoosh", "Length", "Often", "Tone"},
    {"", "", "", ""},
    {"Bars 1", "Bars 2", "Bars 3", "Bars 4"},
    {"Bars 1", "Bars 2", "Bars 3", "Bars 4"},
    {"Key deal", "Keys", "Spice", "Form"},
    {"Thump", "Closed", "Open", ""},
    {"", "", "", ""},
};

/* the screen's colours (Nightscape FM): LowCrabTV's photograph of a pine-lined road at the edge of dusk. The
 * night sky behind it all; the dusk teal behind the pines for the pad (the chord) and the scale, the headlit road
 * for the bells, the blue hour for the bass, the ember band at the horizon for the pulse, and the amber of the
 * curve signs (another night's photograph) for the page and the knob being turned. Each layer has a dark tint for
 * the shapes at rest. (As AMB-1 the colours were the cover of Ambient 1: Music for Airports.) */
#define K_BG RGB(11, 19, 34)
#define K_PANEL RGB(18, 29, 48)
#define K_LINE RGB(38, 54, 80)
#define K_DIM RGB(122, 138, 166)
#define K_TEXT RGB(205, 214, 232)
#define K_ACC RGB(214, 168, 24)          /* the accent: the page, the knob being turned, a message's word */
#define K_BELL RGB(136, 163, 215)
#define K_BELL_T RGB(40, 54, 84)
#define K_PAD RGB(58, 143, 174)
#define K_PAD_T RGB(22, 52, 70)
#define K_PULSE RGB(223, 140, 9)
#define K_PULSE_T RGB(70, 46, 14)
#define K_BASS RGB(92, 124, 182)
#define K_BASS_T RGB(28, 40, 66)
#define K_FROST RGB(159, 211, 224)
#define K_FROST_T RGB(30, 58, 70)

static struct {
    uint8_t view;
    uint32_t btn, keys, btn_used;
    uint32_t enc_t[NE];
    uint8_t enc_run[NE];               /* detents in the current quick run, per knob (accel) */
    uint32_t down_t[NB];               /* when each button went down */
    uint32_t turn_t;                   /* the last knob turn */
    uint8_t help;                      /* a button's card is up: the button + 1 */
    uint8_t reset_done;                /* SAVE held: the piece went back to its recipe this hold */
    uint8_t dirty;                     /* the project changed since the last save */
    uint32_t act_t, saved_t;
    char msg[2][24];
    uint32_t msg_until;
    int8_t touched;                    /* the knob turned last (0..3), shown bright for a moment */
    uint32_t touch_until;
    uint32_t frame;
    uint32_t sig[3];                   /* what each band shows: redrawn when it changes */
    uint16_t dream_now;                /* the number of the dream playing (Dream); 0: none */
    uint8_t hold;                      /* a backup streams: no saving */
    uint32_t boot_t;                   /* the boot card's second part is due; 0: shown, or something said first */
    uint8_t hand;                      /* Seed, Deal, Key deal turned since Dream dealt (1, 2, 4): they show their own */
} ui;

/* the screen reader (announce.c; speak() below): a message went to it this frame, so speak() leaves what it
 * changed be; and the transport as last said (PLAY says its own: the engine takes it a block later) */
static uint8_t sp_said;
static struct {
    uint8_t playing;
    uint32_t play_t;                   /* when PLAY (or the start) set it: the engine catches up within ms */
} sp_tp;
static void sp_piece(void);
static void msg_text(const char *a, const char *b, char *t)   /* the message as a reader gets it: "a b" */
{
    uint32_t i, k = 0;
    for (i = 0; i < 23u && a && a[i]; i++)
        t[k++] = a[i];
    if (b && b[0])
        t[k++] = ' ';
    for (i = 0; i < 23u && b && b[i]; i++)
        t[k++] = b[i];
    while (k && t[k - 1] == '.')               /* "UPDATE MODE IN 3..": the dots are for the eye */
        k--;
    t[k] = 0;
}
/* a message for a moment in the header; flags: how a screen reader gets it (0: the caller tells it) */
static void say_as(const char *a, const char *b, uint32_t flags)
{
    uint32_t i;
    char t[48];
    for (i = 0; i < 23u && a && a[i]; i++)
        ui.msg[0][i] = a[i];
    ui.msg[0][i] = 0;
    for (i = 0; i < 23u && b && b[i]; i++)
        ui.msg[1][i] = b[i];
    ui.msg[1][i] = 0;
    ui.msg_until = plat_ms() + 1300u;
    if (flags && ann_on()) {
        msg_text(a, b, t);
        ann_say(t, 0, flags);
        sp_said = 1;
    }
}
static void say(const char *a, const char *b) { say_as(a, b, ANN_NOW); }
/* something did not happen: a screen reader says it whole (what follows does not cut it off) */
static void warn(const char *a, const char *b) { say_as(a, b, ANN_NOW | ANN_KEEP); }
void ui_say(const char *a, const char *b) { say(a, b); }

static void itoa_u(uint32_t v, char *b)
{
    char t[12];
    int n = 0;
    do {
        t[n++] = (char)('0' + v % 10u);
        v /= 10u;
    } while (v);
    while (n)
        *b++ = t[--n];
    *b = 0;
}

static void mark_dirty(void) { ui.dirty = 1; }
int ui_dirty(void) { return ui.dirty != 0; }

static int pool_count(uint32_t p)
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return n;
}
static int any_step(int first)                 /* any of a page's four steps filled: P_SLOT1 or P_RSLOT1 */
{
    int i;
    for (i = 0; i < EN_NSLOT; i++)
        if (proj.par[first + i])
            return 1;
    return 0;
}
static int root_now(void)                      /* the root playing: the rotator's, while it has a step */
{
    return any_step(P_RSLOT1) ? en_root : proj.par[P_ROOT];
}

static uint32_t pool_abs(void)                 /* the pool as absolute pitch classes */
{
    uint32_t p = proj.pool, r = (uint32_t)root_now();
    return ((p << r) | (p >> (12u - r))) & 0xFFFu;
}
static const char *mode_name(void)
{
    return proj.pool == EN_MODE_POOL[proj.par[P_MODE]] ? EN_MODE_NAME[proj.par[P_MODE]] : "Custom";
}

/* ------------------------------------------------------------ values --- */
static int knob_get(int k)
{
    if (k < P_NPARAMS)
        return proj.par[k];
    if (k == K_LEDS)
        return proj.leds;
    if (k == K_CPU)
        return (int)plat_cpu_pct();
    return 0;
}
static int is_slot(int k) { return k >= P_SLOT1 && k < P_SLOT1 + EN_NSLOT; }
static int hand_bit(int k) { return k == P_SEED ? 1 : k == P_DEAL ? 2 : k == P_RDEAL ? 4 : 0; }
static int dream_drives(int k) { return hand_bit(k) && proj.par[P_DREAM] && !(ui.hand & hand_bit(k)); }
static int is_rslot(int k) { return k >= P_RSLOT1 && k < P_RSLOT1 + EN_NSLOT; }
/* where a Roots step sits on its knob: Off, then the piece's own root, then on up round the notes */
static int rslot_pos(int v) { return v ? (v - 1 - proj.par[P_ROOT] + 12) % 12 + 1 : 0; }
static int rslot_val(int pos) { return pos ? (pos - 1 + proj.par[P_ROOT]) % 12 + 1 : 0; }
static void knob_range(int k, int *lo, int *hi)
{
    if (k < P_NPARAMS) {
        *lo = en_param_info(k)->lo;
        *hi = is_slot(k) ? pool_count(proj.pool) * EN_NSHAPE : en_param_info(k)->hi;   /* a slot: the scale's chords */
    } else {
        *lo = 0;
        *hi = k == K_LEDS ? 2 : k == K_CPU ? 100 : 0;
    }
}
static const char *knob_name(int k)
{
    if (k < P_NPARAMS)
        return en_param_info(k)->name;
    return k == K_LEDS ? "Lights" : k == K_CPU ? "CPU" : "";
}
static void knob_text(int k, char *b)
{
    int v = knob_get(k);
    if (dream_drives(k)) {                       /* Dream deals it: says so, until a hand turns it */
        const char *n = "Dream";
        while ((*b++ = *n++))
            ;
    } else if (k == P_DFORM) {                   /* its shape, and the bars the chords will come to: "Even 32" */
        static const char *const SHAPE[3] = {"Even ", "Home ", "Turn "};
        const char *n = SHAPE[(unsigned)v / 3u < 3u ? v / 3 : 0];
        while (*n)
            *b++ = *n++;
        itoa_u((uint32_t)en_deal_cycle(proj.par[P_DCOUNT], v, proj.par[P_TEMPO]), b);
    } else if (k == P_DRIFT && any_step(P_SLOT1)) {   /* the slate has the chords */
        const char *n = "Chords";
        while ((*b++ = *n++))
            ;
    } else if (k == P_SEED && !v) {              /* a new garden each time */
        const char *n = "Wild";
        while ((*b++ = *n++))
            ;
    } else if (k == P_SLOT1 && !any_step(P_SLOT1)) {    /* an empty page says who's driving */
        const char *n = "Drift";
        while ((*b++ = *n++))
            ;
    } else if (k == P_RSLOT1 && !any_step(P_RSLOT1)) {
        const char *n = EN_NOTE_NAME[proj.par[P_ROOT]];
        b[0] = 'K', b[1] = 'e', b[2] = 'y', b[3] = ' ';
        b += 4;
        while ((*b++ = *n++))
            ;
    } else if (k == P_ROOT && any_step(P_RSLOT1)) {   /* the Keys page has it */
        const char *n = "Keys";
        while ((*b++ = *n++))
            ;
    } else if (is_slot(k) && v) {                /* a slot says its chord: "Dm7" */
        int pcs[4], n = en_chord_shape(proj.pool, root_now(), (v - 1) / EN_NSHAPE, (v - 1) % EN_NSHAPE, pcs);
        en_chord_name(pcs, n, b);
    } else if (k < P_NPARAMS) {
        en_param_text(k, v, b);
    } else if (k == K_LEDS) {                    /* On: the keys, and the unlit buttons glow; Keys: no glow */
        const char *n = v == 2 ? "Keys" : v ? "On" : "Off";
        while ((*b++ = *n++))
            ;
    } else if (k == K_CPU) {
        itoa_u((uint32_t)v, b);
        while (*b)
            b++;
        *b++ = '%';
        *b = 0;
    } else {
        b[0] = 0;
    }
}

static uint32_t shape_mask(int deg, int s)      /* a chord's notes, as a set */
{
    int pcs[4], n = en_chord_shape(proj.pool, root_now(), deg, s, pcs), i;
    uint32_t m = 0;
    for (i = 0; i < n; i++)
        m |= 1u << pcs[i];
    return m;
}
static int slot_dup(int v)                      /* the same notes as a shape before it on this root: the scale's short */
{
    int deg = (v - 1) / EN_NSHAPE, s = (v - 1) % EN_NSHAPE, t;
    uint32_t m = shape_mask(deg, s);
    for (t = 0; t < s; t++)
        if (shape_mask(deg, t) == m)
            return 1;
    return 0;
}

/* the dealers: a number writes the slate's or the rotator's four steps, which can then be turned as any */
static int slate_bars(void)                    /* what the chords come to; 0: none, the chord drifts */
{
    int i, b = 0;
    for (i = 0; i < EN_NSLOT; i++)
        if (proj.par[P_SLOT1 + i])
            b += EN_SLATE_BARS[proj.par[P_LEN1 + i]];
    return b;
}
static void put(int k, int v)
{
    if (proj.par[k] != v) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
    }
}
static void deal_keys(void)
{
    int16_t r[EN_NSLOT], l[EN_NSLOT];
    char t[24];
    int i, n = 0;
    if (!proj.par[P_RDEAL])
        return;
    en_deal_roots(proj.par[P_ROOT], proj.par[P_RDEAL], proj.par[P_RDCOUNT], proj.par[P_RDSPICE], proj.par[P_RDFORM],
                  slate_bars(), proj.par[P_SONG] ? proj.par[P_TEMPO] * 150 / 240 : 0, r, l);   /* Song: a key a section */
    for (i = 0; i < EN_NSLOT; i++) {
        put(P_RSLOT1 + i, r[i]);
        put(P_RLEN1 + i, l[i]);
        if (r[i]) {
            const char *s = EN_NOTE_NAME[(r[i] - 1) % 12];
            if (n)
                t[n++] = ' ';
            while (*s && n < 22)
                t[n++] = *s++;
        }
    }
    t[n] = 0;
    en_restart(3);                               /* the journey from its first key, the chords with it */
    say("KEYS", t);
}
static char dealt[24];                          /* what the chord dealer dealt last, named */
static void deal_chords(void)
{
    int16_t s[EN_NSLOT], l[EN_NSLOT];
    char t[24], nm[12];
    int i, n = 0;
    if (!proj.par[P_DEAL])
        return;
    en_deal_chords(proj.pool, proj.par[P_DEAL], proj.par[P_DCOUNT], proj.par[P_DSPICE], proj.par[P_DFORM],
                   proj.par[P_TEMPO], s, l);
    for (i = 0; i < EN_NSLOT; i++) {
        put(P_SLOT1 + i, s[i]);
        put(P_LEN1 + i, l[i]);
        if (s[i]) {
            int pcs[4], c = en_chord_shape(proj.pool, root_now(), (s[i] - 1) / EN_NSHAPE, (s[i] - 1) % EN_NSHAPE, pcs);
            const char *p = en_chord_name(pcs, c, nm);
            if (n && n < 22)
                t[n++] = ' ';
            while (*p && n < 22)
                t[n++] = *p++;
        }
    }
    t[n] = 0;
    for (i = 0; i <= n; i++)
        dealt[i] = t[i];
    en_restart(1);                               /* heard from the top at the next bar, not after the step playing */
    if (proj.par[P_RDEAL])                       /* the keys last whole passes of these */
        deal_keys();
    say("DEALT", t);
}

/* Dream: one number deals the whole piece: the garden's Seed, both dealers (their Spice no more than the cap) and,
 * with Range, the piece's sound, each knob within a window round the recipe. A part the recipe has off stays off;
 * the scale, the tuning and MIDI are never touched; the tempo moves at most 15%. Wild deals a new one at each PLAY
 * and each piece; turned up from Wild, the knob keeps the one playing. */
static uint32_t dream_rng = 1u;
static uint32_t dream_r(void)
{
    uint32_t x = dream_rng;
    x ^= x << 13, x ^= x >> 17, x ^= x << 5;
    dream_rng = x;
    return x;
}
static float dream_f(void) { return (float)(dream_r() >> 8) * (1.0f / 16777216.0f); }
static int dream_fresh(void)                     /* a new number: the clock, and what the last dream left saved */
{
    uint32_t x = plat_ms() * 2654435761u ^ ui.frame * 0x9E3779B9u ^ (uint32_t)proj.par[P_SEED] * 0x85EBCA6Bu ^
                 (uint32_t)proj.par[P_DEAL] * 0xC2B2AE35u ^ 0x5BD1E995u;
    int n;
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 13;
    n = 1 + (int)(x % 999u);
    return n == ui.dream_now ? n % 999 + 1 : n;
}
static int dream_level(int p)
{
    return p == P_PAD || p == P_BELLS || p == P_BASS || p == P_PULSE || p == P_SPARK || p == P_SWOOSH || p == P_HATS;
}
static void dream_deal(int num)
{
    /* one character for the whole dream: its number, low calm, high lively (a little either way), steering every choice
     * together. Calm: slower, long chords, few keys, sparse bells, gentle Evolve and Motion, more reverb and echo, the
     * bass held, the rings still, Swoosh before Sparkle. Lively: the other way. Range how far the sound moves */
    static const uint8_t SOUND[] = {P_PAD,    P_BELLS,  P_BASS,  P_PULSE, P_DENSITY, P_EVOLVE, P_MOTION, P_HITS,
                                    P_ECHO,   P_ETIME,  P_REVERB, P_SIZE, P_BRIGHT,  P_SWELL,  P_BTONE,  P_TEMPO,
                                    P_SWING,  P_HATS,   P_GLIDE, P_FIFTH};
    static const int8_t LIFT[sizeof SOUND] = {0, 1, 0, 1, 1, 1, 1, 1, -1, -1, -1, -1, 1, -1, 1, 1, 1, 1, -1, 0};
    static const char *const MOOD[4] = {" still", " calm", " moving", " lively"};
    int16_t rec[P_NPARAMS];
    uint16_t pl;
    const int cap = proj.par[P_DCAP], range = proj.par[P_DRANGE];
    const float r = (float)range * 0.01f;
    uint32_t x = (uint32_t)num * 2654435761u ^ 0xD4EA3u, i;
    float ch, live;
    char head[24] = "DREAM ";
    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    dream_rng = x ? x : 1u;
    ch = (float)(num - 1) / 998.0f + (dream_f() - 0.5f) * 0.2f;
    ch = ch < 0.0f ? 0.0f : ch > 1.0f ? 1.0f : ch;
    live = 2.0f * ch - 1.0f;                     /* -1 still .. +1 lively */
    en_piece(proj.piece, rec, &pl);
    put(P_SEED, 1 + (int)(dream_r() % 999u));
    put(P_DEAL, 1 + (int)(dream_r() % 999u));
    {                                            /* the chords: fewer and longer the calmer */
        int n = 2 + (int)(ch * 2.0f + dream_f());
        put(P_DCOUNT, n > 4 ? 4 : n);
        put(P_DSPICE, (int)((0.3f + 0.7f * dream_f()) * (float)cap + 0.5f));
        put(P_DFORM, (int)(dream_r() % 3u) * 3 + (ch < 0.33f ? 2 : ch < 0.66f ? 1 : 0));
    }
    if (ch > 0.5f && dream_f() < (ch - 0.5f) * 0.9f) {   /* a change of key: rare, as in songs; never when calm */
        const int lift = dream_f() < 0.5f;
        put(P_RDEAL, 1 + (int)(dream_r() % 999u));
        put(P_RDCOUNT, lift ? 2 : 2 + (int)(dream_r() % 2u));   /* the lift; or home, a neighbour (or two), home */
        put(P_RDSPICE, (int)(0.25f * (float)cap * dream_f() + 0.5f));   /* near keys */
        put(P_RDFORM, lift ? 3 : 0);
    } else {
        put(P_RDEAL, 0);
        for (i = 0; i < EN_NSLOT; i++)
            put(P_RSLOT1 + (int)i, 0);
    }
    if (range > 0)
        for (i = 0; i < sizeof SOUND; i++) {     /* the sound round the recipe, leaning with the character */
            const int p = SOUND[i], c = rec[p], lo = en_param_info(p)->lo, hi = en_param_info(p)->hi;
            float u = 0.5f * (dream_f() * 2.0f - 1.0f) + 0.6f * (float)LIFT[i] * live, d;
            int v;
            u = u < -1.0f ? -1.0f : u > 1.0f ? 1.0f : u;
            if (dream_level(p) && !c)
                continue;                            /* off in the recipe: off */
            d = p == P_TEMPO ? u * (float)c * 0.15f * r : p == P_ETIME ? u * 2.0f * r : u * 0.35f * (float)(hi - lo) * r;
            v = c + (int)(d + (d < 0.0f ? -0.5f : 0.5f));
            v = v < lo ? lo : v > hi ? hi : v;
            if (dream_level(p) && v < 10)
                v = 10;                              /* on in the recipe: still heard */
            if (p == P_HITS && v > 45)
                v = 45;                              /* the kick sparse: two a bar at most */
            put(p, v);
        }
    if (range > 0) {                             /* the instruments and voicings: more often away as Range grows */
        if (rec[P_PAD]) {
            const int away = dream_f() < 0.6f * r, other = (rec[P_PADTONE] + 1 + (int)(dream_r() % 3u)) % 4;
            const int notes = range >= 30 && dream_f() < 0.5f * r ? 2 + (int)(dream_r() % 3u) : rec[P_PADN];
            put(P_PADTONE, away ? other : rec[P_PADTONE]);
            put(P_PADN, notes);
            if (dream_f() < 0.7f * r) {              /* Spread: wider the calmer */
                const int w = (int)((1.0f - ch) * 2.6f + dream_f() * 1.4f);
                put(P_PSPREAD, w > 3 ? 3 : w);
            } else {
                put(P_PSPREAD, rec[P_PSPREAD]);
            }
        }
        if (rec[P_BELLS]) {
            static const float W[EN_NBCOLOR] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f};   /* Lead, less often */
            int c = rec[P_BCOLOR], o = rec[P_BOCT], k;
            if (dream_f() < 0.6f * r) {
                float sum = 0.0f, y;
                for (k = 0; k < EN_NBCOLOR; k++)
                    sum += k == rec[P_BCOLOR] ? 0.0f : W[k];
                y = dream_f() * sum;
                for (k = 0; k < EN_NBCOLOR; k++)
                    if (k != rec[P_BCOLOR] && (y -= W[k]) < 0.0f)
                        break;
                c = k < EN_NBCOLOR ? k : EN_NBCOLOR - 1;
            }
            if (dream_f() < 0.35f * r)
                o += dream_f() < 0.5f ? -1 : 1;
            if (c == 5)                              /* Lead: an octave down, now and then two: never piercing */
                o = dream_f() < 0.85f ? -1 : -2;
            put(P_BCOLOR, c);
            put(P_BOCT, o < -2 ? -2 : o > 2 ? 2 : o);
            if (c == 5 && proj.par[P_DENSITY] > 45)
                put(P_DENSITY, 45);                 /* the solo sings rather than runs */
        }
        /* the flourishes, subtle: Sparkle the livelier, Swoosh the calmer */
        if (rec[P_BELLS] && dream_f() < 0.35f * r * (0.4f + 1.2f * ch)) {
            put(P_SPARK, 10 + (int)(dream_f() * 30.0f));
            put(P_SPLEN, 20 + (int)(dream_r() % 61u));
            put(P_SPOFT, 20 + (int)(dream_r() % 61u));
        } else {
            put(P_SPARK, rec[P_SPARK]);
        }
        if (dream_f() < 0.35f * r * (1.6f - 1.2f * ch)) {
            put(P_SWOOSH, 5 + (int)(dream_f() * 35.0f));
            put(P_SWLEN, 20 + (int)(dream_r() % 61u));
            put(P_SWOFT, 20 + (int)(dream_r() % 61u));
            put(P_SWTONE, (int)(dream_r() % 101u));
        } else {
            put(P_SWOOSH, rec[P_SWOOSH]);
        }
        if (rec[P_BELLS] && dream_f() < 0.3f * r * (0.3f + 1.4f * ch)) {   /* the arp, through the bells */
            put(P_ARP, 1 + (int)(dream_r() % (uint32_t)(EN_NARP - 1)));
            put(P_ARATE, (int)(dream_r() % (uint32_t)EN_NARATE));
            put(P_ARANGE, 1 + (int)(dream_r() % 3u));
            put(P_APOLY, ch > 0.6f && dream_f() < 0.3f ? 1 + (int)(dream_r() % (uint32_t)(EN_NAPOLY - 1)) : 0);
        } else {
            put(P_ARP, rec[P_ARP]);
            put(P_APOLY, rec[P_APOLY]);
        }
        if (rec[P_BASS]) {                           /* the bass: held, or steady notes, more a bar the livelier */
            static const uint8_t BN[4] = {0, 12, 37, 62};
            int o = rec[P_BASSOCT];
            if (dream_f() < 0.5f * r * (1.0f - ch))
                put(P_BASSN, -1);                    /* a pedal: the key's home under every chord, the calmer */
            else
                put(P_BASSN, dream_f() < 0.4f * r * (0.3f + 1.4f * ch) ? BN[1 + (int)(ch * 2.99f)] : rec[P_BASSN]);
            if (dream_f() < 0.3f * r)
                o += dream_f() < 0.5f ? -1 : 1;
            put(P_BASSOCT, o < -1 ? -1 : o > 1 ? 1 : o);
            {                                        /* Pluck: humming when calm, plucked a little when lively */
                const int pv = rec[P_BPLUCK] + (int)(((ch * 60.0f - 15.0f) + (dream_f() - 0.5f) * 20.0f) * r);
                put(P_BPLUCK, pv < 0 ? 0 : pv > 100 ? 100 : pv);
            }
        }
        {                                            /* the scale: calm, the open bright ones; lively, the darker */
            static const float CALM[EN_NMODES] = {2.0f, 2.0f, 0.3f, 3.0f, 1.0f, 1.0f, 3.0f, 1.0f};
            static const float LIVE[EN_NMODES] = {1.0f, 2.5f, 0.6f, 1.0f, 3.0f, 2.5f, 1.0f, 3.0f};
            int md = rec[P_MODE], k;
            if (dream_f() < 0.6f * r) {
                float sum = 0.0f, y;
                for (k = 0; k < EN_NMODES; k++)
                    sum += CALM[k] * (1.0f - ch) + LIVE[k] * ch;
                y = dream_f() * sum;
                for (k = 0; k < EN_NMODES - 1; k++)
                    if ((y -= CALM[k] * (1.0f - ch) + LIVE[k] * ch) < 0.0f)
                        break;
                md = k;
            }
            if (md != proj.par[P_MODE] || proj.pool != EN_MODE_POOL[md]) {
                put(P_MODE, md);
                proj.pool = EN_MODE_POOL[md];
                en_pool(proj.pool);
            }
        }
        if (rec[P_PULSE]) {                          /* the rings: turns now and then; Still the calmer */
            int d;
            for (d = 0; d < EN_NDRUM; d++)
                put(P_RTURN0 + d, dream_f() < 0.4f * r ? (int)(dream_r() % EN_DRUM_N[d]) : rec[P_RTURN0 + d]);
            put(P_RMOVES, dream_f() < 0.25f * r * (1.6f - 1.2f * ch));
        }
    }
    ui.dream_now = (uint16_t)num;
    ui.hand = 0;
    deal_chords();
    itoa_u((uint32_t)num, head + 6);
    {                                            /* "DREAM 417 calm" */
        const char *m = MOOD[ch < 0.25f ? 0 : ch < 0.5f ? 1 : ch < 0.75f ? 2 : 3];
        char *h = head;
        while (*h)
            h++;
        while (*m && h < head + sizeof head - 1)
            *h++ = *m++;
        *h = 0;
    }
    say(head, dealt);
}

static void knob_set(int k, int v)
{
    int lo, hi;
    if (k == K_CPU || k == K_NONE)
        return;
    knob_range(k, &lo, &hi);
    v = v < lo ? lo : v > hi ? hi : v;
    if (is_slot(k) && v != knob_get(k)) {       /* a turn never lands on the same chord under another shape */
        int dir = v > knob_get(k) ? 1 : -1;
        while (v > 0 && v <= hi && slot_dup(v))
            v += dir;
        if (v > hi)
            return;
    }
    if (v == knob_get(k))
        return;
    if (k < P_NPARAMS) {
        proj.par[k] = (int16_t)v;
        en_set(k, v);
        if (k >= P_DEAL && k <= P_DFORM)
            deal_chords();
        else if ((k >= P_RDEAL && k <= P_RDFORM) || (k == P_SONG && proj.par[P_RDEAL]))
            deal_keys();
        else if (k == P_DREAM && v == 1)
            dream_deal(dream_fresh());
        else if (k == P_DREAM && v >= 2 && v - 1 != ui.dream_now)
            dream_deal(v - 1);
        else if ((k == P_DRANGE || k == P_DCAP) && proj.par[P_DREAM] && ui.dream_now)
            dream_deal(ui.dream_now);                /* the same dream, within the new bounds */
    } else if (k == K_LEDS) {
        proj.leds = (uint8_t)v;
    }
    mark_dirty();
}

/* knob acceleration by speed (X0X's): a few clicks are one step each however quick, so small
 * corrections stay exact; a longer quick turn speeds up to 8 a detent, so half a turn sweeps 0-100.
 * Small ranges never accelerate. */
static uint8_t accel_off;              /* the host simulator's "spin": exact steps */
static int32_t accel(int role, int32_t s, int range)
{
    uint32_t now = plat_ms(), dt = now - ui.enc_t[role], a = (uint32_t)(s < 0 ? -s : s), m;
    ui.enc_t[role] = now;
    if (!a)
        return s;
    if (a > 1)
        dt /= a;                                      /* per detent */
    ui.enc_run[role] = (uint8_t)(dt < 60u ? (ui.enc_run[role] + a > 255u ? 255u : ui.enc_run[role] + a) : a);
    if (range <= 24 || accel_off || ui.enc_run[role] < 4u)
        return s;
    m = dt < 12u ? 8u : dt < 25u ? 5u : dt < 45u ? 3u : 1u;
    if (range < 100 && m > 3u)
        m = 3u;
    return s * (int32_t)m;
}

static void turn(int role, int k, int32_t e)
{
    int lo, hi;
    if (!e || k == K_NONE || k == K_CPU)
        return;
    if (k == P_ROOT && any_step(P_RSLOT1)) {     /* it wouldn't be heard: say where it is */
        say("Root", "Keys page");
        return;
    }
    if (k == P_DRIFT && any_step(P_SLOT1)) {     /* the same: the slate has the chords */
        say("Drift", "Chords page");
        return;
    }
    if (proj.par[P_DREAM])                       /* a hand on what Dream deals: its own from now */
        ui.hand |= (uint8_t)hand_bit(k);
    knob_range(k, &lo, &hi);
    if (k == P_DREAM && knob_get(k) < 2) {       /* Off, Wild: a click is a step; up from Wild, the dream kept */
        if (e > 0)
            knob_set(k, knob_get(k) ? (ui.dream_now ? ui.dream_now + 1 : 2) : 1);
        else if (knob_get(k))
            knob_set(k, 0);
        return;
    }
    if (k >= P_RHITS0 && k <= P_RHITS2 && !knob_get(k)) {   /* up from Auto: the count playing now, kept */
        if (e > 0)
            knob_set(k, 1 + en_ring_auto(k - P_RHITS0, proj.par[P_HITS], proj.par[P_HATS]));
        return;
    }
    if (k == P_SEED && !knob_get(k)) {           /* up from Wild: the garden growing now, kept */
        if (e > 0)
            knob_set(k, en_seed_now ? en_seed_now : 1);
        return;
    }
    if (is_rslot(k)) {
        int pos = rslot_pos(knob_get(k)) + accel(role, e, hi - lo);
        knob_set(k, rslot_val(pos < 0 ? 0 : pos > 12 ? 12 : pos));
        return;
    }
    knob_set(k, knob_get(k) + accel(role, e, hi - lo));
}

/* what a page's knob sets now: on the Chords page with SELECT held, the steps' bars */
static int layer_held(int view)                  /* SELECT held, on a page that has other knobs */
{
    const uint8_t *k = VIEW_KNOB2[view];
    return (ui.btn >> B_SEL & 1u) && (k[0] != K_NONE || k[1] != K_NONE || k[2] != K_NONE || k[3] != K_NONE);
}
static int view_knob(int view, int i) { return layer_held(view) ? VIEW_KNOB2[view][i] : VIEW_KNOB[view][i]; }
static const char *view_label(int view, int i) { return layer_held(view) ? KNOB_LABEL2[view][i] : KNOB_LABEL[view][i]; }

/* ------------------------------------------------------------- the pool --- */
static void toggle_note(int pc)                 /* a key: its note in or out of the scale */
{
    int rel = (pc - root_now() + 12) % 12;
    uint32_t p = proj.pool ^ (1u << rel);
    if (rel == 0) {
        warn("THE ROOT STAYS", "(SETUP)");
        return;
    }
    if (pool_count(p) < 3) {
        warn("3 NOTES", "AT LEAST");
        return;
    }
    proj.pool = (uint16_t)p;
    en_pool(p);
    say(p >> rel & 1u ? "ADDED" : "REMOVED", EN_NOTE_NAME[pc]);
    mark_dirty();
}

static void set_mode(int m)
{
    m = (m % EN_NMODES + EN_NMODES) % EN_NMODES;
    proj.par[P_MODE] = (int16_t)m;
    proj.pool = EN_MODE_POOL[m];
    en_set(P_MODE, m);
    en_pool(proj.pool);
    if (proj.par[P_DEAL])                        /* the Deal again, in this scale's own chords: a new world, composed */
        deal_chords();
    say("MODE", EN_MODE_NAME[m]);
    mark_dirty();
}

static void set_piece(int i)                    /* the piece playing kept as it is; piece i as it was left */
{
    project_piece(i);
    project_apply();
    if (proj.par[P_DREAM] == 1)                  /* Wild: a new dream for it */
        dream_deal(dream_fresh());
    else
        ui.dream_now = (uint16_t)(proj.par[P_DREAM] >= 2 ? proj.par[P_DREAM] - 1 : 0);
    en_seed(plat_ms());
    en_regrow();
    say_as("PIECE", piece_name(proj.piece), 0);
    sp_piece();                                  /* a screen reader: the piece with its key and tempo */
    mark_dirty();
}

/* ------------------------------------------------------------ buttons --- */
static void set_view(int v)
{
    ui.view = (uint8_t)v;
    ui.touched = -1;
}

static void button(int b)
{
    int v;
    if (b == B_SEL) {                            /* the two rotators and their dealer: Chords, Roots, Deal, Chords */
        set_view(ui.view == V_CHORDS ? V_ROOTS : ui.view == V_ROOTS ? V_DEAL : V_CHORDS);
        return;
    }
    if (b == B_HOME) {                           /* the dream first; again, the garden; again, the dream */
        set_view(ui.view == V_DREAM ? V_GARDEN : V_DREAM);
        return;
    }
    if (b == B_ARP) {                            /* the bells, then their arpeggio, then the bells */
        set_view(ui.view == V_BELLS ? V_ARP : V_BELLS);
        return;
    }
    if (b == B_SEQ) {                            /* the pulse, then its rings, then the pulse */
        set_view(ui.view == V_PULSE ? V_RINGS : V_PULSE);
        return;
    }
    for (v = 0; v < NVIEWS; v++)                 /* a page's button: its page */
        if (VIEW_BTN[v] == b) {
            set_view(v);
            return;
        }
    switch (b) {
    case B_EDIT:
        en_next();
        say("NEXT CHORD", en_playing ? "ON THE BAR" : 0);
        break;
    case B_PLAY:
        sp_tp.playing = !en_playing;
        sp_tp.play_t = plat_ms();
        if (en_playing) {
            en_play(0);
            say("RESTING", 0);
        } else {
            en_seed(plat_ms());
            if (proj.par[P_DREAM] == 1)          /* Wild: a new dream, its toast in place of GROWING */
                dream_deal(dream_fresh());
            en_play(1);
            if (proj.par[P_DREAM] != 1)
                say("GROWING", 0);
        }
        break;
    case B_REC:
        say("FREEZE", en_frozen ? "OFF" : "ON");
        en_freeze(!en_frozen);                   /* the engine takes it at its next block */
        break;
    case B_OCTDN: case B_OCTUP:
        knob_set(P_BOCT, proj.par[P_BOCT] + (b == B_OCTUP ? 1 : -1));
        {
            char t[8];
            en_param_text(P_BOCT, proj.par[P_BOCT], t);
            say("BELL OCTAVE", t);
        }
        break;
    case B_SAVE:
        if (project_save() == 0) {
            ui.dirty = 0;
            say("SAVED", 0);
        } else {
            warn("NOT SAVED", "FLASH ERROR");
        }
        break;
    default: break;
    }
}

/* MIDI in (USB and the TRS jack): channel 1 notes add or remove their note from the scale; clock and
 * start / stop drive the grid */
static void midi_in(void)
{
    uint32_t pkt;
    while (plat_midi_in(&pkt, 0)) {
        uint32_t st = (pkt >> 8) & 0xFFu, n = (pkt >> 16) & 0x7Fu, v = pkt >> 24;
        if (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)
            en_clock(st == 0xF8u ? EN_CLK_TICK : st == 0xFAu ? EN_CLK_START : st == 0xFBu ? EN_CLK_CONTINUE : EN_CLK_STOP);
        else if (st == 0x90u && v)
            toggle_note((int)(n % 12u));
    }
}

/* HELP CARDS (X0X's): hold one button alone for HELP_HOLD and a card says what it does. A key, a
 * turn or another button puts it away; letting go after its card does nothing else. PLAY acts on
 * its press, and SEL has no job here: neither has a card. */
#define HELP_HOLD 900u
#define RESET_HOLD 1500u
static void help_tick(uint32_t btn, uint32_t keys)
{
    uint32_t now = plat_ms();
    int i;
    if (ui.help) {
        uint32_t m = 1u << (ui.help - 1);
        if (btn != m || keys || (int32_t)(ui.turn_t - ui.down_t[ui.help - 1]) > 0)
            ui.help = 0;
        return;
    }
    if (!btn || (btn & (btn - 1u)) || keys)
        return;
    for (i = 0; i < NB; i++)
        if (btn == 1u << i && i != B_PLAY && !(ui.btn_used & btn) &&
            now - ui.down_t[i] >= (i == B_SEL ? 2u * HELP_HOLD : HELP_HOLD) &&   /* (SELECT: its layer first) */
            (int32_t)(ui.turn_t - ui.down_t[i]) <= 0) {
            ui.help = (uint8_t)(i + 1);
            ui.btn_used |= btn;                  /* its release now does nothing */
        }
}

static void input(void)
{
    uint32_t btn = plat_buttons(), keys = plat_keys(), ch, i;
    int32_t e;
    ch = btn ^ ui.btn;
    ui.btn = btn;
    for (i = 0; i < NB; i++) {
        uint32_t m = 1u << i;
        if (!(ch & m))
            continue;
        if (btn & m) {
            ui.btn_used &= ~m;
            ui.down_t[i] = plat_ms();
            if (i == B_SAVE)
                ui.reset_done = 0;
            if (i == B_PLAY)                   /* transport on the press */
                button((int)i);
        } else if (!(ui.btn_used & m) && i != B_PLAY) {
            button((int)i);
        }
    }
    ch = keys ^ ui.keys;
    ui.keys = keys;
    for (i = 0; i < NKEYS; i++)
        if ((ch & keys) >> i & 1u)
            toggle_note((int)((5u + i) % 12u));  /* key 0 is F3 */
    if (btn || keys || ch)
        ui.act_t = plat_ms();
    midi_in();
    if ((e = plat_enc(EN_SELECT)) != 0) {        /* SELECT: the pages; with HOME held, the tempo */
        if (btn >> B_HOME & 1u) {
            turn(EN_SELECT, P_TEMPO, e);
            ui.btn_used |= 1u << B_HOME;
            if (ui.view == V_GARDEN) {           /* Tempo is the Garden's first knob */
                ui.touched = 0;
                ui.touch_until = plat_ms() + 1200u;
            } else {
                char t[8];
                itoa_u((uint32_t)proj.par[P_TEMPO], t);
                say("TEMPO", t);
            }
        } else {
            int v = (int)ui.view + (e > 0 ? 1 : -1);
            if (v >= 0 && v < NVIEWS)
                set_view(v);
        }
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_ALGO)) != 0) {
        set_mode(proj.par[P_MODE] + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((e = plat_enc(EN_PRESET)) != 0) {
        set_piece(proj.piece + (e > 0 ? 1 : -1));
        ui.turn_t = plat_ms();
    }
    if ((btn >> B_SAVE & 1u) && !ui.reset_done && plat_ms() - ui.down_t[B_SAVE] > RESET_HOLD) {
        ui.reset_done = 1;                     /* held: the piece back to its recipe (not saved) */
        ui.btn_used |= 1u << B_SAVE;
        ui.help = 0;
        project_piece_reset();
        project_apply();
        en_regrow();
        say("RESET", piece_name(proj.piece));
        mark_dirty();
    }
    for (i = 0; i < 4; i++)
        if ((e = plat_enc(EN_K1 + (int)i)) != 0) {
            turn(EN_K1 + (int)i, view_knob(ui.view, (int)i), e);
            if (btn >> B_SEL & 1u)               /* SELECT held and used: its release does nothing */
                ui.btn_used |= 1u << B_SEL;
            ui.touched = (int8_t)i;
            ui.touch_until = plat_ms() + 1200u;
            ui.act_t = ui.turn_t = plat_ms();
        }
    help_tick(btn, keys);
}

/* ------------------------------------------------------------- screen --- */
/* three bands: the header (28 px), the garden (144), the knobs (68) */
#define HDR_H 28
#define MAIN_Y 28
#define MAIN_H 144
#define KNB_Y 172
#define KNB_H 68

static uint32_t hash(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* a rectangle with rounded corners (radius r, up to 6) */
static void cv_round(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c)
{
    static const uint8_t IN[7][6] = {{0}, {1}, {1, 0}, {2, 1, 0}, {2, 1, 0, 0}, {3, 2, 1, 0, 0}, {4, 2, 1, 1, 0, 0}};
    int32_t j;
    if (r > 6)
        r = 6;
    if (r > h / 2)
        r = h / 2;
    cv_rect(x, y + r, w, h - 2 * r, c);
    for (j = 0; j < r; j++) {
        int32_t d = IN[r][j];
        cv_rect(x + d, y + j, w - 2 * d, 1, c);
        cv_rect(x + d, y + h - 1 - j, w - 2 * d, 1, c);
    }
}

/* a filled circle of radius r (0..6) centred on (cx, cy): HW[r][d] is the half width d rows from the middle */
static void cv_disc(int32_t cx, int32_t cy, int32_t r, uint16_t c)
{
    static const uint8_t HW[7][7] = {{0}, {1, 0}, {2, 1, 0}, {3, 2, 2, 0}, {4, 4, 3, 2, 0}, {5, 5, 4, 4, 3, 0},
                                     {6, 6, 5, 5, 4, 3, 0}};
    int32_t j;
    if (r <= 0) {
        cv_rect(cx, cy, 1, 1, c);
        return;
    }
    if (r > 6)
        r = 6;
    for (j = -r + 1; j < r; j++) {
        int32_t w = HW[r][j < 0 ? -j : j];
        cv_rect(cx - w, cy + j, 2 * w + 1, 1, c);
    }
}

static void text_c(int32_t cx, int32_t y, const felucca_font_t *f, const char *s, uint16_t c)
{
    cv_text(cx - text_w(f, s) / 2, y, f, s, c);
}

/* anti-aliased drawing (X0X's): a pixel blended over the canvas, a disc, and a knob's ring */
static void px_blend(int32_t x, int32_t y, uint16_t c, uint32_t a)
{
    uint16_t *q;
    uint32_t bg;
    y += cv_oy;
    if ((uint32_t)x >= cv_w || y < cv_y0 || y >= cv_y1 || !a)
        return;
    q = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    if (a >= 16u) {
        *q = swap16(c);
        return;
    }
    bg = swap16(*q);
    *q = swap16((uint32_t)(((((c >> 11) * a + (bg >> 11) * (16u - a)) >> 4) << 11) |
                           (((((c >> 5) & 63u) * a + ((bg >> 5) & 63u) * (16u - a)) >> 4) << 5) |
                           (((c & 31u) * a + (bg & 31u) * (16u - a)) >> 4)));
}

static void disc(float cx, float cy, float r, uint16_t c)
{
    int32_t x0 = (int32_t)fm_floorf(cx - r - 1.0f), x1 = (int32_t)(cx + r + 1.0f);
    int32_t y0 = (int32_t)fm_floorf(cy - r - 1.0f), y1 = (int32_t)(cy + r + 1.0f), x, y;
    for (y = y0; y <= y1; y++)
        for (x = x0; x <= x1; x++) {
            float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy, d = fm_sqrtf(dx * dx + dy * dy);
            px_blend(x, y, c, (uint32_t)(fm_clampf(r - d + 0.5f, 0.0f, 1.0f) * 16.0f + 0.5f));
        }
}

/* atan2 in turns, 0..1 from +x going clockwise on screen (y down) */
static float turns(float y, float x)
{
    float ax = fm_fabsf(x), ay = fm_fabsf(y), a = ax < ay ? ax / (ay + 1e-9f) : ay / (ax + 1e-9f), s = a * a, r;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;   /* atan on [0, 1] */
    if (ay > ax)
        r = 1.57079637f - r;
    if (x < 0.0f)
        r = FM_PI - r;
    if (y < 0.0f)
        r = FM_TWO_PI - r;
    return r * (1.0f / FM_TWO_PI);
}

/* a knob: a 270-degree track from 7:30 to 4:30, anti-aliased, round ends; the value filled from
 * the start (or from the top, for a centred value) and a round handle */
static void arc(int32_t cx, int32_t cy, int32_t r, float v, uint16_t track, uint16_t fill, uint16_t bg, int bipolar)
{
    const float th = 3.4f, a0 = 0.375f, sweep = 0.75f;
    float ro = (float)r, ri = ro - th, rm = ro - th * 0.5f, fx = (float)cx + 0.5f, fy = (float)cy + 0.5f, lo, hi;
    int32_t x, y;
    v = fm_clampf(v, 0.0f, 1.0f);
    lo = bipolar ? (v < 0.5f ? v : 0.5f) : 0.0f;
    hi = bipolar ? (v < 0.5f ? 0.5f : v) : v;
    for (y = -r - 1; y <= r + 1; y++)
        for (x = -r - 1; x <= r + 1; x++) {
            float dx = (float)x, dy = (float)y, d = fm_sqrtf(dx * dx + dy * dy), t, a;
            a = fm_clampf(ro + 0.5f - d, 0.0f, 1.0f) * fm_clampf(d - ri + 0.5f, 0.0f, 1.0f);
            if (a <= 0.0f)
                continue;
            t = turns(dy, dx) - a0;
            if (t < 0.0f)
                t += 1.0f;
            t /= sweep;
            if (t > 1.0f)
                continue;
            px_blend(cx + x, cy + y, (t >= lo && t <= hi) ? fill : track, (uint32_t)(a * 16.0f + 0.5f));
        }
    {
        float e0 = a0 * FM_TWO_PI, e1 = (a0 + sweep) * FM_TWO_PI, ev = (a0 + sweep * v) * FM_TWO_PI;
        float el = (a0 + sweep * lo) * FM_TWO_PI;
        disc(fx + fm_cosf(e0) * rm - 0.5f, fy + fm_sinf(e0) * rm - 0.5f, th * 0.5f, lo <= 0.0f && hi > 0.0f ? fill : track);
        disc(fx + fm_cosf(e1) * rm - 0.5f, fy + fm_sinf(e1) * rm - 0.5f, th * 0.5f, hi >= 1.0f ? fill : track);
        if (hi > lo)
            disc(fx + fm_cosf(el) * rm - 0.5f, fy + fm_sinf(el) * rm - 0.5f, th * 0.5f, fill);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 1.3f, bg);
        disc(fx + fm_cosf(ev) * rm - 0.5f, fy + fm_sinf(ev) * rm - 0.5f, th * 0.5f + 0.6f, fill);
    }
}

static void caps(char *d, const char *s, int n)  /* s in capitals, at most n - 1 characters */
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}

static uint16_t mix(uint16_t a, uint16_t b, int t)   /* t 0..16: a -> b */
{
    int r, g, bl;
    if (t <= 0)
        return a;
    if (t >= 16)
        return b;
    r = ((a >> 11) * (16 - t) + (b >> 11) * t) / 16;
    g = (((a >> 5) & 63) * (16 - t) + ((b >> 5) & 63) * t) / 16;
    bl = ((a & 31) * (16 - t) + (b & 31) * t) / 16;
    return (uint16_t)(r << 11 | g << 5 | bl);
}
static int q16(float x) { return x <= 0.0f ? 0 : x >= 1.0f ? 16 : (int)(x * 16.0f + 0.5f); }

static void tag(int32_t *x, const char *s, uint16_t bg, uint16_t fg)
{
    int32_t w = text_w(&FONT_XS, s) + 10;
    cv_round(*x, 6, w, 16, 6, bg);
    cv_text(*x + 5, 7, &FONT_XS, s, fg);
    *x += w + 4;
}

/* the battery (plat_battery): 0 (low) .. 3 bars, 4 a USB host powers the FM-1, -1 not read yet. The
 * simulator's "battery N" (host.c) stands in for it */
#ifdef PLAT_HOST
static int8_t batt_host = -2;                  /* -2: plat_battery's */
static int batt(void) { return batt_host > -2 ? batt_host : plat_battery(); }
#else
#define batt() plat_battery()
#endif
#define K_LOW RGB(196, 84, 52)                 /* the battery at one bar or none */
#define BATT_W 20                              /* the body 18 wide and its nub */
/* lying down at the header's right, its middle on the beat light's: three bars, rust at one or none; a
 * bolt instead on USB power; nothing until it is read */
static void draw_batt(int32_t x, int b)
{
    static const int8_t BOLT[7][2] = {{10, 2}, {9, 2}, {8, 2}, {7, 5}, {9, 2}, {8, 2}, {7, 2}};
    uint16_t c = b <= 1 ? K_LOW : K_DIM;
    int i;
    cv_round(x, 9, 18, 10, 2, c);
    cv_rect(x + 1, 10, 16, 8, K_BG);
    cv_rect(x + 18, 12, 2, 4, c);
    if (b == 4) {
        for (i = 0; i < 7; i++)
            cv_rect(x + BOLT[i][0], 10 + i, BOLT[i][1], 1, K_TEXT);
        return;
    }
    for (i = 0; i < b && i < 3; i++)
        cv_rect(x + 2 + i * 5, 11, 4, 6, b <= 1 ? K_LOW : K_PAD);
}
static const char *batt_words(int b)
{
    static const char *const W[5] = {"Battery low", "Battery one third", "Battery two thirds", "Battery full",
                                     "On USB power"};
    return b >= 0 && b <= 4 ? W[b] : 0;
}

/* the header: the piece in a tag, its root and mode; on the right, the battery, the tempo with a slow
 * beat light (a play triangle while resting) and a dot while there is something unsaved. Messages take
 * its place for a moment. */
static void draw_header(void)
{
    char t[24];
    int msg = plat_ms() < ui.msg_until;
    uint32_t h = hash(hash(hash(2166136261u, proj.piece | (uint32_t)root_now() << 8 | (uint32_t)proj.pool << 12),
                           en_playing | ui.dirty << 4 | (uint32_t)msg << 5 | (uint32_t)en_ext << 6 | (uint32_t)proj.par[P_MODE] << 8),
                      (uint32_t)proj.par[P_TEMPO] | (ui.view == V_SETUP ? plat_cpu_pct() << 8 : 0u) |
                      (uint32_t)(batt() + 1) << 24);
    if (msg)
        h = hash(hash(h, (uint32_t)ui.msg[0][0] << 8 | ui.msg[0][1]), ui.msg_until);
    if (en_playing)
        h = hash(h, (en_bars / 4u) & 1u);
    h = hash(h, (uint32_t)(proj.par[P_DREAM] ? ui.dream_now : 0) | (uint32_t)proj.par[P_SEED] << 16 |
                (uint32_t)(proj.par[P_DREAM] != 0) << 31);
    if (h == ui.sig[0])
        return;
    ui.sig[0] = h;
    cv_begin(240, HDR_H, K_BG);
    if (msg) {
        int32_t x = cv_text(10, 6, &FONT_B, ui.msg[0], K_TEXT);
        cv_text(x + 8, 6, &FONT_B, ui.msg[1], K_ACC);
    } else {
        int32_t x = 232, w;
        if (batt() >= 0) {
            x -= BATT_W;
            draw_batt(x, batt());
            x -= 6;
        }
        if (ui.dirty) {                            /* unsaved */
            cv_round(x - 5, 12, 5, 5, 2, K_DIM);
            x -= 11;
        }
        if (en_ext) {
            t[0] = 'E';
            t[1] = 'X';
            t[2] = 'T';
            t[3] = 0;
        } else {
            itoa_u((uint32_t)proj.par[P_TEMPO], t);
        }
        if (ui.view == V_SETUP) {                 /* Setup: the audio load too */
            char c[12] = "CPU ";
            itoa_u(plat_cpu_pct(), c + 4);
            x -= text_w(&FONT_XS, c);
            cv_text(x, 8, &FONT_XS, c, K_DIM);
            x -= 10;
        }
        x -= text_w(&FONT_S, t);
        cv_text(x, 6, &FONT_S, t, en_playing ? K_TEXT : K_DIM);
        x -= 16;
        if (en_playing) {                         /* a slow beat light */
            int on = ((en_bars / 4u) & 1u) == 0u;
            cv_round(x, 9, 10, 10, 5, on ? K_PAD : K_PAD_T);
        } else {
            int32_t j;                            /* resting: a play triangle */
            for (j = 0; j < 5; j++)
                cv_rect(x + 2 + j, 9 + j, 1, 11 - 2 * j, K_DIM);
        }
        {                                         /* the dream playing (D), or the Seed (S): to note, to render */
            const int n = proj.par[P_DREAM] ? ui.dream_now : proj.par[P_SEED];
            if (n) {
                char d[8];
                d[0] = proj.par[P_DREAM] ? 'D' : 'S';
                itoa_u((uint32_t)n, d + 1);
                x -= text_w(&FONT_XS, d) + 6;
                cv_text(x, 8, &FONT_XS, d, K_ACC);
            }
        }
        w = text_w(&FONT_B, piece_name(proj.piece)) + 12;   /* the piece, in a coral tag */
        cv_round(4, 4, w, 20, 6, K_TEXT);
        cv_text(10, 6, &FONT_B, piece_name(proj.piece), K_BG);
        w += 10;                                  /* the root, then the mode, where they clear the right side */
        if (w + text_w(&FONT_XS, EN_NOTE_NAME[root_now()]) + 4 <= x) {
            w = cv_text(w, 8, &FONT_XS, EN_NOTE_NAME[root_now()], K_TEXT);
            if (w + 3 + text_w(&FONT_XS, mode_name()) + 4 <= x)
                cv_text(w + 3, 8, &FONT_XS, mode_name(), K_DIM);
        }
    }
    cv_blit(0, 0);
}

/* the garden: the scale as a ring of twelve, the chord lit in it; the four bell loops around it, each
 * a ring of its steps with its notes on it and its playhead going round; the three pulses on the
 * right, the four layers' levels on the left */
#define CX 120
#define CY 84
static const int32_t LOOP_R[EN_NLOOP] = {36, 43, 50, 57};

static void polar(int32_t cx, int32_t cy, float r, int i, int n, int32_t *x, int32_t *y)
{
    float a = (float)i * (FM_TWO_PI / (float)n);  /* step i of n, clockwise from the top */
    float sx = fm_sinf(a) * r, sy = fm_cosf(a) * r;
    *x = cx + (int32_t)(sx + (sx < 0.0f ? -0.5f : 0.5f));
    *y = cy - (int32_t)(sy + (sy < 0.0f ? -0.5f : 0.5f));
}

static const char *chord_now(char *nm)          /* the chord's name, as the garden shows it ("Em7") */
{
    int pcs[EN_NPAD + 1], k;
    pcs[0] = en_chord_root;
    for (k = 0; k < EN_NPAD; k++)
        pcs[k + 1] = en_chord[k] % 12;
    return en_chord_name(pcs, EN_NPAD + 1, nm);
}

static void draw_ring(void)
{
    uint32_t abs_pool = pool_abs(), chord = 0;
    int i, l;
    for (i = 0; i < EN_NPAD; i++)
        chord |= 1u << (en_chord[i] % 12u);
    for (l = 0; l < EN_NLOOP; l++) {
        int len = EN_LOOP_LEN[l], s, pos = en_loop_pos[l], g = q16(en_loop_glow[l]);
        for (s = 0; s < len; s++) {
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], s, len, &x, &y);
            if (en_loop_note[l][s] >= 0)
                cv_disc(x, y, 2, (s == pos && en_playing) ? mix(K_BELL, K_FROST, g / 2) : mix(K_BELL_T, K_BELL, 12));
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (en_playing) {                        /* the playhead */
            int32_t x, y;
            polar(CX, CY, (float)LOOP_R[l], pos, len, &x, &y);
            cv_disc(x, y, 1 + g / 6, mix(K_DIM, K_BELL, g));
        }
    }
    for (i = 0; i < 12; i++) {                  /* the scale: C at the top */
        int32_t x, y;
        int in = (int)(abs_pool >> i & 1u), ch = (int)(chord >> i & 1u);
        polar(CX, CY, 24.0f, i, 12, &x, &y);
        if (i == root_now())
            cv_disc(x, y, 5, K_TEXT);           /* the root: ringed in ink */
        if (ch)
            cv_disc(x, y, 4, mix(K_PAD_T, K_PAD, 6 + q16(en_pad_level) * 10 / 16));
        else if (in)
            cv_disc(x, y, 3, mix(K_PAD_T, K_PAD, 7));
        else
            cv_disc(x, y, 1, K_LINE);
    }
    {   /* the chord's name, in the middle */
        char nm[16];
        chord_now(nm);
        if (text_w(&FONT_B, nm) <= 38)
            text_c(CX, CY - 8, &FONT_B, nm, en_frozen ? K_FROST : K_TEXT);
        else
            text_c(CX, CY - 6, &FONT_XS, nm, en_frozen ? K_FROST : K_TEXT);
    }
}

static void draw_meters(void)                    /* left: the four layers */
{
    static const uint16_t C[4] = {K_PAD, K_BELL, K_BASS, K_PULSE}, T[4] = {K_PAD_T, K_BELL_T, K_BASS_T, K_PULSE_T};
    float lv[4], bm = 0.0f, dm = 0.0f;
    int i;
    for (i = 0; i < EN_NLOOP; i++)
        bm = fm_maxf(bm, en_loop_glow[i]);
    for (i = 0; i < EN_NDRUM; i++)
        dm = fm_maxf(dm, en_drum_glow[i]);
    lv[0] = en_pad_level * (float)proj.par[P_PAD] * 0.01f;
    lv[1] = bm * (float)proj.par[P_BELLS] * 0.01f;
    lv[2] = en_bass_level * (float)proj.par[P_BASS] * 0.01f;
    lv[3] = dm * (float)proj.par[P_PULSE] * 0.01f;
    for (i = 0; i < 4; i++) {
        int32_t x = 10 + i * 11, h = (int32_t)(fm_clampf(lv[i], 0.0f, 1.0f) * 80.0f + 0.5f);
        cv_round(x, 46, 7, 86, 3, T[i]);
        if (h > 0)
            cv_round(x, 46 + 86 - h - 2, 7, h + 2, 3, C[i]);
    }
}

static void draw_pulse(void)                     /* right: the three Euclidean rings */
{
    int d;
    for (d = 0; d < EN_NDRUM; d++) {
        int32_t cx = 212, cy = 52 + d * 34, s, n = EN_DRUM_N[d];
        int g = q16(en_drum_glow[d]);
        uint32_t p = en_drum_pat[d];
        for (s = 0; s < n; s++) {
            int32_t x, y;
            int cur = en_playing && s == en_drum_pos[d];
            polar(cx, cy, 13.0f, s, n, &x, &y);
            if ((p >> s & 1u) && en_pulse_rest)   /* sitting a few bars out: faded */
                cv_disc(x, y, 2, K_LINE);
            else if (p >> s & 1u)
                cv_disc(x, y, cur ? 3 : 2, cur ? K_PULSE : mix(K_PULSE_T, K_PULSE, 10));
            else if (cur)
                cv_disc(x, y, 1, K_DIM);
            else
                cv_rect(x, y, 2, 2, K_LINE);
        }
        if (g)
            cv_disc(cx, cy, 1 + g / 4, mix(K_BG, K_PULSE, g));
    }
}

/* a button's card (help_tick): its name, then what it does, in the garden's place. A line starting with a
 * space goes on from the one before (a screen reader joins them: sp_line) */
#define HELP_LINES 4
static const char *const BTN_LABEL[NB] = {"FX", "SEL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ",
                                          "PLAY", "REC", "OCT-", "OCT+"};
static int help_lines(int b, const char *l[HELP_LINES])
{
    int n = 0;
#define L(t) (l[n++] = (t))
    switch (b) {
    case B_HOME: L("THE DREAM: DREAM, RANGE,"); L("SPICE CAP, SEED"); L("AGAIN: THE GARDEN"); L("+ SELECT: TEMPO"); break;
    case B_ENV: L("THE PAD"); L("LEVEL, SWELL, BRIGHT,"); L("COLOUR"); break;
    case B_ARP: L("THE BELLS"); L("LEVEL, DENSITY, TONE,"); L("COLOUR; AGAIN: THE ARP"); break;
    case B_SEL: L("SELECT"); L("TAP: CHORDS, KEYS, DEAL"); L("HOLD: THIS PAGE'S"); L("OTHER KNOBS (+)"); break;
    case B_LFO: L("THE BASS"); L("LEVEL, OCTAVE, GLIDE, FIFTH"); break;
    case B_SEQ: L("THE PULSE"); L("LEVEL, HITS, SWING, HATS"); L("AGAIN: THE RINGS"); break;
    case B_FX: L("THE SPACE"); L("ECHO, ECHO TIME, REVERB,"); L("SIZE"); break;
    case B_GLO: L("SETUP"); L("ROOT, TUNE, MIDI OUT, LIGHTS"); break;
    case B_EDIT: L("THE NEXT CHORD,"); L("ON THE NEXT BAR"); break;
    case B_REC: L("FREEZE: THE CHORD HOLDS"); L(" AND THE REVERB RINGS ON"); L("AGAIN: THAW"); break;
    case B_OCTDN: L("THE BELLS AN OCTAVE DOWN"); L("WITH OCT+, 5 S: UPDATE MODE"); break;
    case B_OCTUP: L("THE BELLS AN OCTAVE UP"); L("WITH OCT-, 5 S: UPDATE MODE"); break;
    case B_SAVE: L("SAVES ALL EIGHT PIECES"); L("KEEP HOLDING: THE PIECE"); L(" BACK TO ITS RECIPE"); break;
    default: break;
    }
#undef L
    return n;
}

static void draw_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    cv_round(16, 14, 208, 116, 6, K_PANEL);
    cv_round(16, 14, 208, 26, 6, K_TEXT);
    cv_rect(16, 34, 208, 6, K_TEXT);
    text_c(120, 18, &FONT_B, BTN_LABEL[ui.help - 1], K_BG);
    for (k = 0; k < n; k++)
        text_c(120, 56 + k * 20, &FONT_S, l[k] + (l[k][0] == ' '), K_TEXT);
}

static void draw_main(void)
{
    uint32_t h = hash(2166136261u, pool_abs() | (uint32_t)root_now() << 12 | (uint32_t)en_frozen << 16 | proj.piece << 24 |
                                        (uint32_t)en_playing << 17 | (uint32_t)proj.par[P_MODE] << 20), i;
    if (ui.frame % 3u)                           /* the garden moves slowly: 20 frames a second is plenty */
        return;
    if (ui.help) {
        h = hash(0x48454C50u, ui.help);
        if (h != ui.sig[1]) {
            ui.sig[1] = h;
            cv_begin(240, MAIN_H, K_BG);
            draw_help();
            cv_blit(0, MAIN_Y);
        }
        return;
    }
    for (i = 0; i < EN_NPAD; i++)
        h = hash(h, en_chord[i]);
    for (i = 0; i < EN_NLOOP; i++) {
        uint32_t s;
        h = hash(h, en_loop_pos[i] | (uint32_t)q16(en_loop_glow[i]) << 8);
        for (s = 0; s < EN_LOOP_LEN[i]; s++)
            h = hash(h, (uint32_t)(en_loop_note[i][s] >= 0));
    }
    for (i = 0; i < EN_NDRUM; i++)
        h = hash(h, en_drum_pos[i] | (uint32_t)q16(en_drum_glow[i]) << 8 | en_drum_pat[i] << 13 | (uint32_t)en_pulse_rest << 31);
    h = hash(h, (uint32_t)(en_drift * 40.0f) | (uint32_t)q16(en_pad_level) << 8 | (uint32_t)q16(en_bass_level) << 13);
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)proj.par[i]);
    if (h == ui.sig[1])
        return;
    ui.sig[1] = h;
    cv_begin(240, MAIN_H, K_BG);
    if (en_frozen) {                             /* the way to the next chord, top right (held: frozen) */
        cv_text(232 - text_w(&FONT_XS, "held"), 4, &FONT_XS, "held", K_FROST);
    } else {
        int32_t w = 30, f = (int32_t)(en_drift * (float)w + 0.5f);
        cv_round(232 - w, 8, w, 6, 3, K_LINE);
        if (f > 0)
            cv_round(232 - w, 8, f < 6 ? 6 : f, 6, 3, K_PAD);
    }
    draw_ring();
    draw_meters();
    draw_pulse();
    cv_blit(0, MAIN_Y);
}

/* the knob strip (X0X's): a rule with the page's name and five page dots at its right end; under
 * it a cell a knob: its name, a ring (or pips, for a switch of a few positions), the value. The
 * knob being turned is drawn on a tint. Only redrawn when what it shows changes (the rings cost a
 * square root and an arctangent per pixel). */
static void draw_knobs(void)
{
    uint32_t h = hash(2166136261u, ui.view | (uint32_t)ui.touched << 8), i;
    int now_touch = plat_ms() < ui.touch_until;
    for (i = 0; i < 4; i++)
        h = hash(h, (uint32_t)knob_get(view_knob(ui.view, (int)i)));
    h = hash(h, (uint32_t)now_touch);
    h = hash(h, (uint32_t)layer_held(ui.view) | proj.pool << 1 | (uint32_t)root_now() << 13);   /* a slot names its chord */
    h = hash(h, (ui.btn >> B_SEL & 1u) | (uint32_t)(proj.par[P_DREAM] != 0) << 1 | (uint32_t)ui.hand << 2 |
                (uint32_t)any_step(P_SLOT1) << 6);
    if (h == ui.sig[2])
        return;
    ui.sig[2] = h;
    cv_begin(240, KNB_H, K_BG);
    {   /* the rule; at its right end, where you are: the page, then the dots */
        char t[12];
        int k;
        int32_t xd = 237 - NVIEWS * 6, xt;
        caps(t, VIEW_NAME[ui.view], sizeof t);
        if (ui.btn >> B_SEL & 1u) {              /* SELECT held: the other knobs (+), or none here (-) */
            int n = 0;                           /* (no libc on the device: the end found by hand) */
            while (t[n])
                n++;
            if (n + 2 < (int)sizeof t)
                t[n] = ' ', t[n + 1] = layer_held(ui.view) ? '+' : '-', t[n + 2] = 0;
        }
        xt = xd - 5 - text_w(&FONT_XS, t);
        cv_rect(4, 5, xt - 9, 1, K_LINE);
        cv_text(xt, -1, &FONT_XS, t, K_ACC);
        for (k = 0; k < NVIEWS; k++)
            cv_round(xd + k * 6, 3, 4, 4, 2, k == ui.view ? K_ACC : K_LINE);
    }
    for (i = 0; i < 4; i++) {
        int k = view_knob(ui.view, (int)i), lo, hi, v, hot = now_touch && ui.touched == (int)i;
        int32_t x = (int32_t)i * 60, cx = x + 30;
        char t[16];
        uint16_t bg = hot ? K_PANEL : K_BG, fill = hot ? K_ACC : K_PAD;
        if (k == K_NONE) {                       /* nothing here: on a second layer, a dash says so */
            if (layer_held(ui.view)) {
                if (i)
                    cv_rect(x, 16, 1, 46, K_LINE);
                text_c(cx, 50, &FONT_B, "-", K_LINE);
            }
            continue;
        }
        if (i)
            cv_rect(x, 16, 1, 46, K_LINE);
        if (hot)
            cv_round(x + 3, 11, 54, 57, 6, K_PANEL);
        knob_range(k, &lo, &hi);
        v = knob_get(k);
        if (v > hi)                              /* a slot, the scale since shorter */
            v = hi;
        if (is_rslot(k))
            v = rslot_pos(v);
        caps(t, view_label(ui.view, (int)i), sizeof t);
        text_c(cx, 12, &FONT_XS, t, hot ? K_TEXT : K_DIM);
        if (k != K_CPU && hi - lo < 6) {         /* a switch: its positions as pips */
            int n = hi - lo + 1, j, pw = n > 4 ? 6 : 9, gap = 3, w0 = n * (pw + gap) - gap;
            for (j = 0; j < n; j++)
                cv_round(cx - w0 / 2 + j * (pw + gap), 33, pw, 8, 2, j == v - lo ? fill : K_LINE);
        } else if (k != K_CPU) {
            arc(cx, 37, 12, hi > lo ? (float)(v - lo) / (float)(hi - lo) : 0.0f, K_LINE, fill, bg, lo < 0);
        }
        knob_text(k, t);
        text_c(cx, 50, &FONT_B, t, hot ? K_ACC : K_TEXT);
    }
    cv_blit(0, KNB_Y);
}

static void leds(void)
{
    uint32_t b = 0, k = 0, i, ap = pool_abs();
    b |= 1u << VIEW_BTN[ui.view];
    if (en_playing)
        b |= 1u << B_PLAY;
    if (en_frozen)
        b |= 1u << B_REC;
    if (proj.leds)
        for (i = 0; i < NKEYS; i++)
            if (ap >> ((5u + i) % 12u) & 1u)
                k |= 1u << i;
    plat_glow(proj.leds == 1);
    plat_leds(b, k);
}

/* AUTOSAVE: a few seconds after a change, while resting and quiet, nothing touched (a flash erase
 * silences the audio for a moment: never while the garden grows; SAVE saves then) */
#define AUTOSAVE_QUIET 4000u
static void autosave(void)
{
    uint32_t now = plat_ms();
    if (!ui.dirty || ui.hold || en_playing || ui.btn || ui.keys || now - ui.act_t < AUTOSAVE_QUIET || en_pad_level > 0.01f)
        return;
    if (project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = now;
    }
}
void ui_backup_flush(void)                      /* a backup starts: what is unsaved, saved */
{
    if (ui.dirty && project_save() == 0) {
        ui.dirty = 0;
        ui.saved_t = plat_ms();
    }
}
void ui_backup_hold(int on) { ui.hold = (uint8_t)(on != 0); }

/* ------------------------------------------------------ screen reader --- */
/* The screen as text for a screen reader on the computer (announce.c; fm1-reader): a page once it settles,
 * with its knobs; a knob's value as it turns; a help card as it comes up; the chord as it moves (queued: the
 * garden moves it by itself, every 20 s at the quickest); growing and resting from MIDI, and a MIDI clock
 * arriving or going; what a button did (say). WHERE (the reader asks, or it has just started listening) is
 * the whole screen in two: the page, its knobs and the header, then the garden (the chord, the scale).
 * Nothing is built while no reader listens. */
static struct {
    uint8_t view, help, ext, started;
    int16_t val[4];
    uint32_t chord;                            /* the chord's name, hashed */
} sp;
static char sp_buf[ANN_MAX + 1];
static uint32_t sp_n;
/* the chord names (en_chord_name's, after the root) in words: "m" and "M7" differ only in capitals, which
 * the words file ignores */
static const char *const CHORD_Q[][2] = {
    {"", "major"}, {"m", "minor"}, {"7", "7"}, {"M7", "major 7"}, {"m7", "minor 7"}, {"6", "6"},
    {"m6", "minor 6"}, {"add9", "add 9"}, {"madd9", "minor add 9"}, {"aug", "augmented"}, {"dim", "diminished"},
    {"dim7", "diminished 7"}, {"dimadd9", "diminished add 9"}, {"m7b5", "half diminished"}, {"mM7", "minor major 7"}, {"sus2", "suspended 2"},
    {"sus4", "suspended 4"}, {"7sus4", "7 suspended 4"}, {"5", "5"},
};
#define NCHORD_Q (sizeof CHORD_Q / sizeof CHORD_Q[0])

static void sp_cat(const char *s)
{
    while (*s && sp_n < ANN_MAX)
        sp_buf[sp_n++] = *s++;
    sp_buf[sp_n] = 0;
}
static void sp_int(uint32_t v)
{
    char t[12];
    itoa_u(v, t);
    sp_cat(t);
}
static int sp_same(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

/* a value as it is said: Drift in minutes and seconds (the screen's "1:05", "45s"), the rest as shown */
static void sp_value(int k)
{
    char t[16];
    knob_text(k, t);
    if (k == P_DRIFT && t[0] >= '0' && t[0] <= '9') {   /* (not when it says Chords) */
        uint32_t m = 0, s = 0, i;
        for (i = 0; t[i] >= '0' && t[i] <= '9'; i++)
            s = s * 10u + (uint32_t)(t[i] - '0');
        if (t[i] == ':') {
            m = s;
            s = 0;
            for (i++; t[i] >= '0' && t[i] <= '9'; i++)
                s = s * 10u + (uint32_t)(t[i] - '0');
        }
        if (m) {
            sp_int(m);
            sp_cat(m == 1u ? " minute" : " minutes");
            if (!s)
                return;
            sp_cat(" ");
        }
        sp_int(s);
        sp_cat(s == 1u ? " second" : " seconds");
        return;
    }
    sp_cat(t);
}
static void sp_knob(int view, int i)           /* "Swell 60": the name the page gives it, then its value */
{
    sp_cat(view_label(view, i));
    sp_cat(" ");
    sp_value(view_knob(view, i));
}

static void sp_chord_q(int root, const char *q)   /* "E minor 7" */
{
    uint32_t i;
    sp_cat(EN_NOTE_NAME[root % 12]);
    sp_cat(" ");
    for (i = 0; i < NCHORD_Q; i++)
        if (sp_same(q, CHORD_Q[i][0])) {
            sp_cat(CHORD_Q[i][1]);
            return;
        }
    sp_cat(q);
}
static void sp_chord(void)                     /* the chord in the middle of the garden */
{
    char nm[16];
    const char *q = chord_now(nm), *r = EN_NOTE_NAME[en_chord_root % 12u];
    while (*r && *q == *r)
        q++, r++;
    sp_chord_q(en_chord_root % 12u, q);
}

/* the page: its name and its knobs; all: the header too (the piece, its key, the tempo, growing or resting) */
static void sp_page(int all)
{
    int i;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(VIEW_NAME[ui.view]);
    sp_cat(" page");
    for (i = 0; i < 4; i++) {
        sp_cat(i ? ", " : ": ");
        sp_knob(ui.view, i);
    }
    if (!all)
        return;
    sp_cat(". Piece ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[root_now()]);
    sp_cat(" ");
    sp_cat(mode_name());
    if (en_ext) {
        sp_cat(", tempo EXT");
    } else if (ui.view != V_GARDEN) {          /* (the Garden's first knob says it) */
        sp_cat(", tempo ");
        sp_int((uint32_t)proj.par[P_TEMPO]);
    }
    sp_cat(sp_tp.playing ? ", growing" : ", resting");
    if (ui.dirty)
        sp_cat(", not saved yet");
    if (ui.view == V_SETUP) {
        sp_cat(". CPU ");
        sp_int(plat_cpu_pct());
        sp_cat("%");
    }
    if (batt_words(batt())) {
        sp_cat(". ");
        sp_cat(batt_words(batt()));
    }
}

/* the garden: the chord and how far to the next, the scale from the root up ("F and F sharp": one letter's
 * two notes joined, so they are heard as two), the pulse sitting out */
static void sp_garden(void)
{
    uint32_t ap = pool_abs();
    int i, last = -1;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("Chord ");
    sp_chord();
    if (en_frozen) {
        sp_cat(", frozen");
    } else {
        sp_cat(", next chord ");
        sp_int((uint32_t)(fm_clampf(en_drift, 0.0f, 1.0f) * 100.0f + 0.5f));
        sp_cat("% of the way");
    }
    sp_cat(". Scale");
    for (i = 0; i < 12; i++) {
        int pc = (root_now() + i) % 12;
        if (ap >> pc & 1u) {
            sp_cat(last < 0 ? " " : EN_NOTE_NAME[last][0] == EN_NOTE_NAME[pc][0] ? " and " : ", ");
            sp_cat(EN_NOTE_NAME[pc]);
            last = pc;
        }
    }
    if (en_pulse_rest && proj.par[P_PULSE])
        sp_cat(". Pulse resting");
}

/* a help card: its button, then its lines ("ENV button. THE PAD. LEVEL, SWELL, BRIGHT, COLOUR") */
static void sp_line(const char *s)
{
    char p = sp_n ? sp_buf[sp_n - 1] : 0;
    if (*s == ' ')                             /* it goes on from the line before */
        s++;
    else if (p != ',' && p != ':')
        sp_cat(".");
    sp_cat(" ");
    sp_cat(s);
}
static void sp_help(void)
{
    const char *l[HELP_LINES];
    int n = help_lines(ui.help - 1, l), k;
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat(BTN_LABEL[ui.help - 1]);
    sp_cat(" button");
    for (k = 0; k < n; k++)
        sp_line(l[k]);
}

static void sp_piece_text(void)                /* "PIECE Departures, C# Ionian, tempo 60" */
{
    sp_n = 0;
    sp_buf[0] = 0;
    sp_cat("PIECE ");
    sp_cat(piece_name(proj.piece));
    sp_cat(", ");
    sp_cat(EN_NOTE_NAME[root_now()]);
    sp_cat(" ");
    sp_cat(mode_name());
    sp_cat(", tempo ");
    sp_int((uint32_t)proj.par[P_TEMPO]);
}
static void sp_piece(void)
{
    if (!ann_on())
        return;
    sp_piece_text();
    ann_say(sp_buf, 0, ANN_NOW);
    sp_said = 1;
}

static uint32_t sp_chord_sig(void)
{
    char nm[16];
    const char *s = chord_now(nm);
    uint32_t h = 2166136261u;
    while (*s)
        h = hash(h, (uint8_t)*s++);
    return h;
}
static void sp_snap(void)
{
    int i;
    for (i = 0; i < 4; i++)
        sp.val[i] = (int16_t)knob_get(view_knob(ui.view, i));
    sp.view = ui.view;
    sp.help = ui.help;
    sp.ext = en_ext;
    sp.chord = sp_chord_sig();
}

static void speak(void)
{
    uint32_t now = plat_ms(), c;
    int where = ann_where(), i, changed = -1;
    if (!ann_on()) {
        sp.started = 0;
        sp_said = 0;
        return;
    }
    if (!sp.started || where) {                       /* everything on the screen, in two */
        sp.started = 1;
        sp_snap();
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            sp_page(1);
        } else {
            sp_page(1);
            ann_say(sp_buf, 0, ANN_NOW);
            sp_garden();
        }
        ann_say(sp_buf, 0, 0);
        sp_said = 0;
        return;
    }
    if (sp_said) {                                    /* a button said what it did (and what it changed) */
        sp_said = 0;
        sp_snap();
        return;
    }
    if (en_playing != sp_tp.playing && now - sp_tp.play_t > 150u) {   /* MIDI start / stop */
        sp_tp.playing = en_playing;
        ann_say(en_playing ? "GROWING" : "RESTING", 0, 0);
    }
    if (en_ext != sp.ext) {                           /* a MIDI clock arrived, or stopped */
        sp.ext = en_ext;
        sp_n = 0;
        sp_cat("Tempo ");
        if (en_ext)
            sp_cat("EXT");
        else
            sp_int((uint32_t)proj.par[P_TEMPO]);
        ann_say(sp_buf, 0, 0);
    }
    if (ui.help != sp.help) {                         /* a help card: at once */
        sp.help = ui.help;
        if (ui.help) {
            sp_help();
            ann_say(sp_buf, 0, ANN_NOW);
            return;
        }
    }
    if (ui.view != sp.view) {                         /* a new page: said when it settles */
        sp_snap();
        sp_page(0);
        ann_later(sp_buf, ANN_NOW, 200u, now);
        return;
    }
    for (i = 0; i < 4; i++) {                         /* a knob turning */
        int v = knob_get(view_knob(ui.view, i));
        if (v != sp.val[i]) {
            sp.val[i] = (int16_t)v;
            if (changed < 0 && view_knob(ui.view, i) != K_NONE)
                changed = i;
        }
    }
    if (changed >= 0) {
        sp_n = 0;
        sp_knob(ui.view, changed);
        ann_later(sp_buf, ANN_NOW, 120u, now);
    }
    c = sp_chord_sig();
    if (c != sp.chord) {                              /* the chord moved: queued, after what is being said */
        sp.chord = c;
        sp_n = 0;
        sp_cat("Chord ");
        sp_chord();
        ann_say(sp_buf, 0, 0);
    }
}

#ifdef PLAT_HOST
/* every page, knob value, help card, chord and message as a screen reader gets them (the "speakdump"
 * command): KIND <tab> the text sent (fm1-reader/tools/audit.mjs puts it in words with web/words/ and checks it) */
static void sp_print(const char *kind, const char *t) { printf("%s\t%s\n", kind, t); }
static void speak_dump(void)
{
    static const char *const MSG[][2] = {{"UPDATE MODE IN", "3.."}, {"UPDATE MODE", "CANCELLED"},
                                         {"NEXT CHORD", "ON THE BAR"}, {"NEXT CHORD", 0}, {"RESTING", 0},
                                         {"GROWING", 0}, {"FREEZE", "ON"}, {"FREEZE", "OFF"}, {"SAVED", 0}};
    static const char *const WARN[][2] = {{"THE ROOT STAYS", "(SETUP)"}, {"3 NOTES", "AT LEAST"},
                                          {"NOT SAVED", "FLASH ERROR"}};
    project_t keep = proj;
    uint8_t keep_view = ui.view, keep_help = ui.help, keep_dirty = ui.dirty, keep_play = sp_tp.playing;
    uint8_t keep_ext = en_ext, keep_frozen = en_frozen, keep_rest = en_pulse_rest;
    int v, i, x, lo, hi;
    char t[48];
    for (v = 0; v < NVIEWS; v++) {
        ui.view = (uint8_t)v;
        sp_page(0);
        sp_print("SCREEN", sp_buf);
        for (i = 0; i < 4; i++) {
            int k = VIEW_KNOB[v][i];
            knob_range(k, &lo, &hi);
            for (x = lo; x <= hi; x++) {
                if (k < P_NPARAMS)
                    proj.par[k] = (int16_t)x;
                else
                    proj.leds = (uint8_t)x;
                sp_n = 0;
                sp_knob(v, i);
                sp_print("KNOB", sp_buf);
            }
            proj = keep;
        }
    }
    for (x = 0; x < 8; x++) {                         /* WHERE: growing, resting, unsaved, a clock, frozen, custom */
        ui.view = (uint8_t)(x * 3 % NVIEWS);
        sp_tp.playing = (uint8_t)(x & 1);
        ui.dirty = (uint8_t)(x >> 1 & 1);
        en_ext = (uint8_t)(x == 3);
        en_frozen = (uint8_t)(x == 5);
        en_pulse_rest = (uint8_t)(x == 2);
        proj.pool = x == 6 ? 0xFFFu : x == 7 ? 0x91u : keep.pool;
        proj.par[P_ROOT] = (int16_t)(x * 5 % 12);
        sp_page(1);
        sp_print("WHERE", sp_buf);
        sp_garden();
        sp_print("WHERE", sp_buf);
    }
    proj = keep;
    ui.dirty = keep_dirty;
    sp_tp.playing = keep_play;
    en_ext = keep_ext;
    en_frozen = keep_frozen;
    en_pulse_rest = keep_rest;
    ui.view = keep_view;
    for (i = 0; i < NB; i++) {
        const char *l[HELP_LINES];
        if (!help_lines(i, l))
            continue;
        ui.help = (uint8_t)(i + 1);
        sp_help();
        sp_print("HELP", sp_buf);
    }
    ui.help = keep_help;
    for (x = 0; x < 12; x++)
        for (i = 0; i < (int)NCHORD_Q; i++) {
            sp_n = 0;
            sp_cat("Chord ");
            sp_chord_q(x, CHORD_Q[i][0]);
            sp_print("CHORD", sp_buf);
        }
    for (i = 0; i < (int)(sizeof MSG / sizeof MSG[0]); i++) {
        msg_text(MSG[i][0], MSG[i][1], t);
        sp_print("MSG", t);
    }
    for (i = 0; i < (int)(sizeof WARN / sizeof WARN[0]); i++) {
        msg_text(WARN[i][0], WARN[i][1], t);
        sp_print("WARN", t);
    }
    for (x = 0; x < 12; x++) {
        msg_text("ADDED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
        msg_text("REMOVED", EN_NOTE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = 0; x < EN_NMODES; x++) {
        msg_text("MODE", EN_MODE_NAME[x], t);
        sp_print("MSG", t);
    }
    for (x = -2; x <= 2; x++) {
        char o[8];
        en_param_text(P_BOCT, x, o);
        msg_text("BELL OCTAVE", o, t);
        sp_print("MSG", t);
    }
    for (x = 30; x <= 120; x += 45) {
        char o[8];
        itoa_u((uint32_t)x, o);
        msg_text("TEMPO", o, t);
        sp_print("MSG", t);
    }
    for (x = 0; x < NPIECES; x++) {
        project_piece(x);
        sp_piece_text();
        sp_print("MSG", sp_buf);
        msg_text("RESET", piece_name(x), t);
        sp_print("MSG", t);
    }
    proj = keep;
    sp_print("MSG", "Tempo EXT");
    sp_print("MSG", "Tempo 72");
    for (x = 0; x <= 4; x++)                          /* the battery, at the end of WHERE's first part */
        sp_print("WHERE", batt_words(x));
}
#endif

void ui_init(void)
{
    memset(&ui, 0, sizeof ui);
    ui.touched = -1;
    ui.view = V_DREAM;                           /* a dreamer's first: the Dream page */
    if (proj.par[P_DREAM] == 1)
        dream_deal(dream_fresh());
    else
        ui.dream_now = (uint16_t)(proj.par[P_DREAM] >= 2 ? proj.par[P_DREAM] - 1 : 0);
    lcd_fill(0, 0, 240, 240, K_BG);
    say_as("NIGHTSCAPE FM", "after AMB-1", 0);   /* the boot card: whose it is; then whose it grew from (ui_frame) */
    ui.msg_until = ui.boot_t = plat_ms() + 2200u;
    if (!en_playing)
        en_play(1);                              /* it grows from the start */
    sp_tp.playing = 1;
    sp_tp.play_t = plat_ms();
}

void ui_frame(void)
{
    if (ui.boot_t && (int32_t)(plat_ms() - ui.boot_t) >= 0) {   /* the boot card's credit, unless a turn spoke first */
        if (ui.msg_until == ui.boot_t) {
            say_as("AMB-1", "by Charles Vestal", 0);
            ui.msg_until = plat_ms() + 2200u;
        }
        ui.boot_t = 0;
    }
    input();
    autosave();
    draw_header();
    draw_main();
    draw_knobs();
    leds();
    speak();
    ann_tick(plat_ms());
    ui.frame++;
}

void ui_input_only(void) { input(); }

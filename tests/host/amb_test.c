/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* The instrument on the host: the pure parts (Euclid, scales, chords, voice leading, names) and the
 * render (sound, no NaN, bounded, the loops evolve, the chord drifts, freeze holds). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "amb.h"
static const uint8_t EN_ROOT_BARS_T[12] = {1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128};   /* the rotator's bars */

static int fails;
#define CHECK(c, ...)                                                                                              \
    do {                                                                                                           \
        if (!(c)) {                                                                                                \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                            \
            printf(__VA_ARGS__);                                                                                   \
            printf("\n");                                                                                          \
            fails++;                                                                                               \
        }                                                                                                          \
    } while (0)

static uint32_t midi_count;
static uint8_t bell_log[4000];                 /* the bells' notes, while logging */
static int bell_n, bell_logging;
static uint8_t bass_log[4000];
static int bass_n;
static int hat_midi, thump_midi;
void en_midi_out(uint32_t st, uint32_t d1, uint32_t d2)
{
    (void)d2;
    midi_count++;
    if (st == 0x99u && (d1 == 42u || d1 == 46u))
        hat_midi++;
    if (st == 0x99u && d1 == 36u)
        thump_midi++;
    if (bell_logging && st == 0x91u && bell_n < (int)sizeof bell_log)
        bell_log[bell_n++] = (uint8_t)d1;
    if (bell_logging && st == 0x92u && bass_n < (int)sizeof bass_log)
        bass_log[bass_n++] = (uint8_t)d1;
}

static uint32_t pat(const char *s)
{
    uint32_t m = 0;
    int i;
    for (i = 0; s[i]; i++)
        if (s[i] == 'x')
            m |= 1u << i;
    return m;
}

static int popcount(uint32_t m)
{
    int c = 0;
    while (m) {
        c += (int)(m & 1u);
        m >>= 1;
    }
    return c;
}

static int32_t buf[2 * 256];
static float peak, sum2;
static int bad;
static void run(float secs)
{
    int blocks = (int)(secs * 44100.0f / 256.0f), b, j;
    for (b = 0; b < blocks; b++) {
        en_render(buf, 256, 2048);
        for (j = 0; j < 512; j++) {
            float x = (float)buf[j] / 8388608.0f;
            if (x != x)
                bad++;
            if (fabsf(x) > peak)
                peak = fabsf(x);
            sum2 += x * x;
        }
    }
}

int main(void)
{
    int pcs[4], n, i;
    char name[16];

    /* Euclid: the classic patterns */
    CHECK(en_euclid(3, 8, 0) == pat("x..x..x."), "E(3,8) %x", en_euclid(3, 8, 0));
    CHECK(en_euclid(5, 8, 0) == pat("x.xx.xx."), "E(5,8) %x", en_euclid(5, 8, 0));
    CHECK(en_euclid(4, 16, 0) == pat("x...x...x...x..."), "E(4,16)");
    CHECK(en_euclid(0, 16, 0) == 0, "E(0,16)");
    CHECK(en_euclid(7, 7, 0) == 0x7Fu, "E(7,7)");
    for (i = 0; i <= 12; i++)
        CHECK(popcount(en_euclid(i, 12, 5)) == i, "E(%d,12) has %d hits", i, popcount(en_euclid(i, 12, 5)));
    CHECK(en_euclid(1, 7, 3) == 1u << 3, "rotation");

    /* scales and chords */
    {
        int st[12];
        CHECK(en_scale(EN_MODE_POOL[MODE_DORIAN], st) == 7 && st[2] == 3 && st[6] == 10, "Dorian steps");
        CHECK(en_scale(EN_MODE_POOL[MODE_PENT_MIN], st) == 5, "pentatonic minor has 5");
        CHECK(en_scale(0, st) == 1, "the root is always in");
    }
    n = en_chord_tones(EN_MODE_POOL[MODE_IONIAN], 0, 0, pcs);
    CHECK(n == 4 && pcs[0] == 0 && pcs[1] == 4 && pcs[2] == 7 && pcs[3] == 11, "C Ionian I = Cmaj7");
    CHECK(!strcmp(en_chord_name(pcs, n, name), "CM7"), "name %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 0, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "Dm7"), "D Dorian i: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_DORIAN], 2, 3, pcs);
    CHECK(!strcmp(en_chord_name(pcs, n, name), "G7"), "D Dorian IV: %s", name);
    n = en_chord_tones(EN_MODE_POOL[MODE_PENT_MAJ], 0, 0, pcs);
    CHECK(n == 3, "pentatonic chords have three tones (%d)", n);
    n = en_chord_tones(1u | 1u << 4 | 1u << 7 | 1u << 9, 0, 0, pcs);   /* four notes: no doubled tones */
    CHECK(n == 3 && pcs[0] != pcs[1] && pcs[1] != pcs[2] && pcs[0] != pcs[2], "4-note pool: distinct");
    n = en_chord_tones(1u | 1u << 4 | 1u << 7, 0, 1, pcs);
    CHECK(n == 3, "3-note pool: all three (%d)", n);

    /* voice leading: small moves, in range, no unisons */
    {
        int from[4] = {50, 57, 62, 66}, to[4], cm7[4] = {0, 4, 7, 11}, f7[4] = {5, 9, 0, 4}, tot = 0;
        en_voice_lead(from, cm7, 4, to, 48, 77);
        for (i = 0; i < 4; i++) {
            CHECK(to[i] >= 48 && to[i] <= 77, "range %d", to[i]);
            tot += abs(to[i] - from[i]);
        }
        CHECK(tot <= 8, "Cmaj7 from D-ish: moved %d", tot);
        memcpy(from, to, sizeof from);
        en_voice_lead(from, f7, 4, to, 48, 77);
        tot = 0;
        for (i = 0; i < 4; i++)
            tot += abs(to[i] - from[i]);
        CHECK(tot <= 6, "CM7 -> FM7 moved %d", tot);
        CHECK(to[0] != to[1] && to[1] != to[2] && to[2] != to[3] && to[0] != to[3], "no unisons");
    }

    /* texts */
    {
        char t[12];
        en_param_text(P_DRIFT, 0, t);
        CHECK(!strcmp(t, "20s"), "drift 0: %s", t);
        en_param_text(P_DRIFT, 100, t);
        CHECK(!strcmp(t, "5:00"), "drift 100: %s", t);
        en_param_text(P_ETIME, 3, t);
        CHECK(!strcmp(t, "1 bar"), "etime: %s", t);
        en_param_text(P_BOCT, 1, t);
        CHECK(!strcmp(t, "+1"), "boct: %s", t);
    }

    /* the render: silent at rest, sound when growing, never NaN, bounded */
    en_init();
    run(1.0f);
    CHECK(peak == 0.0f, "silent before play (%f)", (double)peak);
    en_play(1);
    peak = sum2 = 0.0f;
    run(20.0f);
    CHECK(!bad, "NaN in the output");
    CHECK(peak > 0.05f && peak < 0.99f, "peak %f", (double)peak);
    printf("  20 s playing: peak %.3f, rms %.4f, bars %u\n", (double)peak,
           (double)sqrtf(sum2 / (20.0f * 44100.0f * 2.0f)), en_bars / 16u);
    CHECK(en_bars / 16u >= 5u, "the grid ran (%u sixteenths)", en_bars);
    CHECK(en_pad_level > 0.5f, "the pad swelled (%f)", (double)en_pad_level);

    /* the chord drifts: with Drift at its shortest, a few changes in two minutes */
    {
        int changes = 0, last = en_chord_root * 16 + en_chord[0] % 16, k;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 120; k++) {
            int now;
            run(1.0f);
            now = en_chord_root * 16 + en_chord[0] % 16;
            changes += now != last;
            last = now;
        }
        CHECK(changes >= 3, "chord changes in 2 min at Drift 0: %d", changes);
        printf("  chord changes in 2 min: %d\n", changes);
    }

    /* a chord change is heard: within 2 s no sounding voice sings a note outside the new chord */
    {
        int k, i, bad_s = 0, changes = 0, last = -1;
        en_set(P_DRIFT, 0);
        for (k = 0; k < 90 * 4; k++) {
            int sig = en_chord[0] | en_chord[1] << 7 | en_chord[2] << 14 | en_chord[3] << 21;
            static int since = -1;
            run(0.25f);
            if (sig != last) {
                if (last >= 0) {
                    changes++;
                    since = 0;
                }
                last = sig;
            } else if (since >= 0 && ++since == 8) {   /* 2 s after a change */
                uint32_t pcs = 0;
                for (i = 0; i < EN_NPAD; i++)
                    pcs |= 1u << (en_chord[i] % 12);
                for (i = 0; i < EN_NPAD; i++)
                    if (en_pad_amp[i] > 0.05f && !(pcs >> (en_pad_note[i] % 12) & 1u))
                        bad_s++;
                since = -1;
            }
        }
        CHECK(changes >= 3, "chord changes in 90 s at Drift 0: %d", changes);
        CHECK(!bad_s, "%d voices still off the new chord 2 s after a change", bad_s);
    }

    /* the loops evolve: with Evolve at 100, the phrases change */
    {
        int8_t before[EN_NLOOP][EN_LOOP_MAX];
        int diff = 0, l;
        en_set(P_EVOLVE, 100);
        en_set(P_DENSITY, 80);
        memcpy(before, (const void *)en_loop_note, sizeof before);
        run(90.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                diff += before[l][i] != en_loop_note[l][i];
        CHECK(diff > 0, "the loops evolved");
        for (l = 0; l < EN_NLOOP; l++)
            for (i = EN_LOOP_LEN[l]; i < EN_LOOP_MAX; i++)
                CHECK(en_loop_note[l][i] == -1, "loop %d slot %d past its end", l, i);
    }

    /* freeze: the chord holds */
    {
        int held = en_chord[0] + 128 * en_chord[1], k;
        en_freeze(1);
        for (k = 0; k < 60; k++) {
            run(1.0f);
            CHECK(en_chord[0] + 128 * en_chord[1] == held, "the chord moved while frozen");
        }
        en_freeze(0);
    }

    /* a new key while playing (a piece, the root, the scale): within 3 s no voice still sings the old
     * chord (a voice only takes its note at its next breath otherwise, up to a minute later) */
    {
        int k, stale = 0;
        en_freeze(0);
        en_set(P_ROOT, 6);                       /* a tritone away: nothing in common to hide behind */
        en_pool(EN_MODE_POOL[MODE_LYDIAN]);
        en_regrow();
        run(3.0f);
        for (k = 0; k < EN_NPAD; k++)
            if (en_pad_amp[k] > 0.02f && en_pad_note[k] != en_chord[k]) {
                printf("  voice %d sings %d, the chord has %d (amp %.2f)\n", k, en_pad_note[k], en_chord[k],
                       (double)en_pad_amp[k]);
                stale++;
            }
        CHECK(!stale, "%d voices still in the old key after 3 s", stale);
        en_set(P_ROOT, 2);
    }

    /* a pool change: the chord's tones are in it */
    {
        uint32_t pent = EN_MODE_POOL[MODE_PENT_MIN];
        en_pool(pent);
        run(0.1f);
        for (i = 0; i < 4; i++)
            CHECK(pent >> ((en_chord[i] - 2 + 120) % 12) & 1u, "chord note %d not in the pool", en_chord[i]);
    }

    /* every setting at its extremes: still bounded */
    {
        int p;
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->hi);
        en_set(P_MIDI, 1);
        peak = 0.0f;
        run(30.0f);
        CHECK(!bad && peak < 1.0f, "all at max: peak %f", (double)peak);
        for (p = 0; p < P_NPARAMS; p++)
            en_set(p, en_param_info(p)->lo);
        en_set(P_TEMPO, 30);
        run(10.0f);
        CHECK(!bad, "all at min: NaN");
    }

    /* Bell tone and Bell oct are heard: brighter (zero crossings) and higher with them, bells alone */
    {
        float zc[3];
        int k;
        static const int TONE[3] = {0, 100, 0}, OCT[3] = {0, 0, 2};
        for (k = 0; k < 3; k++) {
            int b2, j;
            long z = 0;
            float prev = 0.0f;
            en_init();
            en_set(P_PAD, 0);
            en_set(P_BASS, 0);
            en_set(P_PULSE, 0);
            en_set(P_ECHO, 0);
            en_set(P_REVERB, 0);
            en_set(P_DENSITY, 100);
            en_set(P_BTONE, TONE[k]);
            en_set(P_BOCT, OCT[k]);
            en_play(1);
            for (b2 = 0; b2 < 20 * 44100 / 256; b2++) {
                en_render(buf, 256, 2048);
                for (j = 0; j < 256; j++) {
                    float x = (float)buf[2 * j];
                    z += (x > 0.0f) != (prev > 0.0f);
                    prev = x;
                }
            }
            zc[k] = (float)z;
        }
        CHECK(zc[1] > 2.5f * zc[0], "Bell tone 100 not brighter: %.0f vs %.0f", (double)zc[1], (double)zc[0]);
        CHECK(zc[2] > 1.8f * zc[0], "Bell oct +2 not higher: %.0f vs %.0f", (double)zc[2], (double)zc[0]);
        en_init();
        en_play(1);
    }

    /* the slate: each step on its bar, for its bars; EDIT moves on; emptied, Drift is back */
    {
        static const int ROOT[3] = {2, 5, 5};      /* D Dorian: degree 1 for a bar, degree 3 for two */
        int bad_bar = 0, b2, seen = 0;
        en_play(0);
        run(0.05f);
        en_init();
        en_set(P_TEMPO, 120);
        en_set(P_DRIFT, 0);
        en_set(P_SLOT1, 1);
        en_set(P_LEN1, 0);
        en_set(P_SLOT3, 2 * EN_NSHAPE + 1);                        /* the empty slot between is skipped */
        en_set(P_LEN3, 1);
        en_play(1);
        for (b2 = 0; b2 < 60 * 44100 / 256; b2++) {   /* a minute: thirty bars, past any Drift */
            en_render(buf, 256, 2048);
            if (en_bars) {
                unsigned bar = (en_bars - 1u) / 16u;
                seen = (int)bar;
                bad_bar += en_chord_root != ROOT[bar % 3u];
            }
        }
        CHECK(seen >= 28, "the slate's bars: %d", seen);
        CHECK(!bad_bar, "the slate off its bars for %d blocks", bad_bar);
        CHECK(en_drift >= 0.0f && en_drift <= 1.0f, "the slate's progress %f", (double)en_drift);

        /* frozen, it holds */
        {
            int r0;
            en_freeze(1);
            run(0.05f);
            r0 = en_chord_root;
            run(10.0f);
            CHECK(en_chord_root == r0, "frozen, the slate moved on");
            en_freeze(0);
        }
        /* a slot past a shorter scale wraps, and stays in the scale */
        {
            int k;
            en_set(P_SLOT1, 11 * EN_NSHAPE + 1);
            en_set(P_SLOT3, 0);
            en_set(P_LEN1, 0);
            en_pool(0x091u);                       /* root, major third, fifth */
            run(5.0f);
            for (k = 0; k < 4; k++) {
                int pc = (en_chord[k] - 2 + 120) % 12;
                CHECK(pc == 0 || pc == 4 || pc == 7, "a wrapped slot left the scale: %d", en_chord[k]);
            }
            en_pool(EN_MODE_POOL[MODE_DORIAN]);
        }
        /* empty: Drift again */
        {
            int changes = 0, last, k;
            en_set(P_SLOT1, 0);
            run(0.1f);
            last = en_chord_root * 16 + en_chord[0] % 16;
            for (k = 0; k < 120; k++) {
                int now;
                run(1.0f);
                now = en_chord_root * 16 + en_chord[0] % 16;
                changes += now != last;
                last = now;
            }
            CHECK(changes >= 3, "the slate empty, Drift did not return: %d", changes);
        }
        en_init();
        en_play(1);
    }

    /* the arp: only the chord's tones, in the style's order, at the rate; Density its steps */
    {
        int style, k, pcs[4], np, n4;
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DENSITY, 100), en_set(P_DRIFT, 100);
        en_set(P_ARATE, 3), en_set(P_ARANGE, 2);
        run(0.05f);
        for (style = 1; style < EN_NARP; style++) {
            int lo = 127, hi = 0, ups = 0, downs = 0, same = 0, out = 0, wraps;
            en_play(0);
            en_set(P_ARP, style);
            run(0.05f);
            en_play(1);
            bell_logging = 1, bell_n = 0;
            run(6.0f);
            bell_logging = 0;
            np = en_chord_n;
            pcs[0] = en_chord[0] % 12, pcs[1] = en_chord[1] % 12, pcs[2] = en_chord[2] % 12, pcs[3] = en_chord[3] % 12;
            CHECK(bell_n >= 20, "arp style %d: %d notes", style, bell_n);
            for (k = 0; k < bell_n; k++) {
                int pc = bell_log[k] % 12, in = 0, j;
                for (j = 0; j < 4; j++)
                    in |= pc == pcs[j];
                out += !in;
                lo = bell_log[k] < lo ? bell_log[k] : lo;
                hi = bell_log[k] > hi ? bell_log[k] : hi;
                if (k) {
                    ups += bell_log[k] > bell_log[k - 1];
                    downs += bell_log[k] < bell_log[k - 1];
                    same += bell_log[k] == bell_log[k - 1];
                }
            }
            CHECK(!out, "arp style %d: %d notes off the chord", style, out);
            CHECK(hi - lo >= 12, "arp style %d: Range 2 spans only %d", style, hi - lo);
            CHECK(!same, "arp style %d: the same note twice running, %d times", style, same);
            wraps = style == 1 ? downs : style == 2 ? ups : 0;
            CHECK(wraps <= bell_n / (2 * np - 1) + 1, "arp style %d: out of order (%d against it)", style, wraps);
            if (style == 3 || style == 4)
                CHECK(ups > bell_n / 3 && downs > bell_n / 3, "arp style %d: %d up, %d down", style, ups, downs);
            if (style == 6 || style == 7)
                CHECK(ups > bell_n / 4 && downs > bell_n / 4, "arp style %d: %d up, %d down", style, ups, downs);
        }
        en_set(P_ARP, 1);                      /* the rate, and Density */
        en_play(0), run(0.05f), en_play(1);
        bell_logging = 1, bell_n = 0;
        run(8.0f);
        n4 = bell_n;
        en_set(P_ARATE, 0);
        en_play(0), run(0.05f), en_play(1);
        bell_n = 0;
        run(8.0f);
        CHECK(n4 > 3 * bell_n && bell_n > 0, "the arp's rate: %d at 1/16, %d at 1/4", n4, bell_n);
        {   /* Poly, at 1/8: so many notes against the grid, still of the chord */
            static const float WANT[EN_NAPOLY] = {1.0f, 0.75f, 1.25f, 1.5f, 1.75f};
            int base = 0, q;
            en_set(P_ARATE, 2);
            for (q = 0; q < EN_NAPOLY; q++) {
                en_set(P_APOLY, q);
                en_play(0), run(0.05f), en_play(1);
                bell_n = 0;
                run(16.0f);
                if (!q)
                    base = bell_n;
                CHECK(fabs((double)bell_n - (double)base * (double)WANT[q]) <= 3.0 + 0.04 * base,
                      "Poly %d: %d notes against %d", q, bell_n, base);
                pcs[0] = en_chord[0] % 12, pcs[1] = en_chord[1] % 12, pcs[2] = en_chord[2] % 12, pcs[3] = en_chord[3] % 12;
                for (k = 0; k < bell_n; k++) {
                    int pc = bell_log[k] % 12, j, in = 0;
                    for (j = 0; j < 4; j++)
                        in |= pc == pcs[j];
                    CHECK(in, "Poly %d: %d is off the chord", q, bell_log[k]);
                }
            }
            en_set(P_APOLY, 0);
        }
        en_set(P_ARATE, 3), en_set(P_DENSITY, 25);
        en_play(0), run(0.05f), en_play(1);
        bell_n = 0;
        run(8.0f);
        CHECK(bell_n > n4 / 6 && bell_n < n4 / 2, "the arp's Density: %d of %d", bell_n, n4);
        en_set(P_DENSITY, 0);
        en_play(0), run(0.05f), en_play(1);
        bell_n = 0;
        run(4.0f);
        CHECK(bell_n == 0, "the arp at Density 0: %d notes", bell_n);
        bell_logging = 0;
        en_init();
        en_play(1);
    }

    /* Notes: the bass's hits, only the chord's tones, more as it goes up, one long note at 0; the pad's chord thinned */
    {
        int k, j, v, last = 0, pcs[4], seen;
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DRIFT, 100), en_set(P_FIFTH, 0), en_set(P_BASS, 80);
        run(0.05f);
        bell_logging = 1, bass_n = 0;
        run(10.0f);
        CHECK(bass_n <= 1, "Bass notes 0: one long note, %d", bass_n);
        for (v = 10; v <= 100; v += 30) {
            float bp;
            en_set(P_BASSN, v);
            run(0.05f);
            bass_n = 0;
            peak = 0.0f;
            run(12.0f);
            bp = peak;
            pcs[0] = en_chord[0] % 12, pcs[1] = en_chord[1] % 12, pcs[2] = en_chord[2] % 12, pcs[3] = en_chord[3] % 12;
            CHECK(bass_n > last, "Bass notes %d: %d hits, no more than %d", v, bass_n, last);
            CHECK(bp > 0.001f, "Bass notes %d silent", v);
            last = bass_n;
            seen = 0;
            for (k = 0; k < bass_n; k++) {
                int pc = bass_log[k] % 12, in = 0;
                for (j = 0; j < 4; j++)
                    in |= pc == pcs[j];
                CHECK(in, "Bass notes %d: %d is off the chord", v, bass_log[k]);
                seen |= 1 << pc;
            }
            for (k = j = 0; k < 12; k++)
                j += seen >> k & 1;
            if (v == 10)
                CHECK(j == 1, "Bass notes 10: the root alone, not %d notes", j);
            if (v == 100)
                CHECK(j >= 3, "Bass notes 100: only %d notes", j);
        }
        /* a locked speed: the arc and Evolve at full, still 4 a bar at Notes 50 (a chord change may add its own note) */
        en_set(P_TEMPO, 120), en_set(P_MOTION, 100), en_set(P_EVOLVE, 100), en_set(P_BASSN, 50);
        run(2.1f);
        for (k = 0; k < 6; k++) {
            bass_n = 0;
            run(16.0f);                         /* 8 bars */
            CHECK(bass_n >= 31 && bass_n <= 41, "Bass notes 50: %d hits in 8 bars, not 32", bass_n);
        }
        en_set(P_MOTION, 0), en_set(P_EVOLVE, 0);
        en_set(P_BASSN, 0);
        run(0.05f);
        bass_n = 0;
        run(8.0f);
        CHECK(bass_n <= 1, "Bass notes back to 0: %d", bass_n);
        bell_logging = 0;
        en_set(P_PADN, 2);
        run(40.0f);
        seen = 0;
        for (k = 0; k < EN_NPAD; k++)
            seen |= 1 << (en_pad_note[k] % 12);
        for (k = j = 0; k < 12; k++)
            j += seen >> k & 1;
        CHECK(j == 2, "Pad notes 2: %d notes", j);
        CHECK(seen >> en_chord_root & 1, "Pad notes 2 without the root");
        CHECK(en_chord_n >= 3, "the chord still whole for the screen: %d", en_chord_n);
        en_init();
        en_play(1);
    }

    /* the shapes: their names, in the scale; a sus has no third, in the pad, the bells, the arp or the bass */
    {
        static const char *const NAME[EN_NSHAPE] = {"Dm7", "Dm", "Dsus2", "Dsus4", "Dmadd9", "Dm6"};
        int s, d, k, j, pcs[4], n;
        char nm[16];
        for (s = 0; s < EN_NSHAPE; s++) {
            n = en_chord_shape(EN_MODE_POOL[MODE_DORIAN], 2, 0, s, pcs);
            CHECK(!strcmp(en_chord_name(pcs, n, nm), NAME[s]), "shape %d named %s", s, nm);
            CHECK(pcs[0] == 2 && pcs[2] == 9, "shape %d: root %d, fifth %d", s, pcs[0], pcs[2]);
            for (d = 0; d < 7; d++) {
                n = en_chord_shape(EN_MODE_POOL[MODE_DORIAN], 2, d, s, pcs);
                CHECK(n >= 3, "shape %d degree %d: %d notes", s, d, n);
                for (k = 0; k < n; k++)
                    CHECK(EN_MODE_POOL[MODE_DORIAN] >> ((pcs[k] - 2 + 12) % 12) & 1u, "shape %d degree %d off the scale", s, d);
            }
            for (d = 0; d < 5; d++) {              /* a short scale, whatever the shape: three notes at least */
                n = en_chord_shape(EN_MODE_POOL[MODE_PENT_MAJ], 4, d, s, pcs);
                CHECK(n >= 3, "pentatonic shape %d degree %d: %d notes", s, d, n);
            }
        }
        for (k = 0; k < 2; k++) {                  /* Dsus4: the loops, then the arp; no F */
            int fs = 0, bad = 0;
            en_init();
            for (i = 0; i < P_NPARAMS; i++)
                en_set(i, en_param_info(i)->def);
            run(0.05f);
            en_set(P_ROOT, 2), en_set(P_DENSITY, 100), en_set(P_MOTION, 0), en_set(P_EVOLVE, 40), en_set(P_BASSN, 100);
            en_set(P_SLOT1, 3 + 1);                /* degree 1, sus4 */
            en_set(P_ARP, k);
            en_pool(EN_MODE_POOL[MODE_DORIAN]);
            run(0.05f);
            en_regrow();
            en_play(0), run(0.05f), en_play(1);
            run(8.0f);                             /* (what was ringing as it began) */
            bell_logging = 1, bell_n = 0, bass_n = 0;
            run(30.0f);
            bell_logging = 0;
            CHECK(bell_n > 10 && bass_n > 10, "Dsus4: %d bells, %d bass", bell_n, bass_n);
            for (j = 0; j < bell_n; j++)
                fs += bell_log[j] % 12 == 5;
            for (j = 0; j < bass_n; j++)
                fs += bass_log[j] % 12 == 5;
            for (j = 0; j < EN_NPAD; j++)
                bad += en_chord[j] % 12 == 5;
            CHECK(!fs, "Dsus4, arp %d: the third, %d times", k, fs);
            CHECK(!bad, "Dsus4: the third in the pad");
        }
        en_init();
        en_play(1);
    }

    /* Sparkle: nothing at 0; in waves, with rests between; of the scale; the third of no sus; faster when Often's short */
    {
        int k, n0, nshort, nlong, off = 0, wave_on = 0, wave_off = 0;
        int t;
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_DENSITY, 0), en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DRIFT, 100);
        run(0.05f);
        en_regrow();
        en_play(0), run(0.05f), en_play(1);
        run(0.1f);
        bell_logging = 1, bell_n = 0;
        run(20.0f);
        n0 = bell_n;
        CHECK(n0 == 0, "Sparkle 0: %d notes", n0);
        en_set(P_SPARK, 100), en_set(P_SPLEN, 0), en_set(P_SPOFT, 0);
        bell_n = 0;
        for (t = 0; t < 600; t++) {                /* a minute of waves */
            run(0.1f);
            if (en_spark > 0.01f)
                wave_on++;
            else
                wave_off++;
            CHECK(en_spark >= 0.0f && en_spark <= 1.0f, "Sparkle's wave %f", (double)en_spark);
        }
        nshort = bell_n;
        CHECK(nshort > 10, "Sparkle at the top: %d notes in a minute", nshort);   /* by the dice alone, 14 to 40 */
        CHECK(wave_on > 60 && wave_off > 200, "Sparkle's waves: on %d, off %d", wave_on, wave_off);
        for (k = 0; k < bell_n; k++)               /* D Dorian */
            off += !(EN_MODE_POOL[MODE_DORIAN] >> ((bell_log[k] - 2 + 120) % 12) & 1u);
        CHECK(!off, "Sparkle: %d notes off the scale", off);
        en_set(P_SPOFT, 100);
        run(0.05f);
        bell_n = 0;
        run(60.0f);
        nlong = bell_n;
        CHECK(nlong < nshort, "Often at its longest: %d notes, not fewer than %d", nlong, nshort);
        /* over a sus, no third: Dsus4 has no F */
        en_set(P_SPOFT, 0);
        en_set(P_SLOT1, 3 + 1);
        run(8.0f);
        bell_n = 0;
        run(60.0f);
        off = 0;
        for (k = 0; k < bell_n; k++)
            off += bell_log[k] % 12 == 5;
        CHECK(bell_n > 10, "Sparkle over Dsus4: %d notes", bell_n);   /* by the dice alone, 13 to 52 */
        CHECK(!off, "Sparkle over Dsus4: the third %d of %d", off, bell_n);
        en_set(P_SPARK, 0);
        run(0.05f);
        CHECK(en_spark == 0.0f, "Sparkle 0: the wave stays");
        bell_logging = 0;
        en_init();
        en_play(1);
    }

    /* the root rotator: each step for its bars, the chords again from their first; of the new root's scale; off, Root */
    {
        /* D Dorian's first, fourth and third chords, a bar each; D for two bars, G for two: only two chords fit */
        static const int WANT[4] = {2, 7, 7, 0};   /* Dm7, G7; Gm7, C7 */
        int bar, wrong = 0, third = 0, last = -1, b2, off = 0, k, starts = 0;
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_TEMPO, 120), en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DENSITY, 100);
        en_set(P_SONG, 0);                         /* (the rotator free: Song would hold each key a section) */
        en_set(P_LEN1, 0), en_set(P_LEN2, 0), en_set(P_LEN3, 0);
        en_set(P_SLOT1, 1), en_set(P_SLOT2, 3 * EN_NSHAPE + 1), en_set(P_SLOT3, 2 * EN_NSHAPE + 1);
        en_set(P_RSLOT1, 2 + 1), en_set(P_RSLOT3, 7 + 1);   /* the empty step between is skipped */
        en_set(P_RLEN1, 1), en_set(P_RLEN3, 1);
        run(0.05f);
        en_play(0), run(0.05f), en_play(1);
        for (bar = 0; bar < 16; bar++) {
            run(1.0f);                             /* mid-bar */
            b2 = en_chord_root;
            wrong += b2 != WANT[bar % 4];
            third += b2 == 5 || b2 == 10;          /* the third chord: F or Bb */
            CHECK(en_root == (bar % 4 < 2 ? 2 : 7), "bar %d: root %d", bar, en_root);
            starts += b2 != last;
            last = b2;
            run(1.0f);
        }
        CHECK(!wrong, "the rotator's chords off %d bars of 16", wrong);
        CHECK(!third, "the third chord played %d bars: it should never fit", third);
        CHECK(starts >= 8, "the rotator: %d changes", starts);
        en_set(P_ARP, 1);                          /* the arp follows: chord tones of the moving root */
        run(0.05f);
        bell_logging = 1, bell_n = 0;
        for (bar = 0; bar < 8; bar++) {
            bell_n = 0;
            run(1.9f);
            for (k = 0; k < bell_n; k++)
                off += !(EN_MODE_POOL[MODE_DORIAN] >> ((bell_log[k] - en_root + 120) % 12) & 1u);
            run(0.1f);
        }
        bell_logging = 0;
        CHECK(off <= 2, "the arp off the moving scale %d times", off);
        en_set(P_RSLOT1, 0), en_set(P_RSLOT3, 0);  /* off: Root again */
        run(0.05f);
        CHECK(en_root == 2, "the rotator off: root %d", en_root);
        en_init();
        en_play(1);
    }

    /* one root on the rotator: it holds; the chords go on through its bars, no starting again */
    {
        int bar, wrong = 0;
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_TEMPO, 120), en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DENSITY, 100);
        en_set(P_LEN1, 1), en_set(P_LEN2, 1);     /* Dm7 and G7, two bars each */
        en_set(P_SLOT1, 1), en_set(P_SLOT2, 3 * EN_NSHAPE + 1);
        en_set(P_RSLOT1, 2 + 1), en_set(P_RLEN1, 0);   /* D, a step of one bar */
        run(0.05f);
        en_play(0), run(0.05f), en_play(1);
        for (bar = 0; bar < 12; bar++) {
            run(1.0f);
            wrong += en_chord_root != (bar % 4 < 2 ? 2 : 7);
            CHECK(en_root == 2, "one root, bar %d: %d", bar, en_root);
            run(1.0f);
        }
        CHECK(!wrong, "one root: the chords went wrong in %d bars of 12", wrong);
        en_init();
        en_play(1);
    }

    /* Hats: how much hat. Nothing at 0; more hits, louder and brighter on up; ghost notes and rolls near the top; with
     * Hits at 0, none up to 50 and plenty at 100; the thump is the same throughout */
    {
        static const int16_t knob[5] = {0, 25, 50, 75, 100};
        double lev[5], dark[5];
        long hits[5], thump[5];
        int c, j, s;
        for (c = 0; c < 7; c++) {
            const int hts = c < 5 ? knob[c] : c == 5 ? 50 : 100;
            double lo[3] = {0.0, 0.0, 0.0}, e = 0.0, low = 0.0, z[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, ll = 0.0;
            en_init();
            for (i = 0; i < P_NPARAMS; i++)
                en_set(i, en_param_info(i)->def);
            run(0.05f);
            en_set(P_PAD, 0), en_set(P_BELLS, 0), en_set(P_BASS, 0), en_set(P_ECHO, 0), en_set(P_REVERB, 0);
            en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_TEMPO, 120), en_set(P_PULSE, 100);
            en_set(P_HITS, c < 5 ? 50 : 0), en_set(P_HATS, hts);
            run(0.05f);
            en_play(1);
            run(5.0f);
            hat_midi = thump_midi = 0;
            bad = 0;
            peak = 0.0f;
            for (j = 0; j < 3445; j++) {          /* 20 s: what few hear (14 kHz up) and the thump filtered out */
                en_render(buf, 256, 4096);
                for (s = 0; s < 256; s++) {
                    double x = ((double)buf[2 * s] + (double)buf[2 * s + 1]) / 16600000.0, y;
                    y = (x + 4.0 * z[0] + 8.0 * z[1] + 10.0 * z[2] + 8.0 * z[3] + 4.0 * z[4] + z[5]) / 36.0;
                    z[5] = z[4], z[4] = z[3], z[3] = z[2], z[2] = z[1], z[1] = z[0], z[0] = x;
                    ll += 0.35 * (y - ll);                /* the part under about 3 kHz, for the dark end */
                    low += ll * ll;
                    lo[0] += 0.35 * (y - lo[0]), y -= lo[0];
                    lo[1] += 0.35 * (y - lo[1]), y -= lo[1];
                    lo[2] += 0.35 * (y - lo[2]), y -= lo[2];
                    e += y * y;
                }
            }
            CHECK(peak < 1.0f, "Hats %d: peak %f", hts, (double)peak);
            CHECK(!bad, "Hats %d: %d bad samples", hts, bad);
            if (c < 5) {
                lev[c] = e, dark[c] = low;
                hits[c] = hat_midi, thump[c] = thump_midi;
            } else if (c == 5) {
                CHECK(!hat_midi, "Hits 0, Hats 50: %d hats", hat_midi);
            } else {
                CHECK(hat_midi > 200, "Hits 0, Hats 100: only %d hats", hat_midi);
                CHECK(!thump_midi, "Hits 0, Hats 100: %d thumps", thump_midi);
            }
        }
        printf("  Hats 0, 25, 50, 75, 100: %ld, %ld, %ld, %ld, %ld hits in 20 s; in hearing %.1f, %.1f, 0, %.1f, %.1f dB against 50\n",
               hits[0], hits[1], hits[2], hits[3], hits[4], 10.0 * log10((lev[0] + 1e-12) / lev[2]), 10.0 * log10(lev[1] / lev[2]),
               10.0 * log10(lev[3] / lev[2]), 10.0 * log10(lev[4] / lev[2]));
        CHECK(hits[0] == 0, "Hats 0: %ld hats", hits[0]);
        for (c = 1; c < 5; c++) {
            CHECK(hits[c] > hits[c - 1], "Hats %d: %ld hits, no more than %ld below it", knob[c], hits[c], hits[c - 1]);
            CHECK(lev[c] > lev[c - 1], "Hats %d: %g, no louder than %g below it", knob[c], lev[c], lev[c - 1]);
            CHECK(thump[c] == thump[0], "Hats %d moved the thump: %ld, %ld", knob[c], thump[c], thump[0]);
        }
        CHECK(lev[4] > 2.0 * lev[2], "Hats 100 against 50: %g, %g", lev[4], lev[2]);
        /* a real change of colour: bright against dark, per unit of the darker band */
        CHECK(lev[2] / dark[2] > 2.0 * lev[1] / dark[1], "Hats 25 is no darker than 50: %g, %g",
              lev[1] / dark[1], lev[2] / dark[2]);
        en_init();
        en_play(1);
    }

    /* the dealers: every guardrail, over every scale, count, Spice and Form, for 300 numbers */
    {
        static const int SP[5] = {0, 25, 50, 75, 100};
        int md, num, cnt, si, fm, k, bad_c = 0, bad_r = 0, bad_det = 0, iii = 0, deals = 0, kinds = 0;
        static uint32_t seen[300];
        for (md = 0; md < EN_NMODES; md++) {
            int st[12], m = en_scale(EN_MODE_POOL[md], st);
            for (num = 1; num <= 300; num++)
                for (cnt = 2; cnt <= 4; cnt++)
                    for (si = 0; si < 5; si++)
                        for (fm = 0; fm < EN_NDFORM; fm++) {
                            int16_t s[4], l[4], s2[4], l2[4];
                            int n = cnt > m ? m : cnt, tot, sum = 0, mx = 0, d[4], ok = 1, j;
                            tot = en_deal_chords(EN_MODE_POOL[md], num, cnt, SP[si], fm, 72, s, l);
                            en_deal_chords(EN_MODE_POOL[md], num, cnt, SP[si], fm, 72, s2, l2);
                            bad_det += memcmp(s, s2, sizeof s) || memcmp(l, l2, sizeof l);
                            for (k = 0; k < 4; k++) {
                                if (k >= n) {
                                    ok &= !s[k];
                                    continue;
                                }
                                ok &= s[k] >= 1 && s[k] <= m * EN_NSHAPE && l[k] >= 0 && l[k] < EN_NBARS;
                                d[k] = (s[k] - 1) / EN_NSHAPE;
                                sum += EN_SLATE_BARS[l[k]];
                                mx = EN_SLATE_BARS[l[k]] > mx ? EN_SLATE_BARS[l[k]] : mx;
                            }
                            ok &= d[0] == 0 && sum == tot && tot >= 4 && tot <= 64 && !(tot & (tot - 1));
                            for (k = 0; k < n; k++) {
                                int pcs[4], next = d[(k + 1) % n];
                                for (j = 0; j < k; j++)
                                    ok &= d[j] != d[k];                   /* no chord twice */
                                en_chord_tones(EN_MODE_POOL[md], 0, d[k], pcs);
                                if (m >= 5 && (pcs[2] - pcs[0] + 12) % 12 == 6)   /* diminished: with Spice, never longest */
                                    ok &= SP[si] >= 50 && EN_SLATE_BARS[l[k]] < mx;
                                if (SP[si] < 70)
                                    ok &= (st[next] - st[d[k]] + 12) % 12 != 6;   /* no tritone, plain */
                            }
                            if (md == 0 && cnt == 4 && si == 0) {
                                deals++;
                                for (k = 1; k < n; k++)
                                    iii += d[k] == 2;
                            }
                            if (md == 1 && cnt == 4 && si == 1 && fm == 1) {
                                uint32_t h = (uint32_t)s[1] << 16 | (uint32_t)s[2] << 8 | (uint32_t)s[3];
                                for (j = 0; j < kinds && seen[j] != h; j++)
                                    ;
                                if (j == kinds && kinds < 300)
                                    seen[kinds++] = h;
                            }
                            bad_c += !ok;
                        }
        }
        CHECK(!bad_c, "Deal: %d progressions break a guardrail", bad_c);
        CHECK(!bad_det, "Deal: %d numbers dealt two ways", bad_det);
        CHECK(iii * 10 < deals, "Deal: the mediant %d times in %d plain deals", iii, deals);
        CHECK(kinds >= 60, "Deal: only %d progressions from 300 numbers", kinds);

        for (num = 1; num <= 300; num++)
            for (cnt = 2; cnt <= 4; cnt++)
                for (si = 0; si < 5; si++)
                    for (fm = 0; fm < EN_NRFORM; fm++)
                        for (k = 0; k < 3; k++) {
                            static const int CYC[3] = {8, 16, 32};
                            int16_t r[4], l[4];
                            int key[4], ok = 1, j, i, mv[4], sp = SP[si];
                            en_deal_roots(2, num, cnt, sp, fm, CYC[k], 0, r, l);
                            for (i = 0; i < 4; i++) {
                                if (i >= cnt) {
                                    ok &= !r[i];
                                    continue;
                                }
                                ok &= r[i] >= 1 && r[i] <= 12 && l[i] >= 0 && l[i] < EN_NRBARS;
                                key[i] = (r[i] - 1 - 2 + 12) % 12;
                                ok &= !(EN_ROOT_BARS_T[l[i]] % CYC[k]);   /* whole passes of the chords */
                                for (j = 0; j < i; j++)
                                    ok &= key[j] != key[i];               /* home once, no key twice */
                            }
                            ok &= key[0] == 0;
                            for (i = 0; i < cnt; i++) {
                                int iv = (key[(i + 1) % cnt] - key[i] + 12) % 12, home = i == cnt - 1;
                                mv[i] = iv;
                                if (fm == 3 && i == cnt - 2) {
                                    ok &= iv == 2;                        /* Lift: up a whole step at the last */
                                    continue;
                                }
                                if (fm == 3 && home)
                                    continue;                             /* (and from there, the drop home) */
                                ok &= iv == 5 || iv == 7 || iv == 2 || iv == 10 || (sp >= 35 && (iv == 3 || iv == 9)) ||
                                      (sp >= 60 && (iv == 4 || iv == 8)) || (sp >= 85 && (iv == 1 || iv == 11 || iv == 6));
                            }
                            if (sp < 85 && fm != 3)
                                for (i = 0; i < cnt; i++) {
                                    int a = mv[i], b = mv[(i + 1) % cnt];
                                    int fa = a == 4 || a == 8 || a == 1 || a == 11 || a == 6, fb = b == 4 || b == 8 || b == 1 || b == 11 || b == 6;
                                    ok &= !(fa && fb && cnt > 2);         /* never two far moves running */
                                }
                            bad_r += !ok;
                        }
        CHECK(!bad_r, "Deal keys: %d journeys break a guardrail", bad_r);
    }

    /* Song: a section holds its motif, even at Evolve 100; the next develops it; a return brings it back exactly.
     * Song off: the loops change within a minute, as they always did */
    {
        static int8_t a[EN_NLOOP][EN_LOOP_MAX], now[EN_NLOOP][EN_LOOP_MAX];
        int l, s, same, held, moved;
        for (held = 0; held < 2; held++) {         /* Evolve 100, then 0 for the exact return */
            en_init();
            for (i = 0; i < P_NPARAMS; i++)
                en_set(i, en_param_info(i)->def);
            run(0.05f);
            en_set(P_TEMPO, 120), en_set(P_MOTION, 0), en_set(P_DENSITY, 60), en_set(P_SEED, 17);
            en_set(P_EVOLVE, held ? 0 : 100);
            run(0.05f);
            en_play(0), run(0.05f), en_play(1);
            run(20.0f);                            /* (Density grown to its count) */
            for (l = 0; l < EN_NLOOP; l++)
                for (s = 0; s < EN_LOOP_MAX; s++)
                    a[l][s] = en_loop_note[l][s];
            run(100.0f);                           /* still the first section (144 s at 120) */
            same = 1;
            for (l = 0; l < EN_NLOOP; l++)
                for (s = 0; s < EN_LOOP_MAX; s++)
                    same &= en_loop_note[l][s] == a[l][s];
            CHECK(same, "Song, Evolve %d: the motif changed within its section", held ? 0 : 100);
            run(60.0f);                            /* the second section: developed */
            for (l = 0, moved = 0; l < EN_NLOOP; l++)
                for (s = 0; s < EN_LOOP_MAX; s++)
                    moved += en_loop_note[l][s] != a[l][s];
            CHECK(moved > 0, "Song: the second section is the first's motif unchanged");
            if (held) {
                run(150.0f);                       /* the third: the first's again (A B A) */
                same = 1;
                for (l = 0; l < EN_NLOOP; l++)
                    for (s = 0; s < EN_LOOP_MAX; s++)
                        same &= en_loop_note[l][s] == a[l][s];
                CHECK(same, "Song: the return isn't the first section's motif");
            }
        }
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_SONG, 0), en_set(P_TEMPO, 120), en_set(P_MOTION, 0), en_set(P_DENSITY, 60), en_set(P_EVOLVE, 100);
        en_set(P_SEED, 17);
        run(0.05f);
        en_play(0), run(0.05f), en_play(1);
        run(20.0f);
        for (l = 0; l < EN_NLOOP; l++)
            for (s = 0; s < EN_LOOP_MAX; s++)
                now[l][s] = en_loop_note[l][s];
        run(60.0f);
        for (l = 0, moved = 0; l < EN_NLOOP; l++)
            for (s = 0; s < EN_LOOP_MAX; s++)
                moved += en_loop_note[l][s] != now[l][s];
        CHECK(moved > 4, "Song off, Evolve 100: only %d notes moved in a minute", moved);
        en_init();
        en_play(1);
    }

    /* a deal lands from the top: en_restart takes the slate back to its first step at the next bar */
    {
        int before;
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_TEMPO, 120), en_set(P_MOTION, 0), en_set(P_EVOLVE, 0);
        en_set(P_SLOT1, 1), en_set(P_SLOT2, 3 * EN_NSHAPE + 1), en_set(P_SLOT3, 4 * EN_NSHAPE + 1);
        en_set(P_SLOT4, 5 * EN_NSHAPE + 1);
        for (i = 0; i < EN_NSLOT; i++)
            en_set(P_LEN1 + i, 3);                 /* four bars each */
        run(0.05f);
        en_play(0), run(0.05f), en_play(1);
        run(19.0f);                                /* bar 10: the third step, A in D Dorian */
        before = en_chord_root;
        en_restart(1);
        run(2.5f);
        CHECK(before == 9 && en_chord_root == 2, "a restart: %d then %d, not 9 then 2", before, en_chord_root);
        en_init();
        en_play(1);
    }

    /* the Rings: a count gives that many hits; a turn rotates the ring; Still holds them through Evolve, Follow lets
     * them move; Auto is what Hits and Hats say */
    {
        int bar, changed, d;
        uint32_t p0[EN_NDRUM];
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        for (d = 0; d < EN_NDRUM; d++) {
            int n = 0;
            uint32_t q;
            for (q = en_drum_pat[d]; q; q &= q - 1u)
                n++;
            CHECK(n == en_ring_auto(d, en_param_info(P_HITS)->def, en_param_info(P_HATS)->def), "ring %d on Auto: %d hits", d, n);
        }
        en_set(P_RHITS1, 1 + 5), en_set(P_RTURN1, 0);
        run(0.05f);
        p0[1] = en_drum_pat[1];
        {
            int n = 0;
            uint32_t q;
            for (q = p0[1]; q; q &= q - 1u)
                n++;
            CHECK(n == 5, "closed hat at 5: %d hits", n);
        }
        en_set(P_RTURN1, 3);
        run(0.05f);
        CHECK(en_drum_pat[1] == en_euclid(5, EN_DRUM_N[1], 3) && en_drum_pat[1] != p0[1], "closed hat turned 3: %x from %x",
              (unsigned)en_drum_pat[1], (unsigned)p0[1]);
        en_set(P_RHITS0, 1 + 16);
        run(0.05f);
        CHECK(en_drum_pat[0] == 0xFFFFu, "thump at 16: every step, %x", (unsigned)en_drum_pat[0]);
        en_set(P_RHITS0, 1 + 4), en_set(P_RHITS2, 1 + 2);
        en_set(P_EVOLVE, 100), en_set(P_PULSE, 60), en_set(P_RMOVES, 1);
        run(0.05f);
        en_play(1);
        for (d = 0; d < EN_NDRUM; d++)
            p0[d] = en_drum_pat[d];
        for (bar = 0, changed = 0; bar < 40; bar++) {
            run(2.0f);
            for (d = 0; d < EN_NDRUM; d++)
                changed += en_drum_pat[d] != p0[d];
        }
        CHECK(!changed, "Still: the rings moved %d times in 40 bars", changed);
        en_set(P_RMOVES, 0);
        for (bar = 0, changed = 0; bar < 40; bar++) {
            run(2.0f);
            for (d = 0; d < EN_NDRUM; d++)
                changed += en_drum_pat[d] != p0[d];
        }
        CHECK(changed > 0, "Follow: the rings never moved in 40 bars at Evolve 100");
        en_init();
        en_play(1);
    }

    /* Seed: a number grows the same garden each PLAY, however long it rested and whatever the pad's breaths did
     * meanwhile; another number, another garden; up from Wild keeps the one growing, its loops as they began */
    {
        static uint8_t first[4000];
        static int8_t loops[EN_NLOOP][EN_LOOP_MAX];
        int k, n0 = 0, l, s, same, kept, rests[3] = {3, 11, 5};
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_DENSITY, 70), en_set(P_EVOLVE, 60), en_set(P_ARP, 1), en_set(P_SPARK, 60), en_set(P_SEED, 417);
        run(0.05f);
        for (k = 0; k < 3; k++) {
            en_play(0);
            run((float)rests[k]);                  /* a rest of its own length each time */
            bell_logging = 1, bell_n = 0;
            en_play(1);
            run(60.0f);
            bell_logging = 0;
            if (!k) {
                n0 = bell_n;
                memcpy(first, bell_log, sizeof first);
            } else {
                same = bell_n == n0 && !memcmp(first, bell_log, (size_t)n0);
                CHECK(same, "Seed 417, play %d: %d notes against %d, or other notes", k + 1, bell_n, n0);
            }
        }
        CHECK(n0 > 40, "Seed 417: only %d notes in a minute", n0);
        CHECK(en_seed_now == 417, "Seed 417: the garden's number is %d", en_seed_now);
        en_set(P_SEED, 418);
        run(0.05f);
        en_play(0);
        run(3.0f);
        bell_logging = 1, bell_n = 0;
        en_play(1);
        run(60.0f);
        bell_logging = 0;
        same = bell_n == n0 && !memcmp(first, bell_log, (size_t)n0);
        CHECK(!same, "Seed 418 grew Seed 417's garden");

        /* Wild: a new number each new garden; turned to it, nothing starts again; PLAY grows those loops again */
        en_play(0);
        en_set(P_SEED, 0);
        run(3.0f);
        en_regrow();
        run(0.05f);
        k = en_seed_now;
        CHECK(k >= 1 && k <= 999, "Wild: the garden's number %d", k);
        en_regrow();
        run(0.05f);
        CHECK(en_seed_now != k || en_seed_now != (en_regrow(), run(0.05f), en_seed_now), "Wild: the same number three times");
        k = en_seed_now;
        for (l = 0; l < EN_NLOOP; l++)
            for (s = 0; s < EN_LOOP_MAX; s++)
                loops[l][s] = en_loop_note[l][s];
        en_play(1);
        run(20.0f);                                /* it grows on, Evolve changing it */
        en_set(P_SEED, k);
        run(0.05f);
        CHECK(en_seed_now == k, "kept: the number moved, %d to %d", k, en_seed_now);
        en_play(0);
        run(4.0f);
        en_play(1);
        run(0.02f);
        kept = 1;
        for (l = 0; l < EN_NLOOP; l++)
            for (s = 0; s < EN_LOOP_MAX; s++)
                kept &= loops[l][s] == en_loop_note[l][s];
        CHECK(kept, "kept: PLAY didn't grow the loops it began with");
        en_play(0);
        en_set(P_SEED, 0);
        en_init();
        en_play(1);
    }

    /* the pad's colours: Rhodes, Voices, Strings, Noir; each sounds, clean and bounded, about as loud as Voices */
    {
        static const char *const PN[4] = {"Rhodes", "Voices", "Strings", "Noir"};
        double lev[4];
        char nm[12];
        int c;
        for (c = 0; c < 4; c++) {
            double e;
            en_param_text(P_PADTONE, c, nm);
            CHECK(!strcmp(nm, PN[c]), "pad colour %d named %s", c, nm);
            en_init();
            for (i = 0; i < P_NPARAMS; i++)
                en_set(i, en_param_info(i)->def);
            run(0.05f);
            en_set(P_BELLS, 0), en_set(P_BASS, 0), en_set(P_PULSE, 0), en_set(P_ECHO, 0), en_set(P_REVERB, 0);
            en_set(P_PAD, 80), en_set(P_PADTONE, c);
            run(0.05f);
            en_play(1);
            run(8.0f);
            peak = sum2 = 0.0f;
            bad = 0;
            run(20.0f);
            lev[c] = e = (double)sum2;
            CHECK(peak < 1.0f, "pad %s peak %f", PN[c], (double)peak);
            CHECK(!bad, "pad %s: %d bad samples", PN[c], bad);
        }
        for (c = 0; c < 4; c++)
            CHECK(lev[c] > 0.4 * lev[1] && lev[c] < 2.5 * lev[1], "pad %s against Voices: %.1f dB", PN[c],
                  10.0 * log10(lev[c] / lev[1]));
        en_init();
        en_play(1);
    }

    /* the bells' colours: each sounds, stays bounded and clean; the strike comes as it should */
    {
        static const char *const NAME[EN_NBCOLOR] = {"Bell", "Soft", "Wood", "Strings", "Piano", "Lead"};
        float early[EN_NBCOLOR], later[EN_NBCOLOR];
        char t[12];
        int c;
        for (c = 0; c < EN_NBCOLOR; c++) {
            en_param_text(P_BCOLOR, c, t);
            CHECK(!strcmp(t, NAME[c]), "colour %d named %s", c, t);
            en_init();
            for (i = 0; i < P_NPARAMS; i++)
                en_set(i, en_param_info(i)->def);
            run(0.05f);
            en_set(P_PAD, 0), en_set(P_BASS, 0), en_set(P_PULSE, 0), en_set(P_ECHO, 0), en_set(P_REVERB, 0);
            en_set(P_MOTION, 0), en_set(P_EVOLVE, 0), en_set(P_DENSITY, 100), en_set(P_BELLS, 100);
            en_set(P_ARP, 1), en_set(P_ARATE, 0), en_set(P_ARANGE, 1), en_set(P_BCOLOR, c);
            run(0.05f);
            en_play(0), en_panic(), run(0.5f), en_play(1);
            peak = 0.0f;
            run(0.03f);                            /* the first note's strike */
            early[c] = peak;
            peak = 0.0f;
            run(0.8f);
            later[c] = peak;
            CHECK(later[c] > 0.005f, "colour %s silent: %f", NAME[c], (double)later[c]);
            peak = 0.0f;
            bad = 0;
            run(30.0f);
            CHECK(peak < 1.0f, "colour %s peak %f", NAME[c], (double)peak);
            CHECK(!bad, "colour %s: %d bad samples", NAME[c], bad);
        }
        CHECK(early[3] / later[3] < 0.5f * early[4] / later[4], "Strings %f/%f should rise slower than Piano %f/%f",
              (double)early[3], (double)later[3], (double)early[4], (double)later[4]);
        CHECK(early[1] / later[1] < early[4] / later[4], "Soft should have no strike: %f/%f, Piano %f/%f",
              (double)early[1], (double)later[1], (double)early[4], (double)later[4]);
        en_init();
        en_play(1);
    }

    /* Swoosh: silence at 0; passes that cross the field; a knob with a real range; dark and bright */
    {
        int run_i, t, passes = 0, was = 0, left_first = 0, right_first = 0;
        float hi[2];
        for (run_i = 0; run_i < 2; run_i++) {     /* level: 100, then 1 */
            en_init();
            for (i = 0; i < P_NPARAMS; i++)
                en_set(i, en_param_info(i)->def);
            run(0.05f);
            en_set(P_PAD, 0), en_set(P_BELLS, 0), en_set(P_BASS, 0), en_set(P_PULSE, 0), en_set(P_ECHO, 0), en_set(P_REVERB, 0);
            run(0.05f);
            en_play(1);
            run(30.0f);
            peak = 0.0f;
            run(20.0f);
            CHECK(peak < 0.001f, "Swoosh 0: not silent, %f", (double)peak);
            en_set(P_SWOOSH, run_i ? 1 : 100), en_set(P_SWLEN, 0), en_set(P_SWOFT, 0);
            peak = 0.0f;
            bad = 0;
            passes = was = 0;
            for (t = 0; t < 1200; t++) {          /* a minute of 2 s passes, 10 s apart */
                float p0 = peak;
                peak = 0.0f;
                run(0.05f);
                {
                    int on = peak > (run_i ? 0.00002f : 0.002f);
                    passes += on && !was;
                    was = on;
                }
                peak = peak > p0 ? peak : p0;
            }
            hi[run_i] = peak;
            CHECK(!bad, "Swoosh: %d bad samples", bad);
            CHECK(peak < 1.0f, "Swoosh: peak %f", (double)peak);
            CHECK(passes >= 3 && passes <= 8, "Swoosh: %d passes in a minute", passes);
        }
        CHECK(hi[0] > 0.1f, "Swoosh 100: only %f", (double)hi[0]);
        CHECK(hi[0] > 20.0f * hi[1] && hi[1] > 0.0005f, "Swoosh 1 against 100: %f, %f", (double)hi[1], (double)hi[0]);

        /* a pass crosses: one side at its start, the other at its end; both directions turn up */
        en_init();
        for (i = 0; i < P_NPARAMS; i++)
            en_set(i, en_param_info(i)->def);
        run(0.05f);
        en_set(P_PAD, 0), en_set(P_BELLS, 0), en_set(P_BASS, 0), en_set(P_PULSE, 0), en_set(P_ECHO, 0), en_set(P_REVERB, 0);
        run(0.05f);
        en_play(1);
        run(30.0f);
        en_set(P_SWOOSH, 100), en_set(P_SWLEN, 100), en_set(P_SWOFT, 0);
        {
            double l[200], r[200];
            int w, j, start = -1;
            for (w = 0; w < 200; w++) {           /* half a second a window: 100 s */
                l[w] = r[w] = 0.0;
                for (j = 0; j < 86; j++) {
                    int s;
                    en_render(buf, 256, 4096);
                    for (s = 0; s < 256; s++) {
                        double a = (double)buf[2 * s] / 8300000.0, b = (double)buf[2 * s + 1] / 8300000.0;
                        l[w] += a * a, r[w] += b * b;
                    }
                }
                if (l[w] + r[w] > 1e-6 && start < 0)
                    start = w;
                if (start >= 0 && l[w] + r[w] < 1e-9 && w - start < 6) {
                    start = -1;                      /* a blip, an earlier sound's last breath: not a pass */
                } else if (start >= 0 && l[w] + r[w] < 1e-9) {   /* a pass ended: its first and last few windows */
                    double el = l[start + 1] + l[start + 2], er = r[start + 1] + r[start + 2];
                    double ll = l[w - 2] + l[w - 3], lr = r[w - 2] + r[w - 3];
                    CHECK((el > 2.0 * er && lr > 2.0 * ll) || (er > 2.0 * el && ll > 2.0 * lr),
                          "Swoosh: no crossing: start %g/%g, end %g/%g", el, er, ll, lr);
                    left_first += el > er;
                    right_first += er > el;
                    start = -1;
                }
            }
            CHECK(left_first + right_first >= 3, "Swoosh: %d passes in 100 s", left_first + right_first);
            CHECK(left_first && right_first, "Swoosh: always one way: %d, %d", left_first, right_first);
        }

        /* Tone: white is brighter than brown */
        {
            long zc[2];
            int c;
            for (c = 0; c < 2; c++) {
                int j, s;
                double last = 0.0;
                en_set(P_SWTONE, c ? 100 : 0);
                zc[c] = 0;
                for (j = 0; j < 2000; j++) {
                    en_render(buf, 256, 4096);
                    for (s = 0; s < 256; s++) {
                        double a = (double)buf[2 * s] + (double)buf[2 * s + 1];
                        zc[c] += a * last < 0.0;
                        last = a;
                    }
                }
            }
            CHECK(zc[0] > 3 * zc[1], "Swoosh tone: white %ld crossings, brown %ld", zc[0], zc[1]);
        }

        /* at rest it fades away with the rest */
        en_play(0);
        run(20.0f);
        peak = 0.0f;
        run(1.0f);
        CHECK(peak < 0.001f, "Swoosh at rest: %f", (double)peak);

        en_init();
        en_play(1);
    }

    /* stop: the tails die away */
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, en_param_info(i)->def);
    en_play(0);
    run(40.0f);
    peak = 0.0f;
    run(1.0f);
    CHECK(peak < 0.01f, "quiet after stopping (%f)", (double)peak);
    CHECK(midi_count > 50u, "MIDI out (%u)", midi_count);

    printf(fails ? "amb: %d FAILED\n" : "amb: ok\n", fails);
    return fails != 0;
}

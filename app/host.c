/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 in fm1-base's host simulator (host/plat_host.c, which includes app.c and so this): AMB-1's checks for
 * "expect", its command, a reboot and the screen reader's dump.
 *
 * AMB-1's checks: playing frozen view piece pool dirty sixteenths ext pad_level bass_level chordroot chordN loopnotes
 * parN battery (fm1-base's own: store_writes clocks_out midi_out peak_db_max peak_db_min). Its commands: storev3 N
 * (the project as AMB-1 0.9 before the layer pages wrote it, format 3, the playing piece's Pad at N); battery N (the
 * battery read as N, plat.h's plat_battery: -1..4; "battery off" the simulator's own, full). */

int app_expect(const char *what, int *got)
{
    int g;
    if (!strcmp(what, "playing"))
        g = en_playing;
    else if (!strcmp(what, "frozen"))
        g = en_frozen;
    else if (!strcmp(what, "view"))
        g = ui.view;
    else if (!strcmp(what, "piece"))
        g = proj.piece;
    else if (!strcmp(what, "pool"))               /* the scale, relative to the root (hex ok) */
        g = proj.pool;
    else if (!strcmp(what, "battery"))
        g = batt();
    else if (!strcmp(what, "dirty"))
        g = ui.dirty;
    else if (!strcmp(what, "sixteenths"))
        g = (int)en_bars;
    else if (!strcmp(what, "ext"))
        g = en_ext;
    else if (!strcmp(what, "pad_level"))
        g = (int)(en_pad_level * 100.0f);
    else if (!strcmp(what, "bass_level"))
        g = (int)(en_bass_level * 100.0f);
    else if (!strncmp(what, "ring", 4)) {        /* ringN: the hits in ring N's pattern now (0 thump, 1, 2 hats) */
        uint32_t p = en_drum_pat[atoi(what + 4) % EN_NDRUM];
        for (g = 0; p; p &= p - 1u)
            g++;
    } else if (!strcmp(what, "dreamkept"))         /* Dream is the number of the dream playing */
        g = proj.par[P_DREAM] >= 2 && proj.par[P_DREAM] - 1 == ui.dream_now;
    else if (!strcmp(what, "dreamdealt"))        /* a dream dealt: its number, the chords dealt */
        g = ui.dream_now && proj.par[P_DEAL] && proj.par[P_SLOT1];
    else if (!strcmp(what, "seedkept"))          /* the Seed is the number of the garden growing */
        g = proj.par[P_SEED] && proj.par[P_SEED] == en_seed_now;
    else if (!strcmp(what, "chordroot"))
        g = en_chord_root;
    else if (!strncmp(what, "chord", 5))          /* chordN: pad voice N's note */
        g = en_chord[atoi(what + 5) & 3];
    else if (!strcmp(what, "loopnotes")) {        /* the notes in all the loops */
        int l, i;
        g = 0;
        for (l = 0; l < EN_NLOOP; l++)
            for (i = 0; i < EN_LOOP_MAX; i++)
                g += en_loop_note[l][i] >= 0;
    } else if (!strncmp(what, "par", 3))          /* parN: parameter N (amb.h P_*) */
        g = proj.par[atoi(what + 3)];
    else
        return 0;
    *got = g;
    return 1;
}

/* render FILE SECONDS [FADEIN] [TAIL]: the piece as set, played from its start straight into a 24-bit 44.1 kHz WAV,
 * as fast as the computer goes (no screen meanwhile). FADEIN seconds of fade at the head; after SECONDS it stops and
 * the tails ring on, up to TAIL seconds or until two seconds of near silence (-89 dB). The output as the synth gives
 * it at full volume; a sample past full scale is held there and counted. */
static void put24(FILE *f, int32_t v)
{
    fputc(v & 255, f);
    fputc((v >> 8) & 255, f);
    fputc((v >> 16) & 255, f);
}
static void hdr24(FILE *f, uint32_t frames)
{
    uint32_t v;
    uint16_t s;
    fseek(f, 0, SEEK_SET);
    fwrite("RIFF", 1, 4, f);
    v = 36u + frames * 6u;
    fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    v = 16u;
    fwrite(&v, 4, 1, f);
    s = 1u;
    fwrite(&s, 2, 1, f);
    s = 2u;
    fwrite(&s, 2, 1, f);
    v = 44100u;
    fwrite(&v, 4, 1, f);
    v = 44100u * 6u;
    fwrite(&v, 4, 1, f);
    s = 6u;
    fwrite(&s, 2, 1, f);
    s = 24u;
    fwrite(&s, 2, 1, f);
    fwrite("data", 1, 4, f);
    v = frames * 6u;
    fwrite(&v, 4, 1, f);
}
static int render_wav(const char *path, double secs, double fade, double tail)
{
    static int32_t blk[2 * APP_BLOCK];
    static const uint8_t blank[44] = {0};
    const uint32_t play = (uint32_t)(secs * 44100.0), fade_n = (uint32_t)(fade * 44100.0),
                   tail_n = (uint32_t)(tail * 44100.0);
    uint32_t frames = 0, quiet = 0, clipped = 0, k;
    int32_t peak = 0;
    int stopped = 0;
    FILE *f = fopen(path, "wb");
    if (!f) {
        printf("render: can't write %s\n", path);
        return 0;
    }
    fwrite(blank, 1, 44, f);
    if (!en_playing)
        en_play(1);                                  /* (from its start: a Seed grows its garden from here) */
    while (frames < play + tail_n) {
        if (!stopped && frames >= play) {
            en_play(0);                              /* the end: the tails ring on */
            stopped = 1;
        }
        app_render(blk, APP_BLOCK, 4096u);
        for (k = 0; k < APP_BLOCK; k++) {
            const uint32_t t = frames + k;
            float g = 1.0f;
            int32_t c[2];
            int ch;
            if (t < fade_n) {
                const float x = (float)t / (float)fade_n;
                g = x * x * (3.0f - 2.0f * x);       /* a smooth rise */
            }
            for (ch = 0; ch < 2; ch++) {
                int32_t v = (int32_t)((float)blk[2 * k + ch] * g);
                if (v > 8388607 || v < -8388608) {
                    clipped++;
                    v = v > 0 ? 8388607 : -8388608;
                }
                peak = (v < 0 ? -v : v) > peak ? (v < 0 ? -v : v) : peak;
                c[ch] = v;
                put24(f, v);
            }
            if (stopped)
                quiet = (c[0] < 300 && c[0] > -300 && c[1] < 300 && c[1] > -300) ? quiet + 1 : 0;
        }
        frames += APP_BLOCK;
        if (stopped && quiet > 2u * 44100u)
            break;
    }
    hdr24(f, frames);
    fclose(f);
    printf("render: %s, %.1f s, peak %.1f dB, %u samples held at full scale\n", path, (double)frames / 44100.0,
           peak ? 20.0 * log10((double)peak / 8388607.0) : -144.0, clipped);
    return 1;
}

int app_host_cmd(const char *cmd, const char *a, const char *b, const char *c, const char *d)
{
    (void)b, (void)c, (void)d;
    if (!strcmp(cmd, "render"))                  /* render FILE SECONDS [FADEIN] [TAIL] (above) */
        return render_wav(a, atof(b), *c ? atof(c) : 0.0, *d ? atof(d) : 30.0);
    if (!strcmp(cmd, "piece")) {                 /* piece N: piece N (0..7), as PRESETS takes it */
        set_piece(atoi(a));
        return 1;
    }
    if (!strcmp(cmd, "knob")) {                  /* knob P V: parameter P (amb.h P_*) to V, as a knob turned to it */
        knob_set(atoi(a), atoi(b));
        return 1;
    }
    if (!strcmp(cmd, "transport")) {             /* transport 1|0: growing or resting, as PLAY (no Dream dealt) */
        en_play(atoi(a));
        sp_tp.playing = (uint8_t)(atoi(a) != 0);
        return 1;
    }
    if (!strcmp(cmd, "battery")) {               /* battery N: plat_battery as N (-1..4), "battery off" its own */
        batt_host = (int8_t)(strcmp(a, "off") ? atoi(a) : -2);
        return 1;
    }
    /* storev6 N, storev7 N: the project as those builds wrote it; 6: Chord 1 on degree N; 7: Chord 1 as N */
    if (!strcmp(cmd, "storev6") || !strcmp(cmd, "storev7")) {
        static project_v7_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = cmd[6] == '6' ? 6u : 7u;
        for (k = 0; k < V7_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_SLOT1] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V7_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].par[P_SLOT1] = i == proj.piece ? o.par[P_SLOT1] : 0;
            o.slot[i].pool = proj.slot[i].pool;
        }
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev8")) {               /* storev8 N: the project as Sparkle's build wrote it (format 8) */
        static project_v8_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 8u;
        for (k = 0; k < V8_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V8_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev9")) {               /* storev9 N: the project as the root rotator's build wrote it (format 9) */
        static project_v9_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 9u;
        for (k = 0; k < V9_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V9_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev14")) {              /* storev14 N: the project as Dream's build wrote it (format 14) */
        static project_v14_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 14u;
        for (k = 0; k < V14_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V14_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev13")) {              /* storev13 N: the project as the dealers' build wrote it (format 13) */
        static project_v13_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 13u;
        for (k = 0; k < V13_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V13_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev12")) {              /* storev12 N: the project as the Seed's build wrote it (format 12) */
        static project_v12_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 12u;
        for (k = 0; k < V12_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V12_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev11")) {              /* storev11 N: the project as Swoosh's build wrote it (format 11) */
        static project_v11_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 11u;
        for (k = 0; k < V11_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V11_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev10")) {              /* storev10 N: the project as the bells' colours' build wrote it (format 10) */
        static project_v10_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 10u;
        for (k = 0; k < V10_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V10_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev5")) {               /* storev5 N: the project as the slate's build wrote it (format 5) */
        static project_v5_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 5u;
        for (k = 0; k < V5_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V5_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev4")) {               /* storev4 N: as storev3, the project as 1.3.1 wrote it (format 4) */
        static project_v4_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 4u;
        for (k = 0; k < V4_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V4_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    if (!strcmp(cmd, "storev3")) {
        static project_v3_t o;
        int i, k;
        memset(&o, 0, sizeof o);
        o.magic = PROJ_MAGIC;
        o.format = 3u;
        for (k = 0; k < V3_NPARAMS; k++)
            o.par[k] = proj.par[k];
        o.par[P_PAD] = (int16_t)atoi(a);
        o.pool = proj.pool;
        o.leds = proj.leds;
        o.piece = proj.piece;
        for (i = 0; i < NPIECES; i++) {
            for (k = 0; k < V3_NPARAMS; k++)
                o.slot[i].par[k] = proj.slot[i].par[k];
            o.slot[i].pool = proj.slot[i].pool;
        }
        o.slot[proj.piece].par[P_PAD] = o.par[P_PAD];
        plat_store_save(OBJ_PROJ, &o, sizeof o);
        return 1;
    }
    return 0;
}

void app_host_reboot(void) { memset(&proj, 0, sizeof proj); }

void app_speak_dump(void) { speak_dump(); }

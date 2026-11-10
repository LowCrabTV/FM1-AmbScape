/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 Charles Vestal */
/* AMB-1 project: one flash object (OBJ_PROJ). A project of another format, or none, gives the
 * defaults. Part of the unity build, after app.h.
 *
 * The pieces (their recipes are the engine's, en_piece): each keeps its own settings as you leave it;
 * Tune and MIDI out are global. */
project_t proj;

const char *piece_name(int i) { return en_piece_name(i); }

static int global_par(int p) { return p == P_TUNE || p == P_MIDI; }

static void recipe(int i, slot_t *s) { en_piece(i, s->par, &s->pool); }

static void take(const slot_t *s)                /* a piece's settings into play, the globals kept */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        if (!global_par(p))
            proj.par[p] = s->par[p];
    proj.pool = s->pool;
}

static void keep(void)                           /* the piece playing, into its slot */
{
    int p;
    for (p = 0; p < P_NPARAMS; p++)
        proj.slot[proj.piece].par[p] = proj.par[p];
    proj.slot[proj.piece].pool = proj.pool;
}

void project_defaults(void)
{
    int i;
    memset(&proj, 0, sizeof proj);
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    for (i = 0; i < NPIECES; i++)
        recipe(i, &proj.slot[i]);
    for (i = 0; i < P_NPARAMS; i++)
        proj.par[i] = en_param_info(i)->def;
    take(&proj.slot[0]);
    proj.leds = 1;
}

static int pool_ok(uint32_t p)                   /* the root and at least two more notes */
{
    int n = 0, i;
    for (i = 0; i < 12; i++)
        n += (int)(p >> i & 1u);
    return (p & 1u) && !(p >> 12) && n >= 3;
}

static int pars_ok(const int16_t *par)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++) {
        const en_param_t *p = en_param_info(i);
        if (par[i] < p->lo || par[i] > p->hi)
            return 0;
    }
    return 1;
}

static int project_valid(void)                  /* an older format: the recipes again */
{
    int i;
    if (proj.magic != PROJ_MAGIC || !pool_ok(proj.pool) || !pars_ok(proj.par))
        return 0;
    if (proj.format != PROJ_FORMAT || proj.piece >= NPIECES)
        return 0;
    for (i = 0; i < NPIECES; i++)
        if (!pool_ok(proj.slot[i].pool) || !pars_ok(proj.slot[i].par))
            return 0;
    return 1;
}

/* format 3 (0.9 before the layer pages): 23 settings a piece. Carried over: each piece's own, the
 * new ones from its recipe */
#define V3_NPARAMS (P_PADTONE + 1)
typedef struct {
    int16_t par[V3_NPARAMS];
    uint16_t pool;
} slot_v3_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V3_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v3_t slot[NPIECES];
    uint8_t rsv[16];
} project_v3_t;

static void from_v3(const project_v3_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V3_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V3_NPARAMS ? o->par[p] : r.par[p];
}

/* format 4 (before the slate): carried over the same way */
#define V4_NPARAMS P_SLOT1
typedef struct {
    int16_t par[V4_NPARAMS];
    uint16_t pool;
} slot_v4_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V4_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v4_t slot[NPIECES];
    uint8_t rsv[16];
} project_v4_t;

static void from_v4(const project_v4_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V4_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V4_NPARAMS ? o->par[p] : r.par[p];
}

/* format 5 (the slate, before the arp): carried over the same way */
#define V5_NPARAMS P_ARP
typedef struct {
    int16_t par[V5_NPARAMS];
    uint16_t pool;
} slot_v5_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V5_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v5_t slot[NPIECES];
    uint8_t rsv[16];
} project_v5_t;

static void from_v5(const project_v5_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V5_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V5_NPARAMS ? o->par[p] : r.par[p];
}

/* formats 7 and 6 (before Sparkle; 6 had no shapes): carried over the same way */
#define V7_NPARAMS P_SPARK
typedef struct {
    int16_t par[V7_NPARAMS];
    uint16_t pool;
} slot_v7_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V7_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v7_t slot[NPIECES];
    uint8_t rsv[16];
} project_v7_t;

static void from_v7(const project_v7_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V7_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V7_NPARAMS ? o->par[p] : r.par[p];
}

/* format 8 (before the root rotator): carried over the same way */
#define V8_NPARAMS P_RSLOT1
typedef struct {
    int16_t par[V8_NPARAMS];
    uint16_t pool;
} slot_v8_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V8_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v8_t slot[NPIECES];
    uint8_t rsv[16];
} project_v8_t;

static void from_v8(const project_v8_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V8_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V8_NPARAMS ? o->par[p] : r.par[p];
}

/* format 9 (before the bells' colour): carried over the same way */
#define V9_NPARAMS P_BCOLOR
typedef struct {
    int16_t par[V9_NPARAMS];
    uint16_t pool;
} slot_v9_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V9_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v9_t slot[NPIECES];
    uint8_t rsv[16];
} project_v9_t;

static void from_v9(const project_v9_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V9_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V9_NPARAMS ? o->par[p] : r.par[p];
}

/* format 10 (before Swoosh): carried over the same way */
#define V10_NPARAMS P_SWOOSH
typedef struct {
    int16_t par[V10_NPARAMS];
    uint16_t pool;
} slot_v10_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V10_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v10_t slot[NPIECES];
    uint8_t rsv[16];
} project_v10_t;

static void from_v10(const project_v10_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V10_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V10_NPARAMS ? o->par[p] : r.par[p];
}

/* format 13 (before Dream): carried over the same way, Dream Off */
#define V13_NPARAMS P_DREAM
typedef struct {
    int16_t par[V13_NPARAMS];
    uint16_t pool;
} slot_v13_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V13_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v13_t slot[NPIECES];
    uint8_t rsv[16];
} project_v13_t;

static void from_v13(const project_v13_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V13_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V13_NPARAMS ? o->par[p] : r.par[p];
}

/* format 12 (before the dealers): carried over the same way, each dealer Off */
#define V12_NPARAMS P_DEAL
typedef struct {
    int16_t par[V12_NPARAMS];
    uint16_t pool;
} slot_v12_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V12_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v12_t slot[NPIECES];
    uint8_t rsv[16];
} project_v12_t;

static void from_v12(const project_v12_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V12_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V12_NPARAMS ? o->par[p] : r.par[p];
}

/* format 11 (before the Seed): carried over the same way, each piece Wild */
#define V11_NPARAMS P_SEED
typedef struct {
    int16_t par[V11_NPARAMS];
    uint16_t pool;
} slot_v11_t;
typedef struct {
    uint32_t magic, format;
    int16_t par[V11_NPARAMS];
    uint16_t pool;
    uint8_t leds, piece, rsv0[2];
    slot_v11_t slot[NPIECES];
    uint8_t rsv[16];
} project_v11_t;

static void from_v11(const project_v11_t *o)
{
    slot_t r;
    int i, p;
    proj.magic = PROJ_MAGIC;
    proj.format = PROJ_FORMAT;
    proj.pool = o->pool;
    proj.leds = o->leds;
    proj.piece = o->piece < NPIECES ? o->piece : 0;
    for (i = 0; i < NPIECES; i++) {
        recipe(i, &r);
        for (p = 0; p < P_NPARAMS; p++)
            proj.slot[i].par[p] = p < V11_NPARAMS ? o->slot[i].par[p] : r.par[p];
        proj.slot[i].pool = o->slot[i].pool;
    }
    recipe(proj.piece, &r);
    for (p = 0; p < P_NPARAMS; p++)
        proj.par[p] = p < V11_NPARAMS ? o->par[p] : r.par[p];
}

/* before format 7, a slot held only its degree: now the seventh on that degree, the chord it always was */
static void slots_up(void)
{
    int i, s;
    for (s = P_SLOT1; s < P_SLOT1 + EN_NSLOT; s++) {
        if (proj.par[s])
            proj.par[s] = (int16_t)((proj.par[s] - 1) * EN_NSHAPE + 1);
        for (i = 0; i < NPIECES; i++)
            if (proj.slot[i].par[s])
                proj.slot[i].par[s] = (int16_t)((proj.slot[i].par[s] - 1) * EN_NSHAPE + 1);
    }
}

int project_load(void)
{
    static union {
        project_t now;
        project_v3_t v3;
        project_v4_t v4;
        project_v5_t v5;
        project_v7_t v7;
        project_v8_t v8;
        project_v9_t v9;
        project_v10_t v10;
        project_v11_t v11;
        project_v12_t v12;
        project_v13_t v13;
    } buf;
    int n;
    memset(&buf, 0, sizeof buf);
    n = plat_store_load(OBJ_PROJ, &buf, sizeof buf);
    if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 3u)
        from_v3(&buf.v3);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 4u)
        from_v4(&buf.v4);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 5u)
        from_v5(&buf.v5), slots_up();
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 6u)
        from_v7(&buf.v7), slots_up();
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 7u)
        from_v7(&buf.v7);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 8u)
        from_v8(&buf.v8);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 9u)
        from_v9(&buf.v9);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 10u)
        from_v10(&buf.v10);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 11u)
        from_v11(&buf.v11);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 12u)
        from_v12(&buf.v12);
    else if (n >= 8 && buf.now.magic == PROJ_MAGIC && buf.now.format == 13u)
        from_v13(&buf.v13);
    else
        proj = buf.now;
    if (n >= 8 && project_valid())
        return 0;
    project_defaults();
    return -1;
}

int project_save(void)
{
    keep();
    return plat_store_save(OBJ_PROJ, &proj, sizeof proj);
}

void project_piece(int i)
{
    keep();
    proj.piece = (uint8_t)((i % NPIECES + NPIECES) % NPIECES);
    take(&proj.slot[proj.piece]);
}

void project_piece_reset(void)
{
    slot_t s;
    recipe(proj.piece, &s);
    take(&s);
}

void project_apply(void)
{
    int i;
    for (i = 0; i < P_NPARAMS; i++)
        en_set(i, proj.par[i]);
    en_pool(proj.pool);
}

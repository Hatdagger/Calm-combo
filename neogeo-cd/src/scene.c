/*
 * Calm Combo - scenery (Neo Geo CD).  See scene.h.
 *
 * Every scene is a stack of "layers", back to front.  A layer is a run of
 * 16 px wide sprite columns cut from one picture (a PNG in assets/gfx):
 *   - the first column carries the position and height, the rest are
 *     chained to it (sticky bit), so moving a layer is one write;
 *   - animation strips show frame f by pointing the columns at frame f's
 *     tiles (all frames are in sprite memory from boot, nothing loads);
 *   - scrolling layers wrap around their own width;
 *   - the rain is one tall layer that slides down and wraps every 64 px.
 * Tile and position writes go through DATlib's SC234 list with a per-frame
 * budget, so building a scene during the fade-out never overflows it.
 *
 * Day and night are palette maths: every scene colour is multiplied by a
 * tint that moves through dawn, day, sunset and night, then by the fade
 * level.  Light sources (sun, moon, fireflies, glitter) only fade.  In the
 * city layer, the lit-window colour #586070 glows warm at night.
 *
 * Strict C89: gcc 2.95, no // comments, declarations at the top of blocks.
 */
#include <neocore.h>
#include <DATlib.h>
#include "externs.h"
#include "scene.h"

#define SPR_SCENE   1             /* sprites 1..120 belong to the scenery */
#define SPR_SCENE_N 120
#define PAL_SCENE   64            /* palettes 64..159 */
#define MAX_PALS    96
#define MAX_LAYERS  32
#define MAX_REG     24
#define WRITE_BUDGET 500          /* SC234 writes per frame for scenery */
#define FADE_MAX    32

/* y position word that also works for y < 0 (it wraps like the hardware) */
#define S_POSY(y, h) ((WORD)((((YSHIFT - (y)) & 0x1ff) << 7) | (h)))

enum { M_TINT, M_GLOW, M_LIGHTS };

typedef struct {
    const pictureInfo *pi;
    WORD  spr, pal;
    BYTE  ncols, fw, h, srcw;     /* columns shown, columns per frame, tiles tall, wrap width */
    BYTE  frame, shown;
    short x, y, scroll;
    /* what the hardware has now */
    BYTE  d_ok, d_frame, d_shown;
    short d_col, d_x, d_y;
} Layer;

typedef struct { const paletteInfo *p; BYTE first, mode; } PalReg;

static Layer  lay[MAX_LAYERS];
static BYTE   nlay;
static WORD   next_spr;
static PalReg reg[MAX_REG];
static BYTE   nreg, npals;
static WORD   palbuf[MAX_PALS * 16];
static int    budget;

static BYTE   cur = 0xff, phase, level;   /* phase: 0 steady, 1 fade out, 2 fade in */
static DWORD  acc_mid, acc_near;
static DWORD  rng = 0x2545f491UL;

/* layer indices in the current scene */
static BYTE   L_sun, L_moon, L_mid, L_near, L_water, L_fly0, L_glit, L_shore, L_palm, L_umb,
              L_city, L_rain, L_drop0, L_steam;
#define N_FLIES 6
#define N_DROPS 6
static short  drop_y[N_DROPS];

static const signed char SIN32[32] = {
    0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
    0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25
};

static WORD rnd(WORD n)
{
    rng = rng * 1103515245UL + 12345UL;
    return (WORD)(((rng >> 16) & 0x7fff) % n);
}

/* ---------- palettes ---------- */

static WORD c5r(WORD w) { return (WORD)(((w >> 7) & 0x1e) | ((w >> 14) & 1)); }
static WORD c5g(WORD w) { return (WORD)(((w >> 3) & 0x1e) | ((w >> 13) & 1)); }
static WORD c5b(WORD w) { return (WORD)(((w << 1) & 0x1e) | ((w >> 12) & 1)); }
static WORD mk(WORD r, WORD g, WORD b)
{
    return (WORD)(((r & 1) << 14) | ((g & 1) << 13) | ((b & 1) << 12) |
                  ((r >> 1) << 8) | ((g >> 1) << 4) | (b >> 1));
}

/* the palette slot for a picture's palettes, shared by every layer using it */
static WORD pal_for(const paletteInfo *p, BYTE mode)
{
    int k;
    for (k = 0; k < nreg; k++)
        if (reg[k].p == p) return (WORD)(PAL_SCENE + reg[k].first);
    if (nreg >= MAX_REG || npals + p->count > MAX_PALS) return PAL_SCENE;   /* out of room: share */
    reg[nreg].p = p; reg[nreg].first = npals; reg[nreg].mode = mode;
    nreg++;
    npals = (BYTE)(npals + p->count);
    return (WORD)(PAL_SCENE + reg[nreg - 1].first);
}

/*
 * Time of day.  u runs 0..1023 over the 10-minute day: the sun is up for
 * u < 512 (rising at 0, setting at 512), the moon for the second half.
 */
#define NKEY 7
static const short KEY_U[NKEY] = { 0, 80, 400, 480, 560, 940, 1024 };
static const short KEY_C[NKEY][3] = {
    { 220, 180, 200 },   /* dawn   */
    { 256, 256, 256 },   /* day    */
    { 256, 256, 256 },
    { 256, 176, 150 },   /* sunset */
    {  88, 100, 176 },   /* night  */
    {  88, 100, 176 },
    { 220, 180, 200 }    /* dawn   */
};

static WORD day_u(DWORD frames) { return (WORD)(((frames % DAY_FRAMES) * 1024L) / DAY_FRAMES); }

/* 0 by day, 256 at night: lights and fireflies */
static WORD night_of(WORD u)
{
    if (u < 470) return 0;
    if (u < 560) return (WORD)(((u - 470) * 256L) / 90);
    if (u < 940) return 256;
    if (u < 1010) return (WORD)(((1010 - u) * 256L) / 70);
    return 0;
}

static BYTE TR[32], TG[32], TB[32], FD[32];

static void make_tables(WORD u)
{
    int k, c;
    long m[3];
    for (k = 0; k < NKEY - 1 && u >= KEY_U[k + 1]; k++) ;
    for (c = 0; c < 3; c++) {
        long a = KEY_C[k][c], b = KEY_C[k + 1][c], span = KEY_U[k + 1] - KEY_U[k];
        m[c] = a + ((b - a) * (long)(u - KEY_U[k])) / span;
    }
    for (c = 0; c < 32; c++) {
        TR[c] = (BYTE)((c * m[0] * level) >> 13);   /* c * m/256 * level/32 */
        TG[c] = (BYTE)((c * m[1] * level) >> 13);
        TB[c] = (BYTE)((c * m[2] * level) >> 13);
        FD[c] = (BYTE)((c * level) >> 5);
        if (TR[c] > 31) TR[c] = 31;
        if (TG[c] > 31) TG[c] = 31;
        if (TB[c] > 31) TB[c] = 31;
    }
}

static const BYTE SKY5[NUM_SCENES][3] = {
    { 0x6f >> 3, 0x6a >> 3, 0xa3 >> 3 },   /* lake (hidden by the sky art) */
    { 0x4f >> 3, 0x97 >> 3, 0xc4 >> 3 },   /* beach                        */
    { 0x5c >> 3, 0x61 >> 3, 0x70 >> 3 }    /* rain: the sky behind the city */
};

/*
 * Recolour the scene palettes.  During a fade every palette changes every
 * frame; otherwise the tint moves slowly, so one picture's palettes are
 * redone per frame in turn, which keeps the CPU cost per frame small.
 */
static BYTE rr;

static void update_palettes(WORD u, BYTE all)
{
    static WORD tu = 0xffff;
    static BYTE tl = 0xff;
    int k, p, e, k0, k1;
    WORD w, r, g, b, n = night_of(u), *out;
    const WORD *src;
    if (u != tu || level != tl) { make_tables(u); tu = u; tl = level; }
    if (!nreg) return;
    if (all) { k0 = 0; k1 = nreg; }
    else { k0 = rr % nreg; k1 = k0 + 1; rr++; }
    for (k = k0; k < k1; k++) {
        for (p = 0; p < reg[k].p->count; p++) {
            src = (const WORD *)reg[k].p->data + p * 16;
            out = palbuf + (reg[k].first + p) * 16;
            out[0] = src[0];
            for (e = 1; e < 16; e++) {
                w = src[e];
                r = c5r(w); g = c5g(w); b = c5b(w);
                if (reg[k].mode == M_GLOW) {
                    out[e] = mk(FD[r], FD[g], FD[b]);
                } else if (reg[k].mode == M_LIGHTS && r == 11 && g == 12 && b == 14) {
                    /* a lit window: grey by day, warm glow at night */
                    out[e] = mk((WORD)((TR[r] * (256 - n) + FD[31] * n) >> 8),
                                (WORD)((TG[g] * (256 - n) + FD[26] * n) >> 8),
                                (WORD)((TB[b] * (256 - n) + FD[17] * n) >> 8));
                } else {
                    out[e] = mk(TR[r], TG[g], TB[b]);
                }
            }
        }
    }
    /* palJobPut is a braced macro: keep the braces here */
    if (all) {
        palJobPut(PAL_SCENE, npals, palbuf);
    } else {
        palJobPut(PAL_SCENE + reg[k0].first, reg[k0].p->count, palbuf + reg[k0].first * 16);
    }
    k = cur < NUM_SCENES ? cur : 0;
    backgroundColor(mk(TR[SKY5[k][0]], TG[SKY5[k][1]], TB[SKY5[k][2]]));
}

/* ---------- layers ---------- */

static BYTE layer_new(const pictureInfo *pi, BYTE mode, short x, short y,
                      BYTE ncols, BYTE fw, BYTE h, BYTE srcw)
{
    Layer *l;
    int k;
    if (nlay >= MAX_LAYERS || next_spr + ncols > SPR_SCENE + SPR_SCENE_N) return 0xff;
    l = &lay[nlay];
    l->pi = pi; l->spr = next_spr; l->pal = pal_for(pi->palInfo, mode);
    l->ncols = ncols; l->fw = fw; l->h = h; l->srcw = srcw;
    l->frame = 0; l->shown = 1;
    l->x = x; l->y = y; l->scroll = 0;
    l->d_ok = 0; l->d_shown = 0;
    for (k = 1; k < ncols; k++) SC234Put(VRAM_POSY_ADDR(next_spr + k), SPR_LINK);
    budget -= ncols;
    next_spr = (WORD)(next_spr + ncols);
    return nlay++;
}

static void layer_draw(Layer *l)
{
    int k, t, sc, col0, th, need;
    WORD a, pal;
    const ushort *col, *m;
    short x;

    if (!l->shown) {
        if (l->d_shown) { SC234Put(VRAM_POSY_ADDR(l->spr), S_POSY(224, 0)); l->d_shown = 0; budget--; }
        return;
    }
    col0 = l->srcw ? (l->scroll >> 4) % l->srcw : 0;
    if (!l->d_ok || l->frame != l->d_frame || col0 != l->d_col) {
        need = l->ncols * l->h * 2;
        /* wait for room; a layer bigger than a whole frame's budget goes alone */
        if (need > budget && budget < WRITE_BUDGET) return;
        th = l->pi->tileHeight;
        pal = (WORD)(l->pal << 8);
        for (k = 0; k < l->ncols; k++) {
            sc = l->frame * l->fw + (l->srcw ? (col0 + k) % l->srcw : k);
            col = l->pi->maps[0] + sc * th * 2;
            a = (WORD)VRAM_SPR_ADDR(l->spr + k);
            for (t = 0; t < l->h; t++) {
                m = col + (t % th) * 2;
                SC234Put(a, m[0]); a++;
                SC234Put(a, (WORD)(m[1] + pal)); a++;
            }
        }
        budget -= need;
        l->d_ok = 1; l->d_frame = l->frame; l->d_col = (short)col0;
    }
    x = (short)(l->x - (l->srcw ? (l->scroll & 15) : 0));
    if (!l->d_shown || l->y != l->d_y) { SC234Put(VRAM_POSY_ADDR(l->spr), S_POSY(l->y, l->h)); budget--; }
    if (!l->d_shown || x != l->d_x) { SC234Put(VRAM_POSX_ADDR(l->spr), (WORD)((x & 0x1ff) << 7)); budget--; }
    l->d_shown = 1; l->d_y = l->y; l->d_x = x;
}

static int all_drawn(void)
{
    int k;
    for (k = 0; k < nlay; k++)
        if (lay[k].shown && !lay[k].d_shown) return 0;
    return 1;
}

/* ---------- scenes ---------- */

static void build(BYTE s)
{
    int k;
    for (k = 0; k < SPR_SCENE_N; k++) SC234Put(VRAM_POSY_ADDR(SPR_SCENE + k), S_POSY(224, 0));
    budget -= SPR_SCENE_N;
    nlay = 0; nreg = 0; npals = 0; next_spr = SPR_SCENE;
    L_sun = L_moon = L_mid = L_near = L_water = L_fly0 = L_glit = L_shore = L_palm = L_umb =
        L_city = L_rain = L_drop0 = L_steam = 0xff;
    cur = s;

    switch (s) {
    case SCENE_LAKE:
        layer_new(&cc_lake_sky, M_TINT, 0, 0, 20, 20, 10, 0);
        L_sun  = layer_new(&cc_sun,  M_GLOW, 0, 0, 2, 2, 2, 0);
        L_moon = layer_new(&cc_moon, M_GLOW, 0, 0, 2, 2, 2, 0);
        L_mid  = layer_new(&cc_ridge_mid,  M_TINT, 0, 88, 21, 32, 4, 32);
        L_near = layer_new(&cc_ridge_near, M_TINT, 0, 104, 21, 32, 3, 32);
        L_water = layer_new(&cc_water, M_TINT, 0, 160, 20, 20, 4, 0);
        L_fly0 = layer_new(&cc_firefly, M_GLOW, 0, 0, 1, 1, 1, 0);
        for (k = 1; k < N_FLIES; k++) layer_new(&cc_firefly, M_GLOW, 0, 0, 1, 1, 1, 0);
        break;
    case SCENE_BEACH:
        layer_new(&cc_sky_sea, M_TINT, 0, 0, 20, 20, 12, 0);
        L_sun  = layer_new(&cc_sun,  M_GLOW, 0, 0, 2, 2, 2, 0);
        L_moon = layer_new(&cc_moon, M_GLOW, 0, 0, 2, 2, 2, 0);
        L_glit = layer_new(&cc_glitter, M_GLOW, 0, 120, 4, 4, 1, 0);
        L_shore = layer_new(&cc_shore, M_TINT, 0, 176, 20, 20, 3, 0);
        L_palm = layer_new(&cc_palm, M_TINT, 240, 32, 5, 5, 12, 0);
        L_umb  = layer_new(&cc_umbrella, M_TINT, 0, 0, 10, 10, 3, 0);
        break;
    default:
        L_sun  = layer_new(&cc_sun,  M_GLOW, 0, 0, 2, 2, 2, 0);
        L_moon = layer_new(&cc_moon, M_GLOW, 0, 0, 2, 2, 2, 0);
        L_city = layer_new(&cc_city, M_LIGHTS, 0, 64, 20, 20, 8, 0);
        L_rain = layer_new(&cc_rain, M_TINT, 0, -64, 20, 4, 18, 4);   /* 288 px tall, slides down */
        L_drop0 = layer_new(&cc_droplet, M_TINT, 0, 0, 1, 1, 2, 0);
        for (k = 1; k < N_DROPS; k++) layer_new(&cc_droplet, M_TINT, 0, 0, 1, 1, 2, 0);
        for (k = 0; k < N_DROPS; k++) drop_y[k] = (short)(8 + rnd(180));
        layer_new(&cc_frame, M_TINT, 0, 0, 20, 20, 14, 0);
        L_steam = layer_new(&cc_steam, M_TINT, 20, 154, 1, 1, 2, 0);
        break;
    }
}

/* the sun or moon along its arc; t runs 0..511 from rising to setting */
static void arc(Layer *l, WORD t, short ytop, short yhor)
{
    long d = (long)t - 256;
    l->x = (short)(-16 + ((long)t * 336L) / 512);
    l->y = (short)(ytop + ((d * d) * (yhor - ytop)) / 65536L);
}

static void animate(DWORD frames, WORD u)
{
    static const BYTE PALM[6] = { 0, 1, 2, 3, 2, 1 };
    static const short FX[N_FLIES] = { 40, 92, 150, 204, 252, 292 };
    static const short FY[N_FLIES] = { 140, 172, 152, 186, 136, 166 };
    static const short DX[N_DROPS] = { 52, 104, 190, 232, 140, 276 };
    WORD n = night_of(u), t = (WORD)(frames >> 3);
    short ytop = 14, yhor = 112;
    Layer *sun = L_sun != 0xff ? &lay[L_sun] : 0, *moon = L_moon != 0xff ? &lay[L_moon] : 0;
    int k;

    if (cur == SCENE_BEACH) { ytop = 12; yhor = 88; }   /* sets onto the horizon */
    if (sun && moon) {
        sun->shown = (BYTE)(u < 512);
        moon->shown = (BYTE)(u >= 512);
        if (u < 512) arc(sun, u, ytop, yhor); else arc(moon, (WORD)(u - 512), ytop, yhor);
    }

    switch (cur) {
    case SCENE_LAKE:
        acc_mid += 10; acc_near += 26;              /* 1 and 2.6 pixels per second */
        lay[L_mid].scroll  = (short)((acc_mid / 600) & 511);
        lay[L_near].scroll = (short)((acc_near / 600) & 511);
        lay[L_water].frame = (BYTE)((frames / 12) & 3);
        for (k = 0; k < N_FLIES; k++) {
            Layer *f = &lay[L_fly0 + k];
            f->shown = (BYTE)(n > 200);
            f->x = (short)(FX[k] + ((SIN32[(t + k * 9) & 31] * 12) >> 7));
            f->y = (short)(FY[k] + ((SIN32[(t * 2 + k * 5 + 8) & 31] * 6) >> 7));
            f->frame = (BYTE)(((frames >> 3) + k) & 3);
        }
        break;
    case SCENE_BEACH:
        lay[L_shore].frame = (BYTE)(((frames % 540) * 8) / 540);   /* one wave every 9 s */
        lay[L_palm].frame = PALM[(frames / 20) % 6];
        lay[L_umb].frame = (BYTE)((frames / 7) & 3);
        lay[L_glit].frame = (BYTE)((frames / 6) & 3);
        lay[L_glit].x = (short)((u < 512 ? sun->x : moon->x) - 16);
        lay[L_glit].shown = (BYTE)((u < 512 ? sun->y : moon->y) < yhor - 6);
        break;
    case SCENE_WINDOW:
        /* the rain block tiles seamlessly, so falling rain is just the layer
           sliding down 3 px a frame and jumping back up every 64 px */
        lay[L_rain].y = (short)(((frames * 3) & 63) - 64);
        lay[L_steam].frame = (BYTE)((frames >> 3) & 3);
        for (k = 0; k < N_DROPS; k++) {
            Layer *d = &lay[L_drop0 + k];
            if ((frames % (3 + k % 3)) == 0) drop_y[k]++;
            if (drop_y[k] > 196) { drop_y[k] = 4; d->x = (short)(16 + rnd(288)); }
            else if (!d->x) d->x = DX[k];
            d->y = drop_y[k];
            d->frame = (BYTE)(((frames >> 4) + k) & 3);
        }
        break;
    }
}

/* ---------- public ---------- */

void scene_boot(void)
{
    int k;
    for (k = 0; k < SPR_SCENE_N; k++) {
        SC234Put(VRAM_SHRINK_ADDR(SPR_SCENE + k), 0xfff);
        SC234Put(VRAM_POSY_ADDR(SPR_SCENE + k), S_POSY(224, 0));
    }
    cur = 0xff;
    phase = 2;
    level = 0;
}

BYTE scene_shown(void) { return (BYTE)(cur < NUM_SCENES ? cur : 0); }

void scene_tick(DWORD frames)
{
    BYTE want = scene_of(frames);
    WORD u = day_u(frames);
    int k;

    budget = WRITE_BUDGET;
    if (cur == 0xff) build(want);

    if (phase == 0 && want != cur) phase = 1;
    if (phase == 1) {
        if (level > 0) level = (BYTE)(level > 2 ? level - 2 : 0);
        else { build(want); phase = 2; }
    } else if (phase == 2 && all_drawn()) {
        level = (BYTE)(level + 2 < FADE_MAX ? level + 2 : FADE_MAX);
        if (level == FADE_MAX) phase = 0;
    }

    animate(frames, u);
    /* layers not drawn yet go first, so a big one gets a whole frame's budget */
    for (k = 0; k < nlay; k++) if (!lay[k].d_ok) layer_draw(&lay[k]);
    for (k = 0; k < nlay; k++) layer_draw(&lay[k]);
    update_palettes(u, (BYTE)(phase != 0));
}

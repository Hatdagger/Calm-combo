/*
 * Calm Combo - the three scenes in portrait (lake, beach, rain window).
 *
 * Same art, timing and day/night maths as neogeo-cd/src/scene.c, laid out
 * for a tall screen: the 224 px scene band sits at the bottom, the sky is
 * carried up to the top, and things at the sides (umbrella, palm, window
 * frame) are pulled in so they stay on screen.
 */
#include "scenery.h"
#include "gfx.h"

#define FADE_STEP 16              /* 0..256 in 16 frames, like the disc's 2/32 per frame */

static int   cur = -1, phase = 2, level = 0;   /* phase: 0 steady, 1 fade out, 2 fade in */
static unsigned rng = 0x2545f491u;
#define N_FLIES 6
#define N_DROPS 6
static int drop_x[N_DROPS], drop_y[N_DROPS];

static const signed char SIN32[32] = {
    0, 25, 49, 71, 90, 106, 117, 125, 127, 125, 117, 106, 90, 71, 49, 25,
    0, -25, -49, -71, -90, -106, -117, -125, -127, -125, -117, -106, -90, -71, -49, -25
};
static int sin256(int p)
{
    int a = SIN32[(p >> 3) & 31], b = SIN32[((p >> 3) + 1) & 31];
    return a + (((b - a) * (p & 7)) >> 3);
}
static int rnd(int n) { rng = rng * 1103515245u + 12345u; return (int)(((rng >> 16) & 0x7fff) % (unsigned)n); }

/* ---------- time of day (scene.c) ---------- */

#define NKEY 7
static const int KEY_U[NKEY] = { 0, 80, 400, 480, 560, 940, 1024 };
static const int KEY_C[NKEY][3] = {
    { 220, 180, 200 }, { 256, 256, 256 }, { 256, 256, 256 }, { 256, 176, 150 },
    { 88, 100, 176 }, { 88, 100, 176 }, { 220, 180, 200 }
};

static int day_u(long frames) { return (int)(((frames % DAY_FRAMES) * 1024L) / DAY_FRAMES); }

static int night_of(int u)
{
    if (u < 470) return 0;
    if (u < 560) return ((u - 470) * 256) / 90;
    if (u < 940) return 256;
    if (u < 1010) return ((1010 - u) * 256) / 70;
    return 0;
}

static void make_tint(int u)
{
    int k, c, m[3];
    for (k = 0; k < NKEY - 1 && u >= KEY_U[k + 1]; k++) ;
    for (c = 0; c < 3; c++) {
        int a = KEY_C[k][c], b = KEY_C[k + 1][c], span = KEY_U[k + 1] - KEY_U[k];
        m[c] = a + ((b - a) * (u - KEY_U[k])) / span;
    }
    for (c = 0; c < 256; c++) {
        int r = (c * m[0]) >> 8, g = (c * m[1]) >> 8, b = (c * m[2]) >> 8;
        tintR[c] = (uint8_t)(r > 255 ? 255 : r);
        tintG[c] = (uint8_t)(g > 255 ? 255 : g);
        tintB[c] = (uint8_t)(b > 255 ? 255 : b);
    }
}

/* ---------- helpers ---------- */

static char nbuf[48];
static const Img *night_of_img(const char *name)
{
    int k = 0;
    while (name[k] && k < 40) { nbuf[k] = name[k]; k++; }
    nbuf[k] = 0;
    {
        const char *suf = "_night";
        int j = 0;
        while (suf[j]) nbuf[k++] = suf[j++];
        nbuf[k] = 0;
    }
    return img(nbuf);
}

/* one frame (fw wide) of a picture strip, coloured for the time of day */
static void layer(const char *name, int frame, int fw, int x, int y)
{
    const Img *im = img(name), *nt = night_of_img(name);
    if (!im) return;
    if (fw <= 0) fw = im->w;
    blit(im, nt, frame * fw, 0, fw, im->h, x, y, B_SCENE);
}

/* a picture repeated across the screen, starting scroll px in */
static void layer_wrap(const char *name, int fw, int scroll, int y)
{
    const Img *im = img(name), *nt = night_of_img(name);
    int x;
    if (!im) return;
    if (fw <= 0) fw = im->w;
    scroll %= fw;
    for (x = -scroll; x < FW; x += fw) blit(im, nt, 0, 0, fw, im->h, x, y, B_SCENE);
}

static void glow(const char *name, int frame, int fw, int x, int y)
{
    const Img *im = img(name);
    if (!im) return;
    if (fw <= 0) fw = im->w;
    blit(im, 0, frame * fw, 0, fw, im->h, x, y, B_GLOW);
}

/* copy row y up to the top of the screen: the sky goes on */
static void extend_up(int y)
{
    int j;
    if (y <= 0 || y >= FH) return;
    for (j = 0; j < y; j++) {
        uint32_t *d = fb + (size_t)j * FSTRIDE, *s = fb + (size_t)y * FSTRIDE;
        int i;
        for (i = 0; i < FW; i++) d[i] = s[i];
    }
}

/* sun or moon on its arc across the screen; t 0..511 from rising to setting */
static void arc(int t, int ytop, int yhor, int *x, int *y)
{
    long d = (long)t - 256;
    *x = -16 + (int)(((long)t * (FW + 16)) / 512);
    *y = ytop + (int)(((d * d) * (yhor - ytop)) / 65536L);
}

static int sun_x, sun_y;
static void sun_moon(int u, int ytop, int yhor)
{
    if (u < 512) { arc(u, ytop, yhor, &sun_x, &sun_y); glow("common/sun", 0, 0, sun_x, sun_y); }
    else         { arc(u - 512, ytop, yhor, &sun_x, &sun_y); glow("common/moon", 0, 0, sun_x, sun_y); }
}

/* ---------- scenes ---------- */

static void lake(long frames, int u)
{
    static const int FX[N_FLIES] = { 40, 92, 150, 204, 252, 292 };
    static const int FY[N_FLIES] = { 140, 172, 152, 186, 136, 166 };
    int yb = FH - 224, cam = (320 - FW) / 2, k, t = (int)(frames >> 3);
    layer("lake/sky", 0, 0, -cam, yb);
    extend_up(yb);
    sun_moon(u, 14, yb + 112);
    /* 1 and 2.6 pixels per second */
    layer_wrap("lake/ridge_mid", 0, (int)((frames / 60) & 511) + cam, yb + 88);
    layer_wrap("lake/ridge_near", 0, (int)((frames * 26 / 600) & 511) + cam, yb + 104);
    layer("lake/water", (int)((frames / 6) & 15), 320, -cam, yb + 160);
    if (night_n > 200)
        for (k = 0; k < N_FLIES; k++)
            glow("lake/firefly", (int)(((frames / 5) + k * 3) & 7), 16,
                 FX[k] - cam + ((SIN32[(t + k * 9) & 31] * 12) >> 7),
                 yb + FY[k] + ((SIN32[(t * 2 + k * 5 + 8) & 31] * 6) >> 7));
}

static void beach(long frames, int u)
{
    int yb = FH - 224, cam = (320 - FW) / 2, yhor = yb + 88;
    layer("beach/sky_sea", 0, 0, -cam, yb);
    extend_up(yb);
    sun_moon(u, 12, yhor);
    if (sun_y < yhor - 6) glow("beach/glitter", (int)((frames >> 2) & 7), 64, sun_x - 16, yb + 120);
    layer("beach/shore", 0, 0, -cam, yb + 176);
    layer("beach/wave", (int)((frames >> 3) & 3), 320, -cam,
          yb + 176 + ((127 + sin256((int)(((frames % 540) * 256L) / 540))) * 14) / 254);
    layer("beach/palm", (int)((frames >> 3) & 15), 80, FW - 70, yb + 32);
    layer("beach/umbrella", (int)((frames >> 2) & 7), 160, -56, 0);
}

/* the window frame, cut so both panes, the bars and the sill fit any size */
static void window_frame(void)
{
    const Img *im = img("window/frame"), *nt = night_of_img("window/frame");
    int pw = (FW - 24) / 2, pw2 = FW - 24 - pw, top = 60, bot = 224 - top, y;
    int xs[5], sx[5], w[5], k;
    if (!im) return;
    /* left edge, left pane, mullion, right pane, right edge */
    xs[0] = 0;               sx[0] = 0;          w[0] = 9;
    xs[1] = 9;               sx[1] = 9;          w[1] = pw;
    xs[2] = 9 + pw;          sx[2] = 157;        w[2] = 6;
    xs[3] = 15 + pw;         sx[3] = 311 - pw2;  w[3] = pw2;
    xs[4] = FW - 9;          sx[4] = 311;        w[4] = 9;
    for (k = 0; k < 5; k++) {
        blit(im, nt, sx[k], 0, w[k], top, xs[k], 0, B_SCENE);
        for (y = top; y < FH - bot; y++) blit(im, nt, sx[k], top, w[k], 1, xs[k], y, B_SCENE);
        blit(im, nt, sx[k], top, w[k], bot, xs[k], FH - bot, B_SCENE);
    }
}

static void window_scene(long frames, int u)
{
    static const int DX[N_DROPS] = { 22, 70, 120, 150, 40, 100 };
    int yb = FH - 224, cam = (320 - FW) / 2, k, y;
    fill(0, 0, FW, FH, scene_px(PX(0x5c, 0x61, 0x70), 0, 0));
    sun_moon(u, 14, yb + 112);
    layer("window/city", 0, 0, -cam, yb + 64);
    for (y = (int)((frames * 3) & 63) - 64; y < FH; y += 64) layer_wrap("window/rain", 64, 0, y);
    for (k = 0; k < N_DROPS; k++) {
        if (!drop_x[k] || drop_x[k] > FW - 16) drop_x[k] = DX[k] * FW / 180;
        layer("window/droplet", 0, 0, drop_x[k], drop_y[k] % (FH - 28));
    }
    window_frame();
    layer("window/steam", (int)((frames / 6) & 7), 16, 20, FH - 70);
}

/* ---------- public ---------- */

int scenery_shown(void) { return cur < 0 ? 0 : cur; }

void scenery_tick(long frames, int h)
{
    int want = (int)((frames / SCENE_FRAMES) % NUM_SCENES), k;
    if (cur < 0) { cur = want; for (k = 0; k < N_DROPS; k++) drop_y[k] = 8 + rnd(200); }
    for (k = 0; k < N_DROPS; k++) {
        if ((frames % (3 + k % 3)) == 0) drop_y[k]++;
        if (drop_y[k] > h - 28) { drop_y[k] = 4; drop_x[k] = 12 + rnd(140); }
    }
    if (phase == 0 && want != cur) phase = 1;
    if (phase == 1) {
        if (level > 0) level = level > FADE_STEP ? level - FADE_STEP : 0;
        else { cur = want; phase = 2; }
    } else if (phase == 2) {
        level = level + FADE_STEP < 256 ? level + FADE_STEP : 256;
        if (level == 256) phase = 0;
    }
}

void scenery_draw(long frames)
{
    int u = day_u(frames);
    if (cur < 0) scenery_tick(frames, FH);
    make_tint(u);
    night_n = night_of(u);
    fade_lv = level;
    switch (cur) {
    case SCENE_LAKE:  lake(frames, u); break;
    case SCENE_BEACH: beach(frames, u); break;
    default:          window_scene(frames, u); break;
    }
    fade_lv = 256;
}

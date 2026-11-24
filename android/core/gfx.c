/*
 * Calm Combo - pictures, the pixel buffer and text.
 *
 * The pak (android/tools/make_pak.py) holds every PNG from
 * neogeo-cd/src/assets/gfx as raw pixels, magenta already made see-through,
 * so the Neo Geo CD art is used as it is.
 *
 * Pak layout (little endian): "CCPK", u32 count, then per picture
 * u8 name length, name, pad to 4, u16 w, u16 h, w*h pixels (R, G, B, A).
 */
#include <string.h>
#include "gfx.h"

#define MAX_IMG 64

typedef struct { char name[40]; Img im; } Entry;
static Entry ent[MAX_IMG];
static int   nent;

uint32_t *fb;
int FW, FH, FSTRIDE;

uint8_t tintR[256], tintG[256], tintB[256];
int     night_n;
int     fade_lv = 256;

static uint32_t rd32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
static int      rd16(const unsigned char *p) { return p[0] | (p[1] << 8); }

int gfx_load_pak(const void *pak, long len)
{
    const unsigned char *p = (const unsigned char *)pak, *end = p + len;
    uint32_t n, k;
    if (len < 8 || memcmp(p, "CCPK", 4) != 0) return -1;
    n = rd32(p + 4);
    p += 8;
    nent = 0;
    for (k = 0; k < n && nent < MAX_IMG; k++) {
        int nl, w, h;
        size_t off;
        if (p >= end) return -1;
        nl = p[0];
        if (nl >= (int)sizeof ent[0].name || p + 1 + nl > end) return -1;
        memcpy(ent[nent].name, p + 1, (size_t)nl);
        ent[nent].name[nl] = 0;
        off = (size_t)(p - (const unsigned char *)pak) + 1 + (size_t)nl;
        off = (off + 3) & ~(size_t)3;
        p = (const unsigned char *)pak + off;
        if (p + 4 > end) return -1;
        w = rd16(p); h = rd16(p + 2);
        p += 4;
        if (p + (size_t)w * h * 4 > end) return -1;
        ent[nent].im.w = w; ent[nent].im.h = h;
        ent[nent].im.px = (const uint32_t *)(const void *)p;   /* pak data is 4-byte aligned */
        p += (size_t)w * h * 4;
        nent++;
    }
    return 0;
}

const Img *img(const char *name)
{
    int k;
    for (k = 0; k < nent; k++)
        if (strcmp(ent[k].name, name) == 0) return &ent[k].im;
    return 0;
}

/* ---------- colour ---------- */

static uint32_t bright(uint32_t p)
{
    int r = PX_R(p), g = PX_G(p), b = PX_B(p);
    r += ((255 - r) * 3) >> 3; g += ((255 - g) * 3) >> 3; b += ((255 - b) * 3) >> 3;
    return PX(r, g, b);
}

uint32_t scene_px(uint32_t day, uint32_t night, int glow)
{
    int r = PX_R(day), g = PX_G(day), b = PX_B(day), n = night_n, m = 256 - night_n;
    if (!glow) {
        if (night && n) {
            /* painted night art: tinted day colour -> night colour */
            r = (tintR[r] * m + (int)PX_R(night) * n) >> 8;
            g = (tintG[g] * m + (int)PX_G(night) * n) >> 8;
            b = (tintB[b] * m + (int)PX_B(night) * n) >> 8;
        } else {
            r = tintR[r]; g = tintG[g]; b = tintB[b];
        }
    }
    if (fade_lv < 256) { r = (r * fade_lv) >> 8; g = (g * fade_lv) >> 8; b = (b * fade_lv) >> 8; }
    return PX(r, g, b);
}

/* ---------- drawing ---------- */

void blit(const Img *im, const Img *night, int sx, int sy, int sw, int sh, int dx, int dy, int mode)
{
    int x, y, x0 = 0, y0 = 0, x1 = sw, y1 = sh;
    if (!im) return;
    if (night && (night->w != im->w || night->h != im->h)) night = 0;
    if (sx < 0) { x0 = -sx; }
    if (sy < 0) { y0 = -sy; }
    if (sx + x1 > im->w) x1 = im->w - sx;
    if (sy + y1 > im->h) y1 = im->h - sy;
    if (dx + x0 < 0) x0 = -dx;
    if (dy + y0 < 0) y0 = -dy;
    if (dx + x1 > FW) x1 = FW - dx;
    if (dy + y1 > FH) y1 = FH - dy;
    for (y = y0; y < y1; y++) {
        const uint32_t *s = im->px + (size_t)(sy + y) * im->w + sx;
        const uint32_t *ns = night ? night->px + (size_t)(sy + y) * night->w + sx : 0;
        uint32_t *d = fb + (size_t)(dy + y) * FSTRIDE + dx;
        for (x = x0; x < x1; x++) {
            uint32_t p = s[x];
            if (!PX_A(p)) continue;
            switch (mode) {
            case B_BRIGHT: p = bright(p); break;
            case B_SCENE:  p = scene_px(p, ns ? ns[x] : 0, 0); break;
            case B_GLOW:   p = scene_px(p, 0, 1); break;
            default: break;
            }
            d[x] = p;
        }
    }
}

void blit_shade(const Img *im, int sx, int sy, int sw, int sh, int dx, int dy, int shade)
{
    int x, y;
    if (!im) return;
    for (y = 0; y < sh; y++) {
        int yy = dy + y;
        if (yy < 0 || yy >= FH || sy + y >= im->h) continue;
        for (x = 0; x < sw; x++) {
            int xx = dx + x;
            uint32_t p;
            if (xx < 0 || xx >= FW || sx + x >= im->w) continue;
            p = im->px[(size_t)(sy + y) * im->w + sx + x];
            if (!PX_A(p)) continue;
            fb[(size_t)yy * FSTRIDE + xx] = PX((PX_R(p) * shade) >> 8, (PX_G(p) * shade) >> 8, (PX_B(p) * shade) >> 8);
        }
    }
}

void fill(int x, int y, int w, int h, uint32_t c)
{
    int i, j;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > FW) w = FW - x;
    if (y + h > FH) h = FH - y;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) fb[(size_t)(y + j) * FSTRIDE + x + i] = c;
}

void fill_alpha(int x, int y, int w, int h, uint32_t c, int a)
{
    int i, j, m = 256 - a, cr = PX_R(c) * a, cg = PX_G(c) * a, cb = PX_B(c) * a;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > FW) w = FW - x;
    if (y + h > FH) h = FH - y;
    for (j = 0; j < h; j++)
        for (i = 0; i < w; i++) {
            uint32_t *d = fb + (size_t)(y + j) * FSTRIDE + x + i, p = *d;
            *d = PX((PX_R(p) * m + cr) >> 8, (PX_G(p) * m + cg) >> 8, (PX_B(p) * m + cb) >> 8);
        }
}

/* ---------- text ---------- */

/* the Neo Geo CD's text colours (main.c FIXCOL) */
static const uint8_t TCOL[T_NPAL][3] = {
    { 0xff, 0x8f, 0x87 }, { 0x7c, 0xc4, 0xff }, { 0x7f, 0xe0, 0xa8 },
    { 0xff, 0xe0, 0x8a }, { 0xc6, 0xa8, 0xff }, { 0xff, 0xb6, 0x7c },
    { 0x85, 0x7c, 0x95 },
    { 0xf3, 0xec, 0xff }, { 0xa9, 0x9b, 0xc4 },
    { 0xff, 0xe7, 0xa8 }, { 0xa8, 0xec, 0xff },
    { 0x9f, 0xf0, 0xc0 }, { 0xff, 0xb0, 0xb8 }, { 0xff, 0xc2, 0xe8 }
};

int clip_x0 = 0, clip_x1 = 1 << 30;

/* the font is drawn in three greys: main = colour, shade = 2/3, outline = 1/5 */
void text(int x, int y, int pal, const char *s)
{
    static const Img *font;
    const uint8_t *c = TCOL[pal < 0 || pal >= T_NPAL ? T_TEXT : pal];
    uint32_t main = PX(c[0], c[1], c[2]), shade = PX(c[0] * 2 / 3, c[1] * 2 / 3, c[2] * 2 / 3),
             line = PX(c[0] / 5, c[1] / 5, c[2] / 5);
    if (!font) font = img("font/font");
    if (!font) return;
    for (; *s; s++, x += 8) {
        int ch = (unsigned char)*s, gx = (ch & 15) * 8, gy = (ch >> 4) * 8, i, j;
        if (ch == ' ') continue;
        for (j = 0; j < 8; j++) {
            int yy = y + j;
            if (yy < 0 || yy >= FH) continue;
            for (i = 0; i < 8; i++) {
                int xx = x + i;
                uint32_t p;
                if (xx < 0 || xx >= FW || xx < clip_x0 || xx >= clip_x1) continue;
                p = font->px[(size_t)(gy + j) * font->w + gx + i];
                if (!PX_A(p)) continue;
                fb[(size_t)yy * FSTRIDE + xx] = PX_R(p) > 200 ? main : PX_R(p) > 100 ? shade : line;
            }
        }
    }
}

int  text_w(const char *s) { return (int)strlen(s) * 8; }
void text_c(int cx, int y, int pal, const char *s) { text(cx - text_w(s) / 2, y, pal, s); }
void text_r(int rx, int y, int pal, const char *s) { text(rx - text_w(s), y, pal, s); }

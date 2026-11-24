/* Calm Combo - pictures, the pixel buffer and text (see gfx.c). */
#ifndef CALM_GFX_H
#define CALM_GFX_H

#include <stddef.h>
#include <stdint.h>

/* pixel in memory order R, G, B, A */
#define PX(r, g, b) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | 0xff000000u)
#define PX_R(p) ((p) & 0xff)
#define PX_G(p) (((p) >> 8) & 0xff)
#define PX_B(p) (((p) >> 16) & 0xff)
#define PX_A(p) ((p) >> 24)

typedef struct {
    int w, h;
    const uint32_t *px;
} Img;

/* the frame buffer being drawn this frame */
extern uint32_t *fb;
extern int FW, FH, FSTRIDE;

int  gfx_load_pak(const void *pak, long len);
const Img *img(const char *name);         /* "board/tiles", "lake/sky_night"; NULL if missing */

/* how a picture is coloured on its way to the screen */
enum { B_PLAIN, B_BRIGHT, B_SCENE, B_GLOW, B_SHADE };

/* scene colouring, set once per frame by the scenery */
extern uint8_t tintR[256], tintG[256], tintB[256];   /* time-of-day tint */
extern int     night_n;                              /* 0 day .. 256 night */
extern int     fade_lv;                              /* 0 black .. 256 full */

/*
 * Copy the sw x sh piece at (sx, sy) of a picture to (dx, dy).  Pixels with
 * alpha 0 are see-through.  For B_SCENE, night is the same picture painted
 * for night (or NULL).  B_SHADE darkens to `shade`/256.
 */
void blit(const Img *im, const Img *night, int sx, int sy, int sw, int sh, int dx, int dy, int mode);
void blit_shade(const Img *im, int sx, int sy, int sw, int sh, int dx, int dy, int shade);

void fill(int x, int y, int w, int h, uint32_t c);
void fill_alpha(int x, int y, int w, int h, uint32_t c, int a);   /* a 0..256 */
uint32_t scene_px(uint32_t day, uint32_t night, int glow);        /* one colour through tint/fade */

/* text: 8x8 font, colour by palette index */
enum { T_TILE0, T_STONE = 6, T_TEXT, T_DIM, T_P1, T_P2, T_GOOD, T_BAD, T_LEVER, T_NPAL };
void text(int x, int y, int pal, const char *s);
void text_c(int cx, int y, int pal, const char *s);   /* centred on cx */
void text_r(int rx, int y, int pal, const char *s);   /* right edge at rx */
int  text_w(const char *s);
extern int clip_x0, clip_x1;   /* text is only drawn between these columns */

#endif

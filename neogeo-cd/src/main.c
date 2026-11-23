/*
 * Calm Combo - Neo Geo CD
 *
 * Milestone 4: sound.  Each scene has its own song on the disc (CD tracks
 * 2-4) and the board plays the 16 sound effects on the sound chip (sound.c).
 *
 * Milestone 2: the board is drawn with sprites.
 *   - Every tile is its own 16x16 sprite, so it can glide when a line is
 *     slid, fall with gravity and play its pop frames from tiles.png.
 *   - The cursor (cursor.png) and a soft panel behind each board (panel.png)
 *     are sprites too.
 *   - The HUD stays on the fix (text) layer, now in the custom font.png,
 *     recoloured per message by swapping its three colours.
 *
 * Strict C89: gcc 2.95, no // comments, declarations at the top of blocks.
 */
#include <neocore.h>
#include <DATlib.h>
#include <input.h>
#include "externs.h"
#include "board.h"
#include "scene.h"
#include "sound.h"

/*
 * Soft plate behind each board.  Off: the scenery needs those sprites (each
 * panel adds 9 to every board line, and the window scene would go over the
 * hardware limit of 96 per line).  The tiles carry their own dark plate.
 */
#define USE_PANEL 0

/* ---------- screen layout ---------- */
/* fix map: 40 columns, visible rows 2..29, so text row n is at pixel y n*8 */
#define ROW(n)       ((n) + 2)
#define BOARD_SROW   9          /* pixel y 72, like the web prototype */
#define HUD_SCORE    3          /* HUD rows are spaced out: the font fills */
#define HUD_MOVES    5          /* its whole 8x8 cell, so adjacent rows touch */
#define HUD_STONES   7
#define MSG_SROW     24
#define STATUS_SROW  26

#define TILE   16
#define BY0    72               /* board top, pixels */
#define NCELL  (CC_ROWS * CC_COLS)
static const short BX0[2] = { 24, 168 };   /* board left, pixels */

/* fix palettes (NeoCore reserves 0 and 1 for the system) */
#define PAL_FIRST  2
#define PAL_TILE0  2            /* 2..7 tile colours (used for text accents) */
#define PAL_STONE  8
#define PAL_TEXT   9
#define PAL_DIM    10
#define PAL_P1     11
#define PAL_P2     12
#define PAL_GOOD   13
#define PAL_BAD    14
#define PAL_LEVER  15
#define NUM_FIXPAL 14
#define FONT_BANK  3            /* font.png lives in fix bank 3 */

/* sprite palettes: 16 = logo, then the board art from 32 upwards */
#define SPR_PAL_LOGO 16
#define SPR_PAL_BASE 32

/* sprite numbers: higher numbers are drawn on top */
#define SPR_LOGO     340        /* 10 sprites, over the scenery */
#define SPR_PANEL    200        /* 2 x 9 sprites */
#define SPR_TILE     220        /* 2 x 56 sprites, one per tile */
#define SPR_HELD     332        /* 2: the lifted tile, above its neighbours */
#define SPR_CURSOR   334        /* 2 */
#define SPR_END      336
#define SPR_N        (SPR_END - SPR_TILE)

/* timings in frames (60 fps) */
#define F_PRE      7
#define F_POP      16
#define F_SHUFFLE  27
#define F_LOCK     54
#define F_MSG      70
#define F_FLASH    10
#define F_BUFFER   20             /* an A press while the board is busy is kept this long */
#define FALL_MAX   9            /* fall speed cap, pixels per frame */

enum { ST_JOIN, ST_IDLE, ST_PRE, ST_POP, ST_FALL, ST_SHUFFLE, ST_OVER, ST_RESULT };
enum { MODE_SCORE, MODE_VS };

/* one on-screen tile; the board maps each cell to one of these */
typedef struct {
    short x, y;                   /* pixel position, glides to its cell */
    BYTE  vy;                     /* fall speed */
    BYTE  col;                    /* colour shown (0..5, 6 = stone) */
    BYTE  flash;                  /* frames left of a bright flash */
} Obj;

typedef struct {
    cc_board b;
    BYTE  i, col0, state;
    BYTE  cr, cc;                 /* cursor */
    BYTE  held, hr, hc, axis, has_t, t;
    cc_matches pv;                /* preview of the held move */
    cc_step step;
    WORD  timer, idle_t, stone_t, lock_t;
    char  msg[17];
    WORD  msg_t;
    BYTE  msg_pal;
    BYTE  win;                    /* versus result */
    cc_grid oid;                  /* which Obj sits in each cell */
    Obj   o[NCELL];
    short cux, cuy;               /* cursor sprite position */
    BYTE  moving;                 /* something was still gliding last frame */
    BYTE  buf_a;                  /* passes left on an A press made while busy */
} Side;

static Side   side[2];
static BYTE   mode = MODE_SCORE, in_title = 1, menu_sel = 0, music_on = 1;
static BYTE   track_on = 0;       /* CD track started last (0: none yet)  */
/* the clock starts 50 s into the day: mid-morning at the lake */
static DWORD  best = 0, frames = 3000, seed_mix = 0x1234567UL;
static picture logo;
#if USE_PANEL
static picture panel[2];
#endif

/* ---------- colours ---------- */

/* 5-bit channels -> Neo Geo colour word (dark bit 0) */
static WORD ng5(WORD r, WORD g, WORD b)
{
    return (WORD)(((r & 1) << 14) | ((g & 1) << 13) | ((b & 1) << 12) |
                  ((r >> 1) << 8) | ((g >> 1) << 4) | (b >> 1));
}


static WORD ch_r(WORD w) { return (WORD)(((w >> 7) & 0x1e) | ((w >> 14) & 1)); }
static WORD ch_g(WORD w) { return (WORD)(((w >> 3) & 0x1e) | ((w >> 13) & 1)); }
static WORD ch_b(WORD w) { return (WORD)(((w << 1) & 0x1e) | ((w >> 12) & 1)); }

/* a colour pushed 3/8 of the way to white, for highlights */
static WORD brighten(WORD w)
{
    WORD r = ch_r(w), g = ch_g(w), b = ch_b(w);
    r = (WORD)(r + ((31 - r) * 3) / 8);
    g = (WORD)(g + ((31 - g) * 3) / 8);
    b = (WORD)(b + ((31 - b) * 3) / 8);
    return ng5(r, g, b);
}

static const BYTE FIXCOL[NUM_FIXPAL][3] = {
    { 0xff, 0x8f, 0x87 }, { 0x7c, 0xc4, 0xff }, { 0x7f, 0xe0, 0xa8 },   /* tiles */
    { 0xff, 0xe0, 0x8a }, { 0xc6, 0xa8, 0xff }, { 0xff, 0xb6, 0x7c },
    { 0x85, 0x7c, 0x95 },                                               /* stone */
    { 0xf3, 0xec, 0xff }, { 0xa9, 0x9b, 0xc4 },                         /* text, dim */
    { 0xff, 0xe7, 0xa8 }, { 0xa8, 0xec, 0xff },                         /* 1P, 2P */
    { 0x9f, 0xf0, 0xc0 }, { 0xff, 0xb0, 0xb8 }, { 0xff, 0xc2, 0xe8 }    /* good, bad, lever */
};
static WORD fixpal[NUM_FIXPAL * 16];

/*
 * The font is drawn in three greys (main #F8F8F8, shade #A8A8A8, outline
 * #282828).  Whatever palette slots the converter gave them, sort each slot
 * by brightness and swap in the tint: main = tint, shade = 2/3 tint,
 * outline = a dark tint.
 */
static void load_fix_palettes(void)
{
    int p, k;
    WORD w, sum, r, g, b;
    for (p = 0; p < NUM_FIXPAL; p++) {
        r = (WORD)(FIXCOL[p][0] >> 3); g = (WORD)(FIXCOL[p][1] >> 3); b = (WORD)(FIXCOL[p][2] >> 3);
        fixpal[p * 16] = 0;
        for (k = 1; k < 16; k++) {
            w = cc_font_Palettes.data[k];
            sum = (WORD)(ch_r(w) + ch_g(w) + ch_b(w));     /* 0..93 */
            if (sum > 78)      fixpal[p * 16 + k] = ng5(r, g, b);
            else if (sum > 39) fixpal[p * 16 + k] = ng5((WORD)(r * 2 / 3), (WORD)(g * 2 / 3), (WORD)(b * 2 / 3));
            else               fixpal[p * 16 + k] = ng5((WORD)(r / 5), (WORD)(g / 5), (WORD)(b / 5));
        }
    }
    palJobPut(PAL_FIRST, NUM_FIXPAL, fixpal);
}

/* board art palettes: tiles, bright tiles, cursor, panel */
static WORD pal_tiles, pal_hi, pal_cursor, pal_panel;
static WORD hipal[16 * 16];

static void load_sprite_palettes(void)
{
    int k, n = cc_tiles_Palettes.count;
    if (n > 16) n = 16;
    pal_tiles  = SPR_PAL_BASE;
    pal_hi     = (WORD)(pal_tiles + n);
    pal_cursor = (WORD)(pal_hi + n);
    pal_panel  = (WORD)(pal_cursor + cc_cursor_Palettes.count);
    for (k = 0; k < n * 16; k++)
        hipal[k] = (k & 15) ? brighten(cc_tiles_Palettes.data[k]) : cc_tiles_Palettes.data[k];
    palJobPut(pal_tiles, n, cc_tiles_Palettes.data);
    palJobPut(pal_hi, n, hipal);
    palJobPut(pal_cursor, cc_cursor_Palettes.count, cc_cursor_Palettes.data);
#if USE_PANEL
    palJobPut(pal_panel, cc_panel_Palettes.count, cc_panel_Palettes.data);
#endif
    palJobPut(SPR_PAL_LOGO, cc_logo_Palettes.count, cc_logo_Palettes.data);
}

/*
 * The fix and board palettes are uploaded once at boot, but on the first
 * hardware test they came out wrong (the scenery, re-sent as it tints, was
 * fine).  So they are re-sent every 16 frames: cheap (20 palettes) and safe
 * whatever overwrote or dropped them.
 */
static void resend_static_palettes(void)
{
    int n = cc_tiles_Palettes.count;
    if (n > 16) n = 16;
    palJobPut(PAL_FIRST, NUM_FIXPAL, fixpal);
    palJobPut(pal_tiles, n, cc_tiles_Palettes.data);
    palJobPut(pal_hi, n, hipal);
    palJobPut(pal_cursor, cc_cursor_Palettes.count, cc_cursor_Palettes.data);
    palJobPut(SPR_PAL_LOGO, cc_logo_Palettes.count, cc_logo_Palettes.data);
}

static const char *SCENE_NAME[3] = { "LAKESIDE ", "SHORELINE", "RAIN     " };

/* track 2 = lake, 3 = beach, 4 = rain window */
static void play_scene_song(void)
{
    if (!music_on) return;
    track_on = (BYTE)(2 + scene_of(frames));
    music_play(track_on);
}

static void next_scene(void)
{
    frames = (frames / SCENE_FRAMES + 1) * SCENE_FRAMES + 1;
    play_scene_song();
}

static void toggle_music(void)
{
    music_on = (BYTE)!music_on;
    if (!music_on) music_pause();
    else if (track_on == 2 + scene_of(frames)) music_resume();
    else play_scene_song();               /* the scene changed while it was off */
}

/* ---------- sprites ---------- */

/*
 * Single-tile sprites written straight into the sprite control blocks with
 * DATlib's SC234 list (sent during vblank).  The last value written to each
 * sprite is cached, so only changes go out each frame.
 */
static WORD c_x[SPR_N], c_y[SPR_N], c_t[SPR_N], c_a[SPR_N];

static void spr_init(void)
{
    int k;
    for (k = 0; k < SPR_N; k++) {
        c_x[k] = c_t[k] = c_a[k] = 0xffff;
        c_y[k] = 0;
        SC234Put(VRAM_SHRINK_ADDR(SPR_TILE + k), 0xfff);
        SC234Put(VRAM_POSY_ADDR(SPR_TILE + k), VRAM_POSY(224, SPR_UNLINK, 0));
    }
}

static void spr_hide(WORD s)
{
    int k = s - SPR_TILE;
    if (c_y[k]) {
        SC234Put(VRAM_POSY_ADDR(s), VRAM_POSY(224, SPR_UNLINK, 0));
        c_y[k] = 0;
    }
}

/* show sprite s at (x, y) with one tile of a picture (frame column fc, row fr) */
static void spr_show(WORD s, short x, short y, const pictureInfo *pi, int fc, int fr, WORD pal)
{
    int k = s - SPR_TILE;
    const ushort *m = pi->maps[0] + (fc * pi->tileHeight + fr) * 2;
    WORD v;
    v = (WORD)VRAM_POSY(y, SPR_UNLINK, 1);
    if (c_y[k] != v) { SC234Put(VRAM_POSY_ADDR(s), v); c_y[k] = v; }
    v = (WORD)VRAM_POSX(x & 0x1ff);
    if (c_x[k] != v) { SC234Put(VRAM_POSX_ADDR(s), v); c_x[k] = v; }
    v = m[0];
    if (c_t[k] != v) { SC234Put(VRAM_SPR_ADDR(s), v); c_t[k] = v; }
    v = (WORD)(m[1] + (pal << 8));
    if (c_a[k] != v) { SC234Put(VRAM_SPR_ADDR(s) + 1, v); c_a[k] = v; }
}

/* glide halfway there each frame (always at least 1 pixel) */
static short ease(short v, short t)
{
    short d = (short)(t - v);
    if (d > 0) return (short)(v + (d + 1) / 2);
    if (d < 0) return (short)(v - (1 - d) / 2);
    return v;
}

/* ---------- text helpers ---------- */

static void put(int x, int srow, int pal, const char *s)
{
    fixPrint((ushort)x, (ushort)ROW(srow), (ushort)pal, FONT_BANK, s);
}

/* right-aligned, zero-padded unsigned number */
static void put_num(int x, int srow, int pal, DWORD v, int width)
{
    char buf[12];
    int k;
    buf[width] = 0;
    for (k = width - 1; k >= 0; k--) { buf[k] = (char)('0' + v % 10); v /= 10; }
    put(x, srow, pal, buf);
}

static void str_copy(char *d, const char *s, int max)
{
    int k = 0;
    while (s[k] && k < max - 1) { d[k] = s[k]; k++; }
    d[k] = 0;
}

static void say(Side *s, const char *text, BYTE pal)
{
    str_copy(s->msg, text, sizeof s->msg);
    s->msg_t = F_MSG;
    s->msg_pal = pal;
}

/* "+120 x2" style */
static void say_points(Side *s, WORD pts, BYTE chain)
{
    char buf[17];
    int n = 0, k;
    char digits[6];
    int nd = 0;
    buf[n++] = '+';
    do { digits[nd++] = (char)('0' + pts % 10); pts /= 10; } while (pts && nd < 5);
    for (k = nd - 1; k >= 0; k--) buf[n++] = digits[k];
    if (chain > 1) { buf[n++] = ' '; buf[n++] = 'x'; buf[n++] = (char)('0' + (chain > 9 ? 9 : chain)); }
    buf[n] = 0;
    say(s, buf, PAL_P1 + s->i);
}

/* ---------- sound ---------- */

/* 1P on the left and 2P on the right while both play; one player alone is centred */
static void sfx(Side *s, BYTE id)
{
    BYTE pan = SND_CENTRE;
    if (side[0].state != ST_JOIN && side[1].state != ST_JOIN) pan = s->i ? SND_RIGHT : SND_LEFT;
    snd_play(id, pan);
}

/* ---------- board sprites ---------- */

static void clear_board_area(Side *s)
{
    int r;
    for (r = 0; r < CC_ROWS * 2; r++) put(s->col0, BOARD_SROW + r, PAL_TEXT, "                ");
}

/* fresh tiles: one Obj per cell, dropping in from above the board */
static void objs_reset(Side *s)
{
    int r, c, k;
    Obj *o;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            k = r * CC_COLS + c;
            s->oid.c[r][c] = (cc_u8)k;
            o = &s->o[k];
            o->x = (short)(BX0[s->i] + c * TILE);
            o->y = (short)(BY0 + r * TILE - (CC_ROWS + 1) * TILE - (CC_COLS - 1 - c) * 6);
            o->vy = 0;
            o->col = s->b.g.c[r][c];
            o->flash = 0;
        }
    s->cux = (short)(BX0[s->i] + s->cc * TILE);
    s->cuy = (short)(BY0 + s->cr * TILE);
}

/*
 * Before cc_gravity(): survivors in each column keep their Obj and slide
 * down; the cleared Objs are reused for the new tiles, stacked above the top.
 */
static void objs_fall(Side *s)
{
    int r, c, n, f, w, k;
    cc_u8 keep[CC_ROWS], freed[CC_ROWS];
    Obj *o;
    for (c = 0; c < CC_COLS; c++) {
        n = f = 0;
        for (r = CC_ROWS - 1; r >= 0; r--) {
            if (s->step.dying[r][c]) freed[f++] = s->oid.c[r][c];
            else keep[n++] = s->oid.c[r][c];
        }
        w = CC_ROWS - 1;
        for (k = 0; k < n; k++) s->oid.c[w--][c] = keep[k];
        for (k = 0; k < f; k++) {
            s->oid.c[k][c] = freed[k];
            o = &s->o[freed[k]];
            o->x = (short)(BX0[s->i] + c * TILE);
            o->y = (short)(BY0 + (k - f) * TILE);
            o->vy = 0;
            o->flash = 0;
        }
    }
}

/* after cc_gravity(): new tiles take their colours without a flash */
static void objs_sync(Side *s)
{
    int r, c;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            s->o[s->oid.c[r][c]].col = s->b.g.c[r][c];
}

static void hide_side(Side *s)
{
    int k;
    for (k = 0; k < NCELL; k++) spr_hide((WORD)(SPR_TILE + s->i * NCELL + k));
    spr_hide((WORD)(SPR_HELD + s->i));
    spr_hide((WORD)(SPR_CURSOR + s->i));
}

/* move every tile one frame towards its cell and send the sprites */
static void render_side(Side *s)
{
    cc_grid ids, cols;
    int r, c, hr = -1, hc = -1, popf = -1, blink, fr, e, moving = 0, held_obj = -1;
    short tx, ty, x0 = BX0[s->i];
    WORD pal, spr;
    Obj *o;

    if (in_title || s->state == ST_JOIN) { hide_side(s); s->moving = 0; return; }

    ids = s->oid;
    cols = s->b.g;
    if (s->held) {
        hr = s->hr; hc = s->hc;
        if (s->has_t) {
            cc_apply_move(&ids, s->hr, s->hc, s->axis, s->t);
            cc_apply_move(&cols, s->hr, s->hc, s->axis, s->t);
            if (s->axis == CC_AXIS_H) hc = s->t; else hr = s->t;
        }
    }
    if (s->state == ST_POP) {
        e = F_POP - s->timer;
        popf = e < 4 ? 1 : e < 10 ? 2 : 3;
    }
    blink = (int)((frames >> 3) & 1);

    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            o = &s->o[ids.c[r][c]];
            spr = (WORD)(SPR_TILE + s->i * NCELL + ids.c[r][c]);
            tx = (short)(x0 + c * TILE);
            ty = (short)(BY0 + r * TILE);

            /* a colour changed in place (stones landed, shuffle): flash it */
            if (o->col != cols.c[r][c]) { o->col = cols.c[r][c]; o->flash = F_FLASH; }

            if (o->x != tx) { o->x = ease(o->x, tx); moving = 1; }
            if (o->y < ty && (s->state == ST_FALL || o->vy || ty - o->y > TILE)) {
                if (o->vy < FALL_MAX) o->vy++;
                o->y = (short)(o->y + o->vy);
                if (o->y >= ty) { o->y = ty; o->vy = 0; }
                moving = 1;
            } else if (o->y != ty) {
                o->y = ease(o->y, ty); o->vy = 0; moving = 1;
            }

            if (o->y < BY0 - 8) { spr_hide(spr); continue; }   /* still above the board */

            fr = 0;
            pal = pal_tiles;
            if (popf >= 0 && s->step.dying[r][c]) fr = popf;
            else if (o->flash) { o->flash--; if (o->flash & 2) pal = pal_hi; }
            else if (s->held && s->has_t && s->pv.count && s->pv.hit[r][c] && blink) pal = pal_hi;

            if (r == hr && c == hc) {             /* the lifted tile, drawn on top */
                held_obj = ids.c[r][c];
                spr_hide(spr);
                spr_show((WORD)(SPR_HELD + s->i), o->x, (short)(o->y - 2), &cc_tiles, 0, o->col, pal_hi);
                continue;
            }
            spr_show(spr, o->x, o->y, &cc_tiles, fr, o->col, pal);
        }
    if (held_obj < 0) spr_hide((WORD)(SPR_HELD + s->i));
    s->moving = (BYTE)moving;

    /* cursor: rides on the held tile, or glides between cells */
    if (s->state == ST_IDLE) {
        if (held_obj >= 0) {
            s->cux = s->o[held_obj].x;
            s->cuy = (short)(s->o[held_obj].y - 2);
        } else {
            s->cux = ease(s->cux, (short)(x0 + s->cc * TILE));
            s->cuy = ease(s->cuy, (short)(BY0 + s->cr * TILE));
        }
        spr_show((WORD)(SPR_CURSOR + s->i), s->cux, s->cuy, &cc_cursor,
                 (int)((frames >> 3) & 3), s->i, pal_cursor);
    } else {
        spr_hide((WORD)(SPR_CURSOR + s->i));
    }
}

static void show_panels(BYTE on)
{
#if USE_PANEL
    if (on) { pictureShow(&panel[0]); pictureShow(&panel[1]); }
    else    { pictureHide(&panel[0]); pictureHide(&panel[1]); }
#else
    (void)on;
#endif
}

/* ---------- HUD ---------- */

static void draw_hud(Side *s)
{
    BYTE pcol = (BYTE)(PAL_P1 + s->i);
    int x = s->col0;
    put(x, HUD_SCORE, pcol, s->i ? "2P" : "1P");
    if (s->state == ST_JOIN) {
        put(x + 3, HUD_SCORE, PAL_DIM, "             ");
        put(x, HUD_MOVES, PAL_DIM, "                ");
        put(x, HUD_STONES, PAL_DIM, "                ");
        return;
    }
    put(x + 3, HUD_SCORE, PAL_TEXT, "SCORE");
    put_num(x + 9, HUD_SCORE, PAL_TEXT, (DWORD)s->b.score, 7);
    put(x, HUD_MOVES, PAL_TEXT, "MOVES");
    put_num(x + 6, HUD_MOVES, s->b.moves <= 3 ? PAL_BAD : PAL_GOOD,
            (DWORD)(s->b.moves < 0 ? 0 : s->b.moves), 2);
    if (mode == MODE_VS) {
        /* move timer: 7 pips that empty as time runs out */
        int pips = 7 - s->idle_t / 60, k;
        char bar[8];
        for (k = 0; k < 7; k++) bar[k] = (char)(k < pips ? '=' : ' ');
        bar[7] = 0;
        put(x + 9, HUD_MOVES, pips <= 2 ? PAL_BAD : PAL_DIM, bar);
        put(x, HUD_STONES, PAL_STONE, "STONES");
        put_num(x + 7, HUD_STONES, s->b.pending ? PAL_BAD : PAL_DIM, (DWORD)s->b.pending, 2);
    } else {
        put(x, HUD_STONES, PAL_TEXT, "         ");
        put(x + 9, HUD_MOVES, PAL_TEXT, "       ");
    }
}

static void draw_side(Side *s)
{
    BYTE pcol = (BYTE)(PAL_P1 + s->i);
    draw_hud(s);
    render_side(s);
    if (s->state == ST_JOIN) {
        put(s->col0 + 2, BOARD_SROW + 6, pcol, ((frames >> 5) & 1) ? "PRESS START" : "           ");
        put(s->col0, MSG_SROW, PAL_TEXT, "                ");
        put(s->col0, STATUS_SROW, PAL_TEXT, "                ");
        return;
    }
    put(s->col0, MSG_SROW, PAL_TEXT, "                ");
    if (s->msg_t) put(s->col0, MSG_SROW, s->msg_pal, s->msg);
    if (s->state == ST_OVER) {
        put(s->col0, STATUS_SROW, PAL_BAD, "GAME OVER  START");
    } else if (s->state == ST_RESULT) {
        put(s->col0, STATUS_SROW, s->win ? PAL_GOOD : PAL_BAD,
            s->win ? "WIN!   START=AGN" : "LOSE   START=AGN");
    } else {
        put(s->col0, STATUS_SROW, PAL_TEXT, "                ");
    }
}

/* ---------- game flow ---------- */

static DWORD fresh_seed(void)
{
    seed_mix = seed_mix * 1103515245UL + 12345UL + frames;
    return seed_mix & 0x3FFFFFFFUL;
}

static void start_side(Side *s, DWORD seed)
{
    cc_board_start(&s->b, seed, fresh_seed());
    s->state = ST_IDLE;
    s->held = 0; s->has_t = 0;
    s->cr = 3; s->cc = 3;
    s->idle_t = 0; s->stone_t = 0; s->msg_t = 0; s->lock_t = 0; s->buf_a = 0;
    s->win = 0;
    clear_board_area(s);
    objs_reset(s);
}

static void reset_side(Side *s, BYTE i)
{
    s->i = i;
    s->col0 = (BYTE)(i ? 21 : 3);
    s->state = ST_JOIN;
    s->held = 0;
    s->msg_t = 0;
}

static void rematch(void)
{
    DWORD seed = fresh_seed();
    start_side(&side[0], seed);
    start_side(&side[1], seed);
}

static void draw_title(void)
{
    put(12, 12, menu_sel == 0 ? PAL_P1 : PAL_DIM, menu_sel == 0 ? "> SCORE ATTACK" : "  SCORE ATTACK");
    put(12, 14, menu_sel == 1 ? PAL_P1 : PAL_DIM, menu_sel == 1 ? "> VERSUS      " : "  VERSUS      ");
    put(8, 18, PAL_DIM, "A GRAB/DROP  B CANCEL");
    put(8, 20, PAL_DIM, "C NEXT SCENE D MUSIC");
    put(8, 22, PAL_DIM, "SELECT MENU");
}

static void to_title(void)
{
    in_title = 1;
    reset_side(&side[0], 0);
    reset_side(&side[1], 1);
    clearFixLayer();
    show_panels(0);
    hide_side(&side[0]);
    hide_side(&side[1]);
    pictureShow(&logo);
}

static void start_game(BYTE m, BYTE p)
{
    in_title = 0;
    mode = m;
    pictureHide(&logo);
    clearFixLayer();
    show_panels(1);
    reset_side(&side[0], 0);
    reset_side(&side[1], 1);
    if (m == MODE_VS) rematch();
    else start_side(&side[p], fresh_seed());
}

static void end_side(Side *s)
{
    Side *o = &side[1 - s->i];
    s->held = 0;
    if (mode == MODE_VS) {
        s->state = ST_RESULT; s->win = 0; s->lock_t = F_LOCK;
        o->state = ST_RESULT; o->win = 1; o->lock_t = F_LOCK; o->held = 0;
    } else {
        s->state = ST_OVER; s->lock_t = F_LOCK;
    }
}

static void note_best(Side *s)
{
    if ((DWORD)s->b.score > best) best = (DWORD)s->b.score;
}

/* the cascade, one phase per call when its timer runs out */
static void resolve_tick(Side *s)
{
    int gain, before;
    if (s->timer) { s->timer--; return; }
    switch (s->state) {
    case ST_PRE:
        if (s->moving) return;            /* let the slide finish first */
        if (cc_resolve_step(&s->b, &s->step)) {
            say_points(s, s->step.points, s->step.chain);
            sfx(s, (BYTE)(SND_CLEAR_1 + (s->step.chain > 9 ? 8 : s->step.chain - 1)));
            if (s->step.lever) { say(s, "LEVER!", PAL_LEVER); sfx(s, SND_LEVER); }
            s->state = ST_POP; s->timer = F_POP;
            return;
        }
        before = s->b.pending;
        gain = cc_resolve_end(&s->b, mode == MODE_VS ? &side[1 - s->i].b : (cc_board *)0);
        if (gain) { say(s, gain > 1 ? "+MOVES!" : "+1 MOVE", PAL_GOOD); sfx(s, SND_MOVES); }
        if (mode == MODE_VS && before && !s->b.pending && cc_count_stones(&s->b)) {
            say(s, "STONES!", PAL_STONE);
            sfx(s, SND_STONES);
        }
        note_best(s);
        if (s->b.moves <= 0) { end_side(s); return; }
        if (!cc_has_valid_move(&s->b.g)) {
            say(s, "SHUFFLE!", PAL_TEXT);
            s->state = ST_SHUFFLE; s->timer = F_SHUFFLE;
            return;
        }
        s->state = ST_IDLE;
        return;
    case ST_POP:
        objs_fall(s);
        cc_gravity(&s->b, &s->step);
        objs_sync(s);
        s->state = ST_FALL; s->timer = 2;
        s->moving = 1;
        return;
    case ST_FALL:
        if (s->moving) return;            /* wait until every tile has landed */
        s->state = ST_PRE; s->timer = 0;
        return;
    case ST_SHUFFLE:
        cc_shuffle(&s->b);
        s->state = ST_IDLE;
        return;
    }
}

static void vs_pressure(Side *s)
{
    if (s->state != ST_IDLE) return;
    s->idle_t++;
    /* the last 2.5 s of the move timer tick twice a second */
    if (s->idle_t > CC_VS_IDLE_FRAMES - 150 && s->idle_t % 30 == 0 && s->idle_t < CC_VS_IDLE_FRAMES)
        sfx(s, SND_TICK);
    if (s->idle_t >= CC_VS_IDLE_FRAMES) {
        s->idle_t = 0;
        s->b.moves--;
        say(s, "-1 MOVE", PAL_BAD);
        sfx(s, SND_NO_MATCH);
        if (s->b.moves <= 0) { end_side(s); return; }
    }
    if (s->b.pending > 0) {
        if (++s->stone_t >= CC_STONE_DELAY_FRAMES) {
            s->stone_t = 0;
            cc_place_stones(&s->b, s->b.pending);
            s->b.pending = 0;
            s->held = 0;
            say(s, "STONES!", PAL_STONE);
            sfx(s, SND_STONES);
            if (!cc_has_valid_move(&s->b.g)) cc_shuffle(&s->b);
        }
    } else {
        s->stone_t = 0;
    }
}

/* ---------- input ---------- */

static void held_dir(Side *s, int dr, int dc)
{
    int axis = dc ? CC_AXIS_H : CC_AXIS_V, step = dc ? dc : dr;
    int o = axis == CC_AXIS_H ? s->hc : s->hr;
    int mx = axis == CC_AXIS_H ? CC_COLS - 1 : CC_ROWS - 1, t;
    if (!s->has_t) {
        t = o + step;
        if (t < 0 || t > mx) return;
        s->axis = (BYTE)axis; s->t = (BYTE)t; s->has_t = 1;
    } else {
        if (s->axis != axis) return;
        t = s->t + step;
        if (t < 0 || t > mx) return;
        if (t == o) { s->has_t = 0; return; }
        s->t = (BYTE)t;
    }
    cc_preview(&s->b, s->hr, s->hc, s->axis, s->t, &s->pv);
}

static void drop(Side *s)
{
    int fr, fc;
    if (s->has_t && s->pv.count) {
        fr = s->axis == CC_AXIS_H ? s->hr : s->t;
        fc = s->axis == CC_AXIS_H ? s->t : s->hc;
        cc_commit(&s->b, s->hr, s->hc, s->axis, s->t);
        cc_apply_move(&s->oid, s->hr, s->hc, s->axis, s->t);   /* tiles stay where they are */
        s->cr = (BYTE)fr; s->cc = (BYTE)fc;
        s->held = 0; s->has_t = 0;
        s->idle_t = 0;
        s->state = ST_PRE; s->timer = F_PRE;
    } else {
        if (s->has_t) { say(s, "NO MATCH", PAL_BAD); sfx(s, SND_NO_MATCH); }   /* tiles glide back by themselves */
        s->held = 0; s->has_t = 0;
    }
}

static void side_input(Side *s, BYTE edge, BYTE rep, BYTE start)
{
    int dr = 0, dc = 0;
    if (s->lock_t) s->lock_t--;
    switch (s->state) {
    case ST_JOIN:
        if (start || (edge & JOY_A)) {
            if (mode == MODE_VS) rematch(); else start_side(s, fresh_seed());
            sfx(s, SND_MENU);
        }
        return;
    case ST_OVER:
        if (!s->lock_t && (start || (edge & JOY_A))) { start_side(s, fresh_seed()); sfx(s, SND_MENU); }
        return;
    case ST_RESULT:
        if (!s->lock_t && (start || (edge & JOY_A))) { rematch(); snd_play(SND_MENU, SND_CENTRE); }
        return;
    case ST_IDLE:
        break;
    default:
        /* busy resolving: remember an A press and use it once the board is ready */
        if (edge & JOY_A) s->buf_a = F_BUFFER;
        else if (s->buf_a) s->buf_a--;
        return;
    }
    if (s->buf_a) { s->buf_a = 0; if (!s->held) edge |= JOY_A; }
    if (rep & JOY_UP) dr = -1; else if (rep & JOY_DOWN) dr = 1;
    else if (rep & JOY_LEFT) dc = -1; else if (rep & JOY_RIGHT) dc = 1;
    if (dr || dc) {
        if (s->held) held_dir(s, dr, dc);
        else {
            int r = s->cr + dr, c = s->cc + dc;
            if (r >= 0 && r < CC_ROWS) s->cr = (BYTE)r;
            if (c >= 0 && c < CC_COLS) s->cc = (BYTE)c;
        }
    }
    if (edge & JOY_A) {
        if (!s->held) { s->held = 1; s->hr = s->cr; s->hc = s->cc; s->has_t = 0; sfx(s, SND_GRAB); }
        else drop(s);
    }
    if ((edge & JOY_B) && s->held) { s->held = 0; s->has_t = 0; }
}

/* ---------- joypads ---------- */

/*
 * Presses are worked out from the buttons held now versus last pass, not
 * read from the BIOS's edge bytes: those only last one video frame, so a
 * press landing on a frame the game was too busy to look at was lost.
 * Held directions repeat after 14 passes, then every 5.
 */
#define REP_FIRST 14
#define REP_NEXT  5

typedef struct { BYTE prev, hold; } Pad;
static Pad pad[2];
static BYTE ps_prev;

static void read_pad(Pad *p, BYTE cur, BYTE *edge, BYTE *rep)
{
    BYTE dirs = (BYTE)(cur & (JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT));
    *edge = (BYTE)(cur & ~p->prev);
    *rep = (BYTE)(*edge & dirs);
    if (!dirs || (*edge & dirs)) p->hold = 0;
    else if (++p->hold >= REP_FIRST && (p->hold - REP_FIRST) % REP_NEXT == 0) *rep = dirs;
    if (p->hold > REP_FIRST + REP_NEXT * 40) p->hold = REP_FIRST;   /* keep it bounded */
    p->prev = cur;
}

/* ---------- main ---------- */

int main(void)
{
    BYTE e1, e2, r1, r2, ps, edges, k;

    initGfx();
    clearFixLayer();
    LSPCmode = 0x1c00;
    load_fix_palettes();
    load_sprite_palettes();
    spr_init();
    snd_init();
    scene_boot();
    pictureInit(&logo, &cc_logo, SPR_LOGO, SPR_PAL_LOGO, 80, 40, FLIP_NONE);
#if USE_PANEL
    for (k = 0; k < 2; k++)
        pictureInit(&panel[k], &cc_panel, (ushort)(SPR_PANEL + k * cc_panel.tileWidth), pal_panel,
                    (short)(BX0[k] - 8), (short)(BY0 - 8), FLIP_NONE);
#endif
    to_title();
    play_scene_song();

    for (;;) {
        SCClose();
        waitVBlank();
        frames++;
        if ((frames & 15) == 0) resend_static_palettes();
        scene_tick(frames);
        snd_tick();

        read_pad(&pad[0], volMEMBYTE(P1_CURRENT), &e1, &r1);
        read_pad(&pad[1], volMEMBYTE(P2_CURRENT), &e2, &r2);
        k = volMEMBYTE(PS_CURRENT);
        ps = (BYTE)(k & ~ps_prev);
        ps_prev = k;
        edges = (BYTE)(e1 | e2);
        seed_mix += e1 + (e2 << 8) + ps;

        if (edges & JOY_C) next_scene();
        if (edges & JOY_D) toggle_music();
        if (frames % SCENE_FRAMES == 0) play_scene_song();

        put(1, 0, PAL_DIM, SCENE_NAME[scene_shown()]);
        put(27, 0, PAL_DIM, "BEST");
        put_num(32, 0, PAL_TEXT, best, 7);

        if (in_title) {
            if ((e1 | e2) & (JOY_UP | JOY_DOWN)) { menu_sel = (BYTE)!menu_sel; snd_play(SND_MENU, SND_CENTRE); }
            if ((e1 & JOY_A) || (ps & P1_START)) {
                start_game(menu_sel ? MODE_VS : MODE_SCORE, 0);
                snd_play(SND_MENU, SND_CENTRE);
            } else if ((e2 & JOY_A) || (ps & P2_START)) {
                start_game(menu_sel ? MODE_VS : MODE_SCORE, 1);
                snd_play(SND_MENU, SND_CENTRE);
            }
            else draw_title();
            continue;
        }

        if (ps & (P1_SELECT | P2_SELECT)) { to_title(); continue; }

        side_input(&side[0], e1, r1, (BYTE)(ps & P1_START));
        side_input(&side[1], e2, r2, (BYTE)(ps & P2_START));

        for (k = 0; k < 2; k++) {
            Side *s = &side[k];
            if (s->state >= ST_PRE && s->state <= ST_SHUFFLE) resolve_tick(s);
            if (mode == MODE_VS) vs_pressure(s);
            if (s->msg_t) s->msg_t--;
            draw_side(s);
        }
    }
    return 0;
}

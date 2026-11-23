/*
 * Calm Combo - Neo Geo CD
 *
 * Milestone 1: the whole game, drawn on the fix (text) layer.
 * Each 16x16 tile is a 2x2 block of font characters in its own colour, so
 * the game is playable before the sprite artwork exists.
 *
 * Strict C89: gcc 2.95, no // comments, declarations at the top of blocks.
 */
#include <neocore.h>
#include <DATlib.h>
#include <input.h>
#include "externs.h"
#include "board.h"

#ifdef PLATFORM_CD
#include <cdda.h>
#endif

/* Set to 1 once the three scene songs are on the disc as tracks 2-4. */
#define HAVE_CDDA 0

/* ---------- screen layout (fix map: 40 columns, visible rows 2..29) ---------- */
#define ROW(n)       ((n) + 2)
#define BOARD_SROW   9          /* pixel y 72, like the web prototype */
#define HUD_SROW     4
#define MSG_SROW     24
#define STATUS_SROW  26

/* fix palettes */
#define PAL_TILE0  1            /* 1..6 tile colours */
#define PAL_STONE  7
#define PAL_TEXT   8
#define PAL_DIM    9
#define PAL_P1     10
#define PAL_P2     11
#define PAL_GOOD   12
#define PAL_BAD    13
#define PAL_LEVER  14
#define PAL_HOT    15

#define SPR_PAL_LOGO 16
#define SPR_LOGO     1

/* timings in frames (60 fps) */
#define F_PRE      7
#define F_POP      16
#define F_FALL     18
#define F_SHUFFLE  27
#define F_LOCK     54
#define F_MSG      70
#define SCENE_FRAMES (300L * 60L)
#define DAY_FRAMES   (600L * 60L)

enum { ST_JOIN, ST_IDLE, ST_PRE, ST_POP, ST_FALL, ST_SHUFFLE, ST_OVER, ST_RESULT };
enum { MODE_SCORE, MODE_VS };

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
} Side;

static Side   side[2];
static BYTE   mode = MODE_SCORE, in_title = 1, menu_sel = 0, music_on = 1;
static DWORD  best = 0, frames = 0, seed_mix = 0x1234567UL;
static picture logo;

/* ---------- colours ---------- */

/* 8-bit RGB -> Neo Geo colour word (5 bits per channel + dark bit 0) */
static WORD ng_rgb(WORD r, WORD g, WORD b)
{
    r >>= 3; g >>= 3; b >>= 3;
    return (WORD)(((r & 1) << 14) | ((g & 1) << 13) | ((b & 1) << 12) |
                  ((r >> 1) << 8) | ((g >> 1) << 4) | (b >> 1));
}

static const BYTE FIXCOL[15][3] = {
    { 0xff, 0x8f, 0x87 }, { 0x7c, 0xc4, 0xff }, { 0x7f, 0xe0, 0xa8 },   /* tiles */
    { 0xff, 0xe0, 0x8a }, { 0xc6, 0xa8, 0xff }, { 0xff, 0xb6, 0x7c },
    { 0x85, 0x7c, 0x95 },                                               /* stone */
    { 0xf3, 0xec, 0xff }, { 0xa9, 0x9b, 0xc4 },                         /* text, dim */
    { 0xff, 0xe7, 0xa8 }, { 0xa8, 0xec, 0xff },                         /* 1P, 2P */
    { 0x9f, 0xf0, 0xc0 }, { 0xff, 0xb0, 0xb8 }, { 0xff, 0xc2, 0xe8 },   /* good, bad, lever */
    { 0xff, 0xff, 0xff }                                                /* hot */
};
static WORD fixpal[15 * 16];

static void load_palettes(void)
{
    int p, k;
    WORD c;
    for (p = 0; p < 15; p++) {
        c = ng_rgb(FIXCOL[p][0], FIXCOL[p][1], FIXCOL[p][2]);
        fixpal[p * 16] = 0;
        for (k = 1; k < 16; k++) fixpal[p * 16 + k] = c;
    }
    palJobPut(1, 15, fixpal);
}

/* scene backdrop: day colour drifting to night and back over 10 minutes */
static const BYTE SKY[3][2][3] = {
    { { 0x6f, 0x6a, 0xa3 }, { 0x18, 0x20, 0x4a } },   /* mountain lake */
    { { 0x4f, 0x97, 0xc4 }, { 0x0a, 0x0f, 0x2e } },   /* beach         */
    { { 0x5c, 0x61, 0x70 }, { 0x1c, 0x22, 0x30 } }    /* rain window   */
};
static const char *SCENE_NAME[3] = { "LAKESIDE ", "SHORELINE", "RAIN     " };

static BYTE scene_of(DWORD t) { return (BYTE)((t / SCENE_FRAMES) % 3); }

static void update_backdrop(void)
{
    BYTE s = scene_of(frames);
    DWORD ph = frames % DAY_FRAMES, f, half = DAY_FRAMES / 2;
    WORD ch[3];
    int k;
    f = ph < half ? ph : DAY_FRAMES - ph;          /* 0 = day, half = night */
    f = (f * 256) / half;
    for (k = 0; k < 3; k++)
        ch[k] = (WORD)((SKY[s][0][k] * (256 - f) + SKY[s][1][k] * f) >> 8);
    backgroundColor(ng_rgb(ch[0], ch[1], ch[2]));
}

static void play_scene_song(void)
{
#if HAVE_CDDA && defined(PLATFORM_CD)
    if (music_on) nc_sound_play_cdda((BYTE)(2 + scene_of(frames)));
#endif
}

static void next_scene(void)
{
    frames = (frames / SCENE_FRAMES + 1) * SCENE_FRAMES + 1;
    play_scene_song();
}

static void toggle_music(void)
{
    music_on = (BYTE)!music_on;
#if HAVE_CDDA && defined(PLATFORM_CD)
    if (music_on) nc_sound_resume_cdda(); else nc_sound_pause_cdda();
#endif
}

/* ---------- text helpers ---------- */

static void put(int x, int srow, int pal, const char *s)
{
    fixPrint((ushort)x, (ushort)ROW(srow), (ushort)pal, 0, s);
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

/* ---------- tiles ---------- */

/* each colour is two rows of two characters; colour + shape, like the web build */
static const char *GLYPH[7][2] = {
    { "()", "()" },     /* circle   */
    { "/\\", "\\/" },   /* diamond  */
    { "[]", "[]" },     /* square   */
    { "/\\", "--" },    /* triangle */
    { "**", "**" },     /* star     */
    { "<>", "<>" },     /* hexagon  */
    { "##", "##" }      /* stone    */
};

static void draw_tile(Side *s, int r, int c, int color, int pal)
{
    int x = s->col0 + c * 2, y = BOARD_SROW + r * 2;
    if (color < 0) { put(x, y, PAL_TEXT, "  "); put(x, y + 1, PAL_TEXT, "  "); return; }
    put(x, y, pal, GLYPH[color][0]);
    put(x, y + 1, pal, GLYPH[color][1]);
}

static int tile_pal(int color) { return color == CC_STONE ? PAL_STONE : PAL_TILE0 + color; }

static void draw_board(Side *s)
{
    cc_grid w;
    int r, c, color, pal, blink = (int)((frames >> 3) & 1), hr = -1, hc = -1;
    BYTE pcol = (BYTE)(PAL_P1 + s->i);

    w = s->b.g;
    if (s->held) {
        hr = s->hr; hc = s->hc;
        if (s->has_t) {
            cc_apply_move(&w, s->hr, s->hc, s->axis, s->t);
            if (s->axis == CC_AXIS_H) hc = s->t; else hr = s->t;
        }
    }
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            color = w.c[r][c];
            pal = tile_pal(color);
            if (s->state == ST_POP && s->step.dying[r][c]) {
                if (blink) { draw_tile(s, r, c, -1, 0); continue; }
                pal = PAL_HOT;
            } else if (s->held) {
                if (r == hr && c == hc) pal = blink ? pcol : PAL_HOT;
                else if (s->has_t && s->pv.count && s->pv.hit[r][c] && blink) pal = PAL_HOT;
            } else if (s->state == ST_IDLE && r == s->cr && c == s->cc && ((frames >> 4) & 1)) {
                pal = pcol;
            }
            draw_tile(s, r, c, color, pal);
        }
}

static void clear_board_area(Side *s, int pal_unused)
{
    int r;
    (void)pal_unused;
    for (r = 0; r < CC_ROWS * 2; r++) put(s->col0, BOARD_SROW + r, PAL_TEXT, "                ");
}

static void draw_hud(Side *s)
{
    BYTE pcol = (BYTE)(PAL_P1 + s->i);
    int x = s->col0;
    put(x, HUD_SROW, pcol, s->i ? "2P" : "1P");
    if (s->state == ST_JOIN) {
        put(x + 3, HUD_SROW, PAL_DIM, "             ");
        put(x, HUD_SROW + 2, PAL_DIM, "                ");
        return;
    }
    put(x + 3, HUD_SROW, PAL_TEXT, "SCORE");
    put_num(x + 9, HUD_SROW, PAL_TEXT, (DWORD)s->b.score, 7);
    put(x, HUD_SROW + 2, PAL_TEXT, "MOVES");
    put_num(x + 6, HUD_SROW + 2, s->b.moves <= 3 ? PAL_BAD : PAL_GOOD,
            (DWORD)(s->b.moves < 0 ? 0 : s->b.moves), 2);
    if (mode == MODE_VS) {
        /* move timer: 7 pips that empty as time runs out */
        int pips = 7 - s->idle_t / 60, k;
        char bar[8];
        for (k = 0; k < 7; k++) bar[k] = (char)(k < pips ? '=' : ' ');
        bar[7] = 0;
        put(x + 9, HUD_SROW + 2, pips <= 2 ? PAL_BAD : PAL_DIM, bar);
        put(x, HUD_SROW + 3, PAL_STONE, "STONES");
        put_num(x + 7, HUD_SROW + 3, s->b.pending ? PAL_BAD : PAL_DIM, (DWORD)s->b.pending, 2);
    } else {
        put(x, HUD_SROW + 3, PAL_TEXT, "         ");
        put(x + 9, HUD_SROW + 2, PAL_TEXT, "       ");
    }
}

static void draw_side(Side *s)
{
    BYTE pcol = (BYTE)(PAL_P1 + s->i);
    draw_hud(s);
    if (s->state == ST_JOIN) {
        clear_board_area(s, 0);
        if ((frames >> 5) & 1) put(s->col0 + 2, BOARD_SROW + 6, pcol, "PRESS START");
        put(s->col0, MSG_SROW, PAL_TEXT, "                ");
        put(s->col0, STATUS_SROW, PAL_TEXT, "                ");
        return;
    }
    draw_board(s);
    if (s->msg_t) {
        put(s->col0, MSG_SROW, PAL_TEXT, "                ");
        put(s->col0, MSG_SROW, s->msg_pal, s->msg);
    } else {
        put(s->col0, MSG_SROW, PAL_TEXT, "                ");
    }
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
    s->idle_t = 0; s->stone_t = 0; s->msg_t = 0; s->lock_t = 0;
    s->win = 0;
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
    put(8, 19, PAL_DIM, "A GRAB/DROP  B CANCEL");
    put(8, 20, PAL_DIM, "C NEXT SCENE D MUSIC");
    put(8, 21, PAL_DIM, "SELECT MENU");
}

static void to_title(void)
{
    in_title = 1;
    reset_side(&side[0], 0);
    reset_side(&side[1], 1);
    clearFixLayer();
    pictureShow(&logo);
}

static void start_game(BYTE m, BYTE p)
{
    in_title = 0;
    mode = m;
    pictureHide(&logo);
    clearFixLayer();
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
        if (cc_resolve_step(&s->b, &s->step)) {
            say_points(s, s->step.points, s->step.chain);
            if (s->step.lever) say(s, "LEVER!", PAL_LEVER);
            s->state = ST_POP; s->timer = F_POP;
            return;
        }
        before = s->b.pending;
        gain = cc_resolve_end(&s->b, mode == MODE_VS ? &side[1 - s->i].b : (cc_board *)0);
        if (gain) say(s, gain > 1 ? "+MOVES!" : "+1 MOVE", PAL_GOOD);
        if (mode == MODE_VS && before && !s->b.pending && cc_count_stones(&s->b)) say(s, "STONES!", PAL_STONE);
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
        cc_gravity(&s->b, &s->step);
        s->state = ST_FALL; s->timer = F_FALL;
        return;
    case ST_FALL:
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
    if (s->idle_t >= CC_VS_IDLE_FRAMES) {
        s->idle_t = 0;
        s->b.moves--;
        say(s, "-1 MOVE", PAL_BAD);
        if (s->b.moves <= 0) { end_side(s); return; }
    }
    if (s->b.pending > 0) {
        if (++s->stone_t >= CC_STONE_DELAY_FRAMES) {
            s->stone_t = 0;
            cc_place_stones(&s->b, s->b.pending);
            s->b.pending = 0;
            s->held = 0;
            say(s, "STONES!", PAL_STONE);
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
        s->cr = (BYTE)fr; s->cc = (BYTE)fc;
        s->held = 0; s->has_t = 0;
        s->idle_t = 0;
        s->state = ST_PRE; s->timer = F_PRE;
    } else {
        if (s->has_t) say(s, "NO MATCH", PAL_BAD);
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
        }
        return;
    case ST_OVER:
        if (!s->lock_t && (start || (edge & JOY_A))) start_side(s, fresh_seed());
        return;
    case ST_RESULT:
        if (!s->lock_t && (start || (edge & JOY_A))) rematch();
        return;
    case ST_IDLE:
        break;
    default:
        return;   /* busy resolving */
    }
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
        if (!s->held) { s->held = 1; s->hr = s->cr; s->hc = s->cc; s->has_t = 0; }
        else drop(s);
    }
    if ((edge & JOY_B) && s->held) { s->held = 0; s->has_t = 0; }
}

/* ---------- main ---------- */

int main(void)
{
    BYTE e1, e2, r1, r2, ps, edges, k;

    initGfx();
    clearFixLayer();
    LSPCmode = 0x1c00;
    load_palettes();
    palJobPut(SPR_PAL_LOGO, cc_logo_Palettes.count, cc_logo_Palettes.data);
    pictureInit(&logo, &cc_logo, SPR_LOGO, SPR_PAL_LOGO, 80, 40, FLIP_NONE);
    to_title();
    play_scene_song();

    for (;;) {
        SCClose();
        waitVBlank();
        frames++;
        update_backdrop();

        e1 = volMEMBYTE(P1_EDGE);  r1 = volMEMBYTE(P1_REPEAT);
        e2 = volMEMBYTE(P2_EDGE);  r2 = volMEMBYTE(P2_REPEAT);
        ps = volMEMBYTE(PS_EDGE);
        edges = (BYTE)(e1 | e2);
        seed_mix += e1 + (e2 << 8) + ps;

        if (edges & JOY_C) next_scene();
        if (edges & JOY_D) toggle_music();
        if (frames % SCENE_FRAMES == 0) play_scene_song();

        put(1, 0, PAL_DIM, SCENE_NAME[scene_of(frames)]);
        put(27, 0, PAL_DIM, "BEST");
        put_num(32, 0, PAL_TEXT, best, 7);

        if (in_title) {
            if ((e1 | e2) & (JOY_UP | JOY_DOWN)) menu_sel = (BYTE)!menu_sel;
            if ((e1 & JOY_A) || (ps & P1_START)) start_game(menu_sel ? MODE_VS : MODE_SCORE, 0);
            else if ((e2 & JOY_A) || (ps & P2_START)) start_game(menu_sel ? MODE_VS : MODE_SCORE, 1);
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

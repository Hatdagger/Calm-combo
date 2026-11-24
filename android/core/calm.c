/*
 * Calm Combo - portrait, one board (see calm.h).
 *
 * The flow follows neogeo-cd/src/main.c: Score Attack, Puzzle, How to play
 * (4 lessons) and the high-score table with initials.  The rules come from
 * the Neo Geo CD's board.c and puzzle.c unchanged.  Everything here works in
 * 60 fps ticks, like the disc.
 *
 * Touch: drag a tile along its row or column and let go to drop it; tap the
 * buttons.  Controller: the disc's layout (d-pad, A grab/drop, B cancel,
 * Start, Select menu, X next scene, Y music).
 */
#include <string.h>
#include "calm.h"
#include "gfx.h"
#include "scenery.h"
#include "board.h"
#include "puzzle.h"

/* sound ids: the Neo Geo CD's list (its other definitions are not used here) */
typedef unsigned char BYTE;
#include "sound_ids.h"

#define TILE   16
#define NCELL  (CC_ROWS * CC_COLS)
#define BOARD_W (CC_COLS * TILE)
#define BOARD_H (CC_ROWS * TILE)

/* timings in frames (60 fps), as on the disc */
#define F_PRE      7
#define F_POP      16
#define F_SHUFFLE  27
#define F_LOCK     54
#define F_MSG      70
#define F_FLASH    10
#define FALL_MAX   9

#define HS_N 5
#define DRAG_START 5              /* pixels before a drag picks its direction */

enum { SCR_TITLE, SCR_SCORES, SCR_GAME };
enum { MODE_SCORE, MODE_PUZZLE, MODE_TUTOR };
enum { MENU_SCORE, MENU_PUZZLE, MENU_TUTOR, MENU_SCORES, MENU_N };
enum { ST_IDLE, ST_PRE, ST_POP, ST_FALL, ST_SHUFFLE, ST_OVER, ST_SOLVED, ST_FAILED, ST_NAME };

typedef struct { int x, y, vy, col, flash; } Obj;

/* ---------- state ---------- */

static int screen = SCR_TITLE, mode = MODE_SCORE, menu_sel = 0, music_on = 1, want_quit = 0;
static int pz_idx = 0, pz_sel = 0;
static long frames = 3000;                 /* the clock starts mid-morning, like the disc */
static unsigned long seed_mix = 0x1234567UL;
static int touch_mode = 1;                 /* last input was touch (else a controller) */

/* the board */
static cc_board b;
static int state, cr = 3, cc = 3, held, hr, hc, axis, has_t, t;
static cc_matches pv;
static cc_step step;
static int timer, lock_t, msg_t, msg_pal, moving, pz_left, got_lever, max_chain;
static char msg[20];
static cc_grid oid;
static Obj o[NCELL];
static int cux, cuy;
static char name[4];
static int name_pos;

/* saved */
static char  hs_name[HS_N][4];
static long  hs_score[HS_N];
static unsigned pz_solved;
static int   save_due;

/* layout, worked out on resize */
static int W = 180, H = 320, BX, BY, HUD_Y, MSG_Y, INFO_Y, BTN_Y, MEDIA_Y, media_room;

/* input */
static int key_down[CK_COUNT], key_hold[CK_COUNT], key_edge[CK_COUNT], key_rep[CK_COUNT];
static int drag_on, drag_x0, drag_y0, drag_x, drag_y, drag_axis;   /* drag_axis -1: not chosen */
static int tap_btn = -1;                   /* button under a finger */
static int tap_board;                      /* a tap began on the board */

/* the phone's music bar */
#define MEDIA_W 152
#define MEDIA_H 36
static int  media_on, media_flags, media_t;
static char media_title[100];
static int  mediaq[8], mq_r, mq_w;

/* sounds for the platform */
static int sndq[16], snd_r, snd_w;

/* buttons drawn this frame, for touch */
typedef struct { int x, y, w, h, id; } Btn;
static Btn btn[24];
static int nbtn;
enum { B_MUSIC = 1, B_SKIP, B_MENU, B_RETRY, B_ITEM0, B_PZ_PREV = B_ITEM0 + MENU_N, B_PZ_NEXT,
       B_BACK, B_UP0, B_DN0 = B_UP0 + 3, B_OK = B_DN0 + 3, B_MPREV, B_MPLAY, B_MNEXT, B_MASK };

static void sfx(int id)
{
    int n = (snd_w + 1) & 15;
    if (n == snd_r) return;
    sndq[snd_w] = id; snd_w = n;
}

static unsigned long fresh_seed(void)
{
    seed_mix = seed_mix * 1103515245UL + 12345UL + (unsigned long)frames;
    return seed_mix & 0x3FFFFFFFUL;
}

static void say(const char *s, int pal)
{
    strncpy(msg, s, sizeof msg - 1);
    msg[sizeof msg - 1] = 0;
    msg_t = F_MSG; msg_pal = pal;
}

static void num(char *d, long v, int width)
{
    int k;
    d[width] = 0;
    for (k = width - 1; k >= 0; k--) { d[k] = (char)('0' + v % 10); v /= 10; }
}

/* ---------- high scores and saving ---------- */

static const char DEF_NAME[HS_N][4] = { "SUN", "SEA", "SKY", "DEW", "OWL" };
static const long DEF_SCORE[HS_N] = { 5000, 4000, 3000, 2000, 1000 };

static void save_defaults(void)
{
    int k;
    for (k = 0; k < HS_N; k++) { memcpy(hs_name[k], DEF_NAME[k], 4); hs_score[k] = DEF_SCORE[k]; }
    pz_solved = 0;
}

/* "CC01", 5 x (3 letters, 4-byte score), 2 bytes of solved puzzles, checksum */
#define SAVE_LEN (4 + HS_N * 7 + 2 + 1)

static int save_write(unsigned char *p)
{
    int k, j, n = 0;
    unsigned char sum = 0;
    memcpy(p, "CC01", 4); n = 4;
    for (k = 0; k < HS_N; k++) {
        for (j = 0; j < 3; j++) p[n++] = (unsigned char)hs_name[k][j];
        for (j = 0; j < 4; j++) p[n++] = (unsigned char)((unsigned long)hs_score[k] >> (8 * j));
    }
    p[n++] = (unsigned char)(pz_solved & 0xff);
    p[n++] = (unsigned char)(pz_solved >> 8);
    for (k = 0; k < n; k++) sum = (unsigned char)(sum + p[k]);
    p[n++] = sum;
    return n;
}

static void save_read(const unsigned char *p, int len)
{
    int k, j, n = 4;
    unsigned char sum = 0;
    save_defaults();
    if (!p || len < SAVE_LEN || memcmp(p, "CC01", 4) != 0) return;
    for (k = 0; k < SAVE_LEN - 1; k++) sum = (unsigned char)(sum + p[k]);
    if (sum != p[SAVE_LEN - 1]) return;
    for (k = 0; k < HS_N; k++) {
        unsigned long v = 0;
        for (j = 0; j < 3; j++) {
            char ch = (char)p[n++];
            hs_name[k][j] = (ch >= 'A' && ch <= 'Z') || ch == ' ' || ch == '.' || ch == '!' ? ch : ' ';
        }
        hs_name[k][3] = 0;
        for (j = 0; j < 4; j++) v |= (unsigned long)p[n++] << (8 * j);
        hs_score[k] = (long)v;
    }
    pz_solved = p[n] | (p[n + 1] << 8);
}

static int hs_rank(long score)
{
    int k;
    for (k = 0; k < HS_N; k++) if (score > hs_score[k]) return k;
    return -1;
}

static void hs_insert(int r, const char *nm, long score)
{
    int k;
    for (k = HS_N - 1; k > r; k--) { memcpy(hs_name[k], hs_name[k - 1], 4); hs_score[k] = hs_score[k - 1]; }
    memcpy(hs_name[r], nm, 3); hs_name[r][3] = 0;
    hs_score[r] = score;
}

/* ---------- board objects (main.c objs_*) ---------- */

static int ease(int v, int to)
{
    int d = to - v;
    if (d > 0) return v + (d + 1) / 2;
    if (d < 0) return v - (1 - d) / 2;
    return v;
}

static void objs_reset(void)
{
    int r, c, k;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            k = r * CC_COLS + c;
            oid.c[r][c] = (cc_u8)k;
            o[k].x = c * TILE;
            o[k].y = r * TILE - (CC_ROWS + 1) * TILE - (CC_COLS - 1 - c) * 6;
            o[k].vy = 0;
            o[k].col = b.g.c[r][c];
            o[k].flash = 0;
        }
    cux = cc * TILE; cuy = cr * TILE;
}

static void objs_fall(void)
{
    int r, c, n, f, w, k;
    cc_u8 keep[CC_ROWS], freed[CC_ROWS];
    for (c = 0; c < CC_COLS; c++) {
        n = f = 0;
        for (r = CC_ROWS - 1; r >= 0; r--) {
            if (step.dying[r][c]) freed[f++] = oid.c[r][c];
            else keep[n++] = oid.c[r][c];
        }
        w = CC_ROWS - 1;
        for (k = 0; k < n; k++) oid.c[w--][c] = keep[k];
        for (k = 0; k < f; k++) {
            Obj *q = &o[freed[k]];
            oid.c[k][c] = freed[k];
            q->x = c * TILE; q->y = (k - f) * TILE; q->vy = 0; q->flash = 0;
        }
    }
}

static void objs_sync(void)
{
    int r, c;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) o[oid.c[r][c]].col = b.g.c[r][c];
}

/* where everything is shown this frame: the held line slid to its target */
static void shown_grids(cc_grid *ids, cc_grid *cols, int *shr, int *shc)
{
    *ids = oid; *cols = b.g; *shr = -1; *shc = -1;
    if (held) {
        *shr = hr; *shc = hc;
        if (has_t) {
            cc_apply_move(ids, hr, hc, axis, t);
            cc_apply_move(cols, hr, hc, axis, t);
            if (axis == CC_AXIS_H) *shc = t; else *shr = t;
        }
    }
}

/* move every tile one tick towards its cell (main.c render_side) */
static void objs_update(void)
{
    cc_grid ids, cols;
    int r, c, shr, shc, mv = 0;
    shown_grids(&ids, &cols, &shr, &shc);
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            Obj *q = &o[ids.c[r][c]];
            int tx = c * TILE, ty = r * TILE;
            if (q->col != cols.c[r][c]) { q->col = cols.c[r][c]; q->flash = F_FLASH; }
            if (q->flash) q->flash--;
            if (q->x != tx) { q->x = ease(q->x, tx); mv = 1; }
            if (q->y < ty && (state == ST_FALL || q->vy || ty - q->y > TILE)) {
                if (q->vy < FALL_MAX) q->vy++;
                q->y += q->vy;
                if (q->y >= ty) { q->y = ty; q->vy = 0; }
                mv = 1;
            } else if (q->y != ty) {
                q->y = ease(q->y, ty); q->vy = 0; mv = 1;
            }
        }
    moving = mv;
    if (held) {
        Obj *q = &o[ids.c[shr][shc]];
        cux = q->x; cuy = q->y - 2;
    } else {
        cux = ease(cux, cc * TILE);
        cuy = ease(cuy, cr * TILE);
    }
}

/* ---------- game flow ---------- */

static const cc_puzzle *cur_puzzle(void)
{
    return mode == MODE_TUTOR ? &CC_TUTORIAL[pz_idx] : &CC_PUZZLES[pz_idx];
}

static int pz_open(void)
{
    int k = 0;
    while (k < CC_NUM_PUZZLES - 1 && ((pz_solved >> k) & 1)) k++;
    return k;
}

static void board_begin(void)
{
    state = ST_IDLE;
    held = has_t = 0; drag_on = 0;
    cr = 3; cc = 3;
    msg_t = 0; lock_t = 0;
    objs_reset();
}

static void start_score(void) { cc_board_start(&b, fresh_seed(), fresh_seed()); board_begin(); }

static void start_pz(void)
{
    cc_puzzle_load(&b, cur_puzzle());
    pz_left = cur_puzzle()->moves;
    got_lever = 0; max_chain = 0;
    board_begin();
}

static void start_game(int m)
{
    screen = SCR_GAME;
    mode = m;
    if (m == MODE_SCORE) start_score(); else start_pz();
}

static void to_title(void) { screen = SCR_TITLE; held = 0; drag_on = 0; }

static void end_game(void)
{
    held = 0; drag_on = 0;
    if (hs_rank(b.score) >= 0) {
        state = ST_NAME; lock_t = F_LOCK;
        name[0] = name[1] = name[2] = 'A'; name[3] = 0;
        name_pos = 0;
    } else {
        state = ST_OVER; lock_t = F_LOCK;
    }
}

static const char ORD[HS_N][4] = { "1ST", "2ND", "3RD", "4TH", "5TH" };

static void name_done(void)
{
    int r = hs_rank(b.score);
    state = ST_OVER; lock_t = F_LOCK;
    if (r < 0) return;
    hs_insert(r, name, b.score);
    save_due = 1;
    strcpy(msg, "HIGH SCORE ");
    strcat(msg, ORD[r]);
    msg_t = 600; msg_pal = T_GOOD;
    sfx(SND_LEVER);
}

static void puzzle_after_move(void)
{
    const cc_puzzle *pz = cur_puzzle();
    int ok;
    switch (pz->goal) {
    case PZ_MATCH: ok = 1; break;
    case PZ_LEVER: ok = got_lever; break;
    case PZ_CHAIN: ok = max_chain >= 2; break;
    default:       ok = cc_count_stones(&b) == 0;
    }
    got_lever = 0; max_chain = 0;
    if (ok) {
        state = ST_SOLVED; lock_t = F_LOCK;
        say(mode == MODE_TUTOR ? "NICE!" : "SOLVED!", T_GOOD);
        sfx(SND_MOVES);
        if (mode == MODE_PUZZLE && !((pz_solved >> pz_idx) & 1)) { pz_solved |= 1u << pz_idx; save_due = 1; }
        return;
    }
    if (pz_left == 0 || !cc_has_valid_move(&b.g)) {
        state = ST_FAILED; lock_t = F_LOCK;
        if (pz->goal == PZ_LEVER) say("NOT A LEVER", T_BAD);
        else if (pz->goal == PZ_CHAIN) say("NO CHAIN YET", T_BAD);
        else if (pz_left == 0) say("OUT OF MOVES", T_BAD);
        else say("NO MOVES LEFT", T_BAD);
        sfx(SND_NO_MATCH);
        return;
    }
    state = ST_IDLE;
}

static void next_puzzle(void)
{
    int n = mode == MODE_TUTOR ? CC_NUM_TUTORIAL : CC_NUM_PUZZLES;
    if (pz_idx + 1 >= n) { to_title(); return; }
    pz_idx++;
    if (mode == MODE_PUZZLE) pz_sel = pz_idx;
    start_pz();
}

static void say_points(int pts, int chain)
{
    char buf[20], d[8];
    int n = 0, k, nd = 0;
    buf[n++] = '+';
    do { d[nd++] = (char)('0' + pts % 10); pts /= 10; } while (pts && nd < 6);
    for (k = nd - 1; k >= 0; k--) buf[n++] = d[k];
    if (chain > 1) { buf[n++] = ' '; buf[n++] = 'x'; buf[n++] = (char)('0' + (chain > 9 ? 9 : chain)); }
    buf[n] = 0;
    say(buf, T_P1);
}

static void resolve_tick(void)
{
    int gain;
    if (timer) { timer--; return; }
    switch (state) {
    case ST_PRE:
        if (moving) return;
        if (cc_resolve_step(&b, &step)) {
            if (step.lever) got_lever = 1;
            if (step.chain > max_chain) max_chain = step.chain;
            say_points(step.points, step.chain);
            sfx(SND_CLEAR_1 + (step.chain > 9 ? 8 : step.chain - 1));
            if (step.lever) { say("LEVER!", T_LEVER); sfx(SND_LEVER); }
            state = ST_POP; timer = F_POP;
            return;
        }
        if (mode != MODE_SCORE) {
            cc_resolve_end(&b, (cc_board *)0);
            puzzle_after_move();
            return;
        }
        gain = cc_resolve_end(&b, (cc_board *)0);
        if (gain) { say(gain > 1 ? "+MOVES!" : "+1 MOVE", T_GOOD); sfx(SND_MOVES); }
        if (b.moves <= 0) { end_game(); return; }
        if (!cc_has_valid_move(&b.g)) { say("SHUFFLE!", T_TEXT); state = ST_SHUFFLE; timer = F_SHUFFLE; return; }
        state = ST_IDLE;
        return;
    case ST_POP:
        objs_fall();
        cc_gravity(&b, &step);
        objs_sync();
        state = ST_FALL; timer = 2;
        moving = 1;
        return;
    case ST_FALL:
        if (moving) return;
        state = ST_PRE; timer = 0;
        return;
    case ST_SHUFFLE:
        cc_shuffle(&b);
        state = ST_IDLE;
        return;
    }
}

/* the held tile's target, from the d-pad (main.c held_dir) */
static void held_dir(int dr, int dc)
{
    int ax = dc ? CC_AXIS_H : CC_AXIS_V, st = dc ? dc : dr;
    int org = ax == CC_AXIS_H ? hc : hr, mx = ax == CC_AXIS_H ? CC_COLS - 1 : CC_ROWS - 1, nt;
    if (!has_t) {
        nt = org + st;
        if (nt < 0 || nt > mx) return;
        axis = ax; t = nt; has_t = 1;
    } else {
        if (axis != ax) return;
        nt = t + st;
        if (nt < 0 || nt > mx) return;
        if (nt == org) { has_t = 0; return; }
        t = nt;
    }
    cc_preview(&b, hr, hc, axis, t, &pv);
}

static void grab(int r, int c)
{
    held = 1; hr = r; hc = c; has_t = 0; cr = r; cc = c;
    pv.count = 0;
    sfx(SND_GRAB);
}

static void drop(void)
{
    if (has_t && pv.count) {
        int fr = axis == CC_AXIS_H ? hr : t, fc = axis == CC_AXIS_H ? t : hc;
        cc_commit(&b, hr, hc, axis, t);
        if (mode != MODE_SCORE && pz_left) pz_left--;
        cc_apply_move(&oid, hr, hc, axis, t);
        cr = fr; cc = fc;
        held = 0; has_t = 0;
        state = ST_PRE; timer = F_PRE;
    } else {
        if (has_t) { say("NO MATCH", T_BAD); sfx(SND_NO_MATCH); }
        held = 0; has_t = 0;
    }
}

/* continue after game over / solved / failed (tap, A or Start) */
static void go_on(void)
{
    if (lock_t) return;
    switch (state) {
    case ST_OVER:   start_score(); sfx(SND_MENU); break;
    case ST_SOLVED: next_puzzle(); sfx(SND_MENU); break;
    case ST_FAILED: start_pz(); sfx(SND_MENU); break;
    default: break;
    }
}

static void next_scene(void) { frames = (frames / SCENE_FRAMES + 1) * SCENE_FRAMES + 1; }

/* ---------- name entry ---------- */

static const char NAME_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ.! ";
#define NAME_NCH 29

static void name_cycle(int pos, int dir)
{
    int k = 0;
    while (k < NAME_NCH - 1 && NAME_CHARS[k] != name[pos]) k++;
    k = dir > 0 ? (k + 1) % NAME_NCH : (k + NAME_NCH - 1) % NAME_NCH;
    name[pos] = NAME_CHARS[k];
    sfx(SND_TICK);
}

/* ---------- controller ---------- */

static void pad_game(void)
{
    int dr = 0, dc = 0;
    if (key_edge[CK_SELECT] || key_edge[CK_BACK]) { to_title(); sfx(SND_MENU); return; }
    switch (state) {
    case ST_OVER: case ST_SOLVED: case ST_FAILED:
        if (key_edge[CK_A] || key_edge[CK_START]) go_on();
        return;
    case ST_NAME:
        if (lock_t) return;
        if (key_rep[CK_UP]) name_cycle(name_pos, 1);
        if (key_rep[CK_DOWN]) name_cycle(name_pos, -1);
        if (key_edge[CK_A] || key_edge[CK_RIGHT] || key_edge[CK_START]) {
            sfx(SND_MENU);
            if (++name_pos >= 3) name_done();
        } else if ((key_edge[CK_B] || key_edge[CK_LEFT]) && name_pos) {
            name_pos--;
        }
        return;
    case ST_IDLE:
        if (mode != MODE_SCORE && key_edge[CK_START]) { start_pz(); sfx(SND_MENU); return; }
        break;
    default:
        return;
    }
    if (drag_on) return;
    if (key_rep[CK_UP]) dr = -1; else if (key_rep[CK_DOWN]) dr = 1;
    else if (key_rep[CK_LEFT]) dc = -1; else if (key_rep[CK_RIGHT]) dc = 1;
    if (dr || dc) {
        if (held) held_dir(dr, dc);
        else {
            if (cr + dr >= 0 && cr + dr < CC_ROWS) cr += dr;
            if (cc + dc >= 0 && cc + dc < CC_COLS) cc += dc;
        }
    }
    if (key_edge[CK_A]) { if (!held) grab(cr, cc); else drop(); }
    if (key_edge[CK_B] && held) { held = 0; has_t = 0; }
}

static void menu_go(int sel)
{
    menu_sel = sel;
    sfx(SND_MENU);
    switch (sel) {
    case MENU_SCORE:  start_game(MODE_SCORE); break;
    case MENU_PUZZLE: pz_idx = pz_sel; start_game(MODE_PUZZLE); break;
    case MENU_TUTOR:  pz_idx = 0; start_game(MODE_TUTOR); break;
    default:          screen = SCR_SCORES; break;
    }
}

static void pad_title(void)
{
    if (key_edge[CK_BACK]) { want_quit = 1; return; }
    if (key_rep[CK_UP] || key_rep[CK_DOWN]) {
        menu_sel = (menu_sel + (key_rep[CK_UP] ? MENU_N - 1 : 1)) % MENU_N;
        sfx(SND_MENU);
    }
    if (menu_sel == MENU_PUZZLE && key_rep[CK_LEFT] && pz_sel > 0) { pz_sel--; sfx(SND_TICK); }
    if (menu_sel == MENU_PUZZLE && key_rep[CK_RIGHT] && pz_sel < pz_open()) { pz_sel++; sfx(SND_TICK); }
    if (key_edge[CK_A] || key_edge[CK_START]) menu_go(menu_sel);
}

/* ---------- touch ---------- */

static int btn_at(int x, int y)
{
    int k;
    for (k = nbtn - 1; k >= 0; k--)
        if (x >= btn[k].x && x < btn[k].x + btn[k].w && y >= btn[k].y && y < btn[k].y + btn[k].h) return btn[k].id;
    return -1;
}

static void press(int id)
{
    if (id == B_MUSIC) { music_on = !music_on; return; }
    if (id == B_SKIP)  { next_scene(); return; }
    if (id >= B_MPREV && id <= B_MASK) {
        int n = (mq_w + 1) & 7;
        if (n != mq_r) { mediaq[mq_w] = CMA_PREV + (id - B_MPREV); mq_w = n; }
        return;
    }
    switch (screen) {
    case SCR_TITLE:
        if (id >= B_ITEM0 && id < B_ITEM0 + MENU_N) {
            menu_go(id - B_ITEM0);
        } else if (id == B_PZ_PREV && pz_sel > 0) { menu_sel = MENU_PUZZLE; pz_sel--; sfx(SND_TICK); }
        else if (id == B_PZ_NEXT && pz_sel < pz_open()) { menu_sel = MENU_PUZZLE; pz_sel++; sfx(SND_TICK); }
        return;
    case SCR_SCORES:
        if (id == B_BACK) { screen = SCR_TITLE; sfx(SND_MENU); }
        return;
    default:
        if (id == B_MENU) { to_title(); sfx(SND_MENU); return; }
        if (id == B_RETRY) {
            if (mode == MODE_SCORE) start_score(); else start_pz();
            sfx(SND_MENU);
            return;
        }
        if (state == ST_NAME && !lock_t) {
            if (id >= B_UP0 && id < B_UP0 + 3) { name_pos = id - B_UP0; name_cycle(name_pos, 1); }
            if (id >= B_DN0 && id < B_DN0 + 3) { name_pos = id - B_DN0; name_cycle(name_pos, -1); }
            if (id == B_OK) { sfx(SND_MENU); name_done(); }
        }
        return;
    }
}

static int in_board(int x, int y) { return x >= BX && x < BX + BOARD_W && y >= BY && y < BY + BOARD_H; }

/* the drag decides the axis, then the cell the tile lands on */
static void drag_update(void)
{
    int dx = drag_x - drag_x0, dy = drag_y - drag_y0, adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy, d, nt, org, mx;
    if (drag_axis < 0) {
        if (adx < DRAG_START && ady < DRAG_START) return;
        drag_axis = adx >= ady ? CC_AXIS_H : CC_AXIS_V;
    } else if (adx < DRAG_START && ady < DRAG_START) {
        drag_axis = -1; has_t = 0;           /* back where it started: free to pick again */
        return;
    }
    d = drag_axis == CC_AXIS_H ? dx : dy;
    org = drag_axis == CC_AXIS_H ? hc : hr;
    mx = drag_axis == CC_AXIS_H ? CC_COLS - 1 : CC_ROWS - 1;
    nt = org + (d >= 0 ? (d + TILE / 2) / TILE : -((-d + TILE / 2) / TILE));
    if (nt < 0) nt = 0;
    if (nt > mx) nt = mx;
    if (nt == org) { has_t = 0; return; }
    if (!has_t || t != nt || axis != drag_axis) {
        axis = drag_axis; t = nt; has_t = 1;
        cc_preview(&b, hr, hc, axis, t, &pv);
        sfx(SND_TICK);
    }
}

void calm_touch(int action, int x, int y)
{
    touch_mode = 1;
    switch (action) {
    case CT_DOWN:
        tap_btn = btn_at(x, y);
        tap_board = 0;
        if (tap_btn >= 0) return;
        if (screen != SCR_GAME) return;
        if (in_board(x, y) || state == ST_OVER || state == ST_SOLVED || state == ST_FAILED) tap_board = 1;
        if (state == ST_IDLE && !held && in_board(x, y)) {
            grab((y - BY) / TILE, (x - BX) / TILE);
            drag_on = 1; drag_axis = -1;
            drag_x0 = drag_x = x; drag_y0 = drag_y = y;
        }
        return;
    case CT_MOVE:
        if (drag_on) { drag_x = x; drag_y = y; drag_update(); }
        return;
    case CT_UP:
        if (drag_on) { drag_on = 0; if (held) drop(); return; }
        if (tap_btn >= 0 && btn_at(x, y) == tap_btn) press(tap_btn);
        else if (tap_board && screen == SCR_GAME) go_on();
        tap_btn = -1; tap_board = 0;
        return;
    default:
        if (drag_on) { drag_on = 0; held = 0; has_t = 0; }
        tap_btn = -1; tap_board = 0;
        return;
    }
}

void calm_key(int key, int down)
{
    if (key < 0 || key >= CK_COUNT) return;
    if (down && !key_down[key] && key != CK_BACK) touch_mode = 0;   /* back is on every phone */
    key_down[key] = down;
}

/* edges and repeats: first press, then every 6 ticks after 18 (d-pad feel of the disc) */
static void read_keys(void)
{
    int k;
    for (k = 0; k < CK_COUNT; k++) {
        key_edge[k] = key_rep[k] = 0;
        if (!key_down[k]) { key_hold[k] = 0; continue; }
        if (key_hold[k] == 0) key_edge[k] = key_rep[k] = 1;
        else if (key_hold[k] >= 18 && (key_hold[k] - 18) % 6 == 0) key_rep[k] = 1;
        key_hold[k]++;
    }
}

/* ---------- tick ---------- */

void calm_tick(void)
{
    frames++;
    media_t++;
    seed_mix += (unsigned long)frames;
    read_keys();
    scenery_tick(frames, H);
    if (key_edge[CK_X]) next_scene();
    if (key_edge[CK_Y]) music_on = !music_on;
    switch (screen) {
    case SCR_TITLE: pad_title(); break;
    case SCR_SCORES:
        if (key_edge[CK_A] || key_edge[CK_B] || key_edge[CK_START] || key_edge[CK_BACK]) { screen = SCR_TITLE; sfx(SND_MENU); }
        break;
    default:
        if (lock_t) lock_t--;
        pad_game();
        if (screen != SCR_GAME) break;
        if (state >= ST_PRE && state <= ST_SHUFFLE) resolve_tick();
        if (msg_t) msg_t--;
        if (state != ST_NAME) objs_update();
        break;
    }
}

/* ---------- drawing ---------- */

static const Img *im_tiles, *im_cursor, *im_panel, *im_logo;

static void add_btn(int x, int y, int w, int h, int id)
{
    if (nbtn >= 24) return;
    btn[nbtn].x = x; btn[nbtn].y = y; btn[nbtn].w = w; btn[nbtn].h = h; btn[nbtn].id = id;
    nbtn++;
}

/* a dark plate with a label; also a touch target */
static void button(int x, int y, int w, const char *label, int pal, int id)
{
    fill_alpha(x, y, w, 16, PX(14, 10, 28), 170);
    fill_alpha(x, y, w, 1, PX(255, 255, 255), 40);
    text_c(x + w / 2, y + 4, pal, label);
    add_btn(x - 2, y - 4, w + 4, 24, id);
}

static void plate(int x, int y, int w, int h) { fill_alpha(x, y, w, h, PX(14, 10, 28), 140); }

static void triangle(int cx, int cy, int up, int pal)
{
    int k;
    uint32_t c = pal == T_DIM ? PX(0xa9, 0x9b, 0xc4) : PX(0xff, 0xe7, 0xa8);
    for (k = 0; k < 5; k++) fill(cx - k, up ? cy - 2 + k : cy + 2 - k, k * 2 + 1, 1, c);
}

static void top_bar(void)
{
    button(4, 4, 76, music_on ? "MUSIC ON" : "MUSIC OFF", music_on ? T_TEXT : T_DIM, B_MUSIC);
    button(W - 28, 4, 24, ">>", T_TEXT, B_SKIP);
}

static void draw_board(void)
{
    cc_grid ids, cols;
    int r, c, shr, shc, popf = -1, blink = (int)((frames >> 3) & 1);
    Obj *heldo = 0;
    if (im_panel) blit(im_panel, 0, 0, 0, im_panel->w, im_panel->h, BX - 8, BY - 8, B_PLAIN);
    else plate(BX - 4, BY - 4, BOARD_W + 8, BOARD_H + 8);
    shown_grids(&ids, &cols, &shr, &shc);
    if (state == ST_POP) { int e = F_POP - timer; popf = e < 4 ? 1 : e < 10 ? 2 : 3; }
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            Obj *q = &o[ids.c[r][c]];
            int fr = 0, mode_ = B_PLAIN, sy = 0, sh = TILE;
            if (q->y < -8) continue;
            if (popf >= 0 && step.dying[r][c]) fr = popf;
            else if (q->flash && (q->flash & 2)) mode_ = B_BRIGHT;
            else if (held && has_t && pv.count && pv.hit[r][c] && blink) mode_ = B_BRIGHT;
            if (r == shr && c == shc) { heldo = q; continue; }
            if (q->y < 0) { sy = -q->y; sh = TILE - sy; }          /* sliding in over the top edge */
            blit(im_tiles, 0, fr * TILE, q->col * TILE + sy, TILE, sh, BX + q->x, BY + q->y + sy, mode_);
        }
    /* the line being slid: green when the drop would clear something */
    if (held) {
        int moved = has_t, ok = has_t && pv.count;
        uint32_t col = ok ? PX(95, 227, 154) : moved ? PX(255, 107, 129) : PX(255, 255, 255);
        int a = ok ? 70 : moved ? 50 : 30;
        if (!has_t || axis == CC_AXIS_H) fill_alpha(BX, BY + hr * TILE, BOARD_W, TILE, col, a);
        if (!has_t || axis == CC_AXIS_V) fill_alpha(BX + hc * TILE, BY, TILE, BOARD_H, col, a);
    }
    if (heldo) blit(im_tiles, 0, 0, heldo->col * TILE, TILE, TILE, BX + heldo->x, BY + heldo->y - 2, B_BRIGHT);
    if (!touch_mode && state == ST_IDLE && im_cursor)
        blit(im_cursor, 0, (int)((frames >> 3) & 3) * TILE, 0, TILE, TILE, BX + cux, BY + cuy, B_PLAIN);
}

static const char *LESSON_NAME[4] = { "SLIDE A LINE", "THE LEVER", "CHAINS", "STONES" };
static const char *TOUCH_LESSON1[PZ_LINES] = {
    "TOUCH A TILE AND", "DRAG IT TO SLIDE", "ITS ROW OR COLUMN.", "LINE UP 3 OF A", "KIND, THEN", "LET GO."
};

static void draw_hud(void)
{
    char n[12];
    int x = BX, rx = BX + BOARD_W, y = HUD_Y;
    plate(x - 4, y - 4, BOARD_W + 8, 40);
    if (mode == MODE_SCORE) {
        text(x, y, T_P1, "SCORE");
        num(n, b.score, 7); text_r(rx, y, T_TEXT, n);
        text(x, y + 12, T_TEXT, "MOVES");
        num(n, b.moves < 0 ? 0 : b.moves, 2); text_r(rx, y + 12, b.moves <= 3 ? T_BAD : T_GOOD, n);
        text(x, y + 24, T_DIM, "BEST");
        num(n, hs_score[0] > b.score ? hs_score[0] : b.score, 7); text_r(rx, y + 24, T_DIM, n);
        return;
    }
    if (mode == MODE_TUTOR) {
        text(x, y, T_P1, "LESSON");
        n[0] = (char)('1' + pz_idx); strcpy(n + 1, "/4"); text_r(rx, y, T_TEXT, n);
    } else {
        text(x, y, T_P1, "PUZZLE");
        num(n, pz_idx + 1, 2); strcat(n, "/"); num(n + 3, CC_NUM_PUZZLES, 2); text_r(rx, y, T_TEXT, n);
    }
    text(x, y + 12, T_TEXT, "MOVES");
    num(n, pz_left, 2); text_r(rx, y + 12, pz_left <= 1 ? T_BAD : T_GOOD, n);
    if (cur_puzzle()->goal == PZ_STONES) {
        int st = cc_count_stones(&b);
        text(x, y + 24, T_STONE, "STONES");
        num(n, st, 2); text_r(rx, y + 24, st ? T_TEXT : T_GOOD, n);
    } else if (mode == MODE_TUTOR) {
        text(x, y + 24, T_LEVER, LESSON_NAME[pz_idx]);
    }
}

static void draw_info(void)
{
    const cc_puzzle *pz = cur_puzzle();
    int k, y = INFO_Y, cx = W / 2;
    char line[24];
    if (mode == MODE_SCORE || state == ST_NAME) return;
    plate(cx - 76, y - 4, 152, PZ_LINES * 10 + 6);
    for (k = 0; k < PZ_LINES; k++, y += 10) {
        if (mode == MODE_TUTOR) {
            text_c(cx, y, T_TEXT, pz_idx == 0 && touch_mode ? TOUCH_LESSON1[k] : pz->text[k]);
            continue;
        }
        switch (k) {
        case 0: text_c(cx, y, T_TEXT, "BREAK EVERY STONE"); break;
        case 1:
            strcpy(line, "WITHIN ");
            line[7] = (char)('0' + pz->moves); line[8] = 0;
            strcat(line, pz->moves == 1 ? " MOVE." : " MOVES.");
            text_c(cx, y, T_GOOD, line);
            break;
        case 3: text_c(cx, y, T_TEXT, "MATCHES NEXT TO A"); break;
        case 4: text_c(cx, y, T_TEXT, "STONE BREAK IT."); break;
        default: break;
        }
    }
}

static void draw_status(void)
{
    const char *a = 0, *bl = 0;
    int pa = T_TEXT, ready = !lock_t && ((frames >> 4) & 1);
    int last = (mode == MODE_TUTOR ? CC_NUM_TUTORIAL : CC_NUM_PUZZLES) <= pz_idx + 1;
    switch (state) {
    case ST_OVER:   a = "GAME OVER"; pa = T_BAD; bl = touch_mode ? "TAP TO PLAY AGAIN" : "A: PLAY AGAIN"; break;
    case ST_SOLVED: a = mode == MODE_TUTOR ? (last ? "ALL DONE!" : "WELL DONE!") : (last ? "ALL SOLVED!" : "SOLVED!");
                    pa = T_GOOD;
                    bl = last ? (touch_mode ? "TAP FOR THE MENU" : "A: MENU") : (touch_mode ? "TAP FOR THE NEXT" : "A: NEXT"); break;
    case ST_FAILED: a = "TRY AGAIN!"; pa = T_BAD; bl = touch_mode ? "TAP TO RETRY" : "A: RETRY"; break;
    default: break;
    }
    if (msg_t) text_c(W / 2, MSG_Y, msg_pal, msg);
    if (!a) return;
    text_c(W / 2, MSG_Y + 12, pa, a);
    if (ready) text_c(W / 2, MSG_Y + 24, T_TEXT, bl);
}

static void draw_name_entry(void)
{
    int k, cx = W / 2, y = BY + 30;
    char one[2];
    plate(BX - 4, BY - 4, BOARD_W + 8, BOARD_H + 8);
    text_c(cx, BY + 6, T_GOOD, "NEW HIGH SCORE!");
    one[1] = 0;
    for (k = 0; k < 3; k++) {
        int x = cx - 36 + k * 36;
        one[0] = name[k];
        if (k == name_pos && !touch_mode && ((frames >> 3) & 1)) one[0] = '_';
        triangle(x, y + 6, 1, k == name_pos ? T_P1 : T_DIM);
        add_btn(x - 16, y - 8, 32, 22, B_UP0 + k);
        fill_alpha(x - 8, y + 16, 16, 16, PX(14, 10, 28), 200);
        text(x - 4, y + 20, k == name_pos ? T_P1 : T_TEXT, one);
        triangle(x, y + 42, 0, k == name_pos ? T_P1 : T_DIM);
        add_btn(x - 16, y + 34, 32, 22, B_DN0 + k);
    }
    button(cx - 24, BY + BOARD_H - 22, 48, "OK", T_GOOD, B_OK);
}

static void draw_game(void)
{
    draw_hud();
    if (state == ST_NAME) draw_name_entry(); else draw_board();
    draw_status();
    draw_info();
    button(8, BTN_Y, 56, "MENU", T_TEXT, B_MENU);
    if (mode != MODE_SCORE || state == ST_OVER) button(W - 64, BTN_Y, 56, "RETRY", T_TEXT, B_RETRY);
}

static void draw_title(void)
{
    static const char *ITEM[MENU_N] = { "SCORE ATTACK", "PUZZLE", "HOW TO PLAY", "HIGH SCORES" };
    int k, cx = W / 2, y0 = H * 2 / 5, ly = y0 - 66 > 26 ? y0 - 66 : 26;
    char n[12];
    if (im_logo) blit(im_logo, 0, 0, 0, im_logo->w, im_logo->h, cx - im_logo->w / 2, ly, B_PLAIN);
    text_c(cx, ly + 38, T_TEXT, "SLIDE  MATCH  UNWIND");
    plate(cx - 72, y0 - 8, 144, MENU_N * 22 + 8);
    for (k = 0; k < MENU_N; k++) {
        int y = y0 + k * 22, sel = menu_sel == k, pal = sel ? T_P1 : T_TEXT;
        add_btn(cx - 72, y - 6, 144, 22, B_ITEM0 + k);
        if (sel && !touch_mode) text(cx - 68, y, T_P1, ">");
        if (k == MENU_PUZZLE) {
            text(cx - 56, y, pal, ITEM[k]);
            num(n, pz_sel + 1, 2);
            text(cx + 22, y, pal, n);
            text(cx + 6, y, pz_sel > 0 ? T_P1 : T_DIM, "<");
            text(cx + 42, y, pz_sel < pz_open() ? T_P1 : T_DIM, ">");
            if ((pz_solved >> pz_sel) & 1) text(cx + 54, y, T_GOOD, "*");
            add_btn(cx - 4, y - 6, 24, 22, B_PZ_PREV);
            add_btn(cx + 36, y - 6, 24, 22, B_PZ_NEXT);
        } else {
            text(cx - 56, y, pal, ITEM[k]);
        }
    }
    num(n, hs_score[0], 7);
    text_c(cx, y0 + MENU_N * 22 + 14, T_DIM, "BEST");
    text_c(cx, y0 + MENU_N * 22 + 26, T_TEXT, n);
    if ((frames >> 5) & 1) text_c(cx, H - 22, T_GOOD, touch_mode ? "TAP A MODE TO PLAY" : "A: START");
}

static void draw_scores(void)
{
    static const char *ORDS[HS_N] = { "1ST", "2ND", "3RD", "4TH", "5TH" };
    int k, cx = W / 2, y = H / 3;
    char n[12];
    plate(cx - 72, y - 12, 144, 24 + HS_N * 16 + 8);
    text_c(cx, y - 4, T_LEVER, "HIGH SCORES");
    for (k = 0; k < HS_N; k++) {
        int yy = y + 18 + k * 16;
        text(cx - 64, yy, T_P1, ORDS[k]);
        text(cx - 28, yy, T_TEXT, hs_name[k]);
        num(n, hs_score[k], 7);
        text_r(cx + 64, yy, T_TEXT, n);
    }
    button(cx - 32, BTN_Y, 64, "BACK", T_TEXT, B_BACK);
}

/* the phone's music: an outline, one line for the song, |< > >| */
static void media_icon(int cx, int cy, int which, int lit)
{
    uint32_t c = lit ? PX(0xff, 0xe7, 0xa8) : PX(0xf3, 0xec, 0xff), sh = PX(0x30, 0x24, 0x48);
    int pass, k;
    for (pass = 0; pass < 2; pass++) {           /* a dark shadow one pixel down, then the icon */
        int y = cy + (pass ? 0 : 1);
        uint32_t col = pass ? c : sh;
        switch (which) {
        case 0:                                  /* previous: bar and a left triangle */
            fill(cx - 4, y - 4, 2, 9, col);
            for (k = 0; k < 5; k++) fill(cx - 1 + k, y - k, 1, k * 2 + 1, col);
            break;
        case 1:                                  /* play: a right triangle */
            for (k = 0; k < 5; k++) fill(cx - 2 + k, y - 4 + k, 1, 9 - k * 2, col);
            break;
        case 2:                                  /* pause: two bars */
            fill(cx - 3, y - 4, 2, 9, col);
            fill(cx + 1, y - 4, 2, 9, col);
            break;
        default:                                 /* next: a right triangle and a bar */
            for (k = 0; k < 5; k++) fill(cx - 4 + k, y - 4 + k, 1, 9 - k * 2, col);
            fill(cx + 2, y - 4, 2, 9, col);
            break;
        }
    }
}

static void draw_media(void)
{
    int x = W / 2 - MEDIA_W / 2, y = MEDIA_Y, cx = W / 2, iy = y + 25, tx = x + 6, inner = MEDIA_W - 12;
    int playing = media_flags & CM_PLAYING, pal, tw, k;
    const char *s;
    uint32_t edge = PX(0xf3, 0xec, 0xff);
    if (!media_on || !media_room) return;
    fill_alpha(x + 1, y + 1, MEDIA_W - 2, MEDIA_H - 2, PX(14, 10, 28), 110);   /* faint, so the song reads on any scene */
    fill_alpha(x, y, MEDIA_W, 1, edge, 90);
    fill_alpha(x, y + MEDIA_H - 1, MEDIA_W, 1, edge, 90);
    fill_alpha(x, y + 1, 1, MEDIA_H - 2, edge, 90);
    fill_alpha(x + MEDIA_W - 1, y + 1, 1, MEDIA_H - 2, edge, 90);

    if (!(media_flags & CM_TITLES)) {
        s = "SHOW SONG TITLES"; pal = T_DIM;
        add_btn(x, y - 2, MEDIA_W, 16, B_MASK);
    } else if (!(media_flags & CM_SESSION)) {
        s = "NOTHING PLAYING"; pal = T_DIM;
    } else {
        s = media_title[0] ? media_title : playing ? "PLAYING" : "PAUSED";
        pal = playing ? T_TEXT : T_DIM;
    }
    tw = text_w(s);
    clip_x0 = tx; clip_x1 = tx + inner;
    if (tw <= inner) {
        text_c(cx, y + 6, pal, s);
    } else {                                     /* too long: rest, then drift left and round again */
        int gap = 40, loop = tw + gap, hold = 150, ph = media_t % (hold + loop * 3);
        int off = ph < hold ? 0 : (ph - hold) / 3;
        text(tx - off, y + 6, pal, s);
        text(tx - off + loop, y + 6, pal, s);
    }
    clip_x0 = 0; clip_x1 = 1 << 30;

    for (k = 0; k < 3; k++) {
        int bx = cx + (k - 1) * 44, id = B_MPREV + k;
        media_icon(bx, iy, k == 0 ? 0 : k == 2 ? 3 : playing ? 2 : 1, tap_btn == id);
        add_btn(bx - 22, iy - 11, 44, 22, id);
    }
}

void calm_render(uint32_t *px, int stride_px)
{
    fb = px; FW = W; FH = H; FSTRIDE = stride_px;
    nbtn = 0;
    scenery_draw(frames);
    top_bar();
    switch (screen) {
    case SCR_TITLE:  draw_title(); break;
    case SCR_SCORES: draw_scores(); break;
    default:         draw_game(); break;
    }
    draw_media();
}

/* ---------- platform ---------- */

int calm_pick_scale(int sw, int sh)
{
    int s = sw / 180, s2 = sh / 300;
    if (s2 < s) s = s2;
    return s < 1 ? 1 : s;
}

void calm_resize(int w, int h)
{
    int extra;
    W = w < 160 ? 160 : w > 400 ? 400 : w;
    H = h < 280 ? 280 : h > 720 ? 720 : h;
    extra = (H - 320) / 3;
    if (extra < 0) extra = 0;
    if (extra > 60) extra = 60;
    BX = (W - BOARD_W) / 2;
    BY = 72 + extra;
    HUD_Y = BY - 46;
    MSG_Y = BY + BOARD_H + 8;
    INFO_Y = MSG_Y + 40;
    BTN_Y = H - 24;
    if (INFO_Y + PZ_LINES * 10 > BTN_Y - 4) INFO_Y = BTN_Y - 4 - PZ_LINES * 10;
    /* the music bar sits above MENU, only where it clears the puzzle notes */
    MEDIA_Y = BTN_Y - 6 - MEDIA_H;
    media_room = MEDIA_Y >= INFO_Y + PZ_LINES * 10 + 2;
}

int calm_init(const void *pak, long pak_len, const void *save, int save_len)
{
    if (gfx_load_pak(pak, pak_len) != 0) return -1;
    im_tiles = img("board/tiles");
    im_cursor = img("board/cursor");
    im_panel = img("board/panel");
    im_logo = img("logo");
    if (!im_tiles || !img("font/font")) return -2;
    save_read((const unsigned char *)save, save_len);
    calm_resize(W, H);
    return 0;
}

int calm_pop_sound(void)
{
    int id;
    if (snd_r == snd_w) return -1;
    id = sndq[snd_r]; snd_r = (snd_r + 1) & 15;
    return id;
}

int calm_music(void) { return music_on ? 1 + (int)((frames / SCENE_FRAMES) % NUM_SCENES) : 0; }

int calm_save(unsigned char *buf, int max)
{
    if (!save_due || max < SAVE_LEN) return 0;
    save_due = 0;
    return save_write(buf);
}

void calm_media(int flags, const char *title)
{
    media_on = 1;
    media_flags = flags;
    if (!title) title = "";
    if (strncmp(title, media_title, sizeof media_title - 1)) {
        strncpy(media_title, title, sizeof media_title - 1);
        media_title[sizeof media_title - 1] = 0;
        media_t = 0;                             /* a new song starts from its first letter */
    }
}

int calm_pop_media(void)
{
    int a;
    if (mq_r == mq_w) return -1;
    a = mediaq[mq_r]; mq_r = (mq_r + 1) & 7;
    return a;
}

int calm_quit(void) { int q = want_quit; want_quit = 0; return q; }

#ifdef CALM_TEST
int calm_test_find_move(int *r, int *c, int *ax, int *tt)
{
    int rr, c2, a, k;
    cc_matches m;
    for (rr = 0; rr < CC_ROWS; rr++)
        for (c2 = 0; c2 < CC_COLS; c2++)
            for (a = 0; a < 2; a++)
                for (k = 0; k < (a ? CC_ROWS : CC_COLS); k++) {
                    if (k == (a ? rr : c2)) continue;
                    if (cc_preview(&b, rr, c2, a, k, &m)) { *r = rr; *c = c2; *ax = a; *tt = k; return 1; }
                }
    return 0;
}
void calm_test_set(int score, int moves) { b.score = score; b.moves = (cc_s16)moves; }
void calm_test_clock(long s) { frames = s * 60; }
const char *calm_test_state(void)
{
    static const char *N[] = { "IDLE", "PRE", "POP", "FALL", "SHUFFLE", "OVER", "SOLVED", "FAILED", "NAME" };
    if (screen == SCR_TITLE) return "TITLE";
    if (screen == SCR_SCORES) return "SCORES";
    return N[state];
}
void calm_test_cell_xy(int r, int c, int *x, int *y) { *x = BX + c * TILE + TILE / 2; *y = BY + r * TILE + TILE / 2; }
#endif

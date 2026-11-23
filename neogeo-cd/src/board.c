/* Calm Combo - board rules. See board.h. C89, integer only. */
#include "board.h"

#define M32(x) ((x) & 0xFFFFFFFFUL)

/* ---------- random (mulberry32, bit-exact with the web prototype) ---------- */

static cc_u32 imul32(cc_u32 a, cc_u32 b)
{
    return M32(a * b);
}

cc_u32 cc_rand32(cc_u32 *state)
{
    cc_u32 t;
    *state = M32(*state + 0x6D2B79F5UL);
    t = *state;
    t = imul32(t ^ (t >> 15), t | 1UL);
    t = M32(t + imul32(t ^ (t >> 7), t | 61UL)) ^ t;
    return M32(t ^ (t >> 14));
}

/* floor(x / 2^32 * n) without 64-bit maths (exact for n < 65536) */
int cc_rand_n(cc_u32 *state, int n)
{
    cc_u32 x = cc_rand32(state);
    cc_u32 hi = x >> 16, lo = x & 0xFFFFUL;
    return (int)((hi * (cc_u32)n + ((lo * (cc_u32)n) >> 16)) >> 16);
}

/* ---------- matching ---------- */

int cc_find_matches(const cc_grid *g, cc_matches *m)
{
    int r, c, e, q, k;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            m->hit[r][c] = 0;
    m->ngroups = 0;
    m->count = 0;

    for (r = 0; r < CC_ROWS; r++) {
        c = 0;
        while (c < CC_COLS) {
            k = g->c[r][c];
            e = c + 1;
            while (e < CC_COLS && g->c[r][e] == k) e++;
            if (k < CC_NC && e - c >= 3) {
                if (m->ngroups < CC_MAX_GROUPS) m->groups[m->ngroups++] = (cc_u8)(e - c);
                for (q = c; q < e; q++) m->hit[r][q] = 1;
            }
            c = e;
        }
    }
    for (c = 0; c < CC_COLS; c++) {
        r = 0;
        while (r < CC_ROWS) {
            k = g->c[r][c];
            e = r + 1;
            while (e < CC_ROWS && g->c[e][c] == k) e++;
            if (k < CC_NC && e - r >= 3) {
                if (m->ngroups < CC_MAX_GROUPS) m->groups[m->ngroups++] = (cc_u8)(e - r);
                for (q = r; q < e; q++) m->hit[q][c] = 1;
            }
            r = e;
        }
    }
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            if (m->hit[r][c]) m->count++;
    return m->count;
}

/* Quick yes/no version for the move search (no bookkeeping). */
static int any_match(const cc_grid *g)
{
    int r, c, k;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c + 2 < CC_COLS; c++) {
            k = g->c[r][c];
            if (k < CC_NC && g->c[r][c + 1] == k && g->c[r][c + 2] == k) return 1;
        }
    for (c = 0; c < CC_COLS; c++)
        for (r = 0; r + 2 < CC_ROWS; r++) {
            k = g->c[r][c];
            if (k < CC_NC && g->c[r + 1][c] == k && g->c[r + 2][c] == k) return 1;
        }
    return 0;
}

/* Take the tile at (r,c) out of its line and re-insert it at index t. */
void cc_apply_move(cc_grid *g, int r, int c, int axis, int t)
{
    int i;
    cc_u8 v;
    if (axis == CC_AXIS_H) {
        v = g->c[r][c];
        if (t > c) for (i = c; i < t; i++) g->c[r][i] = g->c[r][i + 1];
        else       for (i = c; i > t; i--) g->c[r][i] = g->c[r][i - 1];
        g->c[r][t] = v;
    } else {
        v = g->c[r][c];
        if (t > r) for (i = r; i < t; i++) g->c[i][c] = g->c[i + 1][c];
        else       for (i = r; i > t; i--) g->c[i][c] = g->c[i - 1][c];
        g->c[t][c] = v;
    }
}

int cc_has_valid_move(const cc_grid *g)
{
    cc_grid w;
    int r, c, t;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            for (t = 0; t < CC_COLS; t++) {
                if (t == c) continue;
                w = *g; cc_apply_move(&w, r, c, CC_AXIS_H, t);
                if (any_match(&w)) return 1;
            }
            for (t = 0; t < CC_ROWS; t++) {
                if (t == r) continue;
                w = *g; cc_apply_move(&w, r, c, CC_AXIS_V, t);
                if (any_match(&w)) return 1;
            }
        }
    return 0;
}

/* ---------- board ---------- */

static void fill(cc_board *b)
{
    int tries = 0, r, c, k;
    do {
        for (r = 0; r < CC_ROWS; r++)
            for (c = 0; c < CC_COLS; c++) {
                do {
                    k = cc_rand_n(&b->rng, CC_NC);
                } while ((c >= 2 && b->g.c[r][c - 1] == k && b->g.c[r][c - 2] == k) ||
                         (r >= 2 && b->g.c[r - 1][c] == k && b->g.c[r - 2][c] == k));
                b->g.c[r][c] = (cc_u8)k;
            }
    } while (!cc_has_valid_move(&b->g) && ++tries < 50);
}

void cc_board_start(cc_board *b, cc_u32 seed, cc_u32 noise_seed)
{
    b->rng = M32(seed);
    b->rng2 = M32(noise_seed);
    b->score = 0;
    b->moves = CC_START_MOVES;
    b->pending = 0;
    b->chain = 0;
    b->lever_move = 0;
    b->gain = 0;
    b->attack = 0;
    fill(b);
}

int cc_preview(const cc_board *b, int r, int c, int axis, int t, cc_matches *m)
{
    cc_grid w;
    if ((axis == CC_AXIS_H && t == c) || (axis == CC_AXIS_V && t == r)) {
        m->count = 0; m->ngroups = 0;
        return 0;
    }
    w = b->g;
    cc_apply_move(&w, r, c, axis, t);
    return cc_find_matches(&w, m);
}

int cc_commit(cc_board *b, int r, int c, int axis, int t)
{
    cc_matches m;
    int fr, fc;
    if (!cc_preview(b, r, c, axis, t, &m)) return 0;   /* invalid: free */
    cc_apply_move(&b->g, r, c, axis, t);
    fr = axis == CC_AXIS_H ? r : t;
    fc = axis == CC_AXIS_H ? t : c;
    b->lever_move = m.hit[fr][fc] ? 0 : 1;
    b->moves--;
    b->chain = 0;
    b->gain = 0;
    b->attack = 0;
    return 1;
}

int cc_resolve_step(cc_board *b, cc_step *s)
{
    static const signed char DR[4] = { 1, -1, 0, 0 };
    static const signed char DC[4] = { 0, 0, 1, -1 };
    cc_matches m;
    cc_s32 pts = 0, sx = 0, sy = 0, n = 0;
    int i, r, c, d;

    if (!cc_find_matches(&b->g, &m)) return 0;
    b->chain++;

    for (i = 0; i < m.ngroups; i++) {
        int len = m.groups[i];
        pts += len * 10 + (len == 4 ? 30 : len >= 5 ? 80 : 0);
        if (len == 4)      { b->gain += CC_FOUR_MOVES; b->attack += 1; }
        else if (len >= 5) { b->gain += CC_FIVE_MOVES; b->attack += 2; }
    }
    if (b->chain >= 2) { b->gain += CC_CHAIN_MOVES; b->attack += 1; }
    s->lever = (cc_u8)(b->chain == 1 && b->lever_move);
    if (s->lever) { pts += 50; b->attack += 1; }
    pts *= b->chain;
    b->score += pts;

    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            s->dying[r][c] = 0;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            if (m.hit[r][c]) {
                s->dying[r][c] = 1;
                sx += c; sy += r; n++;
                for (d = 0; d < 4; d++) {
                    int r2 = r + DR[d], c2 = c + DC[d];
                    if (r2 >= 0 && r2 < CC_ROWS && c2 >= 0 && c2 < CC_COLS &&
                        b->g.c[r2][c2] == CC_STONE)
                        s->dying[r2][c2] = 1;
                }
            }
    s->points = (cc_u16)(pts > 65535L ? 65535L : pts);
    s->chain = b->chain;
    s->cx = (cc_u8)(sx / n);
    s->cy = (cc_u8)(sy / n);
    return 1;
}

/* Drop survivors, refill each column from the top with the seeded rng. */
void cc_gravity(cc_board *b, const cc_step *s)
{
    int r, c, w, k, n;
    cc_u8 keep[CC_ROWS];
    for (c = 0; c < CC_COLS; c++) {
        n = 0;
        for (r = CC_ROWS - 1; r >= 0; r--)
            if (!s->dying[r][c]) keep[n++] = b->g.c[r][c];
        w = CC_ROWS - 1;
        for (k = 0; k < n; k++) b->g.c[w--][c] = keep[k];
        n = w + 1;
        for (k = 0; k < n; k++) b->g.c[k][c] = (cc_u8)cc_rand_n(&b->rng, CC_NC);
    }
}

int cc_count_stones(const cc_board *b)
{
    int r, c, have = 0;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            if (b->g.c[r][c] == CC_STONE) have++;
    return have;
}

static void shuffle_idx(cc_u32 *st, cc_u8 *a, int len)
{
    int i, j;
    cc_u8 t;
    for (i = len - 1; i > 0; i--) {
        j = cc_rand_n(st, i + 1);
        t = a[i]; a[i] = a[j]; a[j] = t;
    }
}

/* Stones prefer the top three rows, like the prototype. */
void cc_place_stones(cc_board *b, int n)
{
    cc_u8 top[CC_ROWS * CC_COLS], rest[CC_ROWS * CC_COLS];
    int nt = 0, nr = 0, r, c, i, idx;
    int room = CC_MAX_STONES - cc_count_stones(b);
    if (n > room) n = room;
    if (n <= 0) return;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            if (b->g.c[r][c] != CC_STONE) {
                if (r < 3) top[nt++] = (cc_u8)(r * CC_COLS + c);
                else       rest[nr++] = (cc_u8)(r * CC_COLS + c);
            }
    shuffle_idx(&b->rng2, top, nt);
    shuffle_idx(&b->rng2, rest, nr);
    for (i = 0; i < n && i < nt + nr; i++) {
        idx = i < nt ? top[i] : rest[i - nt];
        b->g.c[idx / CC_COLS][idx % CC_COLS] = CC_STONE;
    }
}

void cc_shuffle(cc_board *b)
{
    cc_u8 pos[CC_ROWS * CC_COLS], col[CC_ROWS * CC_COLS];
    int n = 0, r, c, i, tries;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            if (b->g.c[r][c] != CC_STONE) {
                pos[n] = (cc_u8)(r * CC_COLS + c);
                col[n] = b->g.c[r][c];
                n++;
            }
    for (tries = 0; tries < 200; tries++) {
        shuffle_idx(&b->rng2, col, n);
        for (i = 0; i < n; i++) b->g.c[pos[i] / CC_COLS][pos[i] % CC_COLS] = col[i];
        if (!any_match(&b->g) && cc_has_valid_move(&b->g)) return;
    }
    fill(b);
}

int cc_resolve_end(cc_board *b, cc_board *other)
{
    int gain = b->gain;
    b->moves += (cc_s16)gain;
    if (other) {
        int attack = b->attack, cancel;
        if (attack) {
            cancel = attack < b->pending ? attack : b->pending;
            b->pending -= (cc_s16)cancel;
            attack -= cancel;
            other->pending += (cc_s16)attack;
        }
        if (b->pending) {
            cc_place_stones(b, b->pending);
            b->pending = 0;
        }
    }
    b->gain = 0;
    b->attack = 0;
    return gain;
}

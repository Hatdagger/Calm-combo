/*
 * Make and check the fixed boards for the tutorial and Puzzle mode.
 *
 *   gcc -std=c89 -O2 -Isrc -o pgen src/board.c test/puzzle_gen.c
 *   ./pgen > src/puzzles.h            make new boards (takes a few minutes)
 *
 *   gcc -std=c89 -O2 -Isrc -DCHECK -o pcheck src/board.c src/puzzle.c test/puzzle_gen.c
 *   ./pcheck                          check the boards in src/puzzles.h
 *
 * A board is played by brute force: every distinct move, each followed by
 * the full cascade with the seeded refills, exactly like the game.  The
 * check fails unless every board starts with no match and a valid move,
 * can be solved within its moves, and (Puzzle mode) cannot be solved in
 * fewer.  It also prints how many move sequences solve each board.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "board.h"
#include "puzzle.h"

/* ---------- playing a move like the game does ---------- */

typedef struct { cc_u8 r, c, ax, t; } Move;

static int play(cc_board *b, const Move *m, int *lever, int *chain)
{
    cc_step s;
    *lever = 0; *chain = 0;
    if (!cc_commit(b, m->r, m->c, m->ax, m->t)) return 0;
    while (cc_resolve_step(b, &s)) {
        if (s.lever) *lever = 1;
        if (s.chain > *chain) *chain = s.chain;
        cc_gravity(b, &s);
    }
    cc_resolve_end(b, (cc_board *)0);
    return 1;
}

/* every valid move, without two moves that give the same board */
static int moves_of(const cc_board *b, Move *out)
{
    static cc_grid seen[CC_ROWS * CC_COLS * (CC_ROWS + CC_COLS)];
    cc_matches m;
    cc_grid g;
    int n = 0, r, c, ax, t, k, dup;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            for (ax = 0; ax < 2; ax++)
                for (t = 0; t < (ax == CC_AXIS_H ? CC_COLS : CC_ROWS); t++) {
                    if (t == (ax == CC_AXIS_H ? c : r)) continue;
                    if (!cc_preview(b, r, c, ax, t, &m)) continue;
                    g = b->g;
                    cc_apply_move(&g, r, c, ax, t);
                    for (dup = 0, k = 0; k < n && !dup; k++) dup = !memcmp(&seen[k], &g, sizeof g);
                    if (dup) continue;
                    seen[n] = g;
                    out[n].r = (cc_u8)r; out[n].c = (cc_u8)c; out[n].ax = (cc_u8)ax; out[n].t = (cc_u8)t;
                    n++;
                }
    return n;
}

static int goal_met(const cc_board *b, int goal, int lever, int chain)
{
    switch (goal) {
    case PZ_MATCH:  return 1;
    case PZ_LEVER:  return lever;
    case PZ_CHAIN:  return chain >= 2;
    default:        return cc_count_stones(b) == 0;
    }
}

/*
 * Count the move sequences of up to `depth` moves that solve the board (the
 * game stops at the first move that solves it).  *tried counts all
 * sequences looked at, so solved / tried says how rare a solution is.
 */
static long solutions(const cc_board *b, int depth, int goal, long *tried)
{
    Move mv[CC_ROWS * CC_COLS * (CC_ROWS + CC_COLS)];
    cc_board w;
    long found = 0;
    int n, k, lever, chain;
    if (depth <= 0) return 0;
    n = moves_of(b, mv);
    for (k = 0; k < n; k++) {
        w = *b;
        play(&w, &mv[k], &lever, &chain);
        (*tried)++;
        if (goal_met(&w, goal, lever, chain)) found++;
        else found += solutions(&w, depth - 1, goal, tried);
    }
    return found;
}

#ifndef CHECK
/* share of the first moves that are levers / start a chain */
static void first_move_stats(const cc_board *b, int *n, int *levers, int *chains)
{
    Move mv[CC_ROWS * CC_COLS * (CC_ROWS + CC_COLS)];
    cc_board w;
    int k, lever, chain;
    *n = moves_of(b, mv); *levers = 0; *chains = 0;
    for (k = 0; k < *n; k++) {
        w = *b;
        play(&w, &mv[k], &lever, &chain);
        *levers += lever;
        *chains += chain >= 2;
    }
}
#endif

static int starts_clean(const cc_board *b)
{
    cc_matches m;
    return !cc_find_matches(&b->g, &m) && cc_has_valid_move(&b->g);
}

#ifdef CHECK

static int check(const char *what, int i, const cc_puzzle *p, int exact)
{
    cc_board b;
    long tried, s, fewer;
    int k;
    if ((int)strlen(p->grid) != CC_ROWS * CC_COLS) { printf("%s %d: grid is not 56 cells\n", what, i + 1); return 1; }
    for (k = 0; k < PZ_LINES; k++)
        if (p->text[k] && (int)strlen(p->text[k]) > PZ_LINE_LEN) { printf("%s %d: text line %d is too long\n", what, i + 1, k + 1); return 1; }
    cc_puzzle_load(&b, p);
    if (!starts_clean(&b)) { printf("%s %d: starts with a match or with no valid move\n", what, i + 1); return 1; }
    tried = 0;
    s = solutions(&b, p->moves, p->goal, &tried);
    printf("%-8s %2d: %d move%s, %ld of %ld sequences solve it", what, i + 1, p->moves, p->moves == 1 ? " " : "s", s, tried);
    if (!s) { printf("  UNSOLVABLE\n"); return 1; }
    if (exact && p->moves > 1) {
        tried = 0;
        fewer = solutions(&b, p->moves - 1, p->goal, &tried);
        if (fewer) { printf("  but %ld solve it in fewer moves\n", fewer); return 1; }
    }
    printf("\n");
    return 0;
}

int main(void)
{
    int i, bad = 0;
    for (i = 0; i < CC_NUM_TUTORIAL; i++) bad += check("lesson", i, &CC_TUTORIAL[i], 0);
    for (i = 0; i < CC_NUM_PUZZLES; i++) bad += check("puzzle", i, &CC_PUZZLES[i], 1);
    printf(bad ? "%d board(s) failed\n" : "all boards OK\n", bad);
    return bad ? 1 : 0;
}

#else

/* ---------- making boards ---------- */

static cc_u32 gen_rng = 0xC0FFEEUL;
static cc_u32 next_seed(void) { return cc_rand32(&gen_rng) & 0x3FFFFFFFUL; }

typedef struct {
    char grid[CC_ROWS * CC_COLS + 1];
    cc_u32 seed;
    long solved, tried;
} Cand;

/* a random board with `stones` stones; the refill seed is separate */
static void random_board(cc_board *b, int stones, int moves, Cand *out)
{
    int k, r, c, idx;
    do {
        cc_board_start(b, next_seed(), 1);
        for (k = 0; k < stones; ) {
            idx = (int)(cc_rand32(&gen_rng) % (CC_ROWS * CC_COLS));
            r = idx / CC_COLS; c = idx % CC_COLS;
            if (b->g.c[r][c] == CC_STONE) continue;
            b->g.c[r][c] = CC_STONE;
            k++;
        }
    } while (!starts_clean(b));
    out->seed = next_seed();
    b->rng = out->seed;
    b->moves = (cc_s16)moves;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            out->grid[r * CC_COLS + c] = (char)(b->g.c[r][c] == CC_STONE ? '#' : '0' + b->g.c[r][c]);
    out->grid[CC_ROWS * CC_COLS] = 0;
}

static void print_puzzle(const Cand *c, int moves, const char *goal, const char **text)
{
    int k;
    printf("    { \"%s\",\n      0x%08lXUL, %d, %s, {", c->grid, (unsigned long)c->seed, moves, goal);
    for (k = 0; k < PZ_LINES; k++) printf(" \"%s\"%s", text && text[k] ? text[k] : "", k < PZ_LINES - 1 ? "," : "");
    printf(" } },\n");
}

/* tutorial: pick from `tries` boards the one whose first moves fit best */
static void lesson(int goal, int moves, int stones, const char **text)
{
    cc_board b;
    Cand c, best;
    double score, best_score = -1;
    int t, n, lv, ch;
    long tried;
    for (t = 0; t < 400; t++) {
        random_board(&b, stones, moves, &c);
        first_move_stats(&b, &n, &lv, &ch);
        if (n < 4) continue;
        switch (goal) {
        case PZ_MATCH:  score = 1.0 / n; break;                         /* few choices: easy to spot */
        case PZ_LEVER:  score = (double)lv / n; break;                   /* mostly levers */
        case PZ_CHAIN:  score = ch ? 1.0 - (((double)ch / n) - 0.3) * (((double)ch / n) - 0.3) : 0; break;
        default:
            tried = 0;
            c.solved = solutions(&b, 1, goal, &tried);                  /* each stone one move away */
            if (c.solved) continue;                                      /* not both at once */
            tried = 0;
            c.solved = solutions(&b, 2, goal, &tried);
            score = c.solved ? (double)c.solved / tried : 0;
            if (t > 60 && best_score > 0) t = 400;
        }
        if (score > best_score) { best_score = score; best = c; }
    }
    print_puzzle(&best, moves, goal == PZ_MATCH ? "PZ_MATCH" : goal == PZ_LEVER ? "PZ_LEVER" :
                 goal == PZ_CHAIN ? "PZ_CHAIN" : "PZ_STONES", text);
    fprintf(stderr, "lesson goal %d: score %.2f\n", goal, best_score);
}

/* Puzzle mode: needs exactly `moves` moves; `rare` 0..1 picks how hard */
static void puzzle(int num, int stones, int moves, double rare, int want)
{
    static Cand pool[64];
    cc_board b;
    Cand c;
    int n = 0, t, i, j;
    long tried;
    for (t = 0; t < 4000 && n < want; t++) {
        random_board(&b, stones, moves, &c);
        if (moves > 1) {
            tried = 0;
            if (solutions(&b, moves - 1, PZ_STONES, &tried)) continue;
        }
        tried = 0;
        c.solved = solutions(&b, moves, PZ_STONES, &tried);
        c.tried = tried;
        if (!c.solved) continue;
        pool[n++] = c;
    }
    /* sort by how common a solution is, most common first */
    for (i = 1; i < n; i++)
        for (j = i; j > 0 && (double)pool[j].solved / pool[j].tried > (double)pool[j - 1].solved / pool[j - 1].tried; j--) {
            c = pool[j]; pool[j] = pool[j - 1]; pool[j - 1] = c;
        }
    i = (int)(rare * (n - 1) + 0.5);
    {
        static const char *text[PZ_LINES] = { 0 };
        print_puzzle(&pool[i], moves, "PZ_STONES", text);
    }
    fprintf(stderr, "puzzle %2d: %d stones, %d moves, %d candidates, %ld of %ld solve\n",
            num, stones, moves, n, pool[i].solved, pool[i].tried);
}

int main(void)
{
    static const char *L1[PZ_LINES] = { "GRAB A TILE: A", "MOVE TO SLIDE ITS", "ROW OR COLUMN.", "LINE UP 3 OF A", "KIND, THEN DROP", "IT WITH A." };
    static const char *L2[PZ_LINES] = { "ANY MATCH COUNTS!", "THE TILE YOU MOVE", "NEED NOT MATCH:", "PUSH A LINE SO A", "MATCH FORMS", "SOMEWHERE ELSE." };
    static const char *L3[PZ_LINES] = { "CLEARED TILES LET", "THE ONES ABOVE", "FALL. MAKE A", "MATCH THAT DROPS", "TILES INTO A", "SECOND MATCH." };
    static const char *L4[PZ_LINES] = { "STONES NEVER", "MATCH. CLEAR A", "TILE NEXT TO ONE", "TO BREAK IT.", "BREAK BOTH", "STONES." };
    static const int ST[12] = { 2, 2, 3, 3, 4, 3, 4, 5, 4, 5, 6, 6 };
    static const int MV[12] = { 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3 };
    int i;

    printf("/* Generated by test/puzzle_gen.c: do not edit by hand.\n");
    printf(" * Grids are row by row: 0-5 tile colours, # stone. */\n\n");
    printf("const cc_puzzle CC_TUTORIAL[] = {\n");
    lesson(PZ_MATCH, 1, 0, L1);
    lesson(PZ_LEVER, 1, 0, L2);
    lesson(PZ_CHAIN, 3, 0, L3);
    lesson(PZ_STONES, 3, 2, L4);
    printf("};\nconst int CC_NUM_TUTORIAL = 4;\n\n");
    printf("const cc_puzzle CC_PUZZLES[] = {\n");
    for (i = 0; i < 12; i++)
        puzzle(i + 1, ST[i], MV[i], 0.15 + 0.85 * i / 11.0, MV[i] == 3 ? 16 : 40);
    printf("};\nconst int CC_NUM_PUZZLES = 12;\n");
    return 0;
}

#endif

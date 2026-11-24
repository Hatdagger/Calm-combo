/*
 * Calm Combo - board rules (portable C89, no floats, no malloc).
 *
 * This is the game logic only: matching, slide moves, cascades, survival
 * moves and versus stones.  Drawing, timing and sound live elsewhere, so the
 * same file builds for Neo Geo CD (NeoDev GCC 2.95 or ngdevkit GCC 11) and
 * for a PC test harness.
 *
 * Rules mirror web/index.html, which stays the reference implementation.
 */
#ifndef CC_BOARD_H
#define CC_BOARD_H

typedef unsigned char  cc_u8;
typedef signed char    cc_s8;
typedef unsigned short cc_u16;
typedef short          cc_s16;
typedef unsigned long  cc_u32;   /* at least 32 bits on every target */
typedef long           cc_s32;

#define CC_COLS   8
#define CC_ROWS   7
#define CC_NC     6           /* tile colours 0..5 */
#define CC_STONE  6           /* stone: never matches */

/* tuning knobs (prototype values) */
#define CC_START_MOVES  15
#define CC_CHAIN_MOVES  1
#define CC_FOUR_MOVES   1
#define CC_FIVE_MOVES   2
#define CC_MAX_STONES   20

#define CC_AXIS_H 0
#define CC_AXIS_V 1

#define CC_MAX_GROUPS 32

typedef struct {
    cc_u8 c[CC_ROWS][CC_COLS];
} cc_grid;

typedef struct {
    cc_u8 hit[CC_ROWS][CC_COLS];
    cc_u8 groups[CC_MAX_GROUPS];   /* length of each 3+ run */
    cc_u8 ngroups;
    cc_u8 count;                   /* number of tiles cleared */
} cc_matches;

/* Result of one cascade step, for the front end to animate. */
typedef struct {
    cc_u16 points;        /* already multiplied by chain */
    cc_u8  chain;         /* 1 for the first clear */
    cc_u8  lever;         /* 1 if this step paid the LEVER bonus */
    cc_u8  dying[CC_ROWS][CC_COLS]; /* tiles (and broken stones) to pop */
    cc_u8  cx, cy;        /* centre of the clear, in tiles */
} cc_step;

typedef struct {
    cc_grid g;
    cc_u32  rng;          /* seeded: drives fill + refills (identical in VS) */
    cc_u32  rng2;         /* unseeded: stones + shuffles                     */
    cc_s32  score;
    cc_s16  moves;
    cc_s16  pending;      /* incoming stones */
    /* resolve bookkeeping */
    cc_u8   chain;
    cc_u8   lever_move;
    cc_s16  gain;
    cc_s16  attack;
} cc_board;

/* random */
cc_u32 cc_rand32(cc_u32 *state);              /* mulberry32 step  */
int    cc_rand_n(cc_u32 *state, int n);       /* floor(r() * n)   */

/* pure grid helpers */
int  cc_find_matches(const cc_grid *g, cc_matches *m);
void cc_apply_move(cc_grid *g, int r, int c, int axis, int t);
int  cc_has_valid_move(const cc_grid *g);

/* board life cycle */
void cc_board_start(cc_board *b, cc_u32 seed, cc_u32 noise_seed);

/*
 * Moves.  cc_preview() fills m with what the drop would clear (count 0 means
 * invalid, which costs nothing).  cc_commit() performs a valid move, spends a
 * move and arms the resolve loop.
 */
int  cc_preview(const cc_board *b, int r, int c, int axis, int t, cc_matches *m);
int  cc_commit(cc_board *b, int r, int c, int axis, int t);

/*
 * Resolve loop, driven by the front end so it can animate between steps:
 *   while (cc_resolve_step(b, &s)) { animate s.dying; cc_gravity(b); animate fall; }
 *   cc_resolve_end(b, other_or_NULL);
 */
int  cc_resolve_step(cc_board *b, cc_step *s);
void cc_gravity(cc_board *b, const cc_step *s);
/* returns gained moves; in versus pass the opponent so stones are sent */
int  cc_resolve_end(cc_board *b, cc_board *versus_other);

void cc_place_stones(cc_board *b, int n);
void cc_shuffle(cc_board *b);
int  cc_count_stones(const cc_board *b);

#endif

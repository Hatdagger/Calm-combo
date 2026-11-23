/* PC harness: plays scripted games and prints a trace for parity checks. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/board.h"

static int first_move(const cc_board *b, int *R, int *C, int *A, int *T)
{
    cc_matches m; int r, c, t;
    for (r = 0; r < CC_ROWS; r++) for (c = 0; c < CC_COLS; c++) {
        for (t = 0; t < CC_COLS; t++) if (t != c && cc_preview(b, r, c, CC_AXIS_H, t, &m)) { *R=r;*C=c;*A=0;*T=t; return 1; }
        for (t = 0; t < CC_ROWS; t++) if (t != r && cc_preview(b, r, c, CC_AXIS_V, t, &m)) { *R=r;*C=c;*A=1;*T=t; return 1; }
    }
    return 0;
}
static void dump(const cc_board *b)
{
    int r, c;
    for (r = 0; r < CC_ROWS; r++) for (c = 0; c < CC_COLS; c++) putchar('0' + b->g.c[r][c]);
    printf(" %ld %d\n", (long)b->score, b->moves);
}
int main(int argc, char **argv)
{
    cc_board b; cc_step s;
    cc_u32 seed = argc > 1 ? strtoul(argv[1], 0, 10) : 1;
    int turns = argc > 2 ? atoi(argv[2]) : 30, i, r, c, a, t;
    cc_board_start(&b, seed, 777);
    dump(&b);
    for (i = 0; i < turns && b.moves > 0; i++) {
        if (!first_move(&b, &r, &c, &a, &t)) { printf("nomove\n"); break; }
        cc_commit(&b, r, c, a, t);
        while (cc_resolve_step(&b, &s)) cc_gravity(&b, &s);
        cc_resolve_end(&b, NULL);
        dump(&b);
        if (!cc_has_valid_move(&b.g)) { printf("shuffle\n"); break; }
    }
    return 0;
}

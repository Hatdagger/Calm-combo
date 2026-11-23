/* Calm Combo - fixed boards. See puzzle.h. C89, integer only. */
#include "puzzle.h"
#include "puzzles.h"

void cc_puzzle_load(cc_board *b, const cc_puzzle *p)
{
    int r, c;
    char ch;
    cc_board_start(b, p->seed, p->seed ^ 0x5BD1E995UL);
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++) {
            ch = p->grid[r * CC_COLS + c];
            b->g.c[r][c] = (cc_u8)(ch == '#' ? CC_STONE : ch - '0');
        }
    b->rng = p->seed;              /* refills start from the seed every try */
    b->moves = p->moves;
}

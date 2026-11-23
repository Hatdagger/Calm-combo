/*
 * Calm Combo - fixed boards for the tutorial and Puzzle mode (portable C89).
 *
 * A puzzle is a whole starting board plus the seed for its refills, so every
 * try plays out exactly the same way.  The boards are made and checked by
 * test/puzzle_gen.c (every one is solvable in its move count, and Puzzle
 * mode boards cannot be solved in fewer); the data is in puzzles.h.
 */
#ifndef CC_PUZZLE_H
#define CC_PUZZLE_H

#include "board.h"

/* what solves the board */
enum { PZ_MATCH, PZ_LEVER, PZ_CHAIN, PZ_STONES };

#define PZ_LINES 6
#define PZ_LINE_LEN 18

typedef struct {
    const char *grid;          /* 56 chars, row by row: '0'..'5' colours, '#' stone */
    cc_u32 seed;               /* drives the refills */
    cc_u8  moves, goal;
    const char *text[PZ_LINES];/* instructions shown beside the board (tutorial) */
} cc_puzzle;

extern const cc_puzzle CC_TUTORIAL[];
extern const cc_puzzle CC_PUZZLES[];
extern const int CC_NUM_TUTORIAL;
extern const int CC_NUM_PUZZLES;

/* set the board up for a puzzle (score 0, moves = the puzzle's moves) */
void cc_puzzle_load(cc_board *b, const cc_puzzle *p);

#endif

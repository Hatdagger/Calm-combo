/*
 * Calm Combo - high scores and puzzle progress, kept in the Neo Geo CD's
 * backup memory (its built-in "memory card") through the BIOS CARD call.
 */
#ifndef CC_SAVE_H
#define CC_SAVE_H

#define HS_N 5                        /* high-score table size */

extern char  hs_name[HS_N][4];        /* 3 letters + 0 */
extern DWORD hs_score[HS_N];
extern WORD  pz_solved;               /* bit n: puzzle n+1 solved */
extern BYTE  save_answer;             /* last BIOS answer: 0 fine, 0x80+ error */

void save_load(void);                 /* at boot: read, or start with defaults */
void save_store(void);                /* write everything now */
int  hs_rank(DWORD score);            /* table position 0..4 it would take, or -1 */
void hs_insert(int rank, const char *name, DWORD score);

#endif

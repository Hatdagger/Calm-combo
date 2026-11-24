/*
 * Calm Combo - portrait single-board front end (Android, and a PC test harness).
 *
 * The game rules are the Neo Geo CD's own files (neogeo-cd/src/board.c,
 * puzzle.c, puzzles.h), built unchanged.  This front end draws everything
 * into a small pixel buffer at a "logical" resolution about 180 px wide; the
 * platform scales it up by a whole number so the pixel art stays sharp.
 *
 * Pixels are 32-bit, laid out R, G, B, A in memory (Android's ARGB_8888).
 */
#ifndef CALM_H
#define CALM_H

#include <stdint.h>

/* buttons from a controller or keyboard */
enum {
    CK_UP, CK_DOWN, CK_LEFT, CK_RIGHT,
    CK_A,        /* grab / drop / confirm  */
    CK_B,        /* cancel / back          */
    CK_START,    /* start / retry          */
    CK_SELECT,   /* menu                   */
    CK_X,        /* next scene             */
    CK_Y,        /* music on / off         */
    CK_BACK,     /* Android back           */
    CK_COUNT
};

/* touch actions */
enum { CT_DOWN, CT_MOVE, CT_UP, CT_CANCEL };

/* logical size for a screen of sw x sh real pixels: the whole-number scale */
int  calm_pick_scale(int sw, int sh);

/* pak: the art (made by android/tools/make_pak.py), kept alive by the caller */
int  calm_init(const void *pak, long pak_len, const void *save, int save_len);
void calm_resize(int w, int h);          /* logical pixels */
void calm_tick(void);                    /* one 1/60 s step */
void calm_render(uint32_t *px, int stride_px);

void calm_touch(int action, int x, int y);   /* logical pixels */
void calm_key(int key, int down);

int  calm_pop_sound(void);               /* next sound effect id (sound_ids.h), -1 none */
int  calm_music(void);                   /* song wanted: 0 none, 1 lake, 2 beach, 3 rain */
int  calm_save(unsigned char *buf, int max);  /* > 0: the save changed, buf holds it */
int  calm_quit(void);                    /* back pressed on the title: leave */

/*
 * The phone's own music (Android): a bar under the board with previous,
 * play/pause, next and one line for the song.  The game only shows it and
 * reports taps; the app talks to whatever player is running.
 */
enum { CM_PLAYING = 1, CM_TITLES = 2, CM_SESSION = 4 };     /* calm_media flags */
enum { CMA_PREV, CMA_PLAY, CMA_NEXT, CMA_ASK };             /* calm_pop_media actions */
void calm_media(int flags, const char *title);  /* title: plain ASCII; the first call shows the bar */
int  calm_pop_media(void);               /* next tap on the bar, -1 none */

#define CALM_SAVE_MAX 64

#ifdef CALM_TEST
/* test harness helpers */
int  calm_test_find_move(int *r, int *c, int *axis, int *t);
void calm_test_set(int score, int moves);
void calm_test_clock(long seconds);
const char *calm_test_state(void);
void calm_test_cell_xy(int r, int c, int *x, int *y);
#endif

#endif

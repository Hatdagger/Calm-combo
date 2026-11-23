/*
 * Calm Combo - scenery (Neo Geo CD).
 *
 * Three scenes rotate every 5 minutes: mountain lake, beach under an
 * umbrella, rain on a window.  A full day/night cycle takes 10 minutes and
 * is done with palettes only.  Scene changes fade through black.
 */
#ifndef CC_SCENE_H
#define CC_SCENE_H

#define SCENE_FRAMES (300L * 60L)     /* 5 minutes per scene  */
#define DAY_FRAMES   (600L * 60L)     /* 10 minute day        */
#define NUM_SCENES   3

enum { SCENE_LAKE, SCENE_BEACH, SCENE_WINDOW };

/* which scene the clock asks for */
#define scene_of(t) ((BYTE)(((t) / SCENE_FRAMES) % NUM_SCENES))

void scene_boot(void);            /* once, before the main loop          */
void scene_tick(DWORD frames);    /* every frame, before the board draws */
BYTE scene_shown(void);           /* the scene on screen right now       */

#endif

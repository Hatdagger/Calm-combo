/* Calm Combo - portrait scenery (see scenery.c). */
#ifndef CALM_SCENERY_H
#define CALM_SCENERY_H

#define SCENE_FRAMES (300L * 60L)     /* 5 minutes per scene  */
#define DAY_FRAMES   (600L * 60L)     /* 10 minute day        */
#define NUM_SCENES   3
enum { SCENE_LAKE, SCENE_BEACH, SCENE_WINDOW };

void scenery_tick(long frames, int h); /* every 1/60 s: fades, raindrops */
void scenery_draw(long frames);       /* the whole background */
int  scenery_shown(void);             /* scene on screen now */

#endif

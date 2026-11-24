/*
 * Calm Combo (Android) on a PC: plays a script against the real front end
 * and saves screenshots, so the game can be checked without a phone.
 *
 *   sh android/test/build_pc.sh && build-pc/calm-pc calm.pak android/test/tour.txt shots
 *
 * Script lines:
 *   size W H            logical screen size (default 180 360)
 *   wait N              N ticks (1/60 s)
 *   tap X Y             touch down and up at a logical point
 *   button LABEL        not used; tap by position instead
 *   drag R C R2 C2      drag the tile at row R, column C to (R2, C2)
 *   auto                make some valid move by dragging
 *   key NAME            press and release a button (UP DOWN LEFT RIGHT A B START SELECT X Y BACK)
 *   idle                wait until the board is ready for a move
 *   state NAME          fail unless the game is in that state
 *   set SCORE MOVES     force the score and moves (high-score test)
 *   clock SECONDS       set the clock (scene, time of day)
 *   dump NAME           save OUT/NAME.ppm
 *   end
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "calm.h"

static int W = 180, H = 360;
static uint32_t *px;
static int fails, sounds[32];

static void tick(int n)
{
    int k, s;
    for (k = 0; k < n; k++) {
        calm_tick();
        while ((s = calm_pop_sound()) >= 0) if (s < 32) sounds[s]++;
    }
}

static void dump(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    int k;
    calm_render(px, W);
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    f = fopen(path, "wb");
    if (!f) { perror(path); exit(1); }
    fprintf(f, "P6 %d %d 255\n", W, H);
    for (k = 0; k < W * H; k++) {
        unsigned char c[3] = { (unsigned char)(px[k] & 255), (unsigned char)((px[k] >> 8) & 255), (unsigned char)((px[k] >> 16) & 255) };
        fwrite(c, 1, 3, f);
    }
    fclose(f);
    printf("dump %s (%s)\n", name, calm_test_state());
}

static void touch_drag(int x0, int y0, int x1, int y1)
{
    int k, n = 8;
    calm_touch(CT_DOWN, x0, y0); tick(1);
    for (k = 1; k <= n; k++) { calm_touch(CT_MOVE, x0 + (x1 - x0) * k / n, y0 + (y1 - y0) * k / n); tick(1); }
    calm_touch(CT_UP, x1, y1); tick(1);
}

static int key_of(const char *s)
{
    static const char *N[CK_COUNT] = { "UP", "DOWN", "LEFT", "RIGHT", "A", "B", "START", "SELECT", "X", "Y", "BACK" };
    int k;
    for (k = 0; k < CK_COUNT; k++) if (!strcmp(N[k], s)) return k;
    fprintf(stderr, "unknown key %s\n", s);
    exit(1);
}

int main(int argc, char **argv)
{
    FILE *f;
    long len;
    char *pak, line[256];
    unsigned char save[CALM_SAVE_MAX];
    if (argc < 4) { fprintf(stderr, "usage: calm-pc PAK SCRIPT OUTDIR\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END); len = ftell(f); fseek(f, 0, SEEK_SET);
    pak = malloc((size_t)len);
    if (fread(pak, 1, (size_t)len, f) != (size_t)len) return 1;
    fclose(f);
    if (calm_init(pak, len, 0, 0) != 0) { fprintf(stderr, "init failed\n"); return 1; }
    px = malloc(sizeof(uint32_t) * 720 * 400);
    calm_resize(W, H);
    f = fopen(argv[2], "r");
    if (!f) { perror(argv[2]); return 1; }
    while (fgets(line, sizeof line, f)) {
        char a[64], b2[64];
        int n1, n2, n3, n4;
        if (line[0] == '#' || line[0] == '\n') continue;
        if (sscanf(line, "size %d %d", &n1, &n2) == 2) { W = n1; H = n2; calm_resize(W, H); }
        else if (sscanf(line, "wait %d", &n1) == 1) tick(n1);
        else if (sscanf(line, "tap %d %d", &n1, &n2) == 2) { calm_render(px, W); calm_touch(CT_DOWN, n1, n2); tick(1); calm_touch(CT_UP, n1, n2); tick(1); }
        else if (sscanf(line, "drag %d %d %d %d", &n1, &n2, &n3, &n4) == 4) {
            int x0, y0, x1, y1;
            calm_test_cell_xy(n1, n2, &x0, &y0); calm_test_cell_xy(n3, n4, &x1, &y1);
            touch_drag(x0, y0, x1, y1);
        }
        else if (!strncmp(line, "auto", 4)) {
            int r, c, ax, t, x0, y0, x1, y1;
            if (!calm_test_find_move(&r, &c, &ax, &t)) { printf("auto: no move\n"); continue; }
            calm_test_cell_xy(r, c, &x0, &y0);
            calm_test_cell_xy(ax ? t : r, ax ? c : t, &x1, &y1);
            touch_drag(x0, y0, x1, y1);
        }
        else if (sscanf(line, "key %63s", a) == 1) { calm_key(key_of(a), 1); tick(1); calm_key(key_of(a), 0); tick(1); }
        else if (!strncmp(line, "idle", 4)) {
            int k = 0;
            while (strcmp(calm_test_state(), "IDLE") && k++ < 900) tick(1);
            tick(20);
        }
        else if (sscanf(line, "state %63s", a) == 1) {
            if (strcmp(a, calm_test_state())) { printf("FAIL: state %s, wanted %s\n", calm_test_state(), a); fails++; }
        }
        else if (sscanf(line, "set %d %d", &n1, &n2) == 2) calm_test_set(n1, n2);
        else if (sscanf(line, "clock %d", &n1) == 1) calm_test_clock(n1);
        else if (sscanf(line, "dump %63s", b2) == 1) dump(argv[3], b2);
        else if (!strncmp(line, "end", 3)) break;
        else { fprintf(stderr, "bad line: %s", line); return 1; }
    }
    fclose(f);
    if (calm_save(save, sizeof save) > 0) printf("save written\n");
    printf("sounds:");
    { int k; for (k = 0; k < 32; k++) if (sounds[k]) printf(" %d x%d", k, sounds[k]); }
    printf("\n");
    if (fails) { printf("%d check(s) failed\n", fails); return 1; }
    return 0;
}

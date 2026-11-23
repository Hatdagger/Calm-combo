/*
 * PC harness for the Neo Geo CD front end (src/main.c).
 *
 * main.c is compiled against the stand-in headers in test/stubs.  Sprite
 * writes land in a fake VRAM, fix text in a fake fix map.  A script feeds
 * joypad input frame by frame and asks for snapshots, which
 * test/render_frames.py turns into PNG screenshots using the real art.
 *
 *   sh test/build_frontend.sh            (builds build-pc/frontend)
 *   build-pc/frontend test/tour.txt shots/
 *   python3 test/render_frames.py shots/
 *
 * Script commands, one per line (# starts a comment):
 *   wait N             let N frames pass
 *   p1 KEY / p2 KEY    press a button for one frame, then release for one
 *                      (U D L R A B C D, START, SELECT)
 *   auto P [NAME]      player P (1 or 2) makes a valid move; with NAME, take
 *                      a snapshot while the tile is held, just before the drop
 *   idle P             wait until player P's board has settled
 *   dump NAME          snapshot to OUTDIR/NAME.txt
 *   end
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stubs/neocore.h"
#include "stubs/DATlib.h"

/* the game itself, with its main() renamed so the harness drives it */
#define main game_main
#include "main.c"
#undef main

unsigned char stub_in[8];
ushort LSPCmode;

/* ---- fake art: tile word = picture id << 10 | tile index in the picture ---- */
#define PIC_LOGO   1
#define PIC_TILES  2
#define PIC_CURSOR 3
#define PIC_PANEL  4

static ushort map_logo[10 * 2 * 2], map_tiles[4 * 7 * 2], map_cursor[4 * 2 * 2], map_panel[9 * 8 * 2];

const paletteInfo cc_logo_Palettes   = { 1, { 0 } };
const paletteInfo cc_tiles_Palettes  = { 2, { 0 } };
const paletteInfo cc_cursor_Palettes = { 1, { 0 } };
const paletteInfo cc_panel_Palettes  = { 1, { 0 } };
/* the font converter's palette: transparent, then main / shade / outline grey */
const paletteInfo cc_font_Palettes   = { 1, { 0x8000, 0x7fff, 0x7aaa, 0x7222,
                                              0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
                                              0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000 } };

const pictureInfo cc_logo   = { 8,  10, 2, &cc_logo_Palettes,   { map_logo, map_logo, map_logo, map_logo } };
const pictureInfo cc_tiles  = { 28, 4,  7, &cc_tiles_Palettes,  { map_tiles, map_tiles, map_tiles, map_tiles } };
const pictureInfo cc_cursor = { 8,  4,  2, &cc_cursor_Palettes, { map_cursor, map_cursor, map_cursor, map_cursor } };
const pictureInfo cc_panel  = { 32, 9,  8, &cc_panel_Palettes,  { map_panel, map_panel, map_panel, map_panel } };

static void fill_map(ushort *m, int id, int n)
{
    int k;
    for (k = 0; k < n; k++) { m[k * 2] = (ushort)((id << 10) | k); m[k * 2 + 1] = 0; }
}

/* ---- fake hardware ---- */
static WORD vram[0x10000];
static WORD pal_ram[256 * 16];
static WORD bg;
static unsigned char fix_ch[40][32], fix_pal[40][32];
static picture *pics[8];
static int npics;
static long frame;
static FILE *script;
static const char *outdir;
static long sc234_this_frame, sc234_max;

void stub_bg(WORD c) { bg = c; }
void stub_pal(int n, int c, const void *d) { memcpy(&pal_ram[n * 16], d, (size_t)c * 32); }
void stub_vram(WORD a, WORD d) { vram[a] = d; sc234_this_frame++; }

void initGfx(void)
{
    fill_map(map_logo, PIC_LOGO, 20);
    fill_map(map_tiles, PIC_TILES, 28);
    fill_map(map_cursor, PIC_CURSOR, 8);
    fill_map(map_panel, PIC_PANEL, 72);
}

void clearFixLayer(void) { memset(fix_ch, ' ', sizeof fix_ch); memset(fix_pal, 0, sizeof fix_pal); }

void fixPrint(ushort x, ushort y, ushort pal, ushort bank, const char *buf)
{
    (void)bank;
    while (*buf && x < 40) { fix_ch[x][y] = (unsigned char)*buf++; fix_pal[x][y] = (unsigned char)pal; x++; }
}

void pictureInit(picture *p, const pictureInfo *pi, ushort bs, ushort bp, short x, short y, ushort f)
{
    p->baseSprite = bs; p->basePalette = bp; p->posX = x; p->posY = y; p->currentFlip = f;
    p->info = pi; p->shown = 1;
    if (npics < 8) pics[npics++] = p;
}
void pictureHide(picture *p) { p->shown = 0; }
void pictureShow(picture *p) { p->shown = 1; }

void SCClose(void) {}

static void dump(const char *name)
{
    char path[512];
    FILE *f;
    int s, x, y, k;
    sprintf(path, "%s/%s.txt", outdir, name);
    f = fopen(path, "w");
    if (!f) { perror(path); exit(2); }
    fprintf(f, "frame %ld\nbg %04x\n", frame, bg);
    for (k = 0; k < 256 * 16; k += 16)
        if (pal_ram[k + 1] || pal_ram[k + 2]) {
            fprintf(f, "pal %d", k / 16);
            for (x = 0; x < 16; x++) fprintf(f, " %04x", pal_ram[k + x]);
            fprintf(f, "\n");
        }
    for (k = 0; k < npics; k++) {
        const pictureInfo *pi = pics[k]->info;
        int id = pi == &cc_logo ? PIC_LOGO : pi == &cc_panel ? PIC_PANEL : 0;
        if (pics[k]->shown)
            fprintf(f, "pict %d %d %d %d %d %d\n", id, pics[k]->baseSprite, pics[k]->posX, pics[k]->posY,
                    pi->tileWidth, pics[k]->basePalette);
    }
    for (s = 1; s < 381; s++) {
        WORD yw = vram[0x8200 | s];
        if (!(yw & 0x3f)) continue;
        y = YSHIFT - (yw >> 7);
        x = vram[0x8400 | s] >> 7;
        fprintf(f, "spr %d %d %d %04x %04x\n", s, x, y, vram[s << 6], vram[(s << 6) + 1]);
    }
    for (y = 2; y < 30; y++) {
        fprintf(f, "fix ");
        for (x = 0; x < 40; x++) fputc(fix_ch[x][y], f);
        fprintf(f, "|");
        for (x = 0; x < 40; x++) fprintf(f, "%x", fix_pal[x][y] & 15);
        fprintf(f, "\n");
    }
    fclose(f);
}

/* ---- script ---- */
typedef struct { BYTE in[5]; char shot[64]; } Step;
static Step queue[256];
static int qhead, qlen, wait_n, idle_p = -1, idle_guard;

static void push(int pl, BYTE edge, BYTE sys, const char *shot)
{
    Step *q;
    if (qlen >= 255) return;
    q = &queue[(qhead + qlen++) & 255];
    memset(q, 0, sizeof *q);
    if (pl == 1) { q->in[0] = edge; q->in[1] = edge; }
    if (pl == 2) { q->in[2] = edge; q->in[3] = edge; }
    q->in[4] = sys;
    if (shot) strncpy(q->shot, shot, sizeof q->shot - 1);
    q = &queue[(qhead + qlen++) & 255];
    memset(q, 0, sizeof *q);
}

static BYTE key(const char *k, BYTE *sys, int pl)
{
    *sys = 0;
    if (!strcmp(k, "U")) return JOY_UP;
    if (!strcmp(k, "D")) return JOY_DOWN;
    if (!strcmp(k, "L")) return JOY_LEFT;
    if (!strcmp(k, "R")) return JOY_RIGHT;
    if (!strcmp(k, "A")) return JOY_A;
    if (!strcmp(k, "B")) return JOY_B;
    if (!strcmp(k, "C")) return JOY_C;
    if (!strcmp(k, "D2")) return JOY_D;
    if (!strcmp(k, "START")) { *sys = (BYTE)(pl == 1 ? P1_START : P2_START); return 0; }
    if (!strcmp(k, "SELECT")) { *sys = (BYTE)(pl == 1 ? P1_SELECT : P2_SELECT); return 0; }
    fprintf(stderr, "unknown key %s\n", k); exit(2);
    return 0;
}

/* find a valid move for player pl and queue the button presses for it */
static void auto_move(int pl, const char *shot)
{
    Side *s = &side[pl - 1];
    cc_matches m;
    int r, c, ax, t, k, cr = s->cr, cc = s->cc;
    for (r = 0; r < CC_ROWS; r++)
        for (c = 0; c < CC_COLS; c++)
            for (ax = 0; ax < 2; ax++)
                for (t = 0; t < (ax == CC_AXIS_H ? CC_COLS : CC_ROWS); t++) {
                    if (t == (ax == CC_AXIS_H ? c : r)) continue;
                    if (!cc_preview(&s->b, r, c, ax, t, &m)) continue;
                    while (cr < r) { push(pl, JOY_DOWN, 0, 0); cr++; }
                    while (cr > r) { push(pl, JOY_UP, 0, 0); cr--; }
                    while (cc < c) { push(pl, JOY_RIGHT, 0, 0); cc++; }
                    while (cc > c) { push(pl, JOY_LEFT, 0, 0); cc--; }
                    push(pl, JOY_A, 0, 0);
                    k = ax == CC_AXIS_H ? c : r;
                    while (k != t) {
                        push(pl, (BYTE)(ax == CC_AXIS_H ? (t > k ? JOY_RIGHT : JOY_LEFT) : (t > k ? JOY_DOWN : JOY_UP)), 0, 0);
                        k += t > k ? 1 : -1;
                    }
                    for (k = 0; k < 6; k++) push(pl, 0, 0, 0);      /* let the slide glide */
                    push(pl, JOY_A, 0, shot);
                    return;
                }
    fprintf(stderr, "frame %ld: no valid move for player %d\n", frame, pl);
    exit(2);
}

/* next frame: feed this frame's input */
void waitVBlank(void)
{
    char line[256], cmd[32], arg[200];
    int n, pl;
    BYTE sys, k;
    Step *q;

    if (sc234_this_frame > sc234_max) sc234_max = sc234_this_frame;
    if (sc234_this_frame > 1140) { fprintf(stderr, "frame %ld: %ld SC234 writes overflow DATlib's list\n", frame, sc234_this_frame); exit(3); }
    sc234_this_frame = 0;
    frame++;
    memset(stub_in, 0, sizeof stub_in);

    for (;;) {
        if (wait_n > 0) { wait_n--; return; }
        if (idle_p >= 0) {
            if ((side[idle_p].state == ST_IDLE && !side[idle_p].moving) || ++idle_guard > 900) { idle_p = -1; continue; }
            return;
        }
        if (qlen) {
            q = &queue[qhead];
            if (q->shot[0]) dump(q->shot);
            memcpy(stub_in, q->in, 5);
            qhead = (qhead + 1) & 255; qlen--;
            return;
        }
        if (!fgets(line, sizeof line, script)) { fprintf(stderr, "script ended without 'end'\n"); exit(2); }
        arg[0] = 0;
        n = sscanf(line, "%31s %199s", cmd, arg);
        if (n < 1 || cmd[0] == '#') continue;
        if (!strcmp(cmd, "wait")) wait_n = atoi(arg);
        else if (!strcmp(cmd, "dump")) dump(arg);
        else if (!strcmp(cmd, "idle")) { idle_p = atoi(arg) - 1; idle_guard = 0; }
        else if (!strcmp(cmd, "p1") || !strcmp(cmd, "p2")) {
            pl = cmd[1] - '0';
            k = key(arg, &sys, pl);
            push(pl, k, sys, 0);
        } else if (!strcmp(cmd, "auto")) {
            char shot[64] = "";
            sscanf(line, "%*s %d %63s", &pl, shot);
            auto_move(pl, shot[0] ? shot : 0);
        } else if (!strcmp(cmd, "end")) {
            printf("ran %ld frames, peak %ld sprite writes per frame\n", frame, sc234_max);
            exit(0);
        } else { fprintf(stderr, "bad script line: %s", line); exit(2); }
    }
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: frontend SCRIPT OUTDIR\n"); return 1; }
    script = fopen(argv[1], "r");
    if (!script) { perror(argv[1]); return 1; }
    outdir = argv[2];
    clearFixLayer();
    return game_main();
}

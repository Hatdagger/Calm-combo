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
 *   clock S            set the game clock to S seconds (scene = S / 300 % 3,
 *                      time of day = S % 600: sun up for 0-299, moon after)
 *   sounds NAME...     fail unless each named effect (or "cdda") has played
 *   end
 *
 * Every sound command and CD track change is logged to OUTDIR/sounds.txt.
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
#include "scene.c"

/* sound.c, with the Z80 port and the CD BIOS calls recorded instead */
static void stub_snd(BYTE cmd);
static void stub_cdda(int op, BYTE track);
#define SND_WRITE(cmd) stub_snd(cmd)
#define CDDA_PLAY(t)   stub_cdda(0, (t))
#define CDDA_PAUSE()   stub_cdda(1, 0)
#define CDDA_RESUME()  stub_cdda(2, 0)
#include "sound.c"

unsigned char stub_in[8];
unsigned int DAT_droppedFrames;
ushort LSPCmode;

/* ---- fake art: tile word = picture id << 10 | tile index in the picture ----
 * id, name, width and height in 16 px tiles (whole strip), palettes.
 * test/render_frames.py has the same list with the PNG files. */
#define ART_LIST \
    X(1,  cc_logo,       10,  2, 1) X(2,  cc_tiles,       4,  7, 2) \
    X(3,  cc_cursor,      4,  2, 1) X(4,  cc_panel,       9,  8, 1) \
    X(5,  cc_sun,         2,  2, 1) X(6,  cc_moon,        2,  2, 1) \
    X(7,  cc_lake_sky,   20, 10, 1) X(8,  cc_ridge_mid,  32,  4, 1) \
    X(9,  cc_ridge_near, 32,  3, 1) X(10, cc_water,      80,  4, 1) \
    X(11, cc_firefly,     4,  1, 1) X(12, cc_sky_sea,    20, 12, 1) \
    X(13, cc_glitter,    16,  1, 1) X(14, cc_shore,     160,  3, 1) \
    X(15, cc_palm,       20, 12, 1) X(16, cc_umbrella,   40,  3, 1) \
    X(17, cc_city,       20,  8, 1) X(18, cc_rain,       16,  4, 1) \
    X(19, cc_droplet,     4,  2, 1) X(20, cc_frame,      20, 14, 1) \
    X(21, cc_steam,       4,  2, 1)

#define X(id, name, w, h, np) \
    static ushort name##_map[(w) * (h) * 2]; \
    const paletteInfo name##_Palettes = { np, { 0 } }; \
    const pictureInfo name = { (h) * 4, w, h, &name##_Palettes, { name##_map, name##_map, name##_map, name##_map } };
ART_LIST
#undef X

/* the font converter's palette: transparent, then main / shade / outline grey */
const paletteInfo cc_font_Palettes   = { 1, { 0x8000, 0x7fff, 0x7aaa, 0x7222,
                                              0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000,
                                              0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000 } };

static void fill_map(ushort *m, int id, int n)
{
    int k;
    for (k = 0; k < n; k++) { m[k * 2] = (ushort)((id << 10) | k); m[k * 2 + 1] = 0; }
}

static int pict_id(const pictureInfo *pi)
{
#define X(id, name, w, h, np) if (pi == &name) return id;
    ART_LIST
#undef X
    return 0;
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
static FILE *sndlog;
static long snd_heard[SND_COUNT], cdda_plays, snd_this_frame;
static const char *snd_name[SND_COUNT] = { SND_NAMES };
static const char *pan_name = "LCR";

static void stub_snd(BYTE cmd)
{
    int id = (cmd - SND_CMD_BASE) / 3, pan = (cmd - SND_CMD_BASE) % 3;
    if (cmd < SND_CMD_BASE || id >= SND_COUNT) { fprintf(stderr, "frame %ld: bad sound command %02x\n", frame, cmd); exit(4); }
    if (++snd_this_frame > 1) { fprintf(stderr, "frame %ld: two sound commands in one frame\n", frame); exit(4); }
    snd_heard[id]++;
    if (sndlog) fprintf(sndlog, "%ld %s %c\n", frame, snd_name[id], pan_name[pan]);
}

static void stub_cdda(int op, BYTE track)
{
    if (op == 0) {
        if (track < 2 || track > 4) { fprintf(stderr, "frame %ld: CD track %d is not a song\n", frame, track); exit(4); }
        cdda_plays++;
    }
    if (sndlog) fprintf(sndlog, "%ld cdda %s %d\n", frame, op == 0 ? "play" : op == 1 ? "pause" : "resume", track);
}

void stub_bg(WORD c) { bg = c; }
void stub_pal(int n, int c, const void *d) { memcpy(&pal_ram[n * 16], d, (size_t)c * 32); }
void stub_vram(WORD a, WORD d) { vram[a] = d; sc234_this_frame++; }

void initGfx(void)
{
#define X(id, name, w, h, np) fill_map(name##_map, id, (w) * (h));
    ART_LIST
#undef X
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
    for (k = 0; k < npics; k++)
        if (pics[k]->shown)
            fprintf(f, "pict %d %d %d %d %d %d\n", pict_id(pics[k]->info), pics[k]->baseSprite, pics[k]->posX,
                    pics[k]->posY, pics[k]->info->tileWidth, pics[k]->basePalette);
    /* sprites, with sticky chains resolved: x of a chained sprite = previous + 16 */
    {
        int ch = 0, cx = 0, cy = 0, h, t;
        for (s = 1; s < 381; s++) {
            WORD yw = vram[0x8200 | s];
            if (yw & 0x40) {
                cx += 16;
                if (!ch) continue;
            } else {
                ch = yw & 0x3f;
                cx = vram[0x8400 | s] >> 7;
                if (cx >= 400) cx -= 512;
                cy = YSHIFT - (yw >> 7);
                if (cy > 300) cy -= 512;
                if (!ch) continue;
            }
            h = ch > 32 ? 32 : ch;
            fprintf(f, "spr %d %d %d %d", s, cx, cy, h);
            for (t = 0; t < h; t++) fprintf(f, " %04x %04x", vram[(s << 6) + t * 2], vram[(s << 6) + t * 2 + 1]);
            fprintf(f, "\n");
        }
    }
    /* scenery colour state, so the renderer can tint the art the same way */
    fprintf(f, "tint %u %u\n", (unsigned)day_u(frames), (unsigned)level);
    fprintf(f, "scene %d phase %d layers %d:", cur, phase, nlay);
    for (k = 0; k < nlay; k++) fprintf(f, " %d%s", lay[k].spr, lay[k].d_shown ? "" : (lay[k].shown ? "!" : "-"));
    fprintf(f, "\n");
    for (k = 0; k < nreg; k++)
        fprintf(f, "pmode %d %d %d\n", PAL_SCENE + reg[k].first, reg[k].p->count, reg[k].mode);
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
    snd_this_frame = 0;
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
        else if (!strcmp(cmd, "sounds")) {
            char *w = line + 6;
            int id;
            for (w = strtok(w, " \t\r\n"); w; w = strtok(0, " \t\r\n")) {
                if (!strcmp(w, "cdda")) { if (!cdda_plays) { fprintf(stderr, "no CD track was started\n"); exit(4); } continue; }
                for (id = 0; id < SND_COUNT && strcmp(w, snd_name[id]); id++) ;
                if (id == SND_COUNT) { fprintf(stderr, "unknown sound %s\n", w); exit(2); }
                if (!snd_heard[id]) { fprintf(stderr, "frame %ld: sound %s never played\n", frame, w); exit(4); }
            }
        }
        else if (!strcmp(cmd, "clock")) frames = (DWORD)atol(arg) * 60;   /* jump the game clock, in seconds */
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
            int id;
            printf("ran %ld frames, peak %ld sprite writes per frame\n", frame, sc234_max);
            printf("sounds:");
            for (id = 0; id < SND_COUNT; id++) if (snd_heard[id]) printf(" %s x%ld", snd_name[id], snd_heard[id]);
            printf("; CD track starts: %ld\n", cdda_plays);
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
    {
        char path[512];
        sprintf(path, "%s/sounds.txt", outdir);
        sndlog = fopen(path, "a");
    }
    clearFixLayer();
    return game_main();
}

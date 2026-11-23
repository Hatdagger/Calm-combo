/*
 * Calm Combo - saving (Neo Geo CD).  See save.h.
 *
 * The CD systems have 8 KB of battery-backed memory that the BIOS treats as
 * a memory card.  One 64-byte save goes through the BIOS CARD call
 * ($C00468): command 2 loads it, 3 saves it, by game number (NGH, NeoCore's
 * 0x7777) and save number 0.  The data starts with "CC01" and ends with a
 * checksum, so anything else under that number is ignored.  If the memory
 * is missing or not formatted nothing is saved (it is never formatted from
 * here, which would wipe other games' saves).
 *
 * The bytes are packed by hand so the layout doesn't depend on the compiler.
 *   0..3  "CC01"
 *   4..43 5 x (3 letters, 0, score as 4 bytes big-endian)
 *   44,45 puzzles solved (bit mask, big-endian)
 *   46..62 zero
 *   63    checksum: all the other bytes added up
 *
 * Strict C89: gcc 2.95, no // comments, declarations at the top of blocks.
 */
#include <neocore.h>
#include "externs.h"
#include <DATlib.h>
#include "save.h"

#define SAVE_SIZE 64
#define SAVE_NGH  0x7777
#define SAVE_SUB  0
#define CARD_LOAD 2
#define CARD_SAVE 3

/* The PC harness (test/frontend.c) defines SAVE_CARD to use a fake memory. */
#ifndef SAVE_CARD
#define BIOS_CARD_COMMAND (*(volatile BYTE *)0x10FDC4)
#define BIOS_CARD_ANSWER  (*(volatile BYTE *)0x10FDC6)
#define BIOS_CARD_START   (*(volatile DWORD *)0x10FDC8)
#define BIOS_CARD_SIZE    (*(volatile WORD *)0x10FDCC)
#define BIOS_CARD_FCB     (*(volatile WORD *)0x10FDCE)
#define BIOS_CARD_SUB     (*(volatile WORD *)0x10FDD0)

static BYTE bios_card(BYTE cmd, BYTE *buf)
{
    BIOS_CARD_COMMAND = cmd;
    BIOS_CARD_START = (DWORD)buf;
    BIOS_CARD_SIZE = SAVE_SIZE;
    BIOS_CARD_FCB = SAVE_NGH;
    BIOS_CARD_SUB = SAVE_SUB;
    disableIRQ();
    /* the BIOS doesn't keep registers: save the ones GCC expects kept */
    __asm__ volatile (
        "movem.l %%d2-%%d7/%%a2-%%a6,-(%%sp)\n\t"
        "jsr     0xC00468\n\t"
        "movem.l (%%sp)+,%%d2-%%d7/%%a2-%%a6"
        : : : "d0", "d1", "a0", "a1", "memory");
    enableIRQ();
    return BIOS_CARD_ANSWER;
}
#define SAVE_CARD(cmd, buf) bios_card((cmd), (buf))
#endif

char  hs_name[HS_N][4];
DWORD hs_score[HS_N];
WORD  pz_solved;
BYTE  save_answer;

static BYTE buf[SAVE_SIZE];

static const char  DEF_NAME[HS_N][4] = { "SUN", "SEA", "SKY", "DEW", "OWL" };
static const DWORD DEF_SCORE[HS_N]   = { 5000, 4000, 3000, 2000, 1000 };

static void defaults(void)
{
    int k, j;
    for (k = 0; k < HS_N; k++) {
        for (j = 0; j < 4; j++) hs_name[k][j] = DEF_NAME[k][j];
        hs_score[k] = DEF_SCORE[k];
    }
    pz_solved = 0;
}

static BYTE checksum(void)
{
    BYTE s = 0;
    int k;
    for (k = 0; k < SAVE_SIZE - 1; k++) s = (BYTE)(s + buf[k]);
    return s;
}

static void pack(void)
{
    int k, j;
    BYTE *p = buf;
    for (k = 0; k < SAVE_SIZE; k++) buf[k] = 0;
    *p++ = 'C'; *p++ = 'C'; *p++ = '0'; *p++ = '1';
    for (k = 0; k < HS_N; k++) {
        for (j = 0; j < 3; j++) *p++ = (BYTE)hs_name[k][j];
        *p++ = 0;
        *p++ = (BYTE)(hs_score[k] >> 24); *p++ = (BYTE)(hs_score[k] >> 16);
        *p++ = (BYTE)(hs_score[k] >> 8);  *p++ = (BYTE)hs_score[k];
    }
    *p++ = (BYTE)(pz_solved >> 8); *p++ = (BYTE)pz_solved;
    buf[SAVE_SIZE - 1] = checksum();
}

static int unpack(void)
{
    int k, j;
    BYTE *p = buf + 4;
    if (buf[0] != 'C' || buf[1] != 'C' || buf[2] != '0' || buf[3] != '1') return 0;
    if (buf[SAVE_SIZE - 1] != checksum()) return 0;
    for (k = 0; k < HS_N; k++) {
        for (j = 0; j < 3; j++) {
            char ch = (char)p[j];
            hs_name[k][j] = (char)((ch >= 'A' && ch <= 'Z') || ch == ' ' || ch == '.' || ch == '!' ? ch : ' ');
        }
        hs_name[k][3] = 0;
        hs_score[k] = ((DWORD)p[4] << 24) | ((DWORD)p[5] << 16) | ((DWORD)p[6] << 8) | (DWORD)p[7];
        p += 8;
    }
    pz_solved = (WORD)((p[0] << 8) | p[1]);
    return 1;
}

void save_load(void)
{
    defaults();
    save_answer = SAVE_CARD(CARD_LOAD, buf);
    if (save_answer == 0 && !unpack()) defaults();   /* someone else's data: ignore it */
}

void save_store(void)
{
    pack();
    save_answer = SAVE_CARD(CARD_SAVE, buf);
}

int hs_rank(DWORD score)
{
    int k;
    if (!score) return -1;
    for (k = 0; k < HS_N; k++)
        if (score > hs_score[k]) return k;
    return -1;
}

void hs_insert(int rank, const char *name, DWORD score)
{
    int k, j;
    if (rank < 0 || rank >= HS_N) return;
    for (k = HS_N - 1; k > rank; k--) {
        for (j = 0; j < 4; j++) hs_name[k][j] = hs_name[k - 1][j];
        hs_score[k] = hs_score[k - 1];
    }
    for (j = 0; j < 3; j++) hs_name[rank][j] = name[j];
    hs_name[rank][3] = 0;
    hs_score[rank] = score;
}

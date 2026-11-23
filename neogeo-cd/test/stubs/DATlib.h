#ifndef STUB_DATLIB_H
#define STUB_DATLIB_H
typedef struct paletteInfo { ushort count; ushort data[16]; } paletteInfo;
typedef struct pictureInfo { int dummy; } pictureInfo;
typedef struct picture { int dummy; } picture;
extern ushort LSPCmode;
#define FLIP_NONE 0
#define volMEMBYTE(a) (*((volatile BYTE *)(a)))
#define backgroundColor(c) stub_bg(c)
#define palJobPut(n,c,d) stub_pal((n),(c),(d))
void stub_bg(WORD c); void stub_pal(int n, int c, const void *d);
void initGfx(void); void SCClose(void); void waitVBlank(void); void clearFixLayer(void);
void fixPrint(ushort x, ushort y, ushort pal, ushort bank, const char *buf);
void pictureInit(picture *p, const pictureInfo *pi, ushort bs, ushort bp, short x, short y, ushort f);
void pictureHide(picture *p); void pictureShow(picture *p);
#endif

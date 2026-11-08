#ifndef PSYQ_COMPAT_LIBGPU_H
#define PSYQ_COMPAT_LIBGPU_H

/* The graphics library's primitives and calls, for the native build. Each primitive is the
 * console's GPU packet: a tag word (a 24-bit link and the packet's length in words) followed
 * by the GP0 command words, so the layouts below must stay as they are. The software GPU
 * (platform/native/psx_gpu.c) consumes them as they stand. */

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>

typedef struct {
    int16_t x, y, w, h;
} RECT;

/* The tag word. addr is a console address's low 24 bits (the GPU adds 0x80000000). */
typedef struct {
    unsigned addr : 24;
    unsigned len : 8;
    uint8_t r0, g0, b0, code;
} P_TAG;

typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0, x1, y1, x2, y2; } POLY_F3;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0, x1, y1, x2, y2, x3, y3; } POLY_F4;
typedef struct {
    uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut;
    int16_t x1, y1; uint8_t u1, v1; uint16_t tpage; int16_t x2, y2; uint8_t u2, v2; uint16_t pad2;
} POLY_FT3;
typedef struct {
    uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut;
    int16_t x1, y1; uint8_t u1, v1; uint16_t tpage; int16_t x2, y2; uint8_t u2, v2; uint16_t pad2;
    int16_t x3, y3; uint8_t u3, v3; uint16_t pad3;
} POLY_FT4;
typedef struct {
    uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t r1, g1, b1, pad1; int16_t x1, y1;
    uint8_t r2, g2, b2, pad2; int16_t x2, y2;
} POLY_G3;
typedef struct {
    uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t r1, g1, b1, pad1; int16_t x1, y1;
    uint8_t r2, g2, b2, pad2; int16_t x2, y2; uint8_t r3, g3, b3, pad3; int16_t x3, y3;
} POLY_G4;
typedef struct {
    uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut;
    uint8_t r1, g1, b1, p1; int16_t x1, y1; uint8_t u1, v1; uint16_t tpage;
    uint8_t r2, g2, b2, p2; int16_t x2, y2; uint8_t u2, v2; uint16_t pad2;
} POLY_GT3;
typedef struct {
    uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut;
    uint8_t r1, g1, b1, p1; int16_t x1, y1; uint8_t u1, v1; uint16_t tpage;
    uint8_t r2, g2, b2, p2; int16_t x2, y2; uint8_t u2, v2; uint16_t pad2;
    uint8_t r3, g3, b3, p3; int16_t x3, y3; uint8_t u3, v3; uint16_t pad3;
} POLY_GT4;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0, x1, y1; } LINE_F2;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut; int16_t w, h; } SPRT;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut; } SPRT_8;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; uint8_t u0, v0; uint16_t clut; } SPRT_16;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0, w, h; } TILE;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; } TILE_1;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; } TILE_8;
typedef struct { uint32_t tag; uint8_t r0, g0, b0, code; int16_t x0, y0; } TILE_16;
typedef struct { uint32_t tag; uint32_t code[2]; } DR_MODE;
typedef struct { uint32_t tag; uint32_t code[1]; } DR_TPAGE;
typedef struct { uint32_t tag; uint32_t code[2]; } DR_AREA;
typedef struct { uint32_t tag; uint32_t code[1]; } DR_OFFSET;
typedef struct { uint32_t tag; uint32_t code[2]; } DR_STP;
typedef struct { uint32_t tag; uint32_t code[5]; } DR_MOVE;
typedef struct { uint32_t tag; uint32_t code[3]; uint32_t p[13]; } DR_LOAD;
typedef struct { uint32_t tag; uint32_t code[15]; } DR_ENV;

typedef struct {
    RECT clip;
    int16_t ofs[2];
    RECT tw;
    uint16_t tpage;
    uint8_t dtd, dfe, isbg;
    uint8_t r0, g0, b0;
    DR_ENV dr_env;
} DRAWENV;

typedef struct {
    RECT disp;
    RECT screen;
    uint8_t isinter, isrgb24, pad0, pad1;
} DISPENV;

/* ---- packet macros --------------------------------------------------------------------- */

#define setlen(p, _len) (((P_TAG *)(p))->len = (unsigned)(_len))
#define setcode(p, _code) (((P_TAG *)(p))->code = (uint8_t)(_code))
#define getcode(p) (((P_TAG *)(p))->code)
#define getaddr(p) ((unsigned)((P_TAG *)(p))->addr)
#define setaddr(p, _addr) (((P_TAG *)(p))->addr = (unsigned)(uintptr_t)(_addr))
#define nextPrim(p) ((void *)(0x80000000u | ((P_TAG *)(p))->addr))
#define isendprim(p) ((((P_TAG *)(p))->addr) == 0xFFFFFF)
/* Link a packet in at the head of an ordering-table entry. */
#define addPrim(ot, p) (setaddr((p), getaddr(ot)), setaddr((ot), (p)))
#define AddPrim(ot, p) addPrim(ot, p)
#define addPrims(ot, p0, p1) (setaddr((p1), getaddr(ot)), setaddr((ot), (p0)))
#define catPrim(p0, p1) setaddr((p0), (p1))
#define termPrim(p) (((P_TAG *)(p))->addr = 0xFFFFFF)

#define setPolyF3(p) (setlen((p), 4), setcode((p), 0x20))
#define setPolyFT3(p) (setlen((p), 7), setcode((p), 0x24))
#define setPolyG3(p) (setlen((p), 6), setcode((p), 0x30))
#define setPolyGT3(p) (setlen((p), 9), setcode((p), 0x34))
#define setPolyF4(p) (setlen((p), 5), setcode((p), 0x28))
#define setPolyFT4(p) (setlen((p), 9), setcode((p), 0x2C))
#define setPolyG4(p) (setlen((p), 8), setcode((p), 0x38))
#define setPolyGT4(p) (setlen((p), 12), setcode((p), 0x3C))
#define setSprt8(p) (setlen((p), 3), setcode((p), 0x74))
#define setSprt16(p) (setlen((p), 3), setcode((p), 0x7C))
#define setSprt(p) (setlen((p), 4), setcode((p), 0x64))
#define setTile1(p) (setlen((p), 2), setcode((p), 0x68))
#define setTile8(p) (setlen((p), 2), setcode((p), 0x70))
#define setTile16(p) (setlen((p), 2), setcode((p), 0x78))
#define setTile(p) (setlen((p), 3), setcode((p), 0x60))
#define setLineF2(p) (setlen((p), 3), setcode((p), 0x40))

#define setSemiTrans(p, abe) \
    ((abe) ? setcode((p), getcode(p) | 2) : setcode((p), getcode(p) & ~2))
#define setShadeTex(p, tge) \
    ((tge) ? setcode((p), getcode(p) | 1) : setcode((p), getcode(p) & ~1))

#define setRGB0(p, _r0, _g0, _b0) ((p)->r0 = (_r0), (p)->g0 = (_g0), (p)->b0 = (_b0))
#define setRGB1(p, _r1, _g1, _b1) ((p)->r1 = (_r1), (p)->g1 = (_g1), (p)->b1 = (_b1))
#define setRGB2(p, _r2, _g2, _b2) ((p)->r2 = (_r2), (p)->g2 = (_g2), (p)->b2 = (_b2))
#define setRGB3(p, _r3, _g3, _b3) ((p)->r3 = (_r3), (p)->g3 = (_g3), (p)->b3 = (_b3))

#define setXY0(p, _x0, _y0) ((p)->x0 = (_x0), (p)->y0 = (_y0))
#define setXY2(p, _x0, _y0, _x1, _y1) ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1))
#define setXY3(p, _x0, _y0, _x1, _y1, _x2, _y2) \
    ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), (p)->x2 = (_x2), (p)->y2 = (_y2))
#define setXY4(p, _x0, _y0, _x1, _y1, _x2, _y2, _x3, _y3)                                      \
    ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x1), (p)->y1 = (_y1), (p)->x2 = (_x2),      \
     (p)->y2 = (_y2), (p)->x3 = (_x3), (p)->y3 = (_y3))
#define setXYWH(p, _x0, _y0, _w, _h)                                                           \
    ((p)->x0 = (_x0), (p)->y0 = (_y0), (p)->x1 = (_x0) + (_w), (p)->y1 = (_y0),                \
     (p)->x2 = (_x0), (p)->y2 = (_y0) + (_h), (p)->x3 = (_x0) + (_w), (p)->y3 = (_y0) + (_h))
#define setWH(p, _w, _h) ((p)->w = (_w), (p)->h = (_h))

#define setUV0(p, _u0, _v0) ((p)->u0 = (_u0), (p)->v0 = (_v0))
#define setUV3(p, _u0, _v0, _u1, _v1, _u2, _v2) \
    ((p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2), (p)->v2 = (_v2))
#define setUV4(p, _u0, _v0, _u1, _v1, _u2, _v2, _u3, _v3)                                      \
    ((p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u1), (p)->v1 = (_v1), (p)->u2 = (_u2),      \
     (p)->v2 = (_v2), (p)->u3 = (_u3), (p)->v3 = (_v3))
#define setUVWH(p, _u0, _v0, _w, _h)                                                           \
    ((p)->u0 = (_u0), (p)->v0 = (_v0), (p)->u1 = (_u0) + (_w), (p)->v1 = (_v0),                \
     (p)->u2 = (_u0), (p)->v2 = (_v0) + (_h), (p)->u3 = (_u0) + (_w), (p)->v3 = (_v0) + (_h))

/* A CLUT's and texture page's 16-bit ids. */
#define getClut(x, y) (((y) << 6) | (((x) >> 4) & 0x3F))
#define getTPage(tp, abr, x, y) \
    ((((tp) & 3) << 7) | (((abr) & 3) << 5) | (((y) & 0x100) >> 4) | (((x) & 0x3FF) >> 6) | (((y) & 0x200) << 2))

#define setDrawTPage(p, dfe, dtd, tpage)                                                       \
    (setlen((p), 1), ((DR_TPAGE *)(p))->code[0] = 0xE1000000u | ((dtd) ? 0x200u : 0u) |         \
                                                  ((dfe) ? 0x400u : 0u) | ((unsigned)(tpage) & 0x9FFu))

/* ---- functions ------------------------------------------------------------------------- */

int ResetGraph(int mode);
int SetGraphDebug(int level);
int DrawSync(int mode);
int VSync(int mode);
void *VSyncCallback(void (*func)(void));
void SetDispMask(int mask);
int SetVideoMode(int mode);

void ClearOTag(uint32_t *ot, int n);
void ClearOTagR(uint32_t *ot, int n);
void DrawOTag(uint32_t *p);

DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h);
DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h);
DRAWENV *PutDrawEnv(DRAWENV *env);
DISPENV *PutDispEnv(DISPENV *env);

int LoadImage(RECT *rect, uint32_t *p);
int StoreImage(RECT *rect, uint32_t *p);
int MoveImage(RECT *rect, int x, int y);
int MoveImage2(RECT *rect, int x, int y);
int ClearImage(RECT *rect, uint8_t r, uint8_t g, uint8_t b);

uint16_t GetTPage(int tp, int abr, int x, int y);
uint16_t GetClut(int x, int y);

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw);
void SetDrawArea(DR_AREA *p, RECT *r);
void SetDrawLoad(DR_LOAD *p, RECT *rect);
void SetSemiTrans(void *p, int abe);
void SetShadeTex(void *p, int tge);
void SetPolyF4(POLY_F4 *p);

/* The SDK's debug text overlay: not drawn in the native build. */
void FntLoad(int x, int y);
int FntOpen(int x, int y, int w, int h, int isbg, int n);
int FntPrint(const char *format, ...);
uint32_t *FntFlush(int id);
void SetDumpFnt(int id);
void KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx, int cy, int isbg, int n);
void KanjiFntClose(void);
int KanjiFntPrint(const char *format, ...);
uint32_t *KanjiFntFlush(int id);

#endif /* PSYQ_COMPAT_LIBGPU_H */

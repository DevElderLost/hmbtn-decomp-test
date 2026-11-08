/* libgpu and libgs on the software GPU: the drawing, display and ordering-table calls. */

#include <string.h>
#include <libgpu.h>
#include <libgs.h>
#include "psx_gpu.h"
#include "psx_gte.h"
#include "psx_libgpu.h"
#include "psx_mem.h"

void (*psx_vsync_hook)(void);

static int vblank_count;
static void (*vsync_callback)(void);
static int gpu_ready;

MATRIX GsIDMATRIX = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
int HWD0 = 320, VWD0 = 240;

static GsF_LIGHT flat_lights[3];
static uint32_t light_mode;
static int32_t ambient[3];
static PACKET *work_base;
static int draw_buffer_y0 = 0;

static void ensure_gpu(void) {
    if (!gpu_ready) {
        psx_gpu_init();
        gpu_ready = 1;
    }
}

/* ---- control --------------------------------------------------------------------------- */

int ResetGraph(int mode) {
    if (mode == 0 || !gpu_ready) {
        ensure_gpu();
    }
    psx_gpu_enable_display(0);
    return 0;
}

int SetGraphDebug(int level) { (void)level; return 0; }
int DrawSync(int mode) { (void)mode; return 0; }          /* drawing is synchronous */
void SetDispMask(int mask) { ensure_gpu(); psx_gpu_enable_display(mask != 0); }
int SetVideoMode(int mode) { return mode; }

int psx_vblank_count(void) { return vblank_count; }

void *VSyncCallback(void (*func)(void)) {
    void (*old)(void) = vsync_callback;
    vsync_callback = func;
    return (void *)old;
}

/* VSync(0) waits for the next blank, VSync(n) for n of them, VSync(-1) reads the counter and
 * VSync(1) the time since the last blank (here: 0). */
int VSync(int mode) {
    int blanks;
    if (mode < 0) {
        return vblank_count;
    }
    if (mode == 1) {
        return 0;
    }
    for (blanks = mode == 0 ? 1 : mode; blanks > 0; blanks--) {
        if (psx_vsync_hook != NULL) {
            psx_vsync_hook();
        }
        vblank_count++;
        if (vsync_callback != NULL) {
            vsync_callback();
        }
    }
    return vblank_count;
}

/* ---- ordering tables ------------------------------------------------------------------- */

/* The table links each entry to the next; the last one ends it. */
void ClearOTag(uint32_t *ot, int n) {
    int i;
    for (i = 0; i < n - 1; i++) {
        ot[i] = (uint32_t)(uintptr_t)(ot + i + 1) & 0xFFFFFF;
    }
    if (n > 0) {
        ot[n - 1] = 0xFFFFFF;
    }
}

/* The reversed table: draw order runs from the last entry down to the first. */
void ClearOTagR(uint32_t *ot, int n) {
    int i;
    if (n <= 0) {
        return;
    }
    ot[0] = 0xFFFFFF;
    for (i = 1; i < n; i++) {
        ot[i] = (uint32_t)(uintptr_t)(ot + i - 1) & 0xFFFFFF;
    }
}

void DrawOTag(uint32_t *p) {
    ensure_gpu();
    psx_gpu_draw_otag((uint32_t)(uintptr_t)p);
}

/* ---- environments ---------------------------------------------------------------------- */

DRAWENV *SetDefDrawEnv(DRAWENV *env, int x, int y, int w, int h) {
    memset(env, 0, sizeof(*env));
    env->clip.x = (int16_t)x;
    env->clip.y = (int16_t)y;
    env->clip.w = (int16_t)w;
    env->clip.h = (int16_t)h;
    env->ofs[0] = (int16_t)x;
    env->ofs[1] = (int16_t)y;
    env->tpage = 0x0A;
    env->dtd = 1;
    env->dfe = 1;
    return env;
}

DISPENV *SetDefDispEnv(DISPENV *env, int x, int y, int w, int h) {
    memset(env, 0, sizeof(*env));
    env->disp.x = (int16_t)x;
    env->disp.y = (int16_t)y;
    env->disp.w = (int16_t)w;
    env->disp.h = (int16_t)h;
    env->screen.w = 256;
    env->screen.h = 240;
    return env;
}

DRAWENV *PutDrawEnv(DRAWENV *env) {
    ensure_gpu();
    psx_gpu_set_draw_area(env->clip.x, env->clip.y, env->clip.w, env->clip.h);
    psx_gpu_set_draw_offset(env->ofs[0], env->ofs[1]);
    psx_gpu_set_draw_mode(env->tpage, env->dtd, env->dfe);
    if (env->tw.w != 0 || env->tw.h != 0) {
        uint32_t tw = 0xE2000000u | (uint32_t)((-env->tw.w & 0xFF) >> 3) | (uint32_t)(((-env->tw.h & 0xFF) >> 3) << 5) |
                      (uint32_t)(((env->tw.x & 0xFF) >> 3) << 10) | (uint32_t)(((env->tw.y & 0xFF) >> 3) << 15);
        psx_gpu_exec(&tw, 1);
    }
    if (env->isbg) {
        PsxRect r = {env->clip.x, env->clip.y, env->clip.w, env->clip.h};
        psx_gpu_fill(&r, env->r0, env->g0, env->b0);
    }
    return env;
}

DISPENV *PutDispEnv(DISPENV *env) {
    ensure_gpu();
    psx_gpu_set_display(env->disp.x, env->disp.y, env->disp.w, env->disp.h);
    return env;
}

/* ---- image transfers ------------------------------------------------------------------- */

int LoadImage(RECT *rect, uint32_t *p) {
    const PsxRect r = {rect->x, rect->y, rect->w, rect->h};
    ensure_gpu();
    psx_gpu_load_image(&r, (const uint16_t *)(const void *)p);
    return 0;
}

int StoreImage(RECT *rect, uint32_t *p) {
    const PsxRect r = {rect->x, rect->y, rect->w, rect->h};
    ensure_gpu();
    psx_gpu_store_image(&r, (uint16_t *)(void *)p);
    return 0;
}

int MoveImage(RECT *rect, int x, int y) {
    const PsxRect r = {rect->x, rect->y, rect->w, rect->h};
    ensure_gpu();
    psx_gpu_move_image(&r, x, y);
    return 0;
}

int MoveImage2(RECT *rect, int x, int y) { return MoveImage(rect, x, y); }

int ClearImage(RECT *rect, uint8_t r, uint8_t g, uint8_t b) {
    const PsxRect pr = {rect->x, rect->y, rect->w, rect->h};
    ensure_gpu();
    psx_gpu_fill(&pr, r, g, b);
    return 0;
}

/* ---- packet set-up --------------------------------------------------------------------- */

uint16_t GetTPage(int tp, int abr, int x, int y) { return (uint16_t)getTPage(tp, abr, x, y); }
uint16_t GetClut(int x, int y) { return (uint16_t)getClut(x, y); }

static uint32_t mode_word(int dfe, int dtd, int tpage) {
    return 0xE1000000u | (dtd ? 0x200u : 0u) | (dfe ? 0x400u : 0u) | ((unsigned)tpage & 0x9FFu);
}

void SetDrawMode(DR_MODE *p, int dfe, int dtd, int tpage, RECT *tw) {
    setlen(p, 2);
    p->code[0] = mode_word(dfe, dtd, tpage);
    p->code[1] = tw ? (0xE2000000u | (uint32_t)((-tw->w & 0xFF) >> 3) | (uint32_t)(((-tw->h & 0xFF) >> 3) << 5) |
                       (uint32_t)(((tw->x & 0xFF) >> 3) << 10) | (uint32_t)(((tw->y & 0xFF) >> 3) << 15))
                    : 0;
}

void SetDrawArea(DR_AREA *p, RECT *r) {
    setlen(p, 2);
    p->code[0] = 0xE3000000u | (uint32_t)((r->y & 0x3FF) << 10) | (uint32_t)(r->x & 0x3FF);
    p->code[1] = 0xE4000000u | (uint32_t)(((r->y + r->h - 1) & 0x3FF) << 10) | (uint32_t)((r->x + r->w - 1) & 0x3FF);
}

void SetDrawLoad(DR_LOAD *p, RECT *rect) {
    const int words = (rect->w * rect->h + 1) / 2;
    p->code[0] = 0xA0000000u;
    p->code[1] = (uint32_t)(rect->x & 0xFFFF) | ((uint32_t)rect->y << 16);
    p->code[2] = (uint32_t)(rect->w & 0xFFFF) | ((uint32_t)rect->h << 16);
    setlen(p, 3 + words);
}

void SetSemiTrans(void *p, int abe) { setSemiTrans(p, abe); }
void SetShadeTex(void *p, int tge) { setShadeTex(p, tge); }
void SetPolyF4(POLY_F4 *p) { setPolyF4(p); }

/* ---- libgs ----------------------------------------------------------------------------- */

void GsInitGraph(uint16_t x, uint16_t y, uint16_t intmode, uint16_t dith, uint16_t varmmode) {
    (void)intmode; (void)dith; (void)varmmode;
    ensure_gpu();
    HWD0 = x;
    VWD0 = y;
    psx_gpu_set_display(0, 0, x, y);
}

void GsInit3D(void) {
    memset(flat_lights, 0, sizeof(flat_lights));
    light_mode = 0;
}

void GsDefDispBuff(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    (void)x0; (void)x1;
    draw_buffer_y0 = y0 < y1 ? y0 : y1;
}

void GsSetProjection(int32_t h) { SetGeomScreen(h); }
void GsSetOffset(int32_t x, int32_t y) { SetGeomOffset(x, y); }
void GsSetDrawBuffOffset(void) { (void)draw_buffer_y0; }
void GsSetWorkBase(PACKET *p) { work_base = p; }
PACKET *GsGetWorkBase(void) { return work_base; }

void GsSetFlatLight(uint32_t id, GsF_LIGHT *light) {
    if (id < 3) {
        flat_lights[id] = *light;
    }
}

void GsSetLightMode(uint32_t mode) { light_mode = mode; }

void GsSetAmbient(int32_t r, int32_t g, int32_t b) {
    ambient[0] = r;
    ambient[1] = g;
    ambient[2] = b;
}

/* ---- debug text: not drawn ------------------------------------------------------------- */

void FntLoad(int x, int y) { (void)x; (void)y; }
int FntOpen(int x, int y, int w, int h, int isbg, int n) { (void)x; (void)y; (void)w; (void)h; (void)isbg; (void)n; return 0; }
int FntPrint(const char *format, ...) { (void)format; return 0; }
uint32_t *FntFlush(int id) { (void)id; return NULL; }
void SetDumpFnt(int id) { (void)id; }
void KanjiFntOpen(int x, int y, int w, int h, int dx, int dy, int cx, int cy, int isbg, int n) {
    (void)x; (void)y; (void)w; (void)h; (void)dx; (void)dy; (void)cx; (void)cy; (void)isbg; (void)n;
}
void KanjiFntClose(void) {}
int KanjiFntPrint(const char *format, ...) { (void)format; return 0; }
uint32_t *KanjiFntFlush(int id) { (void)id; return NULL; }

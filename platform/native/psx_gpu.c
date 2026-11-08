#include <stdio.h>
#include <string.h>
#include "psx_gpu.h"
#include "psx_mem.h"

unsigned psx_gpu_bad_commands;
unsigned psx_gpu_bad_tags;

static uint16_t vram[PSX_VRAM_W * PSX_VRAM_H];

typedef struct GpuState {
    int tp_x, tp_y;             /* texture page origin in VRAM */
    int tp_depth;               /* 0: 4-bit, 1: 8-bit, 2: 15-bit */
    int semi_mode;              /* 0: B/2+F/2, 1: B+F, 2: B-F, 3: B+F/4 */
    int dither;
    int draw_to_display;
    int tw_mask_x, tw_mask_y, tw_off_x, tw_off_y;   /* texture window, in 8-pixel units */
    int area_x1, area_y1, area_x2, area_y2;
    int ofs_x, ofs_y;
    int disp_x, disp_y, disp_w, disp_h, disp_on;
} GpuState;

static GpuState g;

static const int dither_matrix[4][4] = {
    {-4, 0, -3, 1}, {2, -2, 3, -1}, {-3, 1, -4, 0}, {3, -1, 2, -2},
};

void psx_gpu_init(void) {
    memset(vram, 0, sizeof(vram));
    memset(&g, 0, sizeof(g));
    g.area_x2 = 1023;
    g.area_y2 = 511;
    g.disp_w = 320;
    g.disp_h = 240;
    g.tp_depth = 0;
    psx_gpu_bad_commands = 0;
    psx_gpu_bad_tags = 0;
}

uint16_t *psx_gpu_vram(void) { return vram; }

/* ---- environment ----------------------------------------------------------------------- */

void psx_gpu_set_draw_area(int x, int y, int w, int h) {
    g.area_x1 = x < 0 ? 0 : x;
    g.area_y1 = y < 0 ? 0 : y;
    g.area_x2 = x + w - 1 > 1023 ? 1023 : x + w - 1;
    g.area_y2 = y + h - 1 > 511 ? 511 : y + h - 1;
}

void psx_gpu_set_draw_offset(int x, int y) {
    g.ofs_x = x;
    g.ofs_y = y;
}

/* Bits 0 to 8 of a draw mode (GP0 E1) are the texture page; a textured polygon's own
 * texpage word sets just these. */
static void apply_texpage(uint32_t m) {
    g.tp_x = (int)(m & 0xF) * 64;
    g.tp_y = (int)((m >> 4) & 1) * 256;
    g.semi_mode = (int)((m >> 5) & 3);
    g.tp_depth = (int)((m >> 7) & 3);
    if (g.tp_depth == 3) {
        g.tp_depth = 2;
    }
}

static void apply_draw_mode(uint32_t m) {
    apply_texpage(m);
    g.dither = (int)((m >> 9) & 1);
    g.draw_to_display = (int)((m >> 10) & 1);
}

void psx_gpu_set_draw_mode(unsigned tpage, int dither, int draw_to_display) {
    apply_draw_mode((tpage & 0x1FF) | ((unsigned)(dither != 0) << 9) | ((unsigned)(draw_to_display != 0) << 10));
}

void psx_gpu_set_display(int x, int y, int w, int h) {
    g.disp_x = x;
    g.disp_y = y;
    g.disp_w = w;
    g.disp_h = h;
}

void psx_gpu_enable_display(int on) { g.disp_on = on != 0; }

void psx_gpu_get_display(uint32_t *rgba, int capacity, int *w, int *h) {
    const int dw = g.disp_w, dh = g.disp_h;
    int x, y;

    *w = dw;
    *h = dh;
    if (dw * dh > capacity || dw <= 0 || dh <= 0) {
        return;
    }
    for (y = 0; y < dh; y++) {
        for (x = 0; x < dw; x++) {
            uint32_t out = 0xFF000000u;
            if (g.disp_on) {
                const uint16_t p = vram[((g.disp_y + y) & 511) * PSX_VRAM_W + ((g.disp_x + x) & 1023)];
                const uint32_t r = (uint32_t)(p & 31) << 3, gr = (uint32_t)((p >> 5) & 31) << 3,
                               b = (uint32_t)((p >> 10) & 31) << 3;
                out |= (r | (r >> 5)) << 16 | (gr | (gr >> 5)) << 8 | (b | (b >> 5));
            }
            rgba[y * dw + x] = out;
        }
    }
}

/* ---- VRAM transfers -------------------------------------------------------------------- */

void psx_gpu_load_image(const PsxRect *r, const uint16_t *src) {
    int x, y;
    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            vram[((r->y + y) & 511) * PSX_VRAM_W + ((r->x + x) & 1023)] = src[y * r->w + x];
        }
    }
}

void psx_gpu_store_image(const PsxRect *r, uint16_t *dst) {
    int x, y;
    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            dst[y * r->w + x] = vram[((r->y + y) & 511) * PSX_VRAM_W + ((r->x + x) & 1023)];
        }
    }
}

void psx_gpu_move_image(const PsxRect *r, int dx, int dy) {
    int x, y;
    /* Copy through a buffer so overlapping rectangles behave. */
    static uint16_t tmp[PSX_VRAM_W * PSX_VRAM_H];
    if (r->w <= 0 || r->h <= 0 || r->w > PSX_VRAM_W || r->h > PSX_VRAM_H) {
        return;
    }
    psx_gpu_store_image(r, tmp);
    for (y = 0; y < r->h; y++) {
        for (x = 0; x < r->w; x++) {
            vram[((dy + y) & 511) * PSX_VRAM_W + ((dx + x) & 1023)] = tmp[y * r->w + x];
        }
    }
}

void psx_gpu_fill(const PsxRect *r, uint8_t red, uint8_t green, uint8_t blue) {
    const uint16_t c = (uint16_t)((red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10));
    const int x0 = r->x & 0x3F0, y0 = r->y & 0x1FF;
    const int w = ((r->w & 0x3FF) + 0xF) & ~0xF, h = r->h & 0x1FF;
    int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            vram[((y0 + y) & 511) * PSX_VRAM_W + ((x0 + x) & 1023)] = c;
        }
    }
}

/* ---- pixels ---------------------------------------------------------------------------- */

typedef struct Vtx {
    int x, y;
    int r, g, b;
    int u, v;
} Vtx;

typedef struct Prim {
    int textured, semi, raw;
    int clut_x, clut_y;
} Prim;

static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static int tex_fetch(int u, int v, const Prim *p, uint16_t *out) {
    uint16_t t;
    const int uu = (u & ~(g.tw_mask_x * 8)) | ((g.tw_off_x & g.tw_mask_x) * 8);
    const int vv = (v & ~(g.tw_mask_y * 8)) | ((g.tw_off_y & g.tw_mask_y) * 8);
    const int row = ((g.tp_y + (vv & 255)) & 511) * PSX_VRAM_W;

    u = uu & 255;
    if (g.tp_depth == 0) {
        const uint16_t w = vram[row + ((g.tp_x + u / 4) & 1023)];
        const int idx = (w >> ((u & 3) * 4)) & 0xF;
        t = vram[(p->clut_y & 511) * PSX_VRAM_W + ((p->clut_x + idx) & 1023)];
    } else if (g.tp_depth == 1) {
        const uint16_t w = vram[row + ((g.tp_x + u / 2) & 1023)];
        const int idx = (w >> ((u & 1) * 8)) & 0xFF;
        t = vram[(p->clut_y & 511) * PSX_VRAM_W + ((p->clut_x + idx) & 1023)];
    } else {
        t = vram[row + ((g.tp_x + u) & 1023)];
    }
    *out = t;
    return t != 0;      /* an all-zero texel is transparent */
}

/* Draw one pixel at a screen position (already offset). */
static void put_pixel(int x, int y, int r, int gr, int b, const Prim *p, int u, int v) {
    uint16_t back, out;
    int semi = p->semi;
    int r5, g5, b5;

    if (x < g.area_x1 || x > g.area_x2 || y < g.area_y1 || y > g.area_y2) {
        return;
    }
    if (p->textured) {
        uint16_t t;
        int tr, tg, tb;
        if (!tex_fetch(u, v, p, &t)) {
            return;
        }
        tr = (t & 31) << 3;
        tg = ((t >> 5) & 31) << 3;
        tb = ((t >> 10) & 31) << 3;
        if (!p->raw) {
            tr = clamp255(tr * r / 128);
            tg = clamp255(tg * gr / 128);
            tb = clamp255(tb * b / 128);
        }
        r = tr; gr = tg; b = tb;
        semi = semi && (t & 0x8000);
    }
    if (g.dither) {
        const int d = dither_matrix[y & 3][x & 3];
        r = clamp255(r + d);
        gr = clamp255(gr + d);
        b = clamp255(b + d);
    }
    r5 = r >> 3;
    g5 = gr >> 3;
    b5 = b >> 3;
    back = vram[(y & 511) * PSX_VRAM_W + (x & 1023)];
    if (semi) {
        const int br = back & 31, bg = (back >> 5) & 31, bb = (back >> 10) & 31;
        switch (g.semi_mode) {
        case 0: r5 = (br + r5) / 2; g5 = (bg + g5) / 2; b5 = (bb + b5) / 2; break;
        case 1: r5 = br + r5; g5 = bg + g5; b5 = bb + b5; break;
        case 2: r5 = br - r5; g5 = bg - g5; b5 = bb - b5; break;
        default: r5 = br + r5 / 4; g5 = bg + g5 / 4; b5 = bb + b5 / 4; break;
        }
        r5 = r5 < 0 ? 0 : (r5 > 31 ? 31 : r5);
        g5 = g5 < 0 ? 0 : (g5 > 31 ? 31 : g5);
        b5 = b5 < 0 ? 0 : (b5 > 31 ? 31 : b5);
    }
    out = (uint16_t)(r5 | (g5 << 5) | (b5 << 10));
    vram[(y & 511) * PSX_VRAM_W + (x & 1023)] = out;
}

/* ---- triangles ------------------------------------------------------------------------- */

static int64_t edge(const Vtx *a, const Vtx *b, int px, int py) {
    return (int64_t)(b->x - a->x) * (py - a->y) - (int64_t)(b->y - a->y) * (px - a->x);
}

static int top_left(const Vtx *a, const Vtx *b) {
    const int dx = b->x - a->x, dy = b->y - a->y;
    return (dy == 0 && dx > 0) || dy < 0;
}

static int64_t floor_div(int64_t n, int64_t d) {
    int64_t q = n / d;
    if ((n % d != 0) && ((n < 0) != (d < 0))) {
        q--;
    }
    return q;
}

static void draw_triangle(Vtx v0, Vtx v1, Vtx v2, const Prim *p, int shaded) {
    int64_t area;
    int min_x, max_x, min_y, max_y, x, y;
    Vtx t;

    /* The console drops triangles that span too far. */
    if (v0.x - v1.x > 1023 || v1.x - v0.x > 1023 || v0.x - v2.x > 1023 || v2.x - v0.x > 1023 ||
        v1.x - v2.x > 1023 || v2.x - v1.x > 1023 || v0.y - v1.y > 511 || v1.y - v0.y > 511 ||
        v0.y - v2.y > 511 || v2.y - v0.y > 511 || v1.y - v2.y > 511 || v2.y - v1.y > 511) {
        return;
    }
    v0.x += g.ofs_x; v0.y += g.ofs_y;
    v1.x += g.ofs_x; v1.y += g.ofs_y;
    v2.x += g.ofs_x; v2.y += g.ofs_y;

    area = edge(&v0, &v1, v2.x, v2.y);
    if (area == 0) {
        return;
    }
    if (area < 0) {         /* orient so the edge functions are positive inside */
        t = v1; v1 = v2; v2 = t;
        area = -area;
    }
    min_x = v0.x < v1.x ? (v0.x < v2.x ? v0.x : v2.x) : (v1.x < v2.x ? v1.x : v2.x);
    max_x = v0.x > v1.x ? (v0.x > v2.x ? v0.x : v2.x) : (v1.x > v2.x ? v1.x : v2.x);
    min_y = v0.y < v1.y ? (v0.y < v2.y ? v0.y : v2.y) : (v1.y < v2.y ? v1.y : v2.y);
    max_y = v0.y > v1.y ? (v0.y > v2.y ? v0.y : v2.y) : (v1.y > v2.y ? v1.y : v2.y);
    if (min_x < g.area_x1) min_x = g.area_x1;
    if (max_x > g.area_x2) max_x = g.area_x2;
    if (min_y < g.area_y1) min_y = g.area_y1;
    if (max_y > g.area_y2) max_y = g.area_y2;

    {
        const int tl0 = top_left(&v1, &v2), tl1 = top_left(&v2, &v0), tl2 = top_left(&v0, &v1);
        for (y = min_y; y <= max_y; y++) {
            for (x = min_x; x <= max_x; x++) {
                const int64_t w0 = edge(&v1, &v2, x, y);
                const int64_t w1 = edge(&v2, &v0, x, y);
                const int64_t w2 = edge(&v0, &v1, x, y);
                int r, gr, b, u, v;

                if (w0 < 0 || w1 < 0 || w2 < 0 || (w0 == 0 && !tl0) || (w1 == 0 && !tl1) || (w2 == 0 && !tl2)) {
                    continue;
                }
                if (shaded) {
                    r = (int)floor_div(w0 * v0.r + w1 * v1.r + w2 * v2.r, area);
                    gr = (int)floor_div(w0 * v0.g + w1 * v1.g + w2 * v2.g, area);
                    b = (int)floor_div(w0 * v0.b + w1 * v1.b + w2 * v2.b, area);
                } else {
                    r = v0.r; gr = v0.g; b = v0.b;
                }
                u = (int)floor_div(w0 * v0.u + w1 * v1.u + w2 * v2.u, area);
                v = (int)floor_div(w0 * v0.v + w1 * v1.v + w2 * v2.v, area);
                put_pixel(x, y, clamp255(r), clamp255(gr), clamp255(b), p, u, v);
            }
        }
    }
}

/* ---- rectangles and lines -------------------------------------------------------------- */

static void draw_rect(int x, int y, int w, int h, int r, int gr, int b, int u0, int v0, const Prim *p) {
    int dx, dy;
    x += g.ofs_x;
    y += g.ofs_y;
    for (dy = 0; dy < h; dy++) {
        for (dx = 0; dx < w; dx++) {
            put_pixel(x + dx, y + dy, r, gr, b, p, u0 + dx, v0 + dy);
        }
    }
}

static void draw_line(Vtx a, Vtx b, const Prim *p, int shaded) {
    int dx, dy, steps, i;
    a.x += g.ofs_x; a.y += g.ofs_y;
    b.x += g.ofs_x; b.y += g.ofs_y;
    dx = b.x - a.x;
    dy = b.y - a.y;
    if (dx > 1023 || dx < -1023 || dy > 511 || dy < -511) {
        return;
    }
    steps = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx) : (dy < 0 ? -dy : dy);
    for (i = 0; i <= steps; i++) {
        const int x = steps ? a.x + (int)((int64_t)dx * i / steps) : a.x;
        const int y = steps ? a.y + (int)((int64_t)dy * i / steps) : a.y;
        int r = a.r, gr = a.g, bl = a.b;
        if (shaded && steps) {
            r = a.r + (b.r - a.r) * i / steps;
            gr = a.g + (b.g - a.g) * i / steps;
            bl = a.b + (b.b - a.b) * i / steps;
        }
        put_pixel(x, y, r, gr, bl, p, 0, 0);
    }
}

/* ---- command interpreter --------------------------------------------------------------- */

static int s11(uint32_t v) { return ((int)(v & 0x7FF) ^ 0x400) - 0x400; }
static int xs(uint32_t w) { return (int16_t)(w & 0xFFFF); }
static int ys(uint32_t w) { return (int16_t)(w >> 16); }

static void set_color(Vtx *v, uint32_t w) {
    v->r = (int)(w & 0xFF);
    v->g = (int)((w >> 8) & 0xFF);
    v->b = (int)((w >> 16) & 0xFF);
}

/* Returns the number of words consumed (0 on a truncated packet). */
static int exec_one(const uint32_t *w, int avail) {
    const uint32_t cmd = w[0] >> 24;
    const int kind = (int)(cmd >> 5);

    if (kind == 1) {                                       /* polygon */
        const int gouraud = (cmd >> 4) & 1, quad = (cmd >> 3) & 1, tex = (cmd >> 2) & 1;
        const int nv = quad ? 4 : 3;
        int need = 1, i, pos;
        Vtx v[4];
        Prim p;

        for (i = 0; i < nv; i++) {
            need += 1 + (tex ? 1 : 0) + ((gouraud && i > 0) ? 1 : 0);
        }
        if (avail < need) {
            return 0;
        }
        memset(v, 0, sizeof(v));
        memset(&p, 0, sizeof(p));
        p.textured = tex;
        p.semi = (cmd >> 1) & 1;
        p.raw = cmd & 1;
        pos = 1;
        for (i = 0; i < nv; i++) {
            /* Flat polygons carry one colour, in the command word; gouraud ones a colour
             * word before each vertex but the first. */
            set_color(&v[i], (i == 0 || !gouraud) ? w[0] : w[pos++]);
            v[i].x = xs(w[pos]);
            v[i].y = ys(w[pos]);
            pos++;
            if (tex) {
                const uint32_t uvw = w[pos++];
                v[i].u = (int)(uvw & 0xFF);
                v[i].v = (int)((uvw >> 8) & 0xFF);
                if (i == 0) {
                    p.clut_x = (int)((uvw >> 16) & 0x3F) * 16;
                    p.clut_y = (int)((uvw >> 22) & 0x1FF);
                } else if (i == 1) {
                    apply_texpage((uvw >> 16) & 0x1FF);
                }
            }
        }
        if (!gouraud) {
            for (i = 1; i < nv; i++) {
                v[i].r = v[0].r; v[i].g = v[0].g; v[i].b = v[0].b;
            }
        }
        draw_triangle(v[0], v[1], v[2], &p, gouraud);
        if (quad) {
            draw_triangle(v[1], v[2], v[3], &p, gouraud);
        }
        return need;
    }

    if (kind == 2) {                                       /* line (single segment) */
        const int gouraud = (cmd >> 4) & 1;
        const int need = gouraud ? 4 : 3;
        Vtx a, b;
        Prim p;
        if (avail < need) {
            return 0;
        }
        memset(&a, 0, sizeof(a));
        memset(&b, 0, sizeof(b));
        memset(&p, 0, sizeof(p));
        p.semi = (cmd >> 1) & 1;
        set_color(&a, w[0]);
        a.x = xs(w[1]); a.y = ys(w[1]);
        if (gouraud) {
            set_color(&b, w[2]);
            b.x = xs(w[3]); b.y = ys(w[3]);
        } else {
            b.r = a.r; b.g = a.g; b.b = a.b;
            b.x = xs(w[2]); b.y = ys(w[2]);
        }
        draw_line(a, b, &p, gouraud);
        return need;
    }

    if (kind == 3) {                                       /* rectangle */
        const int tex = (cmd >> 2) & 1, size = (int)((cmd >> 3) & 3);
        const int need = 2 + tex + (size == 0 ? 1 : 0);
        int pos = 2, rw = 1, rh = 1, u = 0, v = 0;
        Prim p;
        Vtx c;
        if (avail < need) {
            return 0;
        }
        memset(&p, 0, sizeof(p));
        memset(&c, 0, sizeof(c));
        p.textured = tex;
        p.semi = (cmd >> 1) & 1;
        p.raw = cmd & 1;
        set_color(&c, w[0]);
        if (tex) {
            const uint32_t uvw = w[pos++];
            u = (int)(uvw & 0xFF);
            v = (int)((uvw >> 8) & 0xFF);
            p.clut_x = (int)((uvw >> 16) & 0x3F) * 16;
            p.clut_y = (int)((uvw >> 22) & 0x1FF);
        }
        if (size == 0) {
            rw = (int)(w[pos] & 0x3FF);
            rh = (int)((w[pos] >> 16) & 0x1FF);
        } else {
            rw = rh = size == 1 ? 1 : (size == 2 ? 8 : 16);
        }
        draw_rect(xs(w[1]), ys(w[1]), rw, rh, c.r, c.g, c.b, u, v, &p);
        return need;
    }

    switch (cmd) {
    case 0x00: case 0x01: case 0xE0: case 0xE7: case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC:
    case 0xED: case 0xEE: case 0xEF:
        return 1;
    case 0x02: {                                           /* fill rectangle */
        PsxRect r;
        if (avail < 3) {
            return 0;
        }
        r.x = (int16_t)(w[1] & 0xFFFF); r.y = (int16_t)(w[1] >> 16);
        r.w = (int16_t)(w[2] & 0xFFFF); r.h = (int16_t)(w[2] >> 16);
        psx_gpu_fill(&r, (uint8_t)w[0], (uint8_t)(w[0] >> 8), (uint8_t)(w[0] >> 16));
        return 3;
    }
    case 0x80: {                                           /* VRAM to VRAM */
        PsxRect r;
        if (avail < 4) {
            return 0;
        }
        r.x = (int16_t)(w[1] & 0xFFFF); r.y = (int16_t)(w[1] >> 16);
        r.w = (int16_t)(w[3] & 0xFFFF); r.h = (int16_t)(w[3] >> 16);
        psx_gpu_move_image(&r, (int16_t)(w[2] & 0xFFFF), (int16_t)(w[2] >> 16));
        return 4;
    }
    case 0xA0: {                                           /* CPU to VRAM; the pixels follow */
        PsxRect r;
        int words;
        if (avail < 3) {
            return 0;
        }
        r.x = (int16_t)(w[1] & 0xFFFF); r.y = (int16_t)(w[1] >> 16);
        r.w = (int16_t)(w[2] & 0xFFFF); r.h = (int16_t)(w[2] >> 16);
        words = (r.w * r.h + 1) / 2;
        if (r.w <= 0 || r.h <= 0 || avail < 3 + words) {
            return avail < 3 + words ? 0 : 3;
        }
        psx_gpu_load_image(&r, (const uint16_t *)(const void *)(w + 3));
        return 3 + words;
    }
    case 0xE1:
        apply_draw_mode(w[0]);
        return 1;
    case 0xE2:
        g.tw_mask_x = (int)(w[0] & 0x1F);
        g.tw_mask_y = (int)((w[0] >> 5) & 0x1F);
        g.tw_off_x = (int)((w[0] >> 10) & 0x1F);
        g.tw_off_y = (int)((w[0] >> 15) & 0x1F);
        return 1;
    case 0xE3:
        g.area_x1 = (int)(w[0] & 0x3FF);
        g.area_y1 = (int)((w[0] >> 10) & 0x3FF);
        if (g.area_y1 > 511) g.area_y1 = 511;
        return 1;
    case 0xE4:
        g.area_x2 = (int)(w[0] & 0x3FF);
        g.area_y2 = (int)((w[0] >> 10) & 0x3FF);
        if (g.area_y2 > 511) g.area_y2 = 511;
        return 1;
    case 0xE5:
        g.ofs_x = s11(w[0]);
        g.ofs_y = s11(w[0] >> 11);
        return 1;
    case 0xE6:                                             /* mask bit settings: not modelled */
        return 1;
    default:
        psx_gpu_bad_commands++;
        return 1;
    }
}

void psx_gpu_exec(const uint32_t *words, int count) {
    int i = 0;
    while (i < count) {
        const int n = exec_one(words + i, count - i);
        if (n <= 0) {
            break;
        }
        i += n;
    }
}

void psx_gpu_draw_otag(uint32_t addr) {
    int guard;
    for (guard = 0; guard < (1 << 22); guard++) {
        const uint32_t *tag = (const uint32_t *)psx_ptr(addr, 4);
        uint32_t len, next;
        if (tag == NULL) {
            psx_gpu_bad_tags++;
            return;
        }
        len = *tag >> 24;
        next = *tag & 0xFFFFFF;
        if (len != 0) {
            const uint32_t *data = (const uint32_t *)psx_ptr(addr + 4, len * 4);
            if (data == NULL) {
                psx_gpu_bad_tags++;
                return;
            }
            psx_gpu_exec(data, (int)len);
        }
        if (next == 0xFFFFFF) {
            return;
        }
        addr = PSX_RAM_BASE | next;
    }
    psx_gpu_bad_tags++;                                    /* a cycle in the table */
}

/* End to end through the compat API, written the way the game's own code is: geometry
 * through the gte_* macros, packets from libgpu's structs, an ordering table at a console
 * address, DrawOTag, and the display read back. The expected picture comes from an
 * independent floating-point model of the same scene. */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <inline_c.h>
#include "gte.h"
#include "psx_gpu.h"
#include "psx_gte.h"
#include "psx_libgpu.h"
#include "psx_mem.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#define PI_D 3.14159265358979323846
#define OT_LEN 1024
#define OT ((u_long *)0x80100000u)
#define PRIMS ((POLY_F4 *)0x80110000u)

static const SVECTOR cube[8] = {
    {-200, -200, -200, 0}, {200, -200, -200, 0}, {-200, 200, -200, 0}, {200, 200, -200, 0},
    {-200, -200, 200, 0},  {200, -200, 200, 0},  {-200, 200, 200, 0},  {200, 200, 200, 0},
};
/* Quads in the SDK's "Z" vertex order (the fourth vertex is opposite the first). */
static const int faces[6][4] = {
    {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 6, 2}, {1, 5, 3, 7}, {4, 5, 0, 1}, {2, 3, 6, 7},
};
static const uint8_t colors[6][3] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 0}, {255, 0, 255}, {0, 255, 255}};

static int rv_vsync_calls;
static void vsync_hook(void) { rv_vsync_calls++; }

static void draw_cube(const SVECTOR *rot, int32_t distance, int *nprims) {
    MATRIX m;
    VECTOR t;
    int f, n = 0;

    memset(&m, 0, sizeof(m));
    RotMatrix((SVECTOR *)rot, &m);
    t.vx = 0; t.vy = 0; t.vz = distance;
    TransMatrix(&m, &t);
    SetRotMatrix(&m);
    SetTransMatrix(&m);

    ClearOTagR(OT, OT_LEN);
    for (f = 0; f < 6; f++) {
        POLY_F4 *p = &PRIMS[n];
        int32_t otz;

        gte_ldv3(&cube[faces[f][0]], &cube[faces[f][1]], &cube[faces[f][2]]);
        gte_rtpt();
        gte_stsxy3_f4(p);
        gte_ldv0(&cube[faces[f][3]]);
        gte_rtps();
        gte_stsxy(&p->x3);
        gte_avsz4();
        gte_stotz(&otz);
        if (otz <= 0 || otz >= OT_LEN) {
            continue;
        }
        setPolyF4(p);
        setRGB0(p, colors[f][0], colors[f][1], colors[f][2]);
        addPrim(&OT[otz], p);
        n++;
    }
    *nprims = n;
    DrawOTag(&OT[OT_LEN - 1]);
}

/* The colour of the face nearest the viewer along the centre ray, by a float model. */
static int nearest_face(const SVECTOR *rot, int32_t distance) {
    const double a = rot->vx * 2 * PI_D / 4096, b = rot->vy * 2 * PI_D / 4096;
    const double ca = cos(a), sa = sin(a), cb = cos(b), sb = sin(b);
    int f, best = -1;
    double best_z = 1e30;

    for (f = 0; f < 6; f++) {
        /* A face's centre is the mean of its four corners. */
        double cx = 0, cy = 0, cz = 0;
        int i;
        for (i = 0; i < 4; i++) {
            const SVECTOR *v = &cube[faces[f][i]];
            cx += v->vx / 4.0; cy += v->vy / 4.0; cz += v->vz / 4.0;
        }
        {   /* The depth of the centre under Rz * Ry * Rx: z is not touched by Rz. */
            const double z = cy * sa + cz * ca;
            const double depth = -cx * sb + z * cb + distance;
            if (depth < best_z) {
                best_z = depth;
                best = f;
            }
        }
    }
    return best;
}

int main(void) {
    SVECTOR rot[3] = {{300, 500, 0, 0}, {-250, 120, 0, 0}, {900, -700, 0, 0}};
    int i, w, h, nprims;
    static uint32_t frame[320 * 240];
    DRAWENV draw;
    DISPENV disp;
    FILE *fp;

    if (psx_mem_init() != 0) {
        printf("FAIL: psx_mem_init\n");
        return 1;
    }
    psx_vsync_hook = vsync_hook;
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(160, 120);
    SetGeomScreen(400);
    SetDefDrawEnv(&draw, 0, 0, 320, 240);
    SetDefDispEnv(&disp, 0, 0, 320, 240);
    draw.isbg = 1;
    draw.r0 = 20; draw.g0 = 20; draw.b0 = 40;
    PutDrawEnv(&draw);
    PutDispEnv(&disp);
    SetDispMask(1);

    for (i = 0; i < 3; i++) {
        PutDrawEnv(&draw);
        draw_cube(&rot[i], 1200, &nprims);
        CHECK(nprims == 6);
        psx_gpu_get_display(frame, 320 * 240, &w, &h);
        CHECK(w == 320 && h == 240);
        CHECK(frame[0] == 0xFF101029u);                         /* (20, 20, 40) kept to 5 bits per channel */
        {
            const int f = nearest_face(&rot[i], 1200);
            const uint32_t c = frame[120 * 320 + 160];
            const uint32_t expect = 0xFF000000u | ((colors[f][0] ? 0xFFu : 0u) << 16) |
                                    ((colors[f][1] ? 0xFFu : 0u) << 8) | (colors[f][2] ? 0xFFu : 0u);
            if (c != expect) {
                printf("frame %d: centre %08X, expected face %d (%08X)\n", i, c, f, expect);
            }
            CHECK(c == expect);
        }
        if (i == 0) {
            fp = fopen("/tmp/cube.ppm", "wb");
            if (fp != NULL) {
                int p;
                fprintf(fp, "P6\n320 240\n255\n");
                for (p = 0; p < 320 * 240; p++) {
                    fputc((frame[p] >> 16) & 255, fp);
                    fputc((frame[p] >> 8) & 255, fp);
                    fputc(frame[p] & 255, fp);
                }
                fclose(fp);
            }
        }
    }
    CHECK(psx_gpu_bad_tags == 0 && psx_gpu_bad_commands == 0 && psx_gte_unimplemented == 0);

    /* A textured sprite through the same path: POLY_FT4 with a 15-bit texture and DR_MODE. */
    {
        static uint16_t tex[16 * 16];
        RECT r = {320, 0, 16, 16};
        DR_MODE *mode = (DR_MODE *)0x80120000u;
        POLY_FT4 *quad = (POLY_FT4 *)0x80120100u;
        int x, y;

        for (y = 0; y < 16; y++) {
            for (x = 0; x < 16; x++) {
                tex[y * 16 + x] = (uint16_t)((x << 1) | ((y << 1) << 5) | 0x8000);
            }
        }
        LoadImage(&r, (u_long *)tex);
        PutDrawEnv(&draw);
        ClearOTagR(OT, OT_LEN);
        SetDrawMode(mode, 0, 1, getTPage(2, 0, 320, 0), NULL);
        setPolyFT4(quad);
        setRGB0(quad, 128, 128, 128);
        setXYWH(quad, 100, 100, 16, 16);
        setUVWH(quad, 0, 0, 15, 15);
        quad->tpage = getTPage(2, 0, 320, 0);
        quad->clut = 0;
        addPrim(&OT[10], quad);
        addPrim(&OT[10], mode);                              /* the mode packet runs first */
        DrawOTag(&OT[OT_LEN - 1]);
        {
            uint16_t *vram = psx_gpu_vram();
            const uint16_t t0 = vram[100 * 1024 + 100], t1 = vram[115 * 1024 + 115];
            CHECK((t0 & 31) == 0 && ((t0 >> 5) & 31) == 0);
            CHECK((t1 & 31) >= 25 && ((t1 >> 5) & 31) >= 25);   /* the far corner of the ramp (dithered) */
        }
    }

    /* VSync waits through the platform hook and counts blanks. */
    {
        const int before = psx_vblank_count();
        VSync(0);
        VSync(3);
        CHECK(psx_vblank_count() == before + 4 && rv_vsync_calls == 4);
        CHECK(VSync(-1) == before + 4);
    }

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("compat_render: ok\n");
    return 0;
}

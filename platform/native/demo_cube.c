#include <string.h>
#include <libgpu.h>
#include <libgs.h>
#include <libgte.h>
#include <inline_c.h>
#include "gte.h"
#include "demo_cube.h"
#include "psx_buttons.h"

#define OT_LEN 1024
#define OT ((u_long *)0x80100000u)          /* the ordering table and the packets live in console RAM */
#define PRIMS ((POLY_G4 *)0x80110000u)

static const SVECTOR corners[8] = {
    {-200, -200, -200, 0}, {200, -200, -200, 0}, {-200, 200, -200, 0}, {200, 200, -200, 0},
    {-200, -200, 200, 0},  {200, -200, 200, 0},  {-200, 200, 200, 0},  {200, 200, 200, 0},
};

/* Quads in the SDK's Z vertex order: the fourth corner is opposite the first. */
static const int faces[6][4] = {
    {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 6, 2}, {1, 5, 3, 7}, {4, 5, 0, 1}, {2, 3, 6, 7},
};

static const uint8_t face_color[6][3] = {
    {235, 70, 70}, {70, 200, 90}, {80, 120, 240}, {240, 220, 80}, {220, 90, 220}, {80, 220, 220},
};

static DRAWENV draw_env[2];
static DISPENV disp_env[2];
static int buffer;
static SVECTOR angle = {300, 500, 0, 0};
static int32_t distance = 1300;
static int spinning = 1;
static uint16_t previous_held;

void demo_init(void) {
    ResetGraph(0);
    InitGeom();
    SetGeomOffset(160, 120);
    SetGeomScreen(400);
    SetDefDrawEnv(&draw_env[0], 0, 0, 320, 240);
    SetDefDispEnv(&disp_env[0], 0, 0, 320, 240);
    SetDefDrawEnv(&draw_env[1], 0, 240, 320, 240);
    SetDefDispEnv(&disp_env[1], 0, 240, 320, 240);
    draw_env[0].isbg = draw_env[1].isbg = 1;
    draw_env[0].r0 = draw_env[1].r0 = 24;
    draw_env[0].g0 = draw_env[1].g0 = 28;
    draw_env[0].b0 = draw_env[1].b0 = 56;
    PutDispEnv(&disp_env[0]);
    SetDispMask(1);
}

static int draw_cube(void) {
    MATRIX m;
    VECTOR t;
    int f, n = 0;

    memset(&m, 0, sizeof(m));
    RotMatrix(&angle, &m);
    t.vx = 0;
    t.vy = 0;
    t.vz = distance;
    TransMatrix(&m, &t);
    SetRotMatrix(&m);
    SetTransMatrix(&m);

    ClearOTagR(OT, OT_LEN);
    for (f = 0; f < 6; f++) {
        POLY_G4 *p = &PRIMS[n];
        int32_t otz;

        gte_ldv3(&corners[faces[f][0]], &corners[faces[f][1]], &corners[faces[f][2]]);
        gte_rtpt();
        gte_stsxy3_g4(p);
        gte_ldv0(&corners[faces[f][3]]);
        gte_rtps();
        gte_stsxy(&p->x3);
        gte_avsz4();
        gte_stotz(&otz);
        if (otz <= 0 || otz >= OT_LEN) {
            continue;
        }
        setPolyG4(p);
        /* Shade the four corners from light to dark so the faces read as solids. */
        setRGB0(p, face_color[f][0], face_color[f][1], face_color[f][2]);
        setRGB1(p, face_color[f][0] * 3 / 4, face_color[f][1] * 3 / 4, face_color[f][2] * 3 / 4);
        setRGB2(p, face_color[f][0] * 3 / 4, face_color[f][1] * 3 / 4, face_color[f][2] * 3 / 4);
        setRGB3(p, face_color[f][0] / 2, face_color[f][1] / 2, face_color[f][2] / 2);
        addPrim(&OT[otz], p);
        n++;
    }
    DrawOTag(&OT[OT_LEN - 1]);
    return n;
}

void demo_frame(uint16_t held) {
    const uint16_t pressed = (uint16_t)(held & ~previous_held);

    if (held & PSX_PAD_UP) angle.vx -= 24;
    if (held & PSX_PAD_DOWN) angle.vx += 24;
    if (held & PSX_PAD_LEFT) angle.vy -= 24;
    if (held & PSX_PAD_RIGHT) angle.vy += 24;
    if ((held & PSX_PAD_L1) && distance > 700) distance -= 16;
    if ((held & PSX_PAD_R1) && distance < 3000) distance += 16;
    if (pressed & PSX_PAD_CROSS) spinning = !spinning;
    if (spinning) {
        angle.vy += 8;
        angle.vx += 5;
    }
    previous_held = held;

    buffer ^= 1;
    PutDrawEnv(&draw_env[buffer]);
    draw_cube();
    DrawSync(0);
    VSync(0);
    PutDispEnv(&disp_env[buffer]);
}

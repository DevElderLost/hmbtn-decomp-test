/* libgte on the software GTE: the geometry functions the game calls. Angles are 4096 to a
 * turn, sines and cosines are scaled by 4096, matrices are fixed point with 4096 = 1.0. */

#include <math.h>
#include <string.h>
#include <libgte.h>
#include <inline_c.h>
#include "psx_gte.h"

#define PI 3.14159265358979323846

/* ---- trigonometry ---------------------------------------------------------------------- */

static int16_t sin_table[4096];
static int sin_ready;

static void init_sin(void) {
    int i;
    if (sin_ready) {
        return;
    }
    for (i = 0; i < 4096; i++) {
        sin_table[i] = (int16_t)lround(sin(i * 2.0 * PI / 4096.0) * 4096.0);
    }
    sin_ready = 1;
}

int32_t rsin(int32_t a) {
    init_sin();
    return sin_table[a & 4095];
}

int32_t rcos(int32_t a) {
    init_sin();
    return sin_table[(a + 1024) & 4095];
}

int32_t csin(int32_t a) { return rsin(a); }
int32_t ccos(int32_t a) { return rcos(a); }

/* The angle of (x, y) around the origin, in -2048..2048. */
int32_t ratan2(int32_t y, int32_t x) {
    if (x == 0 && y == 0) {
        return 0;
    }
    return (int32_t)lround(atan2((double)y, (double)x) * 4096.0 / (2.0 * PI));
}

int32_t SquareRoot0(int32_t a) {
    return a <= 0 ? 0 : (int32_t)sqrt((double)(uint32_t)a);
}

/* The square root in 12-bit fixed point: sqrt(a / 4096) * 4096. */
int32_t csqrt(int32_t a) {
    return a <= 0 ? 0 : (int32_t)sqrt((double)(uint32_t)a * 4096.0);
}

/* ---- geometry set-up ------------------------------------------------------------------- */

void InitGeom(void) {
    psx_gte_reset();
    psx_gte_ctc2(GTEC_OFX, 0);
    psx_gte_ctc2(GTEC_OFY, 0);
    psx_gte_ctc2(GTEC_H, 1000);
    psx_gte_ctc2(GTEC_ZSF3, 0x155);
    psx_gte_ctc2(GTEC_ZSF4, 0x100);
}

void SetGeomOffset(int32_t ofx, int32_t ofy) {
    psx_gte_ctc2(GTEC_OFX, (uint32_t)ofx << 16);
    psx_gte_ctc2(GTEC_OFY, (uint32_t)ofy << 16);
}

void SetGeomScreen(int32_t h) { psx_gte_ctc2(GTEC_H, (uint32_t)h); }

void SetRotMatrix(MATRIX *m) { gte_SetRotMatrix(m); }
void SetTransMatrix(MATRIX *m) { gte_SetTransMatrix(m); }
void SetLightMatrix(MATRIX *m) { gte_SetLightMatrix(m); }
void SetColorMatrix(MATRIX *m) { gte_SetColorMatrix(m); }
void ReadRotMatrix(MATRIX *m) { gte_ReadRotMatrix(m); }

void SetBackColor(int32_t r, int32_t g, int32_t b) { gte_SetBackColor(r, g, b); }

void SetFarColor(int32_t r, int32_t g, int32_t b) {
    psx_gte_ctc2(GTEC_RFC, (uint32_t)r << 4);
    psx_gte_ctc2(GTEC_GFC, (uint32_t)g << 4);
    psx_gte_ctc2(GTEC_BFC, (uint32_t)b << 4);
}

/* Depth cue from distance `a` (none) to 3a (full), with the projection distance h. */
void SetFogNear(int32_t a, int32_t h) {
    if (h == 0) {
        return;
    }
    psx_gte_ctc2(GTEC_DQA, (uint32_t)(int32_t)(-384 * a / h) & 0xFFFF);
    psx_gte_ctc2(GTEC_DQB, 0x1800000);
}

/* ---- matrices -------------------------------------------------------------------------- */

static void mul3(const int16_t a[3][3], const int16_t b[3][3], int16_t out[3][3]) {
    int16_t t[3][3];
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            t[i][j] = (int16_t)(((int64_t)a[i][0] * b[0][j] + (int64_t)a[i][1] * b[1][j] +
                                 (int64_t)a[i][2] * b[2][j]) >> 12);
        }
    }
    memcpy(out, t, sizeof(t));
}

static void axis_matrix(int axis, int32_t angle, int16_t out[3][3]) {
    const int16_t c = (int16_t)rcos(angle), s = (int16_t)rsin(angle);
    memset(out, 0, 18);
    switch (axis) {
    case 0: out[0][0] = 4096; out[1][1] = c; out[1][2] = (int16_t)-s; out[2][1] = s; out[2][2] = c; break;
    case 1: out[1][1] = 4096; out[0][0] = c; out[0][2] = s; out[2][0] = (int16_t)-s; out[2][2] = c; break;
    default: out[2][2] = 4096; out[0][0] = c; out[0][1] = (int16_t)-s; out[1][0] = s; out[1][1] = c; break;
    }
}

/* Rotate m about an axis: m = R * m. */
static MATRIX *rotate_about(int axis, int32_t angle, MATRIX *m) {
    int16_t r[3][3];
    axis_matrix(axis, angle, r);
    mul3(r, m->m, m->m);
    return m;
}

MATRIX *RotMatrixX(int32_t angle, MATRIX *m) { return rotate_about(0, angle, m); }
MATRIX *RotMatrixY(int32_t angle, MATRIX *m) { return rotate_about(1, angle, m); }
MATRIX *RotMatrixZ(int32_t angle, MATRIX *m) { return rotate_about(2, angle, m); }

/* Build a rotation matrix from three angles as a product of the axis rotations, applied in
 * the order given (the last one is the outermost). The translation is left alone. */
static MATRIX *compose(const int order[3], const int32_t angles[3], MATRIX *m) {
    int16_t r[3][3], acc[3][3];
    int i;
    memset(acc, 0, sizeof(acc));
    acc[0][0] = acc[1][1] = acc[2][2] = 4096;
    for (i = 0; i < 3; i++) {
        axis_matrix(order[i], angles[order[i]], r);
        mul3(r, acc, acc);
    }
    memcpy(m->m, acc, sizeof(acc));
    return m;
}

/* RotMatrix: x first, then y, then z (m = Rz * Ry * Rx). */
MATRIX *RotMatrix(SVECTOR *r, MATRIX *m) {
    static const int order[3] = {0, 1, 2};
    const int32_t a[3] = {r->vx, r->vy, r->vz};
    return compose(order, a, m);
}

MATRIX *RotMatrixZYX(SVECTOR *r, MATRIX *m) { return RotMatrix(r, m); }

/* RotMatrixYXZ: z first, then x, then y (m = Ry * Rx * Rz). */
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m) {
    static const int order[3] = {2, 0, 1};
    const int32_t a[3] = {r->vx, r->vy, r->vz};
    return compose(order, a, m);
}

/* RotMatrixZXY: y first, then x, then z (m = Rz * Rx * Ry). */
MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m) {
    static const int order[3] = {1, 0, 2};
    const int32_t a[3] = {r->vx, r->vy, r->vz};
    return compose(order, a, m);
}

MATRIX *TransMatrix(MATRIX *m, VECTOR *v) {
    m->t[0] = v->vx;
    m->t[1] = v->vy;
    m->t[2] = v->vz;
    return m;
}

MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2) {
    mul3(m0->m, m1->m, m2->m);
    return m2;
}

#define MATRIX_STACK 20
static struct {
    uint32_t rt[5];
    uint32_t tr[3];
} matrix_stack[MATRIX_STACK];
static int matrix_depth;

void PushMatrix(void) {
    int i;
    if (matrix_depth >= MATRIX_STACK) {
        return;
    }
    for (i = 0; i < 5; i++) matrix_stack[matrix_depth].rt[i] = psx_gte_cfc2(GTEC_RT0 + i);
    for (i = 0; i < 3; i++) matrix_stack[matrix_depth].tr[i] = psx_gte_cfc2(GTEC_TRX + i);
    matrix_depth++;
}

void PopMatrix(void) {
    int i;
    if (matrix_depth <= 0) {
        return;
    }
    matrix_depth--;
    for (i = 0; i < 5; i++) psx_gte_ctc2(GTEC_RT0 + i, matrix_stack[matrix_depth].rt[i]);
    for (i = 0; i < 3; i++) psx_gte_ctc2(GTEC_TRX + i, matrix_stack[matrix_depth].tr[i]);
}

/* ---- vectors --------------------------------------------------------------------------- */

/* v1 = RT * v0 + TR, through the GTE; *flag receives its FLAG register. */
int32_t RotTrans(SVECTOR *v0, VECTOR *v1, int32_t *flag) {
    gte_ldv0(v0);
    psx_gte_exec(0x4A480012);
    gte_stlvnl(v1);
    if (flag != NULL) {
        *flag = (int32_t)psx_gte_cfc2(GTEC_FLAG);
    }
    return (int32_t)(psx_gte_mfc2(GTE_MAC3) >> 2);
}

VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1) {
    const int64_t x = v0->vx, y = v0->vy, z = v0->vz;
    int32_t out[3];
    int i;
    for (i = 0; i < 3; i++) {
        out[i] = (int32_t)((m->m[i][0] * x + m->m[i][1] * y + m->m[i][2] * z) >> 12);
    }
    v1->vx = out[0];
    v1->vy = out[1];
    v1->vz = out[2];
    return v1;
}

/* v2 = v0 x v1, unscaled. */
VECTOR *OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2) {
    const int64_t ax = v0->vx, ay = v0->vy, az = v0->vz, bx = v1->vx, by = v1->vy, bz = v1->vz;
    v2->vx = (int32_t)(ay * bz - az * by);
    v2->vy = (int32_t)(az * bx - ax * bz);
    v2->vz = (int32_t)(ax * by - ay * bx);
    return v2;
}

VECTOR *OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2) {
    gte_OuterProduct12(v0, v1, v2);
    return v2;
}

/* Scale (x, y, z) to a length of 4096. Returns the squared length. */
static int32_t normalize(int64_t x, int64_t y, int64_t z, int32_t out[3]) {
    const int64_t sq = x * x + y * y + z * z;
    const double len = sqrt((double)sq);
    if (sq == 0) {
        out[0] = out[1] = out[2] = 0;
        return 0;
    }
    out[0] = (int32_t)lround((double)x * 4096.0 / len);
    out[1] = (int32_t)lround((double)y * 4096.0 / len);
    out[2] = (int32_t)lround((double)z * 4096.0 / len);
    return (int32_t)sq;
}

int32_t VectorNormal(VECTOR *v0, VECTOR *v1) {
    int32_t o[3];
    const int32_t sq = normalize(v0->vx, v0->vy, v0->vz, o);
    v1->vx = o[0]; v1->vy = o[1]; v1->vz = o[2];
    return sq;
}

int32_t VectorNormalS(VECTOR *v0, SVECTOR *v1) {
    int32_t o[3];
    const int32_t sq = normalize(v0->vx, v0->vy, v0->vz, o);
    v1->vx = (int16_t)o[0]; v1->vy = (int16_t)o[1]; v1->vz = (int16_t)o[2];
    return sq;
}

int32_t VectorNormalSS(SVECTOR *v0, SVECTOR *v1) {
    int32_t o[3];
    const int32_t sq = normalize(v0->vx, v0->vy, v0->vz, o);
    v1->vx = (int16_t)o[0]; v1->vy = (int16_t)o[1]; v1->vz = (int16_t)o[2];
    return sq;
}

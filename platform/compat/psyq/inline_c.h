#ifndef PSYQ_COMPAT_INLINE_C_H
#define PSYQ_COMPAT_INLINE_C_H

/* GTE load/store macros (the SDK's inline_c.h), built on the software GTE's register moves.
 * Commands themselves are issued through the game's gte.h (see compat/override). */

#include <string.h>
#include <libgte.h>
#include "psx_gte.h"

static inline void hmbtn_lwc2(int reg, const void *p) {
    uint32_t w;
    memcpy(&w, p, 4);
    psx_gte_mtc2(reg, w);
}

static inline void hmbtn_swc2(int reg, void *p) {
    const uint32_t w = psx_gte_mfc2(reg);
    memcpy(p, &w, 4);
}

static inline void hmbtn_lh(int reg, const void *p) {
    int16_t h;
    memcpy(&h, p, 2);
    psx_gte_mtc2(reg, (uint32_t)(int32_t)h);
}

static inline void hmbtn_sh(int reg, void *p) {
    const uint16_t h = (uint16_t)psx_gte_mfc2(reg);
    memcpy(p, &h, 2);
}

/* Vectors in: V0 to V2 from SVECTORs. */
#define gte_ldv0(r) (hmbtn_lwc2(GTE_VXY0, (const char *)(r)), hmbtn_lwc2(GTE_VZ0, (const char *)(r) + 4))
#define gte_ldv1(r) (hmbtn_lwc2(GTE_VXY1, (const char *)(r)), hmbtn_lwc2(GTE_VZ1, (const char *)(r) + 4))
#define gte_ldv2(r) (hmbtn_lwc2(GTE_VXY2, (const char *)(r)), hmbtn_lwc2(GTE_VZ2, (const char *)(r) + 4))
#define gte_ldv3(r0, r1, r2) (gte_ldv0(r0), gte_ldv1(r1), gte_ldv2(r2))

/* A long vector's low halves into V0, or into IR1..IR3. */
static inline void hmbtn_ldlv0(const VECTOR *v) {
    psx_gte_mtc2(GTE_VXY0, ((uint32_t)v->vx & 0xFFFF) | ((uint32_t)v->vy << 16));
    psx_gte_mtc2(GTE_VZ0, (uint32_t)v->vz & 0xFFFF);
}

static inline void hmbtn_ldlvl(const VECTOR *v) {
    psx_gte_mtc2(GTE_IR1, (uint32_t)v->vx);
    psx_gte_mtc2(GTE_IR2, (uint32_t)v->vy);
    psx_gte_mtc2(GTE_IR3, (uint32_t)v->vz);
}

#define gte_ldlv0(r) hmbtn_ldlv0((const VECTOR *)(r))
#define gte_ldlvl(r) hmbtn_ldlvl((const VECTOR *)(r))

/* Column 0 of a rotation matrix to and from IR1..IR3. */
#define gte_ldclmv(r)                                                                         \
    (hmbtn_lh(GTE_IR1, (const char *)(r)), hmbtn_lh(GTE_IR2, (const char *)(r) + 6),            \
     hmbtn_lh(GTE_IR3, (const char *)(r) + 12))
#define gte_stclmv(r)                                                                         \
    (hmbtn_sh(GTE_IR1, (char *)(r)), hmbtn_sh(GTE_IR2, (char *)(r) + 6), hmbtn_sh(GTE_IR3, (char *)(r) + 12))

/* Results out. */
#define gte_stsxy(r) hmbtn_swc2(GTE_SXY2, (r))
#define gte_stsxy2(r) hmbtn_swc2(GTE_SXY2, (r))
#define gte_stsxy01(r0, r1) (hmbtn_swc2(GTE_SXY0, (r0)), hmbtn_swc2(GTE_SXY1, (r1)))
#define gte_stotz(r) (*(int32_t *)(r) = (int32_t)psx_gte_mfc2(GTE_OTZ))
#define gte_stopz(r) (*(int32_t *)(r) = (int32_t)psx_gte_mfc2(GTE_MAC0))
#define gte_stflg(r) (*(int32_t *)(r) = (int32_t)psx_gte_cfc2(GTEC_FLAG))
#define gte_stszotz(r) (*(int32_t *)(r) = (int32_t)(psx_gte_mfc2(GTE_SZ3) >> 2))
#define gte_stlvnl(r)                                                                         \
    (((VECTOR *)(r))->vx = (int32_t)psx_gte_mfc2(GTE_MAC1), ((VECTOR *)(r))->vy = (int32_t)psx_gte_mfc2(GTE_MAC2), \
     ((VECTOR *)(r))->vz = (int32_t)psx_gte_mfc2(GTE_MAC3))
#define gte_stlvl(r)                                                                          \
    (((VECTOR *)(r))->vx = (int32_t)psx_gte_mfc2(GTE_IR1), ((VECTOR *)(r))->vy = (int32_t)psx_gte_mfc2(GTE_IR2), \
     ((VECTOR *)(r))->vz = (int32_t)psx_gte_mfc2(GTE_IR3))

/* The first three projected points straight into a polygon's coordinate words. The macros
 * take any pointer to the packet and use the packet layout's byte offsets of x0, x1, x2. */
#define HMBTN_STSXY3(p, o0, o1, o2)                                                           \
    (hmbtn_swc2(GTE_SXY0, (char *)(p) + (o0)), hmbtn_swc2(GTE_SXY1, (char *)(p) + (o1)),       \
     hmbtn_swc2(GTE_SXY2, (char *)(p) + (o2)))
#define gte_stsxy3_f3(p) HMBTN_STSXY3(p, 8, 12, 16)
#define gte_stsxy3_f4(p) HMBTN_STSXY3(p, 8, 12, 16)
#define gte_stsxy3_g3(p) HMBTN_STSXY3(p, 8, 16, 24)
#define gte_stsxy3_g4(p) HMBTN_STSXY3(p, 8, 16, 24)
#define gte_stsxy3_ft3(p) HMBTN_STSXY3(p, 8, 16, 24)
#define gte_stsxy3_ft4(p) HMBTN_STSXY3(p, 8, 16, 24)
#define gte_stsxy3_gt3(p) HMBTN_STSXY3(p, 8, 20, 32)
#define gte_stsxy3_gt4(p) HMBTN_STSXY3(p, 8, 20, 32)

/* Matrices and colours in. The rotation and translation registers follow MATRIX's layout. */
static inline void hmbtn_load_matrix(int base, const MATRIX *m) {
    uint32_t w[5] = {0};
    memcpy(w, m->m, 18);
    psx_gte_ctc2(base + 0, w[0]);
    psx_gte_ctc2(base + 1, w[1]);
    psx_gte_ctc2(base + 2, w[2]);
    psx_gte_ctc2(base + 3, w[3]);
    psx_gte_ctc2(base + 4, (uint32_t)(uint16_t)m->m[2][2]);
}

static inline void hmbtn_read_matrix(int base, MATRIX *m) {
    uint32_t w[5];
    int i;
    for (i = 0; i < 5; i++) {
        w[i] = psx_gte_cfc2(base + i);
    }
    m->m[0][0] = (int16_t)w[0]; m->m[0][1] = (int16_t)(w[0] >> 16); m->m[0][2] = (int16_t)w[1];
    m->m[1][0] = (int16_t)(w[1] >> 16); m->m[1][1] = (int16_t)w[2]; m->m[1][2] = (int16_t)(w[2] >> 16);
    m->m[2][0] = (int16_t)w[3]; m->m[2][1] = (int16_t)(w[3] >> 16); m->m[2][2] = (int16_t)w[4];
}

#define gte_SetRotMatrix(m) hmbtn_load_matrix(GTEC_RT0, (m))
#define gte_SetLightMatrix(m) hmbtn_load_matrix(GTEC_L0, (m))
#define gte_SetColorMatrix(m) hmbtn_load_matrix(GTEC_LC0, (m))
#define gte_SetTransMatrix(m) \
    (psx_gte_ctc2(GTEC_TRX, (uint32_t)(m)->t[0]), psx_gte_ctc2(GTEC_TRY, (uint32_t)(m)->t[1]), \
     psx_gte_ctc2(GTEC_TRZ, (uint32_t)(m)->t[2]))
#define gte_ReadRotMatrix(m)                                                                  \
    (hmbtn_read_matrix(GTEC_RT0, (m)), (m)->t[0] = (int32_t)psx_gte_cfc2(GTEC_TRX),           \
     (m)->t[1] = (int32_t)psx_gte_cfc2(GTEC_TRY), (m)->t[2] = (int32_t)psx_gte_cfc2(GTEC_TRZ))
#define gte_SetBackColor(r, g, b)                                                             \
    (psx_gte_ctc2(GTEC_RBK, (uint32_t)(r) << 4), psx_gte_ctc2(GTEC_GBK, (uint32_t)(g) << 4),   \
     psx_gte_ctc2(GTEC_BBK, (uint32_t)(b) << 4))

/* The operand loads of the cross product: the first vector in the rotation diagonal, the
 * second in IR. */
#define gte_OuterProduct12(r0, r1, r2)                                                        \
    (hmbtn_op12((const VECTOR *)(r0), (const VECTOR *)(r1), (VECTOR *)(r2)))
static inline void hmbtn_op12(const VECTOR *a, const VECTOR *b, VECTOR *out) {
    /* The GTE sees only the low 16 bits of each component. */
    const int64_t ax = (int16_t)a->vx, ay = (int16_t)a->vy, az = (int16_t)a->vz;
    const int64_t bx = (int16_t)b->vx, by = (int16_t)b->vy, bz = (int16_t)b->vz;
    out->vx = (int32_t)((ay * bz - az * by) >> 12);
    out->vy = (int32_t)((az * bx - ax * bz) >> 12);
    out->vz = (int32_t)((ax * by - ay * bx) >> 12);
}

#define gte_MulMatrix0(m0, m1, m2) ((void)MulMatrix0((m0), (m1), (m2)))

#endif /* PSYQ_COMPAT_INLINE_C_H */

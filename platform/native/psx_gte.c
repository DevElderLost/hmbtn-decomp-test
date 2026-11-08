#include <stdio.h>
#include <string.h>
#include "psx_gte.h"

PsxGte psx_gte;
unsigned psx_gte_unimplemented;

/* FLAG bits. */
#define F_MAC1_POS (1u << 30)
#define F_MAC2_POS (1u << 29)
#define F_MAC3_POS (1u << 28)
#define F_MAC1_NEG (1u << 27)
#define F_MAC2_NEG (1u << 26)
#define F_MAC3_NEG (1u << 25)
#define F_IR1 (1u << 24)
#define F_IR2 (1u << 23)
#define F_IR3 (1u << 22)
#define F_COL_R (1u << 21)
#define F_COL_G (1u << 20)
#define F_COL_B (1u << 19)
#define F_SZ_OTZ (1u << 18)
#define F_DIV (1u << 17)
#define F_MAC0_POS (1u << 16)
#define F_MAC0_NEG (1u << 15)
#define F_SX (1u << 14)
#define F_SY (1u << 13)
#define F_IR0 (1u << 12)
/* Bits that set the summary bit 31. */
#define F_ERROR_MASK 0x7F87E000u

#define D psx_gte.d
#define C psx_gte.c

static uint32_t flag;

static int32_t s16(uint32_t v) { return (int16_t)(uint16_t)v; }
static int16_t lo16(uint32_t v) { return (int16_t)(uint16_t)v; }
static int16_t hi16(uint32_t v) { return (int16_t)(uint16_t)(v >> 16); }

void psx_gte_reset(void) {
    memset(&psx_gte, 0, sizeof(psx_gte));
    psx_gte_unimplemented = 0;
}

/* ---- register moves -------------------------------------------------------------------- */

static int32_t sat_ir_value(int32_t v, int lm) {
    const int32_t lo = lm ? 0 : -0x8000;
    return v < lo ? lo : (v > 0x7FFF ? 0x7FFF : v);
}

static void push_sxy(uint32_t xy) {
    D[GTE_SXY0] = D[GTE_SXY1];
    D[GTE_SXY1] = D[GTE_SXY2];
    D[GTE_SXY2] = xy;
}

uint32_t psx_gte_mfc2(int reg) {
    switch (reg) {
    case GTE_VZ0: case GTE_VZ1: case GTE_VZ2:
    case GTE_IR0: case GTE_IR1: case GTE_IR2: case GTE_IR3:
        return (uint32_t)s16(D[reg]);                   /* sign-extended halfwords */
    case GTE_OTZ: case GTE_SZ0: case GTE_SZ1: case GTE_SZ2: case GTE_SZ3:
        return D[reg] & 0xFFFF;
    case GTE_SXYP:
        return D[GTE_SXY2];
    case GTE_ORGB: {
        uint32_t r = (uint32_t)sat_ir_value(s16(D[GTE_IR1]) >> 7, 1);
        uint32_t g = (uint32_t)sat_ir_value(s16(D[GTE_IR2]) >> 7, 1);
        uint32_t b = (uint32_t)sat_ir_value(s16(D[GTE_IR3]) >> 7, 1);
        r = r > 0x1F ? 0x1F : r;
        g = g > 0x1F ? 0x1F : g;
        b = b > 0x1F ? 0x1F : b;
        return r | (g << 5) | (b << 10);
    }
    default:
        return D[reg];
    }
}

void psx_gte_mtc2(int reg, uint32_t v) {
    switch (reg) {
    case GTE_SXYP:
        push_sxy(v);
        break;
    case GTE_IRGB:
        D[GTE_IRGB] = v & 0x7FFF;
        D[GTE_IR1] = (v & 0x1F) * 0x80;
        D[GTE_IR2] = ((v >> 5) & 0x1F) * 0x80;
        D[GTE_IR3] = ((v >> 10) & 0x1F) * 0x80;
        break;
    case GTE_ORGB:
    case GTE_LZCR:
        break;                                          /* read only */
    case GTE_LZCS: {
        uint32_t x = (v & 0x80000000u) ? ~v : v;
        uint32_t n = 0;
        D[GTE_LZCS] = v;
        while (n < 32 && !(x & (0x80000000u >> n))) {
            n++;
        }
        D[GTE_LZCR] = n;
        break;
    }
    case GTE_OTZ: case GTE_SZ0: case GTE_SZ1: case GTE_SZ2: case GTE_SZ3:
        D[reg] = v & 0xFFFF;
        break;
    default:
        D[reg] = v;
        break;
    }
}

uint32_t psx_gte_cfc2(int reg) {
    switch (reg) {
    case GTEC_RT4: case GTEC_L4: case GTEC_LC4:
    case GTEC_H: case GTEC_DQA: case GTEC_ZSF3: case GTEC_ZSF4:
        return (uint32_t)s16(C[reg]);                   /* H reads back sign-extended (a known quirk) */
    default:
        return C[reg];
    }
}

void psx_gte_ctc2(int reg, uint32_t v) {
    if (reg == GTEC_FLAG) {
        v &= 0x7FFFF000u;
        if (v & F_ERROR_MASK) {
            v |= 0x80000000u;
        }
    }
    C[reg] = v;
}

/* ---- arithmetic helpers ---------------------------------------------------------------- */

/* A MAC1..3 accumulator is 44 bits wide; record overflow and wrap like the hardware. */
static int64_t check_mac(int n, int64_t v) {
    static const uint32_t pos[3] = {F_MAC1_POS, F_MAC2_POS, F_MAC3_POS};
    static const uint32_t neg[3] = {F_MAC1_NEG, F_MAC2_NEG, F_MAC3_NEG};
    if (v > 0x7FFFFFFFFFFLL) {
        flag |= pos[n];
    } else if (v < -0x80000000000LL) {
        flag |= neg[n];
    }
    v &= 0xFFFFFFFFFFFLL;
    if (v & 0x80000000000LL) {
        v -= 0x100000000000LL;
    }
    return v;
}

static int32_t check_mac0(int64_t v) {
    if (v > 0x7FFFFFFFLL) {
        flag |= F_MAC0_POS;
    } else if (v < -0x80000000LL) {
        flag |= F_MAC0_NEG;
    }
    return (int32_t)v;
}

static int32_t sat_ir(int n, int32_t v, int lm) {
    static const uint32_t bits[4] = {F_IR1, F_IR1, F_IR2, F_IR3};
    const int32_t r = sat_ir_value(v, lm);
    if (r != v) {
        flag |= bits[n];
    }
    return r;
}

static void set_ir(int n, int32_t v) { D[GTE_IR0 + n] = (uint32_t)(v & 0xFFFF); }
static void set_mac(int n, int32_t v) { D[GTE_MAC0 + n] = (uint32_t)v; }

static int32_t sat_otz(int64_t v) {
    if (v < 0) { flag |= F_SZ_OTZ; return 0; }
    if (v > 0xFFFF) { flag |= F_SZ_OTZ; return 0xFFFF; }
    return (int32_t)v;
}

static void push_sz(int32_t z) {
    D[GTE_SZ0] = D[GTE_SZ1];
    D[GTE_SZ1] = D[GTE_SZ2];
    D[GTE_SZ2] = D[GTE_SZ3];
    D[GTE_SZ3] = (uint32_t)z & 0xFFFF;
}

static void push_color(void) {
    int32_t r = (int32_t)D[GTE_MAC1] >> 4;
    int32_t g = (int32_t)D[GTE_MAC2] >> 4;
    int32_t b = (int32_t)D[GTE_MAC3] >> 4;
    if (r < 0) { r = 0; flag |= F_COL_R; } else if (r > 255) { r = 255; flag |= F_COL_R; }
    if (g < 0) { g = 0; flag |= F_COL_G; } else if (g > 255) { g = 255; flag |= F_COL_G; }
    if (b < 0) { b = 0; flag |= F_COL_B; } else if (b > 255) { b = 255; flag |= F_COL_B; }
    D[GTE_RGB0] = D[GTE_RGB1];
    D[GTE_RGB1] = D[GTE_RGB2];
    D[GTE_RGB2] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | (D[GTE_RGBC] & 0xFF000000u);
}

/* The unsigned Newton-Raphson divide of the perspective step: H*0x20000/SZ3, rounded, at
 * most 0x1FFFF. */
static uint32_t divide(uint32_t h, uint32_t sz3) {
    uint64_t n;
    if (h >= sz3 * 2u) {
        flag |= F_DIV;
        return 0x1FFFF;
    }
    n = (((uint64_t)h * 0x20000u) / sz3 + 1) / 2;
    return n > 0x1FFFF ? 0x1FFFF : (uint32_t)n;
}

static void load_matrix(int base, int32_t m[3][3], int is_control) {
    const uint32_t *r = is_control ? &C[base] : &D[base];
    m[0][0] = lo16(r[0]); m[0][1] = hi16(r[0]); m[0][2] = lo16(r[1]);
    m[1][0] = hi16(r[1]); m[1][1] = lo16(r[2]); m[1][2] = hi16(r[2]);
    m[2][0] = lo16(r[3]); m[2][1] = hi16(r[3]); m[2][2] = lo16(r[4]);
}

static void load_vector(int i, int32_t v[3]) {
    v[0] = lo16(D[GTE_VXY0 + 2 * i]);
    v[1] = hi16(D[GTE_VXY0 + 2 * i]);
    v[2] = lo16(D[GTE_VZ0 + 2 * i]);
}

/* ---- commands -------------------------------------------------------------------------- */

static void cmd_rtp(int vi, int sf, int lm, int last) {
    int32_t m[3][3], v[3];
    int64_t a[3];
    int i;
    uint32_t n;
    int64_t m0;
    int32_t sz;
    const int shift = sf ? 12 : 0;

    load_matrix(GTEC_RT0, m, 1);
    load_vector(vi, v);
    for (i = 0; i < 3; i++) {
        a[i] = ((int64_t)(int32_t)C[GTEC_TRX + i] << 12) + (int64_t)m[i][0] * v[0] + (int64_t)m[i][1] * v[1] +
               (int64_t)m[i][2] * v[2];
        a[i] = check_mac(i, a[i]);
        set_mac(i + 1, (int32_t)(a[i] >> shift));
    }
    set_ir(1, sat_ir(1, (int32_t)D[GTE_MAC1], lm));
    set_ir(2, sat_ir(2, (int32_t)D[GTE_MAC2], lm));
    /* IR3's saturation flag looks at the unshifted-by-sf value (a known hardware quirk). */
    {
        const int32_t z = (int32_t)(a[2] >> 12);
        if (z < -0x8000 || z > 0x7FFF) {
            flag |= F_IR3;
        }
        set_ir(3, sat_ir_value((int32_t)D[GTE_MAC3], lm));
    }
    sz = (int32_t)(a[2] >> 12);
    push_sz(sat_otz(sz));

    n = divide((uint32_t)(C[GTEC_H] & 0xFFFF), D[GTE_SZ3] & 0xFFFF);

    m0 = (int64_t)n * s16(D[GTE_IR1]) + (int32_t)C[GTEC_OFX];
    {
        int32_t x = check_mac0(m0) >> 16;
        if (x < -0x400) { x = -0x400; flag |= F_SX; } else if (x > 0x3FF) { x = 0x3FF; flag |= F_SX; }
        D[GTE_MAC0] = (uint32_t)check_mac0(m0);
        m0 = (int64_t)n * s16(D[GTE_IR2]) + (int32_t)C[GTEC_OFY];
        {
            int32_t y = check_mac0(m0) >> 16;
            if (y < -0x400) { y = -0x400; flag |= F_SY; } else if (y > 0x3FF) { y = 0x3FF; flag |= F_SY; }
            push_sxy((uint32_t)(x & 0xFFFF) | ((uint32_t)(y & 0xFFFF) << 16));
        }
    }
    if (last) {
        int32_t ir0;
        m0 = (int64_t)n * s16(C[GTEC_DQA]) + (int32_t)C[GTEC_DQB];
        D[GTE_MAC0] = (uint32_t)check_mac0(m0);
        ir0 = (int32_t)D[GTE_MAC0] >> 12;
        if (ir0 < 0) { ir0 = 0; flag |= F_IR0; } else if (ir0 > 0x1000) { ir0 = 0x1000; flag |= F_IR0; }
        D[GTE_IR0] = (uint32_t)ir0;
    }
}

static void cmd_nclip(void) {
    const int32_t x0 = lo16(D[GTE_SXY0]), y0 = hi16(D[GTE_SXY0]);
    const int32_t x1 = lo16(D[GTE_SXY1]), y1 = hi16(D[GTE_SXY1]);
    const int32_t x2 = lo16(D[GTE_SXY2]), y2 = hi16(D[GTE_SXY2]);
    const int64_t v = (int64_t)x0 * (y1 - y2) + (int64_t)x1 * (y2 - y0) + (int64_t)x2 * (y0 - y1);
    D[GTE_MAC0] = (uint32_t)check_mac0(v);
}

static void cmd_avsz(int four) {
    int64_t v;
    if (four) {
        v = (int64_t)s16(C[GTEC_ZSF4]) * (int64_t)((D[GTE_SZ0] & 0xFFFF) + (D[GTE_SZ1] & 0xFFFF) +
                                                    (D[GTE_SZ2] & 0xFFFF) + (D[GTE_SZ3] & 0xFFFF));
    } else {
        v = (int64_t)s16(C[GTEC_ZSF3]) *
            (int64_t)((D[GTE_SZ1] & 0xFFFF) + (D[GTE_SZ2] & 0xFFFF) + (D[GTE_SZ3] & 0xFFFF));
    }
    D[GTE_MAC0] = (uint32_t)check_mac0(v);
    D[GTE_OTZ] = (uint32_t)sat_otz((int32_t)D[GTE_MAC0] >> 12);
}

static void cmd_mvmva(int sf, int lm, int mx, int vv, int cv) {
    int32_t m[3][3], v[3];
    int64_t a[3];
    int i;
    const int shift = sf ? 12 : 0;

    switch (mx) {
    case 0: load_matrix(GTEC_RT0, m, 1); break;
    case 1: load_matrix(GTEC_L0, m, 1); break;
    case 2: load_matrix(GTEC_LC0, m, 1); break;
    default: {  /* the "garbage" matrix the hardware builds for mx=3 */
        const int32_t rt[3][3] = {{0}};
        int32_t rtm[3][3];
        (void)rt;
        load_matrix(GTEC_RT0, rtm, 1);
        m[0][0] = -(int32_t)((D[GTE_RGBC] & 0xFF) << 4);
        m[0][1] = (int32_t)((D[GTE_RGBC] & 0xFF) << 4);
        m[0][2] = s16(D[GTE_IR0]);
        m[1][0] = m[1][1] = m[1][2] = rtm[0][2];
        m[2][0] = m[2][1] = m[2][2] = rtm[1][1];
        break;
    }
    }
    if (vv < 3) {
        load_vector(vv, v);
    } else {
        v[0] = s16(D[GTE_IR1]);
        v[1] = s16(D[GTE_IR2]);
        v[2] = s16(D[GTE_IR3]);
    }
    for (i = 0; i < 3; i++) {
        int64_t t = 0;
        if (cv == 0) t = (int64_t)(int32_t)C[GTEC_TRX + i];
        else if (cv == 1) t = (int64_t)(int32_t)C[GTEC_RBK + i];
        else if (cv == 2) t = (int64_t)(int32_t)C[GTEC_RFC + i];
        /* The far-colour case has a further hardware quirk (its first product is saturated
         * and dropped); it is not modelled. */
        a[i] = check_mac(i, (t << 12) + (int64_t)m[i][0] * v[0] + (int64_t)m[i][1] * v[1] + (int64_t)m[i][2] * v[2]);
        set_mac(i + 1, (int32_t)(a[i] >> shift));
        set_ir(i + 1, sat_ir(i + 1, (int32_t)D[GTE_MAC1 + i], lm));
    }
}

static void cmd_sqr(int sf, int lm) {
    int i;
    const int shift = sf ? 12 : 0;
    for (i = 1; i <= 3; i++) {
        const int64_t v = (int64_t)s16(D[GTE_IR0 + i]) * s16(D[GTE_IR0 + i]);
        set_mac(i, (int32_t)(check_mac(i - 1, v) >> shift));
        set_ir(i, sat_ir(i, (int32_t)D[GTE_MAC0 + i], lm));
    }
}

static void cmd_op(int sf, int lm) {
    int32_t m[3][3];
    const int shift = sf ? 12 : 0;
    int64_t a[3];
    int i;
    int32_t d1, d2, d3, i1, i2, i3;

    load_matrix(GTEC_RT0, m, 1);
    d1 = m[0][0]; d2 = m[1][1]; d3 = m[2][2];
    i1 = s16(D[GTE_IR1]); i2 = s16(D[GTE_IR2]); i3 = s16(D[GTE_IR3]);
    a[0] = (int64_t)d2 * i3 - (int64_t)d3 * i2;
    a[1] = (int64_t)d3 * i1 - (int64_t)d1 * i3;
    a[2] = (int64_t)d1 * i2 - (int64_t)d2 * i1;
    for (i = 0; i < 3; i++) {
        set_mac(i + 1, (int32_t)(check_mac(i, a[i]) >> shift));
        set_ir(i + 1, sat_ir(i + 1, (int32_t)D[GTE_MAC1 + i], lm));
    }
}

static void cmd_gp(int sf, int lm, int add) {
    const int shift = sf ? 12 : 0;
    int i;
    for (i = 1; i <= 3; i++) {
        int64_t v = (int64_t)s16(D[GTE_IR0]) * s16(D[GTE_IR0 + i]);
        if (add) {
            v += (int64_t)(int32_t)D[GTE_MAC0 + i] << shift;
        }
        set_mac(i, (int32_t)(check_mac(i - 1, v) >> shift));
        set_ir(i, sat_ir(i, (int32_t)D[GTE_MAC0 + i], lm));
    }
    push_color();
}

void psx_gte_exec(uint32_t cmd) {
    const int sf = (cmd >> 19) & 1;
    const int lm = (cmd >> 10) & 1;
    const int op = (int)(cmd & 0x3F);

    flag = 0;
    switch (op) {
    case 0x01: cmd_rtp(0, sf, lm, 1); break;                       /* RTPS */
    case 0x30: cmd_rtp(0, sf, lm, 0); cmd_rtp(1, sf, lm, 0); cmd_rtp(2, sf, lm, 1); break; /* RTPT */
    case 0x06: cmd_nclip(); break;
    case 0x2D: cmd_avsz(0); break;
    case 0x2E: cmd_avsz(1); break;
    case 0x12: cmd_mvmva(sf, lm, (int)((cmd >> 17) & 3), (int)((cmd >> 15) & 3), (int)((cmd >> 13) & 3)); break;
    case 0x28: cmd_sqr(sf, lm); break;
    case 0x0C: cmd_op(sf, lm); break;
    case 0x3D: cmd_gp(sf, lm, 0); break;                           /* GPF */
    case 0x3E: cmd_gp(sf, lm, 1); break;                           /* GPL */
    default:
        /* The lighting and depth-cue commands: the game does not issue them directly. */
        if (psx_gte_unimplemented++ == 0) {
            fprintf(stderr, "psx_gte: command 0x%08X (function 0x%02X) is not implemented\n", cmd, op);
        }
        break;
    }
    if (flag & F_ERROR_MASK) {
        flag |= 0x80000000u;
    }
    C[GTEC_FLAG] = flag;
}

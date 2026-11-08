#include <stdio.h>
#include "psx_gte.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#define RTPS 0x4A180001u
#define RTPT 0x4A280030u
#define NCLIP 0x4B400006u
#define AVSZ3 0x4B58002Du
#define AVSZ4 0x4B68002Eu
#define SQR0 0x4AA00428u
#define OP12 0x4B78000Cu
#define RTV0 0x4A480012u
#define RTIR 0x4A49E012u

static uint32_t pack(int x, int y) { return (uint32_t)(x & 0xFFFF) | ((uint32_t)(y & 0xFFFF) << 16); }
static int sx(uint32_t v) { return (int16_t)(v & 0xFFFF); }
static int sy(uint32_t v) { return (int16_t)(v >> 16); }

static void identity(void) {
    psx_gte_reset();
    psx_gte_ctc2(GTEC_RT0, 0x1000);        /* RT11 = 1.0, RT12 = 0 */
    psx_gte_ctc2(GTEC_RT2, 0x1000);        /* RT22 */
    psx_gte_ctc2(GTEC_RT4, 0x1000);        /* RT33 */
}

static void set_v(int i, int x, int y, int z) {
    psx_gte_mtc2(GTE_VXY0 + 2 * i, pack(x, y));
    psx_gte_mtc2(GTE_VZ0 + 2 * i, (uint32_t)z & 0xFFFF);
}

int main(void) {
    int i;

    /* RTPS: x*H/z. H = 1000, point (1000, 2000, 3000): 333.3 and 666.7 truncate to 333 and 666. */
    identity();
    psx_gte_ctc2(GTEC_H, 1000);
    set_v(0, 1000, 2000, 3000);
    psx_gte_exec(RTPS);
    CHECK(sx(psx_gte_mfc2(GTE_SXY2)) == 333);
    CHECK(sy(psx_gte_mfc2(GTE_SXY2)) == 666);
    CHECK(psx_gte_mfc2(GTE_SZ3) == 3000);
    CHECK((int32_t)psx_gte_mfc2(GTE_MAC1) == 1000 && (int32_t)psx_gte_mfc2(GTE_MAC3) == 3000);
    CHECK((int32_t)psx_gte_mfc2(GTE_IR1) == 1000 && (int32_t)psx_gte_mfc2(GTE_IR2) == 2000);
    CHECK(psx_gte_cfc2(GTEC_FLAG) == 0);

    /* Screen offset is 16.16, and a translation moves the point. */
    identity();
    psx_gte_ctc2(GTEC_H, 500);
    psx_gte_ctc2(GTEC_OFX, 160u << 16);
    psx_gte_ctc2(GTEC_OFY, 120u << 16);
    psx_gte_ctc2(GTEC_TRZ, 1000);
    set_v(0, -100, 50, 0);
    psx_gte_exec(RTPS);            /* z = 1000: x = -100*500/1000 = -50 -> 110, y = 25 -> 145 */
    CHECK(sx(psx_gte_mfc2(GTE_SXY2)) == 110);
    CHECK(sy(psx_gte_mfc2(GTE_SXY2)) == 145);

    /* sf = 0 leaves MAC unshifted: with an identity matrix MAC1 = x * 0x1000. */
    identity();
    psx_gte_ctc2(GTEC_H, 1000);
    set_v(0, 3, 0, 1000);
    psx_gte_exec(RTPS & ~(1u << 19));
    CHECK((int32_t)psx_gte_mfc2(GTE_MAC1) == 3 * 0x1000);

    /* RTPT: three vertices fill the fifo in order. */
    identity();
    psx_gte_ctc2(GTEC_H, 1000);
    set_v(0, 0, 0, 1000);
    set_v(1, 100, 0, 1000);
    set_v(2, 0, 100, 1000);
    psx_gte_exec(RTPT);
    CHECK(sx(psx_gte_mfc2(GTE_SXY0)) == 0 && sy(psx_gte_mfc2(GTE_SXY0)) == 0);
    CHECK(sx(psx_gte_mfc2(GTE_SXY1)) == 100 && sy(psx_gte_mfc2(GTE_SXY1)) == 0);
    CHECK(sx(psx_gte_mfc2(GTE_SXY2)) == 0 && sy(psx_gte_mfc2(GTE_SXY2)) == 100);
    CHECK(psx_gte_mfc2(GTE_SZ1) == 1000 && psx_gte_mfc2(GTE_SZ3) == 1000);

    /* NCLIP: the sign of MAC0 is the facing (this winding is positive). */
    psx_gte_exec(NCLIP);
    CHECK((int32_t)psx_gte_mfc2(GTE_MAC0) == 100 * 100);
    psx_gte_mtc2(GTE_SXY0, pack(0, 0));
    psx_gte_mtc2(GTE_SXY1, pack(0, 100));
    psx_gte_mtc2(GTE_SXY2, pack(100, 0));
    /* mtc2 SXY0 writes the register directly; SXYP pushes. Use the fifo order explicitly. */
    psx_gte_mtc2(GTE_SXYP, pack(0, 0));
    psx_gte_mtc2(GTE_SXYP, pack(0, 100));
    psx_gte_mtc2(GTE_SXYP, pack(100, 0));
    psx_gte_exec(NCLIP);
    CHECK((int32_t)psx_gte_mfc2(GTE_MAC0) == -100 * 100);

    /* AVSZ3 / AVSZ4: ZSF is the scale (4096/3 and 4096/4 give the mean). */
    psx_gte_reset();
    psx_gte_mtc2(GTE_SZ0, 400);
    psx_gte_mtc2(GTE_SZ1, 800);
    psx_gte_mtc2(GTE_SZ2, 1200);
    psx_gte_mtc2(GTE_SZ3, 1600);
    psx_gte_ctc2(GTEC_ZSF3, 0x155);       /* 341/4096 ~ 1/12, a typical OT shift */
    psx_gte_ctc2(GTEC_ZSF4, 0x100);
    psx_gte_exec(AVSZ3);
    CHECK(psx_gte_mfc2(GTE_OTZ) == (0x155u * 3600u) >> 12);
    psx_gte_exec(AVSZ4);
    CHECK(psx_gte_mfc2(GTE_OTZ) == (0x100u * 4000u) >> 12);

    /* SQR, OP, MVMVA. */
    psx_gte_reset();
    psx_gte_mtc2(GTE_IR1, 5);
    psx_gte_mtc2(GTE_IR2, (uint32_t)-6 & 0xFFFF);
    psx_gte_mtc2(GTE_IR3, 7);
    psx_gte_exec(SQR0 & ~(1u << 19));
    CHECK((int32_t)psx_gte_mfc2(GTE_IR1) == 25 && (int32_t)psx_gte_mfc2(GTE_IR2) == 36 &&
          (int32_t)psx_gte_mfc2(GTE_IR3) == 49);

    psx_gte_reset();                       /* cross product (1,2,3) x (4,5,6) = (-3, 6, -3) */
    psx_gte_ctc2(GTEC_RT0, 1);
    psx_gte_ctc2(GTEC_RT2, 2);
    psx_gte_ctc2(GTEC_RT4, 3);
    psx_gte_mtc2(GTE_IR1, 4);
    psx_gte_mtc2(GTE_IR2, 5);
    psx_gte_mtc2(GTE_IR3, 6);
    psx_gte_exec(OP12 & ~(1u << 19));
    CHECK((int32_t)psx_gte_mfc2(GTE_MAC1) == -3 && (int32_t)psx_gte_mfc2(GTE_MAC2) == 6 &&
          (int32_t)psx_gte_mfc2(GTE_MAC3) == -3);

    identity();                            /* rotate 90 degrees about z: (x,y) -> (-y,x) */
    psx_gte_ctc2(GTEC_RT0, (uint32_t)(0 & 0xFFFF) | ((uint32_t)(-0x1000 & 0xFFFF) << 16));
    psx_gte_ctc2(GTEC_RT1, 0 | ((uint32_t)0x1000 << 16));
    psx_gte_ctc2(GTEC_RT2, 0);
    psx_gte_ctc2(GTEC_RT3, 0);
    psx_gte_ctc2(GTEC_RT4, 0x1000);
    psx_gte_ctc2(GTEC_TRX, 10);
    set_v(0, 100, 0, 0);
    psx_gte_exec(RTV0);                    /* RT * V0 + TR */
    CHECK((int32_t)psx_gte_mfc2(GTE_IR1) == 10 && (int32_t)psx_gte_mfc2(GTE_IR2) == 100);
    psx_gte_mtc2(GTE_IR1, 100);
    psx_gte_mtc2(GTE_IR2, 0);
    psx_gte_mtc2(GTE_IR3, 0);
    psx_gte_exec(RTIR);                    /* RT * IR, no translation */
    CHECK((int32_t)psx_gte_mfc2(GTE_IR1) == 0 && (int32_t)psx_gte_mfc2(GTE_IR2) == 100);

    /* Saturation and flags: a huge z overflows SZ3, a tiny one the divide. */
    identity();
    psx_gte_ctc2(GTEC_H, 1000);
    set_v(0, 0, 0, 0x7FFF);
    psx_gte_ctc2(GTEC_TRZ, 0x7FFF);        /* z = 0xFFFE, still fits */
    psx_gte_exec(RTPS);
    CHECK(psx_gte_mfc2(GTE_SZ3) == 0xFFFE);
    psx_gte_ctc2(GTEC_TRZ, 0x3000000);
    psx_gte_exec(RTPS);
    CHECK(psx_gte_mfc2(GTE_SZ3) == 0xFFFF);
    CHECK((psx_gte_cfc2(GTEC_FLAG) & (1u << 18)) && (psx_gte_cfc2(GTEC_FLAG) & 0x80000000u));
    identity();
    psx_gte_ctc2(GTEC_H, 1000);
    set_v(0, 10, 10, 100);                 /* z < H/2 ... the divide overflows */
    psx_gte_exec(RTPS);
    CHECK(psx_gte_cfc2(GTEC_FLAG) & (1u << 17));
    CHECK(psx_gte_unimplemented == 0);

    /* IRGB / LZCS register behaviour. */
    psx_gte_mtc2(GTE_IRGB, 0x7FFF);
    CHECK(psx_gte_mfc2(GTE_IR1) == 0xF80);
    CHECK(psx_gte_mfc2(GTE_ORGB) == 0x1F + (0x1F << 5) + (0x1F << 10));
    psx_gte_mtc2(GTE_LZCS, 0x00FF0000);
    CHECK(psx_gte_mfc2(GTE_LZCR) == 8);
    psx_gte_mtc2(GTE_LZCS, 0xFFFFFF00u);
    CHECK(psx_gte_mfc2(GTE_LZCR) == 24);

    /* The fifo's reads wrap as the hardware's do. */
    for (i = 0; i < 3; i++) {
        psx_gte_mtc2(GTE_SXYP, pack(i, -i));
    }
    CHECK(sx(psx_gte_mfc2(GTE_SXY0)) == 0 && sx(psx_gte_mfc2(GTE_SXY2)) == 2 && sy(psx_gte_mfc2(GTE_SXYP)) == -2);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("psx_gte: ok\n");
    return 0;
}

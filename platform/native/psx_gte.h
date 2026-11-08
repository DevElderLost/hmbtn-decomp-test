#ifndef PSX_GTE_H
#define PSX_GTE_H

/* A software GTE (the console's geometry coprocessor).
 *
 * The register file is the console's own: 32 data and 32 control registers of 32 bits,
 * moved with mfc2/mtc2/cfc2/ctc2 semantics, and a command is the raw coprocessor-2 word
 * (the `.word 0x4A180001` of the game's gte.h). Saturation and the FLAG register follow
 * the documented hardware behaviour. The perspective divide uses the exact quotient
 * rather than the hardware's reciprocal-table approximation, so a projected coordinate can
 * differ from the console's by one unit. */

#include <stdint.h>

typedef struct PsxGte {
    uint32_t d[32];     /* data registers: V0..V2, RGBC, OTZ, IR0..3, SXY fifo, SZ fifo, MAC0..3, ... */
    uint32_t c[32];     /* control registers: RT, TR, L, BK, LR, FC, OFX, OFY, H, DQA, DQB, ZSF3, ZSF4, FLAG */
} PsxGte;

extern PsxGte psx_gte;

/* Data register numbers. */
enum {
    GTE_VXY0 = 0, GTE_VZ0, GTE_VXY1, GTE_VZ1, GTE_VXY2, GTE_VZ2, GTE_RGBC, GTE_OTZ,
    GTE_IR0, GTE_IR1, GTE_IR2, GTE_IR3, GTE_SXY0, GTE_SXY1, GTE_SXY2, GTE_SXYP,
    GTE_SZ0, GTE_SZ1, GTE_SZ2, GTE_SZ3, GTE_RGB0, GTE_RGB1, GTE_RGB2, GTE_RES1,
    GTE_MAC0, GTE_MAC1, GTE_MAC2, GTE_MAC3, GTE_IRGB, GTE_ORGB, GTE_LZCS, GTE_LZCR
};

/* Control register numbers. */
enum {
    GTEC_RT0 = 0, GTEC_RT1, GTEC_RT2, GTEC_RT3, GTEC_RT4, GTEC_TRX, GTEC_TRY, GTEC_TRZ,
    GTEC_L0, GTEC_L1, GTEC_L2, GTEC_L3, GTEC_L4, GTEC_RBK, GTEC_GBK, GTEC_BBK,
    GTEC_LC0, GTEC_LC1, GTEC_LC2, GTEC_LC3, GTEC_LC4, GTEC_RFC, GTEC_GFC, GTEC_BFC,
    GTEC_OFX, GTEC_OFY, GTEC_H, GTEC_DQA, GTEC_DQB, GTEC_ZSF3, GTEC_ZSF4, GTEC_FLAG
};

void psx_gte_reset(void);

uint32_t psx_gte_mfc2(int reg);             /* read a data register */
void psx_gte_mtc2(int reg, uint32_t value); /* write a data register (SXYP pushes the fifo, ...) */
uint32_t psx_gte_cfc2(int reg);             /* read a control register */
void psx_gte_ctc2(int reg, uint32_t value); /* write a control register */

/* Execute one coprocessor command word. */
void psx_gte_exec(uint32_t command);

/* Commands that were issued but are not implemented, for diagnosis. */
extern unsigned psx_gte_unimplemented;

#endif /* PSX_GTE_H */

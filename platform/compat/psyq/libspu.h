#ifndef PSYQ_COMPAT_LIBSPU_H
#define PSYQ_COMPAT_LIBSPU_H

#include <sys/types.h>

typedef struct {
    short left, right;
} SpuVolume;

typedef struct {
    unsigned long mask;
    SpuVolume mvol;
    SpuVolume mvolmode;
    SpuVolume mvolx;
    struct {
        unsigned long mask;
        long mode;
        SpuVolume depth;
        long delay;
        long feedback;
    } reverb;
    struct {
        SpuVolume volume;
        long reverb;
        long mix;
    } cd;
    struct {
        SpuVolume volume;
        long reverb;
        long mix;
    } ext;
} SpuCommonAttr;

/* One bit per voice, 24 voices. */
#define SPU_00CH (0x1L << 0)
#define SPU_01CH (0x1L << 1)
#define SPU_02CH (0x1L << 2)
#define SPU_03CH (0x1L << 3)
#define SPU_04CH (0x1L << 4)
#define SPU_05CH (0x1L << 5)
#define SPU_06CH (0x1L << 6)
#define SPU_07CH (0x1L << 7)
#define SPU_08CH (0x1L << 8)
#define SPU_09CH (0x1L << 9)
#define SPU_10CH (0x1L << 10)
#define SPU_11CH (0x1L << 11)
#define SPU_12CH (0x1L << 12)
#define SPU_13CH (0x1L << 13)
#define SPU_14CH (0x1L << 14)
#define SPU_15CH (0x1L << 15)
#define SPU_16CH (0x1L << 16)
#define SPU_17CH (0x1L << 17)
#define SPU_18CH (0x1L << 18)
#define SPU_19CH (0x1L << 19)
#define SPU_20CH (0x1L << 20)
#define SPU_21CH (0x1L << 21)
#define SPU_22CH (0x1L << 22)
#define SPU_23CH (0x1L << 23)

#define SPU_NUM_VOICES 24

/* A voice's key status. */
#define SPU_ON 0
#define SPU_ON_ENV_OFF 1
#define SPU_OFF_ENV_ON 2
#define SPU_OFF 3

void SpuSetCommonAttr(SpuCommonAttr *attr);
long SpuGetKeyStatus(unsigned long voice_bit);
void SpuGetAllKeysStatus(char *status);

#endif /* PSYQ_COMPAT_LIBSPU_H */

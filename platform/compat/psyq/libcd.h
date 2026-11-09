#ifndef PSYQ_COMPAT_LIBCD_H
#define PSYQ_COMPAT_LIBCD_H

/* The CD-ROM library: types, command codes and calls. Implemented in
 * platform/native/psx_cd.c on top of a disc image. */

#include <sys/types.h>

typedef struct {
    uint8_t minute, second, sector, track;
} CdlLOC;

typedef struct {
    CdlLOC pos;
    uint32_t size;
    char name[16];
} CdlFILE;

typedef struct {
    uint8_t file, chan;
    uint16_t pad;
} CdlFILTER;

typedef struct {
    uint8_t val0, val1, val2, val3;
} CdlATV;

/* Commands. */
#define CdlNop 0x01
#define CdlSetloc 0x02
#define CdlPlay 0x03
#define CdlForward 0x04
#define CdlBackward 0x05
#define CdlReadN 0x06
#define CdlStandby 0x07
#define CdlStop 0x08
#define CdlPause 0x09
#define CdlInit 0x0A
#define CdlMute 0x0B
#define CdlDemute 0x0C
#define CdlSetfilter 0x0D
#define CdlSetmode 0x0E
#define CdlGetparam 0x0F
#define CdlGetlocL 0x10
#define CdlGetlocP 0x11
#define CdlSetsession 0x12
#define CdlGetTN 0x13
#define CdlGetTD 0x14
#define CdlSeekL 0x15
#define CdlSeekP 0x16
#define CdlReadS 0x1B

/* Interrupt results (CdSync, CdReady). */
#define CdlNoIntr 0x00
#define CdlDataReady 0x01
#define CdlComplete 0x02
#define CdlAcknowledge 0x03
#define CdlDataEnd 0x04
#define CdlDiskError 0x05

/* Mode bits (CdlSetmode). */
#define CdlModeStream 0x100
#define CdlModeStream2 0x120
#define CdlModeSpeed 0x80
#define CdlModeRT 0x40
#define CdlModeSize1 0x20
#define CdlModeSize0 0x10
#define CdlModeSF 0x08
#define CdlModeRept 0x04
#define CdlModeAP 0x02
#define CdlModeDA 0x01

/* Status byte bits. */
#define CdlStatPlay 0x80
#define CdlStatSeek 0x40
#define CdlStatRead 0x20
#define CdlStatShellOpen 0x10
#define CdlStatSeekError 0x04
#define CdlStatStandby 0x02
#define CdlStatError 0x01

int CdInit(void);
int CdControl(uint8_t com, uint8_t *param, uint8_t *result);
int CdControlF(uint8_t com, uint8_t *param);
int CdControlB(uint8_t com, uint8_t *param, uint8_t *result);
int CdSync(int mode, uint8_t *result);
int CdReady(int mode, uint8_t *result);
int CdRead(int sectors, uint32_t *buf, int mode);
int CdReadSync(int mode, uint8_t *result);
int CdReadBreak(void);
int CdFlush(void);
int CdLastCom(void);
CdlFILE *CdSearchFile(CdlFILE *fp, char *name);
CdlLOC *CdIntToPos(int i, CdlLOC *p);
int CdPosToInt(CdlLOC *p);

#endif /* PSYQ_COMPAT_LIBCD_H */

#ifndef PSYQ_COMPAT_LIBGS_H
#define PSYQ_COMPAT_LIBGS_H

/* The graphics-system library: only what the game uses (the ordering-table header, the
 * screen set-up and the light state). */

#include <libgte.h>
#include <libgpu.h>

typedef struct {
    unsigned p : 24;
    unsigned num : 8;
} GsOT_TAG;

typedef struct {
    uint32_t length;
    GsOT_TAG *org;
    uint32_t offset;
    uint32_t point;
    GsOT_TAG *tag;
} GsOT;

typedef struct {
    int32_t vx, vy, vz;
    uint8_t r, g, b;
    uint8_t pad;
} GsF_LIGHT;

typedef uint32_t PACKET;

extern MATRIX GsIDMATRIX;

/* The screen size given to GsInitGraph. */
extern int HWD0, VWD0;

#define GsNONINTER 0
#define GsINTER 1
#define GsOFSGPU 4
#define GsOFSGTE 8

void GsInitGraph(uint16_t x, uint16_t y, uint16_t intmode, uint16_t dith, uint16_t varmmode);
void GsInit3D(void);
void GsDefDispBuff(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void GsSetProjection(int32_t h);
void GsSetOffset(int32_t x, int32_t y);
void GsSetDrawBuffOffset(void);
void GsSetWorkBase(PACKET *outpacketp);
PACKET *GsGetWorkBase(void);
void GsSetFlatLight(uint32_t id, GsF_LIGHT *light);
void GsSetLightMode(uint32_t mode);
void GsSetAmbient(int32_t r, int32_t g, int32_t b);

#endif /* PSYQ_COMPAT_LIBGS_H */

#ifndef PSYQ_COMPAT_LIBGTE_H
#define PSYQ_COMPAT_LIBGTE_H

/* The geometry library's types and functions, for the native build. The structures have the
 * console's layouts; the functions are implemented in platform/native/psx_libgte.c on top of
 * the software GTE. */

#include <sys/types.h>

typedef struct {
    int16_t vx, vy, vz, pad;
} SVECTOR;

typedef struct {
    int32_t vx, vy, vz, pad;
} VECTOR;

typedef struct {
    int16_t vx, vy;
} DVECTOR;

typedef struct {
    uint8_t r, g, b, cd;
} CVECTOR;

typedef struct {
    int16_t m[3][3];
    int32_t t[3];
} MATRIX;

/* Angles are 4096 to a turn; sine and cosine are scaled by 4096. */
#define ONE 4096

void InitGeom(void);
void SetGeomOffset(int32_t ofx, int32_t ofy);
void SetGeomScreen(int32_t h);
void SetRotMatrix(MATRIX *m);
void SetTransMatrix(MATRIX *m);
void SetLightMatrix(MATRIX *m);
void SetColorMatrix(MATRIX *m);
void SetBackColor(int32_t r, int32_t g, int32_t b);
void SetFarColor(int32_t r, int32_t g, int32_t b);
void SetFogNear(int32_t a, int32_t h);
void ReadRotMatrix(MATRIX *m);

MATRIX *RotMatrix(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixYXZ(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixZYX(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixZXY(SVECTOR *r, MATRIX *m);
MATRIX *RotMatrixX(int32_t angle, MATRIX *m);
MATRIX *RotMatrixY(int32_t angle, MATRIX *m);
MATRIX *RotMatrixZ(int32_t angle, MATRIX *m);
MATRIX *TransMatrix(MATRIX *m, VECTOR *v);
MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2);
void PushMatrix(void);
void PopMatrix(void);

int32_t RotTrans(SVECTOR *v0, VECTOR *v1, int32_t *flag);
VECTOR *ApplyMatrixLV(MATRIX *m, VECTOR *v0, VECTOR *v1);
VECTOR *OuterProduct0(VECTOR *v0, VECTOR *v1, VECTOR *v2);

int32_t SquareRoot0(int32_t a);
int32_t VectorNormal(VECTOR *v0, VECTOR *v1);
int32_t VectorNormalS(VECTOR *v0, SVECTOR *v1);
int32_t VectorNormalSS(SVECTOR *v0, SVECTOR *v1);
VECTOR *OuterProduct12(VECTOR *v0, VECTOR *v1, VECTOR *v2);

/* PSY-Q's csin, ccos and csqrt share their names with libm's complex-number functions;
 * renamed so that neither is shadowed. */
#define csin hmbtn_csin
#define ccos hmbtn_ccos
#define csqrt hmbtn_csqrt
int32_t csin(int32_t a);
int32_t ccos(int32_t a);
int32_t csqrt(int32_t a);

int32_t rsin(int32_t a);
int32_t rcos(int32_t a);
int32_t ratan2(int32_t y, int32_t x);

#endif /* PSYQ_COMPAT_LIBGTE_H */

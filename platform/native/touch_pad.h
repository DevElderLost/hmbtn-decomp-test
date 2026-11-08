#ifndef TOUCH_PAD_H
#define TOUCH_PAD_H

/* The on-screen controller: d-pad, L1/L2/R1/R2, triangle/circle/cross/square, select and
 * start. Pure geometry with no SDL dependency, so it can be unit-tested on the host.
 *
 * The mask is recomputed from the fingers currently on the glass every frame, so a finger
 * that slides from one button to another releases the first and presses the second, and
 * several fingers combine (walk with the d-pad while pressing cross).
 *
 * The layout is sized from the screen height and anchored to the corners; it is meant for
 * landscape screens of 16:9 or wider. */

#include <stdint.h>

#define TOUCH_MAX_POINTS 10

typedef struct TouchPoint {
    float x, y;                 /* pixels */
} TouchPoint;

typedef struct TouchCircle {
    float cx, cy, r;
} TouchCircle;

typedef struct TouchRect {
    float x, y, w, h;
} TouchRect;

typedef struct TouchPadLayout {
    int width, height;
    float unit;                 /* layout unit: the screen height */

    float dpad_cx, dpad_cy;
    float dpad_hit_r;           /* outer edge of the d-pad's touch area */
    float dpad_dead_r;          /* centre dead zone */

    TouchCircle face[4];        /* triangle, circle, cross, square */
    uint16_t face_bit[4];
    float face_hit_scale;       /* touch area = drawn radius * this */

    TouchRect shoulder[4];      /* L2, R2, L1, R1 */
    uint16_t shoulder_bit[4];

    TouchRect system[2];        /* select, start */
    uint16_t system_bit[2];
} TouchPadLayout;

void touch_pad_layout_init(TouchPadLayout *layout, int width, int height);

/* The PSX_PAD_* mask held by `count` fingers (at most TOUCH_MAX_POINTS are read). */
uint16_t touch_pad_mask(const TouchPadLayout *layout, const TouchPoint *points, int count);

#endif /* TOUCH_PAD_H */

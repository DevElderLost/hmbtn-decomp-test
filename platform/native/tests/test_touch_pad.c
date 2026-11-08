#include <stdio.h>
#include "touch_pad.h"
#include "psx_buttons.h"

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static uint16_t one(const TouchPadLayout *l, float x, float y) {
    TouchPoint p = {x, y};
    return touch_pad_mask(l, &p, 1);
}

static void test_layout(int w, int h) {
    TouchPadLayout l;
    int i;
    float cx, cy, r;

    touch_pad_layout_init(&l, w, h);

    /* Nothing touched, or touched outside every control. */
    CHECK(touch_pad_mask(&l, NULL, 0) == 0);
    CHECK(one(&l, (float)w * 0.5f, (float)h * 0.3f) == 0);

    /* D-pad: four directions, two diagonals, the dead zone and outside it. */
    cx = l.dpad_cx;
    cy = l.dpad_cy;
    r = l.dpad_hit_r * 0.7f;
    CHECK(one(&l, cx, cy - r) == PSX_PAD_UP);
    CHECK(one(&l, cx, cy + r) == PSX_PAD_DOWN);
    CHECK(one(&l, cx - r, cy) == PSX_PAD_LEFT);
    CHECK(one(&l, cx + r, cy) == PSX_PAD_RIGHT);
    CHECK(one(&l, cx + r * 0.7f, cy - r * 0.7f) == (PSX_PAD_UP | PSX_PAD_RIGHT));
    CHECK(one(&l, cx - r * 0.7f, cy + r * 0.7f) == (PSX_PAD_DOWN | PSX_PAD_LEFT));
    CHECK(one(&l, cx, cy) == 0);
    CHECK(one(&l, cx + l.dpad_dead_r * 0.5f, cy) == 0);
    CHECK(one(&l, cx, cy - l.dpad_hit_r * 1.2f) == 0);
    /* 75 degrees off the horizontal is still up only; 60 degrees is already a diagonal. */
    CHECK(one(&l, cx + r * 0.2588f, cy - r * 0.9659f) == PSX_PAD_UP);
    CHECK(one(&l, cx + r * 0.5f, cy - r * 0.866f) == (PSX_PAD_UP | PSX_PAD_RIGHT));

    /* Face buttons: each centre presses exactly its own button. */
    for (i = 0; i < 4; i++) {
        CHECK(one(&l, l.face[i].cx, l.face[i].cy) == l.face_bit[i]);
        /* The edge of the drawn button still counts; well outside does not. */
        CHECK(one(&l, l.face[i].cx + l.face[i].r * 0.95f, l.face[i].cy) == l.face_bit[i]);
        CHECK(one(&l, l.face[i].cx + l.face[i].r * 1.6f, l.face[i].cy) == 0);
    }
    /* The PlayStation layout: triangle up, cross down, square left, circle right. */
    CHECK(l.face[0].cy < l.face[2].cy);
    CHECK(l.face[3].cx < l.face[1].cx);
    CHECK(l.face_bit[0] == PSX_PAD_TRIANGLE && l.face_bit[1] == PSX_PAD_CIRCLE);
    CHECK(l.face_bit[2] == PSX_PAD_CROSS && l.face_bit[3] == PSX_PAD_SQUARE);

    /* Shoulders and system buttons. */
    for (i = 0; i < 4; i++) {
        CHECK(one(&l, l.shoulder[i].x + l.shoulder[i].w * 0.5f, l.shoulder[i].y + l.shoulder[i].h * 0.5f) ==
              l.shoulder_bit[i]);
    }
    for (i = 0; i < 2; i++) {
        CHECK(one(&l, l.system[i].x + l.system[i].w * 0.5f, l.system[i].y + l.system[i].h * 0.5f) ==
              l.system_bit[i]);
    }

    /* No two controls overlap: every control centre reads as exactly one button's worth. */
    {
        uint16_t seen = 0;
        for (i = 0; i < 4; i++) {
            CHECK((seen & l.face_bit[i]) == 0);
            seen |= l.face_bit[i];
            CHECK((seen & l.shoulder_bit[i]) == 0);
            seen |= l.shoulder_bit[i];
        }
    }

    /* Several fingers combine: walk right while holding cross and R1. */
    {
        TouchPoint pts[3] = {
            {cx + r, cy},
            {l.face[2].cx, l.face[2].cy},
            {l.shoulder[3].x + 5.0f, l.shoulder[3].y + 5.0f},
        };
        CHECK(touch_pad_mask(&l, pts, 3) == (PSX_PAD_RIGHT | PSX_PAD_CROSS | PSX_PAD_R1));
    }

    /* Only the first TOUCH_MAX_POINTS fingers are read: a touch in slot 11 is ignored. */
    {
        TouchPoint many[TOUCH_MAX_POINTS + 1];
        for (i = 0; i < TOUCH_MAX_POINTS; i++) {
            many[i].x = (float)w * 0.5f;
            many[i].y = (float)h * 0.3f;
        }
        many[TOUCH_MAX_POINTS].x = l.face[2].cx;
        many[TOUCH_MAX_POINTS].y = l.face[2].cy;
        CHECK(touch_pad_mask(&l, many, TOUCH_MAX_POINTS + 1) == 0);
    }
}

int main(void) {
    test_layout(2400, 1080);  /* 20:9 phone */
    test_layout(1920, 1080);  /* 16:9 */
    test_layout(2560, 1600);  /* 16:10 tablet */
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("touch_pad: ok\n");
    return 0;
}

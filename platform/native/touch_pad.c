#include "touch_pad.h"
#include "psx_buttons.h"

/* Bit order of TouchPadLayout.face: triangle, circle, cross, square. */
void touch_pad_layout_init(TouchPadLayout *l, int width, int height) {
    const float u = (float)height;
    const float face_offset = 0.18f * u;
    const float face_cx = (float)width - 0.30f * u;
    const float face_cy = (float)height - 0.32f * u;
    const float shoulder_w = 0.28f * u;
    const float shoulder_h = 0.10f * u;
    const float system_w = 0.20f * u;
    const float system_h = 0.07f * u;
    const float margin = 0.04f * u;
    int i;

    l->width = width;
    l->height = height;
    l->unit = u;

    l->dpad_hit_r = 0.26f * u;
    l->dpad_dead_r = 0.06f * u;
    l->dpad_cx = margin + l->dpad_hit_r;
    l->dpad_cy = (float)height - 0.32f * u;

    l->face_hit_scale = 1.25f;
    for (i = 0; i < 4; i++) {
        l->face[i].r = 0.095f * u;
    }
    l->face[0].cx = face_cx;                l->face[0].cy = face_cy - face_offset; /* triangle */
    l->face[1].cx = face_cx + face_offset;  l->face[1].cy = face_cy;               /* circle */
    l->face[2].cx = face_cx;                l->face[2].cy = face_cy + face_offset; /* cross */
    l->face[3].cx = face_cx - face_offset;  l->face[3].cy = face_cy;               /* square */
    l->face_bit[0] = PSX_PAD_TRIANGLE;
    l->face_bit[1] = PSX_PAD_CIRCLE;
    l->face_bit[2] = PSX_PAD_CROSS;
    l->face_bit[3] = PSX_PAD_SQUARE;

    /* L2 and R2 sit above L1 and R1. */
    l->shoulder[0] = (TouchRect){margin, 0.03f * u, shoulder_w, shoulder_h};
    l->shoulder[1] = (TouchRect){(float)width - margin - shoulder_w, 0.03f * u, shoulder_w, shoulder_h};
    l->shoulder[2] = (TouchRect){margin, 0.15f * u, shoulder_w, shoulder_h};
    l->shoulder[3] = (TouchRect){(float)width - margin - shoulder_w, 0.15f * u, shoulder_w, shoulder_h};
    l->shoulder_bit[0] = PSX_PAD_L2;
    l->shoulder_bit[1] = PSX_PAD_R2;
    l->shoulder_bit[2] = PSX_PAD_L1;
    l->shoulder_bit[3] = PSX_PAD_R1;

    l->system[0] = (TouchRect){(float)width * 0.5f - 0.03f * u - system_w, (float)height - margin - system_h,
                               system_w, system_h};
    l->system[1] = (TouchRect){(float)width * 0.5f + 0.03f * u, (float)height - margin - system_h,
                               system_w, system_h};
    l->system_bit[0] = PSX_PAD_SELECT;
    l->system_bit[1] = PSX_PAD_START;
}

static int in_rect(const TouchRect *r, const TouchPoint *p) {
    return p->x >= r->x && p->x < r->x + r->w && p->y >= r->y && p->y < r->y + r->h;
}

/* Up, down, left and right fire when the touch is within about 68 degrees of that axis,
 * so a diagonal zone of about 45 degrees presses two directions at once. */
static uint16_t dpad_bits(const TouchPadLayout *l, const TouchPoint *p) {
    const float dx = p->x - l->dpad_cx;
    const float dy = p->y - l->dpad_cy;
    const float d2 = dx * dx + dy * dy;
    const float k = 0.1444f; /* 0.38 squared */
    uint16_t bits = 0;

    if (d2 > l->dpad_hit_r * l->dpad_hit_r || d2 < l->dpad_dead_r * l->dpad_dead_r) {
        return 0;
    }
    if (dy < 0 && dy * dy > k * d2) bits |= PSX_PAD_UP;
    if (dy > 0 && dy * dy > k * d2) bits |= PSX_PAD_DOWN;
    if (dx < 0 && dx * dx > k * d2) bits |= PSX_PAD_LEFT;
    if (dx > 0 && dx * dx > k * d2) bits |= PSX_PAD_RIGHT;
    return bits;
}

uint16_t touch_pad_mask(const TouchPadLayout *l, const TouchPoint *points, int count) {
    uint16_t mask = 0;
    int n, i;

    if (count > TOUCH_MAX_POINTS) {
        count = TOUCH_MAX_POINTS;
    }
    for (n = 0; n < count; n++) {
        const TouchPoint *p = &points[n];

        mask |= dpad_bits(l, p);
        for (i = 0; i < 4; i++) {
            const float dx = p->x - l->face[i].cx;
            const float dy = p->y - l->face[i].cy;
            const float r = l->face[i].r * l->face_hit_scale;
            if (dx * dx + dy * dy <= r * r) {
                mask |= l->face_bit[i];
            }
            if (in_rect(&l->shoulder[i], p)) {
                mask |= l->shoulder_bit[i];
            }
        }
        for (i = 0; i < 2; i++) {
            if (in_rect(&l->system[i], p)) {
                mask |= l->system_bit[i];
            }
        }
    }
    return mask;
}

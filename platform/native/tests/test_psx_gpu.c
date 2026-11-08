#include <stdio.h>
#include <string.h>
#include "psx_gpu.h"
#include "psx_mem.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static uint16_t px(int x, int y) { return psx_gpu_vram()[y * PSX_VRAM_W + x]; }
static uint16_t rgb15(int r, int g, int b) { return (uint16_t)((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)); }
static uint32_t xy(int x, int y) { return (uint32_t)(x & 0xFFFF) | ((uint32_t)(y & 0xFFFF) << 16); }
static uint32_t col(int cmd, int r, int g, int b) { return ((uint32_t)cmd << 24) | (uint32_t)(b << 16) | (uint32_t)(g << 8) | (uint32_t)r; }

static int count_pixels(int x0, int y0, int x1, int y1, uint16_t color) {
    int n = 0, x, y;
    for (y = y0; y < y1; y++) {
        for (x = x0; x < x1; x++) {
            n += px(x, y) == color;
        }
    }
    return n;
}

static void reset(void) {
    psx_gpu_init();
    psx_gpu_set_draw_area(0, 0, 320, 240);
    psx_gpu_set_draw_offset(0, 0);
    psx_gpu_set_draw_mode(0, 0, 1);
}

static void test_fill_and_transfer(void) {
    PsxRect r = {10, 20, 32, 4};
    uint16_t buf[32 * 4], back[32 * 4];
    int i;

    reset();
    {   /* GP0 02: fill, with the x/width alignment the console applies */
        const uint32_t cmd[3] = {col(0x02, 255, 0, 0), xy(16, 8), xy(32, 2)};
        psx_gpu_exec(cmd, 3);
    }
    CHECK(px(16, 8) == rgb15(255, 0, 0) && px(47, 9) == rgb15(255, 0, 0));
    CHECK(px(48, 8) == 0 && px(16, 10) == 0);

    for (i = 0; i < 32 * 4; i++) {
        buf[i] = (uint16_t)(0x1234 + i);
    }
    psx_gpu_load_image(&r, buf);
    psx_gpu_store_image(&r, back);
    CHECK(memcmp(buf, back, sizeof(buf)) == 0);
    CHECK(px(10, 20) == 0x1234 && px(41, 23) == 0x1234 + 127);

    {   /* move, including a source that overlaps its destination */
        PsxRect m = {10, 20, 32, 4};
        psx_gpu_move_image(&m, 12, 21);
        CHECK(px(12, 21) == 0x1234 && px(43, 24) == 0x1234 + 127);
    }
    {   /* wrap around the VRAM edge */
        PsxRect e = {1022, 0, 4, 1};
        const uint16_t four[4] = {1, 2, 3, 4};
        psx_gpu_load_image(&e, four);
        CHECK(px(1022, 0) == 1 && px(1023, 0) == 2 && px(0, 0) == 3 && px(1, 0) == 4);
    }
    {   /* GP0 A0 CPU-to-VRAM, as a DR_LOAD packet carries it */
        uint32_t cmd[5] = {col(0xA0, 0, 0, 0), xy(100, 100), xy(2, 2), 0x00020001u, 0x00040003u};
        psx_gpu_exec(cmd, 5);
        CHECK(px(100, 100) == 1 && px(101, 100) == 2 && px(100, 101) == 3 && px(101, 101) == 4);
    }
}

static void test_flat_polygons(void) {
    reset();
    {   /* a right triangle 10x10 covers 55 pixels with the top-left rule (n(n+1)/2) */
        const uint32_t cmd[4] = {col(0x20, 255, 255, 255), xy(0, 0), xy(10, 0), xy(0, 10)};
        psx_gpu_exec(cmd, 4);
        CHECK(count_pixels(0, 0, 20, 20, rgb15(255, 255, 255)) == 55);
        CHECK(px(0, 0) != 0 && px(9, 0) != 0 && px(10, 0) == 0 && px(0, 10) == 0);
    }
    reset();
    {   /* a quad is drawn with no gap and no overdraw along the shared diagonal: a 16x16 square is 256 pixels */
        const uint32_t cmd[5] = {col(0x28, 0, 255, 0), xy(20, 20), xy(36, 20), xy(20, 36), xy(36, 36)};
        psx_gpu_exec(cmd, 5);
        CHECK(count_pixels(0, 0, 100, 100, rgb15(0, 255, 0)) == 256);
        CHECK(px(20, 20) != 0 && px(35, 35) != 0 && px(36, 36) == 0 && px(19, 20) == 0);
    }
    reset();
    {   /* winding does not matter */
        const uint32_t cmd[4] = {col(0x20, 0, 0, 255), xy(0, 0), xy(0, 10), xy(10, 0)};
        psx_gpu_exec(cmd, 4);
        CHECK(count_pixels(0, 0, 20, 20, rgb15(0, 0, 255)) == 55);
    }
    reset();
    {   /* draw offset moves it; the draw area clips it */
        uint32_t off = (0xE5u << 24) | (5u) | (7u << 11);
        const uint32_t cmd[4] = {col(0x20, 255, 0, 0), xy(0, 0), xy(10, 0), xy(0, 10)};
        psx_gpu_exec(&off, 1);
        psx_gpu_set_draw_area(8, 0, 4, 100);
        psx_gpu_exec(cmd, 4);
        CHECK(px(5, 7) == 0 && px(8, 8) != 0 && px(11, 8) != 0 && px(12, 8) == 0);
        psx_gpu_set_draw_offset(0, 0);
    }
}

static void test_gouraud(void) {
    reset();
    {   /* a horizontal ramp from black to white across a wide quad */
        const uint32_t cmd[9] = {col(0x38, 0, 0, 0), xy(0, 0), col(0, 255, 255, 255), xy(128, 0),
                                 col(0, 0, 0, 0), xy(0, 8), col(0, 255, 255, 255), xy(128, 8)};
        psx_gpu_exec(cmd, 8);
        {
            const int a = px(8, 4) & 31, b = px(64, 4) & 31, c = px(120, 4) & 31;
            CHECK(a < b && b < c);
            CHECK(b >= 14 && b <= 17);
        }
    }
}

static void test_textures(void) {
    int i;
    reset();
    /* A 4-bit texture in page 5 (x = 320, y = 0): texel (u,v) has CLUT index (u + v) & 15.
     * The CLUT at (0, 480) maps index n to a grey of n*16, index 0 to transparent. */
    for (i = 0; i < 16; i++) {
        psx_gpu_vram()[480 * PSX_VRAM_W + i] = (uint16_t)(i == 0 ? 0 : rgb15(i * 16, i * 16, i * 16));
    }
    {
        int u, v;
        for (v = 0; v < 16; v++) {
            for (u = 0; u < 16; u += 4) {
                const uint16_t word = (uint16_t)(((u + v) & 15) | ((((u + 1) + v) & 15) << 4) |
                                                 ((((u + 2) + v) & 15) << 8) | ((((u + 3) + v) & 15) << 12));
                psx_gpu_vram()[v * PSX_VRAM_W + 320 + u / 4] = word;
            }
        }
    }
    {   /* a textured sprite: 16x16 at (50, 60), neutral colour 0x80, uv (0,0), clut (0, 480) */
        const uint32_t clut = (uint32_t)(480 << 6 | 0) << 16;
        const uint32_t mode = (0xE1u << 24) | 5u;               /* page 5, 4-bit */
        const uint32_t cmd[3] = {col(0x64, 0x80, 0x80, 0x80), xy(50, 60), clut | 0u};
        uint32_t sz[1] = {xy(16, 16)};
        uint32_t all[4];
        psx_gpu_exec(&mode, 1);
        memcpy(all, cmd, sizeof(cmd));
        all[3] = sz[0];
        psx_gpu_exec(all, 4);
        /* texel (3, 5): index 8 -> grey 128 (modulated by 0x80 -> unchanged) */
        CHECK(px(53, 65) == (uint16_t)(rgb15(128, 128, 128) | 0));
        /* texel (0,0) is index 0: transparent, VRAM untouched */
        CHECK(px(50, 60) == 0);
        /* every non-transparent texel is drawn: 256 minus the 16 index-0 texels */
        {
            int n = 0, x, y;
            for (y = 60; y < 76; y++) for (x = 50; x < 66; x++) n += px(x, y) != 0;
            CHECK(n == 256 - 16);
        }
    }
    reset();
    {   /* modulation: colour 0x40 halves a texel, raw (cmd bit 0) leaves it */
        const uint32_t mode = (0xE1u << 24) | (2u << 7) | 5u;   /* page 5, 15-bit */
        const uint32_t cmd[4] = {col(0x64, 0x40, 0x40, 0x40), xy(10, 10), 0u, xy(1, 1)};
        const uint32_t raw[4] = {col(0x65, 0x40, 0x40, 0x40), xy(12, 10), 0u, xy(1, 1)};
        psx_gpu_vram()[320] = rgb15(200, 100, 50) | 0x8000;
        psx_gpu_exec(&mode, 1);
        psx_gpu_exec(cmd, 4);
        psx_gpu_exec(raw, 4);
        CHECK(px(10, 10) == rgb15(100, 50, 24));
        CHECK(px(12, 10) == rgb15(200, 100, 50));
    }
}

static void test_semitrans(void) {
    int mode;
    const int expect_r[4] = {(10 + 20) / 2, 10 + 20, 10 - 20 < 0 ? 0 : 10 - 20, 10 + 20 / 4};
    for (mode = 0; mode < 4; mode++) {
        const uint32_t dm = (0xE1u << 24) | ((uint32_t)mode << 5);
        const uint32_t cmd[3] = {col(0x62, 20 << 3, 0, 0), xy(4, 4), xy(1, 1)};
        reset();
        psx_gpu_vram()[4 * PSX_VRAM_W + 4] = (uint16_t)10;      /* back: r = 10 */
        psx_gpu_exec(&dm, 1);
        psx_gpu_exec(cmd, 3);
        CHECK((px(4, 4) & 31) == expect_r[mode]);
    }
}

static void test_ordering_table(void) {
    uint32_t *ot = (uint32_t *)0x80100000u;
    uint32_t *a = (uint32_t *)0x80100100u;
    uint32_t *b = (uint32_t *)0x80100200u;
    int i;

    reset();
    /* ClearOTagR(ot, 4): each entry links to the one before, ot[0] ends the table. */
    for (i = 1; i < 4; i++) {
        ot[i] = (PSX_RAM_BASE + 0x100000u + (uint32_t)(i - 1) * 4) & 0xFFFFFF;
    }
    ot[0] = 0xFFFFFF;
    /* A (red tile) in slot 3, B (blue tile) in slot 1: A is drawn first (reversed table
     * runs from the highest entry down), so B ends up on top where they overlap. */
    a[1] = col(0x60, 255, 0, 0); a[2] = xy(0, 0); a[3] = xy(8, 8);
    a[0] = (3u << 24) | (ot[3] & 0xFFFFFF);
    ot[3] = (uint32_t)(0x80100100u) & 0xFFFFFF;
    b[1] = col(0x60, 0, 0, 255); b[2] = xy(4, 4); b[3] = xy(8, 8);
    b[0] = (3u << 24) | (ot[1] & 0xFFFFFF);
    ot[1] = (uint32_t)(0x80100200u) & 0xFFFFFF;
    psx_gpu_draw_otag(0x80100000u + 3 * 4);
    CHECK(px(1, 1) == rgb15(255, 0, 0));
    CHECK(px(6, 6) == rgb15(0, 0, 255));
    CHECK(px(11, 11) == rgb15(0, 0, 255));
    CHECK(psx_gpu_bad_tags == 0 && psx_gpu_bad_commands == 0);
    /* A link out of mapped memory is reported, not followed. */
    ot[2] = 0x7FFFFF | (0u << 24);
    psx_gpu_draw_otag(0x80100000u + 2 * 4);
    CHECK(psx_gpu_bad_tags == 1);
}

static void test_display(void) {
    uint32_t out[16 * 16];
    int w, h;
    reset();
    psx_gpu_vram()[0] = rgb15(255, 0, 0);
    psx_gpu_set_display(0, 0, 16, 16);
    psx_gpu_get_display(out, 16 * 16, &w, &h);
    CHECK(w == 16 && h == 16 && out[0] == 0xFF000000u);          /* off: black */
    psx_gpu_enable_display(1);
    psx_gpu_get_display(out, 16 * 16, &w, &h);
    CHECK(out[0] == 0xFFFF0000u && out[1] == 0xFF000000u);
}

int main(void) {
    if (psx_mem_init() != 0) {
        printf("FAIL: psx_mem_init\n");
        return 1;
    }
    test_fill_and_transfer();
    test_flat_polygons();
    test_gouraud();
    test_textures();
    test_semitrans();
    test_ordering_table();
    test_display();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("psx_gpu: ok\n");
    return 0;
}

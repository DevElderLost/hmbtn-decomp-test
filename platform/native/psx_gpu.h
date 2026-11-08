#ifndef PSX_GPU_H
#define PSX_GPU_H

/* A software GPU: 1024x512 15-bit VRAM and an interpreter for the console's GP0 command
 * words, which are exactly the packets the game builds (POLY_*, SPRT, TILE, DR_MODE, ...).
 *
 * Rasterisation follows the console's rules where they are visible: integer vertices with
 * the top-left fill rule, affine texture mapping, 4/8/15-bit textures through a CLUT,
 * colour modulation (0x80 is neutral), the four semi-transparency modes, 4x4 dithering and
 * the draw area and offset. It does not model timing, the texture cache or the mask bit. */

#include <stddef.h>
#include <stdint.h>

#define PSX_VRAM_W 1024
#define PSX_VRAM_H 512

typedef struct PsxRect {
    int16_t x, y, w, h;
} PsxRect;

void psx_gpu_init(void);
uint16_t *psx_gpu_vram(void);

/* Run `count` words of GP0 commands (one packet's payload, after its tag word). */
void psx_gpu_exec(const uint32_t *words, int count);

/* Walk an ordering table from the tag at console address `addr`, running each packet. */
void psx_gpu_draw_otag(uint32_t addr);

/* VRAM transfers; coordinates wrap around the VRAM edges as the console's do. */
void psx_gpu_load_image(const PsxRect *r, const uint16_t *src);
void psx_gpu_store_image(const PsxRect *r, uint16_t *dst);
void psx_gpu_move_image(const PsxRect *r, int dst_x, int dst_y);
void psx_gpu_fill(const PsxRect *r, uint8_t red, uint8_t green, uint8_t blue);

/* Drawing environment (what PutDrawEnv sets up). */
void psx_gpu_set_draw_area(int x, int y, int w, int h);
void psx_gpu_set_draw_offset(int x, int y);
void psx_gpu_set_draw_mode(unsigned tpage, int dither, int draw_to_display);

/* Display: which VRAM rectangle is shown, and whether the picture is on. */
void psx_gpu_set_display(int x, int y, int w, int h);
void psx_gpu_enable_display(int on);

/* Convert the displayed rectangle to 0xAARRGGBB pixels. `rgba` must hold w*h entries for
 * the size the display was set to; the size is returned. Black when the display is off. */
void psx_gpu_get_display(uint32_t *rgba, int capacity, int *w, int *h);

/* Counters for diagnosis. */
extern unsigned psx_gpu_bad_commands;   /* unknown GP0 opcodes met */
extern unsigned psx_gpu_bad_tags;       /* ordering-table links that left mapped memory */

#endif /* PSX_GPU_H */

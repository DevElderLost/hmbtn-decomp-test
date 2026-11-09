#ifndef PSX_LIBGPU_H
#define PSX_LIBGPU_H

/* Host hooks of the libgpu/libgs implementation. */

#include <stdint.h>

/* Called by VSync() for each vertical blank it waits for: the platform presents the frame
 * and paces to the display here. If NULL, VSync does not wait. */
extern void (*psx_vsync_hook)(void);

/* Register a function VSync() calls once per vertical blank (the CD drive uses this to
 * advance its read head). Up to 8. */
void psx_vblank_add_hook(void (*hook)(void));

/* The vertical-blank counter (what VSync(-1) returns). */
int psx_vblank_count(void);

#endif /* PSX_LIBGPU_H */

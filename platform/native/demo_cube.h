#ifndef DEMO_CUBE_H
#define DEMO_CUBE_H

#include <stdint.h>

/* A spinning cube drawn through the PSY-Q compat API (GTE macros, libgpu primitives, an
 * ordering table in console RAM, double-buffered draw/display environments), the way the
 * game's own code draws. Stage 2's proof that the graphics path works on the device.
 * Needs psx_mem_init() to have succeeded. */

void demo_init(void);

/* Draw and show one frame; waits for the vertical blank (VSync) at the end. `held` is the
 * pad's button mask (PSX_PAD_*): the d-pad turns the cube, L1/R1 move it closer or away,
 * cross stops and starts the spin. */
void demo_frame(uint16_t held);

#endif /* DEMO_CUBE_H */

/* The overlay load regions, as addresses in the console's RAM.
 *
 * The original links each overlay at one of two fixed addresses (psx_overlay_region in the
 * top-level CMakeLists.txt) and the loader reads the overlay's image there. In the native
 * build the overlays' code is linked in with the rest, so the read only fills console RAM that
 * nothing executes, but the farm still uses the space after its image as a loading window, so
 * the regions must be real RAM at these addresses. Replaces src/main/overlay_regions.c. */

#include "types.h"

u8 *const overlay_region_table[] = {
    (u8 *)0x800B7E20,   /* low: where the EXE's .bss ends */
    (u8 *)0x8012DDE8,   /* high: where the farm's image ends */
};

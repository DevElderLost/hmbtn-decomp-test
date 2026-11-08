#include <stdio.h>
#include <string.h>
#include "psx_mem.h"

int main(void) {
    uint32_t *w;
    if (psx_mem_init() != 0) {
        printf("FAIL: psx_mem_init\n");
        return 1;
    }
    /* The game's own fixed addresses must be mapped and writable. */
    w = (uint32_t *)0x801BE1B0u;
    *w = 0x12345678u;
    ((uint32_t *)0x801FCFF8u)[1] = 1;
    *(uint32_t *)0x1F800000u = 7;
    if (*w != 0x12345678u || psx_ptr(0x801BE1B0u, 4) != (void *)0x801BE1B0u) {
        printf("FAIL: ram\n");
        return 1;
    }
    if (psx_ptr(0x80200000u, 4) != NULL || psx_ptr(0x801FFFFCu, 8) != NULL || psx_ptr(0x10u, 4) != NULL) {
        printf("FAIL: bounds\n");
        return 1;
    }
    if (psx_mem_init() != 0) { /* idempotent */
        return 1;
    }
    psx_mem_shutdown();
    printf("psx_mem: ok\n");
    return 0;
}

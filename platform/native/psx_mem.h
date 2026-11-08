#ifndef PSX_MEM_H
#define PSX_MEM_H

/* The console's address space, mapped at the same addresses on the host.
 *
 * The game keeps its ordering tables, primitive buffers and work areas at fixed PS1
 * addresses (0x801BE1B0 and so on) and links primitives through 24-bit tag addresses that
 * the GPU reads back as 0x80000000 | addr. Mapping RAM at its real address keeps all of
 * that working unchanged. Needs a 64-bit (or 32-bit) process whose low 2 GB are free,
 * which holds on Linux and Android. */

#include <stddef.h>
#include <stdint.h>

#define PSX_RAM_BASE     0x80000000u
#define PSX_RAM_SIZE     0x00200000u    /* 2 MB */
#define PSX_SCRATCH_BASE 0x1F800000u
#define PSX_SCRATCH_SIZE 0x1000u        /* one host page; the console has 1 KB */

/* Map RAM and the scratchpad. Returns 0 on success; safe to call twice. */
int psx_mem_init(void);
void psx_mem_shutdown(void);

/* Host pointer for a console address, or NULL if it is not mapped (`size` bytes must fit). */
void *psx_ptr(uint32_t addr, size_t size);

#endif /* PSX_MEM_H */

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#include "psx_mem.h"

#ifndef MAP_FIXED_NOREPLACE
#define MAP_FIXED_NOREPLACE 0x100000
#endif

static void *ram;
static void *scratch;

static void *map_at(uint32_t addr, size_t size) {
    void *p = mmap((void *)(uintptr_t)addr, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED) {
        return NULL;
    }
    if ((uintptr_t)p != addr) {
        /* An old kernel ignores MAP_FIXED_NOREPLACE and treats the address as a hint. */
        munmap(p, size);
        return NULL;
    }
    return p;
}

int psx_mem_init(void) {
    if (ram != NULL) {
        return 0;
    }
    ram = map_at(PSX_RAM_BASE, PSX_RAM_SIZE);
    scratch = map_at(PSX_SCRATCH_BASE, PSX_SCRATCH_SIZE);
    if (ram == NULL || scratch == NULL) {
        fprintf(stderr, "psx_mem: cannot map the PS1 address space (ram %p, scratchpad %p)\n", ram, scratch);
        psx_mem_shutdown();
        return -1;
    }
    return 0;
}

void psx_mem_shutdown(void) {
    if (ram != NULL) {
        munmap(ram, PSX_RAM_SIZE);
        ram = NULL;
    }
    if (scratch != NULL) {
        munmap(scratch, PSX_SCRATCH_SIZE);
        scratch = NULL;
    }
}

void *psx_ptr(uint32_t addr, size_t size) {
    if (ram != NULL && addr >= PSX_RAM_BASE && size <= PSX_RAM_SIZE && addr - PSX_RAM_BASE <= PSX_RAM_SIZE - size) {
        return (void *)(uintptr_t)addr;
    }
    if (scratch != NULL && addr >= PSX_SCRATCH_BASE && size <= PSX_SCRATCH_SIZE &&
        addr - PSX_SCRATCH_BASE <= PSX_SCRATCH_SIZE - size) {
        return (void *)(uintptr_t)addr;
    }
    return NULL;
}

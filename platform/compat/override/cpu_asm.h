#ifndef CPU_ASM_H
#define CPU_ASM_H

/* Native build: the CPU operations of the original, which have no host equivalent, become
 * no-ops. The scratchpad stays at its console address (see platform/native/psx_mem.c); the
 * code that runs on its own stack simply runs on the host stack. */

#define SCRATCHPAD          0x1F800000
#define SCRATCHPAD_SIZE     0x400
#define SCRATCHPAD_STACK_TOP (SCRATCHPAD + SCRATCHPAD_SIZE - 4)

#define SCRATCHPAD_STACK_ENTER() \
    {                            \
    }
#define SCRATCHPAD_STACK_LEAVE() \
    {                            \
    }

/* The saved return address is only ever read by the original's debugging aids. */
#define SAVE_RETURN_ADDRESS(where) \
    {                              \
        (void)(where);             \
    }

#define pollhost() ((void)0)

#endif /* CPU_ASM_H */

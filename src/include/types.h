#ifndef TYPES_H
#define TYPES_H

/* Primitive integer typedefs shared across the source tree. */

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;

/* Addresses held in integers. The console's pointers are 32 bits, so the game stores addresses in
 * s32 and u32 and does arithmetic on them. The native build (HMBTN_NATIVE) has 64-bit pointers
 * but keeps the console's RAM at the console's addresses (0x80000000 and up), so an address held
 * in a 32-bit integer is valid if it is zero-extended, not sign-extended. On the console these
 * expand to exactly the original expressions.
 *
 *   addr32                 an integer that holds a console RAM address (s32 on the console)
 *   ADDR_OFFSET(off, ptr)  the address `off` bytes from `ptr`, to cast back to a pointer */
#ifdef HMBTN_NATIVE
#include <stdint.h>
typedef unsigned int addr32;
#define ADDR_OFFSET(offset, ptr) ((uintptr_t)(ptr) + (offset))
#else
typedef signed int addr32;
#define ADDR_OFFSET(offset, ptr) ((offset) + (s32)(ptr))
#endif

#endif /* TYPES_H */

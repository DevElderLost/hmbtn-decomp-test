#ifndef PSYQ_COMPAT_SYS_TYPES_H
#define PSYQ_COMPAT_SYS_TYPES_H

/* The console's integer widths: PSY-Q's u_long is 32 bits, but the host libc's is 64. The
 * host's own typedefs are kept out of the way under other names while its header loads. */

#define u_char hmbtn_libc_u_char
#define u_short hmbtn_libc_u_short
#define u_int hmbtn_libc_u_int
#define u_long hmbtn_libc_u_long
#include_next <sys/types.h>
#undef u_char
#undef u_short
#undef u_int
#undef u_long

#include <stddef.h>
#include <stdint.h>

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef uint32_t u_long;

#endif /* PSYQ_COMPAT_SYS_TYPES_H */

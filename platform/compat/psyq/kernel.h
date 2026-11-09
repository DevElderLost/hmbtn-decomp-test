#ifndef PSYQ_COMPAT_KERNEL_H
#define PSYQ_COMPAT_KERNEL_H

#include <sys/types.h>

/* The kernel's fatal error call: shows its code and halts. */
void SystemError(char kind, int32_t code);

#endif /* PSYQ_COMPAT_KERNEL_H */

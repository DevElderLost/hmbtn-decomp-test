#ifndef PSYQ_COMPAT_LIBETC_H
#define PSYQ_COMPAT_LIBETC_H

#include <sys/types.h>

int VSync(int mode);
void *VSyncCallback(void (*func)(void));

#endif /* PSYQ_COMPAT_LIBETC_H */

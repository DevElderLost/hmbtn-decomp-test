#ifndef PSYQ_COMPAT_LIBAPI_H
#define PSYQ_COMPAT_LIBAPI_H

/* The BIOS-level calls: callbacks, memory size, the controller clear flag, and the directory
 * entry the memory card library fills. */

#include <sys/types.h>

struct DIRENTRY {
    char name[20];
    int32_t attr;
    int32_t size;
    struct DIRENTRY *next;
    int32_t head;
    char system[4];
};

int ResetCallback(void);
int StopCallback(void);
void SetMem(int megabytes);
void ChangeClearPAD(int flag);

#endif /* PSYQ_COMPAT_LIBAPI_H */

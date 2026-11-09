/* libapi, libetc and the kernel: callbacks, memory size and the fatal-error call. */

#include <stdio.h>
#include <libapi.h>
#include <kernel.h>
#include "psx_platform.h"

int ResetCallback(void) { return 0; }
int StopCallback(void) { return 0; }
void SetMem(int megabytes) { (void)megabytes; }
void ChangeClearPAD(int flag) { (void)flag; }

/* The kernel prints the code on screen and halts. `kind` is a letter or a small number naming
 * the part of the game. */
void SystemError(char kind, int32_t code) {
    if (kind >= 'A' && kind <= 'Z') {
        psx_fatal("SystemError %c (0x%X)", kind, (unsigned)code);
    } else {
        psx_fatal("SystemError 0x%02X (0x%X)", (unsigned)(unsigned char)kind, (unsigned)code);
    }
}

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include "psx_platform.h"

static void default_fatal(const char *message) {
    fprintf(stderr, "fatal: %s\n", message);
    abort();
}

void (*psx_fatal_hook)(const char *message) = default_fatal;

void psx_fatal(const char *format, ...) {
    char message[256];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    psx_fatal_hook(message);
    abort();                    /* a hook that returns is a bug */
}

#ifndef PSX_PLATFORM_H
#define PSX_PLATFORM_H

/* Hooks the host platform can take over. */

/* The console's code reached a state it cannot continue from (SystemError, a file that is
 * not on the disc). The default prints the message and aborts; the front end replaces it to
 * show the message and stop cleanly. It must not return. */
extern void (*psx_fatal_hook)(const char *message);

void psx_fatal(const char *format, ...);

#endif /* PSX_PLATFORM_H */

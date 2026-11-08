#ifndef PSX_PAD_H
#define PSX_PAD_H

/* The host side of the controller shim: the platform layer feeds it button masks and
 * commits them once per frame; the game then reads them through the libpad API
 * (platform/compat/libpad.h) exactly as it reads a real pad. */

#include <stdint.h>

/* Set the held buttons (PSX_PAD_* bits) of port 0 or 1. */
void psx_pad_set_buttons(int port, uint16_t mask);

/* A port with no pad connected reports PadStateDiscon. Port 0 starts connected. */
void psx_pad_set_connected(int port, int connected);

/* Write the receive buffers handed to PadInitDirect. Call once per frame, where the
 * console would take the vertical blank. */
void psx_pad_commit(void);

#endif /* PSX_PAD_H */

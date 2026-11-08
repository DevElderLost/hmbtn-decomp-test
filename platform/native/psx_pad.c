#include <stddef.h>
#include "libpad.h"
#include "psx_pad.h"

/* Receive buffer, as the game's PadBuffer lays it out:
 *   [0] status: 0 when the pad answered, 0xFF when there is none
 *   [1] type:   high nibble 4 = digital pad, low nibble = data length in halfwords
 *   [2] buttons:     select, L3, R3, start, up, right, down, left (bit 0 to 7)
 *   [3] buttonsHigh: L2, R2, L1, R1, triangle, circle, cross, square (bit 0 to 7)
 * Buttons are active low. This shim presents a plain digital pad. */
#define PAD_TYPE_DIGITAL_1 0x41

static unsigned char *buffers[2];
static unsigned char *actuators[2];
static int actuator_length[2];
static uint16_t held[2];
static int connected[2] = {1, 0};
static int started;

static int port_index(int port) {
    return (port & 0x10) ? 1 : 0;
}

void psx_pad_set_buttons(int port, uint16_t mask) {
    if (port == 0 || port == 1) {
        held[port] = mask;
    }
}

void psx_pad_set_connected(int port, int is_connected) {
    if (port == 0 || port == 1) {
        connected[port] = is_connected != 0;
    }
}

void psx_pad_commit(void) {
    int i;

    for (i = 0; i < 2; i++) {
        unsigned char *b = buffers[i];
        if (b == NULL) {
            continue;
        }
        if (!started || !connected[i]) {
            b[0] = 0xFF;
            b[1] = 0xFF;
            b[2] = 0xFF;
            b[3] = 0xFF;
        } else {
            b[0] = 0;
            b[1] = PAD_TYPE_DIGITAL_1;
            b[2] = (unsigned char)~(held[i] >> 8);
            b[3] = (unsigned char)~held[i];
        }
    }
}

void PadInitDirect(unsigned char *pad1, unsigned char *pad2) {
    buffers[0] = pad1;
    buffers[1] = pad2;
    psx_pad_commit();
}

void PadStartCom(void) {
    started = 1;
    psx_pad_commit();
}

void PadStopCom(void) {
    started = 0;
    psx_pad_commit();
}

unsigned int PadGetState(int port) {
    int i = port_index(port);
    return (started && connected[i]) ? PadStateStable : PadStateDiscon;
}

int PadInfoMode(int port, int term, int offs) {
    int i = port_index(port);
    (void)offs;
    if (!connected[i]) {
        return 0;
    }
    /* CurID is the pad's terminal id (4: digital); the extended id and its offset are 0. */
    return term == InfoModeCurID ? 4 : 0;
}

int PadSetAct(int port, unsigned char *act, int length) {
    int i = port_index(port);
    actuators[i] = act;
    actuator_length[i] = length;
    return 1;
}

int PadSetActAlign(int port, unsigned char *align) {
    (void)port;
    (void)align;
    return 1;
}

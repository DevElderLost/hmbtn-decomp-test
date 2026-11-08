#include <stdio.h>
#include <string.h>
#include "libpad.h"
#include "psx_pad.h"
#include "psx_buttons.h"

static int failures;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

/* The game's PadBuffer (src/main/input/pad.h): 8 bytes of header, then 0x1A bytes. */
typedef struct {
    unsigned char status, type, buttons, buttonsHigh, rightX, rightY, leftX, leftY;
    unsigned char pad8[0x1A];
} Buf;

/* What pad_read_buttons and pad_read_record in src/main/input/pad.c do to a digital pad. */
static unsigned short decode(const Buf *b) {
    return (unsigned short)~(((unsigned)b->buttons << 8) | b->buttonsHigh);
}

int main(void) {
    Buf a, b;
    unsigned char act[2][8];
    const unsigned short masks[] = {
        0,
        PSX_PAD_CROSS,
        PSX_PAD_UP | PSX_PAD_RIGHT | PSX_PAD_CIRCLE,
        PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_L2 | PSX_PAD_R2,
        PSX_PAD_START | PSX_PAD_SELECT | PSX_PAD_TRIANGLE | PSX_PAD_SQUARE,
        PSX_PAD_LEFT | PSX_PAD_DOWN,
        0xFFFF,
    };
    unsigned i;

    memset(&a, 0xAA, sizeof(a));
    memset(&b, 0xAA, sizeof(b));

    /* Before PadStartCom no pad is ready. */
    CHECK(PadGetState(0x00) == PadStateDiscon);
    PadInitDirect((unsigned char *)&a, (unsigned char *)&b);
    CHECK(PadSetAct(0x00, act[0], 8) == 1);
    CHECK(PadSetAct(0x10, act[1], 8) == 1);
    PadStartCom();

    /* Port 1 is a ready digital pad; port 2 has none. */
    CHECK(PadGetState(0x00) == PadStateStable);
    CHECK(PadGetState(0x10) == PadStateDiscon);
    CHECK(PadInfoMode(0x00, InfoModeCurID, 0) == 4);
    CHECK(PadInfoMode(0x00, InfoModeCurExID, 0) == 0);
    CHECK(PadSetActAlign(0x00, act[0]) != 0);
    CHECK(b.status != 0); /* pad_read_buttons reports an error for the empty port */

    for (i = 0; i < sizeof(masks) / sizeof(masks[0]); i++) {
        psx_pad_set_buttons(0, masks[i]);
        psx_pad_commit();
        CHECK(a.status == 0);
        CHECK((a.type >> 4) == 4);          /* PAD_TYPE_DIGITAL */
        CHECK(decode(&a) == masks[i]);
    }

    /* Idle reads all ones, as real hardware does. */
    psx_pad_set_buttons(0, 0);
    psx_pad_commit();
    CHECK(a.buttons == 0xFF && a.buttonsHigh == 0xFF);

    /* A bit position check against the hardware layout. */
    psx_pad_set_buttons(0, PSX_PAD_UP);
    psx_pad_commit();
    CHECK(a.buttons == (unsigned char)~0x10 && a.buttonsHigh == 0xFF);
    psx_pad_set_buttons(0, PSX_PAD_START);
    psx_pad_commit();
    CHECK(a.buttons == (unsigned char)~0x08);
    psx_pad_set_buttons(0, PSX_PAD_SQUARE);
    psx_pad_commit();
    CHECK(a.buttonsHigh == (unsigned char)~0x80);

    /* A second pad can be connected. */
    psx_pad_set_connected(1, 1);
    psx_pad_set_buttons(1, PSX_PAD_TRIANGLE);
    psx_pad_commit();
    CHECK(PadGetState(0x10) == PadStateStable);
    CHECK(b.status == 0 && decode(&b) == PSX_PAD_TRIANGLE);

    /* Stopping the pad library marks both ports as empty. */
    PadStopCom();
    CHECK(PadGetState(0x00) == PadStateDiscon);
    CHECK(a.status != 0);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("psx_pad: ok\n");
    return 0;
}

#ifndef PSX_BUTTONS_H
#define PSX_BUTTONS_H

/* PS1 button bits, positive logic. The values are those of enum PadButton in
 * src/main/input/pad.h (InputRecord.held), so a mask built here can be handed to the
 * game's pad code unchanged. */

#define PSX_PAD_L2       0x0001
#define PSX_PAD_R2       0x0002
#define PSX_PAD_L1       0x0004
#define PSX_PAD_R1       0x0008
#define PSX_PAD_TRIANGLE 0x0010
#define PSX_PAD_CIRCLE   0x0020
#define PSX_PAD_CROSS    0x0040
#define PSX_PAD_SQUARE   0x0080
#define PSX_PAD_SELECT   0x0100
#define PSX_PAD_START    0x0800
#define PSX_PAD_UP       0x1000
#define PSX_PAD_RIGHT    0x2000
#define PSX_PAD_DOWN     0x4000
#define PSX_PAD_LEFT     0x8000

#endif /* PSX_BUTTONS_H */

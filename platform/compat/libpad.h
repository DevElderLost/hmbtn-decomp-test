#ifndef COMPAT_LIBPAD_H
#define COMPAT_LIBPAD_H

/* The controller API of PSY-Q's libpad that src/main/input/pad.c uses, for builds that do
 * not have the PSY-Q headers (the native Android build). Implemented in
 * platform/native/psx_pad.c. */

/* PadGetState results. */
#define PadStateDiscon  0
#define PadStateFindPad 1
#define PadStateFindCTP1 2
#define PadStateFindCTP2 3
#define PadStateReqInfo 4
#define PadStateExecCmd 5
#define PadStateStable  6

/* PadInfoMode `term` values. */
#define InfoModeCurID     1
#define InfoModeCurExID   2
#define InfoModeCurExOffs 3
#define InfoModeIdTable   4

/* Port numbers, as the game passes them: 0x00 is port 1, 0x10 is port 2. */
void PadInitDirect(unsigned char *pad1, unsigned char *pad2);
void PadStartCom(void);
void PadStopCom(void);
unsigned int PadGetState(int port);
int PadInfoMode(int port, int term, int offs);
int PadSetAct(int port, unsigned char *actuators, int length);
int PadSetActAlign(int port, unsigned char *align);

#endif /* COMPAT_LIBPAD_H */

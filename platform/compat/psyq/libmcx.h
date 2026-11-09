#ifndef PSYQ_COMPAT_LIBMCX_H
#define PSYQ_COMPAT_LIBMCX_H

/* The PocketStation extension of the memory card library. The native build has no
 * PocketStation: McxCardType reports an ordinary card and the rest report that no
 * PocketStation is present. */

#include <sys/types.h>

#define McxErrSuccess 0
#define McxErrNoCard 1
#define McxErrInvalid 2
#define McxErrNewCard 3
#define McxErrNotFormat 4
#define McxErrFileNotExist 5
#define McxErrAlreadyExist 6
#define McxErrBlockFull 7
#define McxErrNotPDA 8

#define McxFuncExecFlag 0x41
#define McxFuncExecApl 0x42

/* The PocketStation calls take and return `long`, and fill buffers the game declares in
 * several ways; the buffers are `void *` here (these calls fill nothing). */
long McxStartCom(void);
long McxStopCom(void);
long McxCardType(long chan);
long McxSync(long mode, long *cmds, long *rslt);
long McxAllInfo(long chan, void *info);
long McxGetApl(long chan, void *info);
long McxGetTime(long chan, void *time);
long McxGetSerial(long chan, void *serial);
long McxGetMem(long chan, void *buf, long addr, long size);
long McxSetMem(long chan, void *buf, long addr, long size);
long McxReadDev(long chan, long device, void *fix, void *read);
long McxWriteDev(long chan, long device, void *fix, void *read);
long McxExecFlag(long chan, long block, long flag);
long McxExecApl(long chan, long apl, long param);
long McxCurrCtrl(long chan, long sound, long infrared, long other);
long McxFlashAcs(long chan, long flag);
long McxShowTrans(long chan, long shown, long timeout);
long McxHideTrans(long chan);
long McxSetLED(long chan, long flag);

#endif /* PSYQ_COMPAT_LIBMCX_H */

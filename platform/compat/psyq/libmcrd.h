#ifndef PSYQ_COMPAT_LIBMCRD_H
#define PSYQ_COMPAT_LIBMCRD_H

/* The memory card library: asynchronous in the original, here each command completes at
 * once and MemCardSync reports it. Implemented in platform/native/psx_mcard.c. */

#include <libapi.h>

/* Results of a command (MemCardSync's `result`). */
#define McErrNone 0
#define McErrCardNotExist 1
#define McErrCardInvalid 2
#define McErrNewCard 3
#define McErrNotFormat 4
#define McErrFileNotExist 5
#define McErrAlreadyExist 6
#define McErrBlockFull 7
#define McErrExtend 0x8

/* The command in flight (MemCardSync's `cmds`). */
#define McFuncExist 1
#define McFuncAccept 2
#define McFuncReadFile 3
#define McFuncWriteFile 4
#define McFuncReadData 5
#define McFuncWriteData 6

/* The parameters are `long`, as in the SDK: the game keeps `long cmd, result` and passes their
 * addresses, so they must be the host's long too. */
void MemCardInit(long val);
void MemCardEnd(void);
void MemCardStart(void);
void MemCardStop(void);
long MemCardExist(long chan);
long MemCardAccept(long chan);
long MemCardReadFile(long chan, char *file, unsigned long *buf, long offset, long bytes);
long MemCardWriteFile(long chan, char *file, unsigned long *buf, long offset, long bytes);
long MemCardCreateFile(long chan, char *file, long blocks);
long MemCardDeleteFile(long chan, char *file);
long MemCardFormat(long chan);
long MemCardUnformat(long chan);
long MemCardSync(long mode, long *cmds, long *rslt);
long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir, long *files, long ofs, long max);

#endif /* PSYQ_COMPAT_LIBMCRD_H */

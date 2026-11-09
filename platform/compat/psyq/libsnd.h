#ifndef PSYQ_COMPAT_LIBSND_H
#define PSYQ_COMPAT_LIBSND_H

/* The sound library: sequences (SEQ) played through sound banks (VAB). Implemented in
 * platform/native/psx_snd.c, which keeps the library's state but does not yet synthesise. */

#include <libspu.h>

#define SS_WAIT_COMPLETED 0x01
#define SS_IMMEDIATE 0x00
#define SS_TRANS_NOT_COMPLETED 0
#define SS_TRANS_COMPLETED 1

void SsInit(void);
void SsStart(void);
void SsEnd(void);
void SsSetTableSize(char *table, short s_max, short t_max);
void SsSetTickMode(long tick_mode);
void SsSetSerialAttr(char s_num, char attr, char mode);
void SsSetSerialVol(char s_num, short voll, short volr);
void SsSetMVol(short voll, short volr);
void SsSetRVol(short voll, short volr);
void SsSetStereo(void);
void SsSetMono(void);
char SsSetReservedVoice(char voices);

short SsVabOpenHead(unsigned char *addr, short vab_id);
short SsVabTransBodyPartly(unsigned char *addr, unsigned long bufsize, short vab_id);
short SsVabTransCompleted(short immediate_flag);
void SsVabClose(short vab_id);

short SsSeqOpen(u_long *addr, short vab_id);
void SsSeqClose(short seq_access_num);
void SsSeqPlay(short seq_access_num, char play_mode, short l_count);
void SsSeqPause(short seq_access_num);
void SsSeqReplay(short seq_access_num);
void SsSeqStop(short seq_access_num);
void SsSeqSetVol(short seq_access_num, short voll, short volr);

short SsUtKeyOn(short vab_id, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOff(short voice, short vab_id, short prog, short tone, short note);
short SsUtKeyOnV(short voice, short vab_id, short prog, short tone, short note, short fine, short voll, short volr);
short SsUtKeyOffV(short voice);
short SsUtAllKeyOff(short mode);
short SsUtSetVVol(short voice, short voll, short volr);
void SsVoKeyOff(long vc, long mode);
void SsUtReverbOn(void);
void SsUtReverbOff(void);
short SsUtSetReverbType(short type);
void SsUtSetReverbDepth(short ldepth, short rdepth);
void SsUtSetReverbFeedback(short feedback);
void SsUtSetReverbDelay(short delay);

#endif /* PSYQ_COMPAT_LIBSND_H */

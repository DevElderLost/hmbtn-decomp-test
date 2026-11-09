#ifndef PSX_SND_H
#define PSX_SND_H

/* The sound library's state, for the platform to read (and, later, to synthesise from). */

#include <stdint.h>

#define PSX_SND_MAX_SEQ 32
#define PSX_SND_MAX_VAB 16

typedef struct PsxSeq {
    int open;
    const void *data;           /* the SEQ in console RAM */
    int vab;
    int playing, paused;
    int volume_left, volume_right;
} PsxSeq;

const PsxSeq *psx_snd_seq(int index);
int psx_snd_vab_open(int vab);
/* The number of voices currently keyed on. */
int psx_snd_voices_on(void);

#endif /* PSX_SND_H */

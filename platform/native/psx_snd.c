#include <string.h>
#include <libsnd.h>
#include "psx_libgpu.h"
#include "psx_snd.h"

/* A key-on lasts this many vertical blanks unless the game keys the voice off first. There is no
 * synthesis yet, so a note's real length is not known; without an end the game would think its
 * voices stay busy for ever. */
#define NOTE_FRAMES 60

static PsxSeq seqs[PSX_SND_MAX_SEQ];
static int vab_open[PSX_SND_MAX_VAB];
static int voice_on[SPU_NUM_VOICES];       /* frames left; 0 means off */
static int hook_added;

static void vblank(void) {
    int v;
    for (v = 0; v < SPU_NUM_VOICES; v++) {
        if (voice_on[v] > 0) {
            voice_on[v]--;
        }
    }
}

static void ensure_hook(void) {
    if (!hook_added) {
        psx_vblank_add_hook(vblank);
        hook_added = 1;
    }
}

const PsxSeq *psx_snd_seq(int index) { return index >= 0 && index < PSX_SND_MAX_SEQ ? &seqs[index] : NULL; }
int psx_snd_vab_open(int vab) { return vab >= 0 && vab < PSX_SND_MAX_VAB && vab_open[vab]; }

int psx_snd_voices_on(void) {
    int v, n = 0;
    for (v = 0; v < SPU_NUM_VOICES; v++) {
        n += voice_on[v] > 0;
    }
    return n;
}

/* ---- set-up ---------------------------------------------------------------------------- */

void SsInit(void) {
    memset(seqs, 0, sizeof(seqs));
    memset(vab_open, 0, sizeof(vab_open));
    memset(voice_on, 0, sizeof(voice_on));
    ensure_hook();
}

void SsStart(void) { ensure_hook(); }
void SsEnd(void) {}
void SsSetTableSize(char *table, short s_max, short t_max) { (void)table; (void)s_max; (void)t_max; }
void SsSetTickMode(long tick_mode) { (void)tick_mode; }
void SsSetSerialAttr(char s_num, char attr, char mode) { (void)s_num; (void)attr; (void)mode; }
void SsSetSerialVol(char s_num, short voll, short volr) { (void)s_num; (void)voll; (void)volr; }
void SsSetMVol(short voll, short volr) { (void)voll; (void)volr; }
void SsSetRVol(short voll, short volr) { (void)voll; (void)volr; }
void SsSetStereo(void) {}
void SsSetMono(void) {}
char SsSetReservedVoice(char voices) { return voices; }

void SpuSetCommonAttr(SpuCommonAttr *attr) { (void)attr; }

/* ---- sound banks ----------------------------------------------------------------------- */

short SsVabOpenHead(unsigned char *addr, short vab_id) {
    (void)addr;
    if (vab_id < 0) {
        for (vab_id = 0; vab_id < PSX_SND_MAX_VAB && vab_open[vab_id]; vab_id++) {
        }
    }
    if (vab_id >= PSX_SND_MAX_VAB) {
        return -1;
    }
    vab_open[vab_id] = 1;
    return vab_id;
}

short SsVabTransBodyPartly(unsigned char *addr, unsigned long bufsize, short vab_id) {
    (void)addr; (void)bufsize;
    return vab_id;
}

/* Transfers finish at once. */
short SsVabTransCompleted(short immediate_flag) {
    (void)immediate_flag;
    return SS_TRANS_COMPLETED;
}

void SsVabClose(short vab_id) {
    if (vab_id >= 0 && vab_id < PSX_SND_MAX_VAB) {
        vab_open[vab_id] = 0;
    }
}

/* ---- sequences ------------------------------------------------------------------------- */

short SsSeqOpen(u_long *addr, short vab_id) {
    short i;
    for (i = 0; i < PSX_SND_MAX_SEQ; i++) {
        if (!seqs[i].open) {
            memset(&seqs[i], 0, sizeof(seqs[i]));
            seqs[i].open = 1;
            seqs[i].data = addr;
            seqs[i].vab = vab_id;
            seqs[i].volume_left = seqs[i].volume_right = 127;
            return i;
        }
    }
    return -1;
}

static PsxSeq *seq_at(short n) { return n >= 0 && n < PSX_SND_MAX_SEQ && seqs[n].open ? &seqs[n] : NULL; }

void SsSeqClose(short n) { PsxSeq *s = seq_at(n); if (s) memset(s, 0, sizeof(*s)); }
void SsSeqPlay(short n, char play_mode, short l_count) {
    PsxSeq *s = seq_at(n);
    (void)play_mode; (void)l_count;
    if (s) { s->playing = 1; s->paused = 0; }
}
void SsSeqPause(short n) { PsxSeq *s = seq_at(n); if (s) s->paused = 1; }
void SsSeqReplay(short n) { PsxSeq *s = seq_at(n); if (s) s->paused = 0; }
void SsSeqStop(short n) { PsxSeq *s = seq_at(n); if (s) { s->playing = 0; s->paused = 0; } }
void SsSeqSetVol(short n, short voll, short volr) {
    PsxSeq *s = seq_at(n);
    if (s) { s->volume_left = voll; s->volume_right = volr; }
}

/* ---- voices ---------------------------------------------------------------------------- */

short SsUtKeyOn(short vab_id, short prog, short tone, short note, short fine, short voll, short volr) {
    int v;
    (void)vab_id; (void)prog; (void)tone; (void)note; (void)fine; (void)voll; (void)volr;
    for (v = 0; v < SPU_NUM_VOICES; v++) {
        if (voice_on[v] == 0) {
            voice_on[v] = NOTE_FRAMES;
            return (short)v;
        }
    }
    return -1;
}

short SsUtKeyOnV(short voice, short vab_id, short prog, short tone, short note, short fine, short voll, short volr) {
    (void)vab_id; (void)prog; (void)tone; (void)note; (void)fine; (void)voll; (void)volr;
    if (voice < 0 || voice >= SPU_NUM_VOICES) {
        return -1;
    }
    voice_on[voice] = NOTE_FRAMES;
    return voice;
}

short SsUtKeyOff(short voice, short vab_id, short prog, short tone, short note) {
    (void)vab_id; (void)prog; (void)tone; (void)note;
    return SsUtKeyOffV(voice);
}

short SsUtKeyOffV(short voice) {
    if (voice < 0 || voice >= SPU_NUM_VOICES) {
        return -1;
    }
    voice_on[voice] = 0;
    return 0;
}

short SsUtAllKeyOff(short mode) {
    (void)mode;
    memset(voice_on, 0, sizeof(voice_on));
    return 0;
}

short SsUtSetVVol(short voice, short voll, short volr) { (void)voice; (void)voll; (void)volr; return 0; }
/* The voice number is in the low bits (the bank id sits above them). */
void SsVoKeyOff(long vc, long mode) {
    (void)mode;
    SsUtKeyOffV((short)(vc & 0xFF));
}

long SpuGetKeyStatus(unsigned long voice_bit) {
    int v;
    for (v = 0; v < SPU_NUM_VOICES; v++) {
        if (voice_bit & (1UL << v)) {
            return voice_on[v] > 0 ? SPU_ON : SPU_OFF;
        }
    }
    return SPU_OFF;
}

void SpuGetAllKeysStatus(char *status) {
    int v;
    for (v = 0; v < SPU_NUM_VOICES; v++) {
        status[v] = (char)(voice_on[v] > 0 ? SPU_ON : SPU_OFF);
    }
}

/* ---- reverb ---------------------------------------------------------------------------- */

void SsUtReverbOn(void) {}
void SsUtReverbOff(void) {}
short SsUtSetReverbType(short type) { return type; }
void SsUtSetReverbDepth(short ldepth, short rdepth) { (void)ldepth; (void)rdepth; }
void SsUtSetReverbFeedback(short feedback) { (void)feedback; }
void SsUtSetReverbDelay(short delay) { (void)delay; }

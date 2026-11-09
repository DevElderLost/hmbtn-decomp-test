#include <stdio.h>
#include <string.h>
#include <libmcrd.h>
#include <libmcx.h>
#include "psx_mcard.h"

#define FRAME 128
#define BLOCK 0x2000
#define NAME_LEN 20

/* A directory frame's allocation state. */
#define ALLOC_FIRST 0x51
#define ALLOC_MIDDLE 0x52
#define ALLOC_LAST 0x53
#define ALLOC_FREE 0xA0
#define ALLOC_DELETED_FIRST 0xA1
#define ALLOC_DELETED_MIDDLE 0xA2
#define ALLOC_DELETED_LAST 0xA3

typedef struct Card {
    uint8_t data[PSX_MCARD_SIZE];
    int attached;
    int new_card;               /* the first command after insertion reports McErrNewCard */
    char path[512];
} Card;

static Card cards[2];
static int pending;
static long pending_command;
static long pending_result;

static int slot_of(long chan) { return (int)((chan >> 4) & 1); }

/* ---- the card image -------------------------------------------------------------------- */

static void set_checksum(uint8_t *frame) {
    uint8_t x = 0;
    int i;
    for (i = 0; i < FRAME - 1; i++) {
        x ^= frame[i];
    }
    frame[FRAME - 1] = x;
}

static int frame_ok(const uint8_t *frame) {
    uint8_t x = 0;
    int i;
    for (i = 0; i < FRAME - 1; i++) {
        x ^= frame[i];
    }
    return x == frame[FRAME - 1];
}

int psx_mcard_formatted(int slot) {
    const uint8_t *d = cards[slot].data;
    return cards[slot].attached && d[0] == 'M' && d[1] == 'C' && frame_ok(d);
}

static void make_free_directory_frame(uint8_t *frame) {
    memset(frame, 0, FRAME);
    frame[0] = ALLOC_FREE;
    frame[8] = frame[9] = 0xFF;
    set_checksum(frame);
}

static void format_image(Card *c) {
    int i;
    memset(c->data, 0, sizeof(c->data));
    c->data[0] = 'M';
    c->data[1] = 'C';
    set_checksum(c->data);
    for (i = 1; i <= 15; i++) {
        make_free_directory_frame(c->data + i * FRAME);
    }
    for (i = 16; i < 36; i++) {                     /* the broken-sector list: none */
        memset(c->data + i * FRAME, 0, FRAME);
        memset(c->data + i * FRAME, 0xFF, 4);
        set_checksum(c->data + i * FRAME);
    }
    memcpy(c->data + 63 * FRAME, c->data, FRAME);   /* the write-test frame */
}

static void save(Card *c) {
    FILE *f = fopen(c->path, "wb");
    if (f != NULL) {
        fwrite(c->data, 1, sizeof(c->data), f);
        fclose(f);
    }
}

int psx_mcard_attach(int slot, const char *path) {
    Card *c;
    FILE *f;
    if (slot < 0 || slot > 1) {
        return -1;
    }
    c = &cards[slot];
    memset(c, 0, sizeof(*c));
    strncpy(c->path, path, sizeof(c->path) - 1);
    f = fopen(path, "rb");
    if (f != NULL) {
        const size_t n = fread(c->data, 1, sizeof(c->data), f);
        fclose(f);
        if (n != sizeof(c->data)) {
            memset(c->data, 0, sizeof(c->data));        /* not a card image: treat as a blank card */
        }
    } else {
        FILE *created = fopen(path, "wb");
        if (created == NULL) {
            return -1;
        }
        fwrite(c->data, 1, sizeof(c->data), created);
        fclose(created);
    }
    c->attached = 1;
    c->new_card = 1;
    return 0;
}

void psx_mcard_detach(int slot) {
    if (slot == 0 || slot == 1) {
        memset(&cards[slot], 0, sizeof(cards[slot]));
    }
}

/* ---- directory ------------------------------------------------------------------------- */

static uint8_t *dir_frame(Card *c, int block) { return c->data + block * FRAME; }

static int is_first(const uint8_t *frame) { return frame[0] == ALLOC_FIRST; }

static uint32_t frame_size(const uint8_t *frame) {
    return (uint32_t)frame[4] | ((uint32_t)frame[5] << 8) | ((uint32_t)frame[6] << 16) | ((uint32_t)frame[7] << 24);
}

static int name_equals(const uint8_t *frame, const char *name) {
    char padded[NAME_LEN + 1] = {0};
    strncpy(padded, name, NAME_LEN);
    return memcmp(frame + 10, padded, NAME_LEN) == 0;
}

/* The directory block (1..15) of file `name`, or 0. */
static int find_file(Card *c, const char *name) {
    int b;
    for (b = 1; b <= 15; b++) {
        if (is_first(dir_frame(c, b)) && name_equals(dir_frame(c, b), name)) {
            return b;
        }
    }
    return 0;
}

/* `*` matches the rest and `?` one character, as the SDK's name patterns do. */
static int pattern_matches(const char *pattern, const uint8_t *name) {
    int i;
    for (i = 0; i < NAME_LEN; i++) {
        const char p = pattern[i];
        if (p == '*') {
            return 1;
        }
        if (p == 0) {
            return name[i] == 0;
        }
        if (p != '?' && p != (char)name[i]) {
            return 0;
        }
    }
    return 1;
}

/* The card state check shared by the commands: a McErr code, or McErrNone if usable. */
static long check_card(int slot) {
    if (!cards[slot].attached) {
        return McErrCardNotExist;
    }
    if (!psx_mcard_formatted(slot)) {
        return McErrNotFormat;
    }
    return McErrNone;
}

/* ---- commands -------------------------------------------------------------------------- */

static long complete(long command, long result) {
    pending = 1;
    pending_command = command;
    pending_result = result;
    return 1;
}

void MemCardInit(long val) { (void)val; pending = 0; }
void MemCardEnd(void) {}
void MemCardStart(void) {}
void MemCardStop(void) {}

/* Exist and Accept: is a card there, was it just inserted, is it formatted? */
static long probe(long chan, long command, int check_format) {
    const int slot = slot_of(chan);
    Card *c = &cards[slot];
    if (!c->attached) {
        return complete(command, McErrCardNotExist);
    }
    if (c->new_card) {
        c->new_card = 0;
        return complete(command, McErrNewCard);
    }
    if (check_format && !psx_mcard_formatted(slot)) {
        return complete(command, McErrNotFormat);
    }
    return complete(command, McErrNone);
}

long MemCardExist(long chan) { return probe(chan, McFuncExist, 0); }
long MemCardAccept(long chan) { return probe(chan, McFuncAccept, 1); }

/* 1 when a command finished (its result is returned), -1 when none is pending. */
long MemCardSync(long mode, long *cmds, long *rslt) {
    (void)mode;
    if (!pending) {
        return -1;
    }
    pending = 0;
    if (cmds != NULL) {
        *cmds = pending_command;
    }
    if (rslt != NULL) {
        *rslt = pending_result;
    }
    return 1;
}

long MemCardFormat(long chan) {
    const int slot = slot_of(chan);
    if (!cards[slot].attached) {
        return McErrCardNotExist;
    }
    format_image(&cards[slot]);
    cards[slot].new_card = 0;
    save(&cards[slot]);
    return McErrNone;
}

long MemCardUnformat(long chan) {
    const int slot = slot_of(chan);
    if (!cards[slot].attached) {
        return McErrCardNotExist;
    }
    memset(cards[slot].data, 0, sizeof(cards[slot].data));
    save(&cards[slot]);
    return McErrNone;
}

long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir, long *files, long ofs, long max) {
    const int slot = slot_of(chan);
    Card *c = &cards[slot];
    long found = 0, skipped = 0;
    int b;
    const long state = check_card(slot);

    if (files != NULL) {
        *files = 0;
    }
    if (state != McErrNone) {
        return state;
    }
    for (b = 1; b <= 15 && found < max; b++) {
        const uint8_t *frame = dir_frame(c, b);
        if (!is_first(frame) || !pattern_matches(name, frame + 10)) {
            continue;
        }
        if (skipped < ofs) {
            skipped++;
            continue;
        }
        memset(&dir[found], 0, sizeof(dir[found]));
        memcpy(dir[found].name, frame + 10, NAME_LEN);
        dir[found].size = (int32_t)frame_size(frame);
        dir[found].head = b;
        found++;
    }
    if (files != NULL) {
        *files = found;
    }
    return McErrNone;
}

long MemCardCreateFile(long chan, char *file, long blocks) {
    const int slot = slot_of(chan);
    Card *c = &cards[slot];
    int free_blocks[15], nfree = 0, b, i;
    const long state = check_card(slot);

    if (state != McErrNone) {
        return state;
    }
    if (blocks < 1 || blocks > 15) {
        return McErrBlockFull;
    }
    if (find_file(c, file) != 0) {
        return McErrAlreadyExist;
    }
    for (b = 1; b <= 15; b++) {
        const uint8_t a = dir_frame(c, b)[0];
        if (a == ALLOC_FREE || a == ALLOC_DELETED_FIRST || a == ALLOC_DELETED_MIDDLE || a == ALLOC_DELETED_LAST) {
            free_blocks[nfree++] = b;
        }
    }
    if (nfree < blocks) {
        return McErrBlockFull;
    }
    for (i = 0; i < blocks; i++) {
        uint8_t *frame = dir_frame(c, free_blocks[i]);
        memset(frame, 0, FRAME);
        frame[0] = i == 0 ? ALLOC_FIRST : (i == blocks - 1 ? ALLOC_LAST : ALLOC_MIDDLE);
        if (i == 0) {
            const uint32_t size = (uint32_t)blocks * BLOCK;
            frame[4] = (uint8_t)size; frame[5] = (uint8_t)(size >> 8);
            frame[6] = (uint8_t)(size >> 16); frame[7] = (uint8_t)(size >> 24);
            strncpy((char *)frame + 10, file, NAME_LEN);
        }
        if (i == blocks - 1) {
            frame[8] = frame[9] = 0xFF;
        } else {
            frame[8] = (uint8_t)(free_blocks[i + 1] - 1);   /* the next directory frame, from 0 */
            frame[9] = 0;
        }
        set_checksum(frame);
        memset(c->data + (size_t)free_blocks[i] * BLOCK, 0, BLOCK);
    }
    save(c);
    return McErrNone;
}

long MemCardDeleteFile(long chan, char *file) {
    const int slot = slot_of(chan);
    Card *c = &cards[slot];
    int b;
    const long state = check_card(slot);

    if (state != McErrNone) {
        return state;
    }
    b = find_file(c, file);
    if (b == 0) {
        return McErrFileNotExist;
    }
    for (;;) {
        uint8_t *frame = dir_frame(c, b);
        const int last = frame[8] == 0xFF && frame[9] == 0xFF;
        const int next = frame[8] + 1;
        frame[0] = (uint8_t)(frame[0] + 0x50);            /* 51/52/53 become A1/A2/A3 */
        set_checksum(frame);
        if (last) {
            break;
        }
        b = next;
    }
    save(c);
    return McErrNone;
}

/* Read or write `bytes` of file `file` from `offset`, across its chain of blocks. Returns a
 * McErr code. */
static long transfer(long chan, char *file, unsigned long *buf, long offset, long bytes, int writing) {
    const int slot = slot_of(chan);
    Card *c = &cards[slot];
    uint8_t *p = (uint8_t *)buf;
    int chain[15], n = 0, b;
    long state = check_card(slot);

    if (state != McErrNone) {
        return state;
    }
    b = find_file(c, file);
    if (b == 0) {
        return McErrFileNotExist;
    }
    for (;;) {
        const uint8_t *frame = dir_frame(c, b);
        chain[n++] = b;
        if ((frame[8] == 0xFF && frame[9] == 0xFF) || n == 15) {
            break;
        }
        b = frame[8] + 1;
    }
    if (offset < 0 || bytes < 0 || offset + bytes > (long)n * BLOCK) {
        return McErrFileNotExist;
    }
    while (bytes > 0) {
        const long in_block = offset % BLOCK;
        long chunk = BLOCK - in_block;
        uint8_t *at = c->data + (size_t)chain[offset / BLOCK] * BLOCK + in_block;
        if (chunk > bytes) {
            chunk = bytes;
        }
        if (writing) {
            memcpy(at, p, (size_t)chunk);
        } else {
            memcpy(p, at, (size_t)chunk);
        }
        p += chunk;
        offset += chunk;
        bytes -= chunk;
    }
    if (writing) {
        save(c);
    }
    return McErrNone;
}

long MemCardReadFile(long chan, char *file, unsigned long *buf, long offset, long bytes) {
    return complete(McFuncReadFile, transfer(chan, file, buf, offset, bytes, 0));
}

long MemCardWriteFile(long chan, char *file, unsigned long *buf, long offset, long bytes) {
    return complete(McFuncWriteFile, transfer(chan, file, buf, offset, bytes, 1));
}

/* ---- PocketStation: not present --------------------------------------------------------- */

long McxStartCom(void) { return 1; }
long McxStopCom(void) { return 1; }
long McxCardType(long chan) { (void)chan; return 0; }      /* an ordinary memory card */
long McxSync(long mode, long *cmds, long *rslt) {
    (void)mode;
    if (cmds != NULL) *cmds = 0;
    if (rslt != NULL) *rslt = McxErrNoCard;
    return 1;
}
long McxAllInfo(long chan, void *info) { (void)chan; (void)info; return 0; }
long McxGetApl(long chan, void *info) { (void)chan; (void)info; return 0; }
long McxGetTime(long chan, void *time) { (void)chan; (void)time; return 0; }
long McxGetSerial(long chan, void *serial) { (void)chan; (void)serial; return 0; }
long McxGetMem(long chan, void *buf, long addr, long size) { (void)chan; (void)buf; (void)addr; (void)size; return 0; }
long McxSetMem(long chan, void *buf, long addr, long size) { (void)chan; (void)buf; (void)addr; (void)size; return 0; }
long McxReadDev(long chan, long device, void *fix, void *read) { (void)chan; (void)device; (void)fix; (void)read; return 0; }
long McxWriteDev(long chan, long device, void *fix, void *read) { (void)chan; (void)device; (void)fix; (void)read; return 0; }
long McxExecFlag(long chan, long block, long flag) { (void)chan; (void)block; (void)flag; return 0; }
long McxExecApl(long chan, long apl, long param) { (void)chan; (void)apl; (void)param; return 0; }
long McxCurrCtrl(long chan, long sound, long infrared, long other) { (void)chan; (void)sound; (void)infrared; (void)other; return 0; }
long McxFlashAcs(long chan, long flag) { (void)chan; (void)flag; return 0; }
long McxShowTrans(long chan, long shown, long timeout) { (void)chan; (void)shown; (void)timeout; return 0; }
long McxHideTrans(long chan) { (void)chan; return 0; }
long McxSetLED(long chan, long flag) { (void)chan; (void)flag; return 0; }

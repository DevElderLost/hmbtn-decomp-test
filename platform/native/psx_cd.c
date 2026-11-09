#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <libcd.h>
#include "psx_cd.h"
#include "psx_libgpu.h"
#include "psx_platform.h"

#define SECTOR_DATA 2048

static FILE *image;
static long sector_size;        /* 2048, 2336 or 2352 */
static long data_offset;        /* where a sector's 2048 data bytes start */
static uint32_t image_sectors;
static uint32_t root_lba, root_size;

/* Drive state. */
static int position;            /* the read head, as a sector number */
static int drive_mode;
static int streaming;
static int last_command;
static uint8_t last_result[8];
static int rate_accumulator;
static int search_failures;
static char search_failure_name[64];
static int read_failed;

/* ---- the image ------------------------------------------------------------------------- */

int psx_cd_is_open(void) { return image != NULL; }

int psx_cd_read_sector(uint32_t lba, uint8_t out[2048]) {
    if (image == NULL || lba >= image_sectors) {
        return -1;
    }
    if (fseek(image, (long)lba * sector_size + data_offset, SEEK_SET) != 0) {
        return -1;
    }
    return fread(out, 1, SECTOR_DATA, image) == SECTOR_DATA ? 0 : -1;
}

static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void detect_layout(long size) {
    static const uint8_t sync[12] = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};
    uint8_t head[24];

    sector_size = 2048;
    data_offset = 0;
    if (fseek(image, 0, SEEK_SET) == 0 && fread(head, 1, 24, image) == 24 && memcmp(head, sync, 12) == 0 &&
        size % 2352 == 0) {
        sector_size = 2352;
        data_offset = head[15] == 1 ? 16 : 24;      /* MODE1, or MODE2 form 1 with its subheader */
    } else if (size % 2048 != 0 && size % 2336 == 0) {
        sector_size = 2336;
        data_offset = 8;
    }
    image_sectors = (uint32_t)(size / sector_size);
}

static void vblank(void);

int psx_cd_open(const char *path) {
    uint8_t pvd[SECTOR_DATA];
    long size;

    psx_cd_close();
    image = fopen(path, "rb");
    if (image == NULL) {
        return -1;
    }
    if (fseek(image, 0, SEEK_END) != 0 || (size = ftell(image)) <= 0) {
        psx_cd_close();
        return -1;
    }
    detect_layout(size);
    if (psx_cd_read_sector(16, pvd) != 0 || pvd[0] != 1 || memcmp(pvd + 1, "CD001", 5) != 0) {
        psx_cd_close();
        return -1;
    }
    root_lba = le32(pvd + 156 + 2);
    root_size = le32(pvd + 156 + 10);
    position = 0;
    streaming = 0;
    drive_mode = 0;
    last_command = 0;
    rate_accumulator = 0;
    psx_vblank_add_hook(vblank);
    return 0;
}

void psx_cd_close(void) {
    if (image != NULL) {
        fclose(image);
        image = NULL;
    }
    image_sectors = 0;
}

/* ---- ISO 9660 -------------------------------------------------------------------------- */

/* A name as the game spells it against a name on the disc: no case, no ";1" version. */
static int same_name(const char *want, size_t want_len, const char *have, size_t have_len) {
    size_t i;
    const char *semi;

    if ((semi = memchr(want, ';', want_len)) != NULL) {
        want_len = (size_t)(semi - want);
    }
    if ((semi = memchr(have, ';', have_len)) != NULL) {
        have_len = (size_t)(semi - have);
    }
    if (want_len > 0 && want[want_len - 1] == '.') want_len--;
    if (have_len > 0 && have[have_len - 1] == '.') have_len--;
    if (want_len != have_len) {
        return 0;
    }
    for (i = 0; i < want_len; i++) {
        if (tolower((unsigned char)want[i]) != tolower((unsigned char)have[i])) {
            return 0;
        }
    }
    return 1;
}

/* Find `name` in the directory at (lba, size). Fills the entry's extent, size, directory flag
 * and spelling. Returns 0 if found. */
static int find_in_directory(uint32_t lba, uint32_t size, const char *name, size_t name_len, uint32_t *extent,
                             uint32_t *file_size, int *is_dir, char *spelled) {
    uint8_t sector[SECTOR_DATA];
    uint32_t done;

    for (done = 0; done < size; done += SECTOR_DATA, lba++) {
        uint32_t at = 0;
        if (psx_cd_read_sector(lba, sector) != 0) {
            return -1;
        }
        while (at < SECTOR_DATA && sector[at] != 0) {
            const uint8_t len = sector[at];
            const uint8_t id_len = sector[at + 32];
            const char *id = (const char *)sector + at + 33;
            if (at + len > SECTOR_DATA || at + 33u + id_len > SECTOR_DATA) {
                break;
            }
            if (!(id_len == 1 && (id[0] == 0 || id[0] == 1)) && same_name(name, name_len, id, id_len)) {
                *extent = le32(sector + at + 2);
                *file_size = le32(sector + at + 10);
                *is_dir = (sector[at + 25] & 2) != 0;
                if (spelled != NULL) {
                    size_t n = id_len, semi;
                    for (semi = 0; semi < n; semi++) {
                        if (id[semi] == ';') {
                            n = semi;
                            break;
                        }
                    }
                    if (n > 15) n = 15;
                    memcpy(spelled, id, n);
                    spelled[n] = 0;
                }
                return 0;
            }
            at += len;
        }
    }
    return -1;
}

/* ---- positions ------------------------------------------------------------------------- */

static uint8_t to_bcd(int v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
static int from_bcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }

CdlLOC *CdIntToPos(int i, CdlLOC *p) {
    i += 150;                       /* the disc's first two seconds are the lead-in */
    p->minute = to_bcd(i / (75 * 60));
    p->second = to_bcd((i / 75) % 60);
    p->sector = to_bcd(i % 75);
    p->track = 0;
    return p;
}

int CdPosToInt(CdlLOC *p) {
    return (from_bcd(p->minute) * 60 + from_bcd(p->second)) * 75 + from_bcd(p->sector) - 150;
}

CdlFILE *CdSearchFile(CdlFILE *fp, char *name) {
    uint32_t lba = root_lba, size = root_size, file_size = 0;
    int is_dir = 1;
    const char *p = name;
    char spelled[16] = {0};

    if (image == NULL) {
        return NULL;
    }
    while (*p == '\\' || *p == '/') {
        p++;
    }
    while (*p != 0) {
        const char *end = p;
        uint32_t extent;
        while (*end != 0 && *end != '\\' && *end != '/') {
            end++;
        }
        if (!is_dir || find_in_directory(lba, size, p, (size_t)(end - p), &extent, &file_size, &is_dir, spelled) != 0) {
            goto missing;
        }
        lba = extent;
        size = file_size;
        p = end;
        while (*p == '\\' || *p == '/') {
            p++;
        }
    }
    if (is_dir) {
        goto missing;
    }
    CdIntToPos((int)lba, &fp->pos);
    fp->size = file_size;
    memset(fp->name, 0, sizeof(fp->name));
    memcpy(fp->name, spelled, sizeof(fp->name) - 1);
    search_failures = 0;
    return fp;

missing:
    /* The game polls for a file until it shows up; one that is not on this disc never does. */
    if (strncmp(search_failure_name, name, sizeof(search_failure_name) - 1) != 0) {
        strncpy(search_failure_name, name, sizeof(search_failure_name) - 1);
        search_failure_name[sizeof(search_failure_name) - 1] = 0;
        search_failures = 0;
    }
    if (++search_failures >= 50) {
        psx_fatal("The disc has no file %s", name);
    }
    return NULL;
}

/* ---- the drive ------------------------------------------------------------------------- */

int CdInit(void) {
    position = 0;
    streaming = 0;
    drive_mode = 0;
    return image != NULL;
}

static uint8_t status_byte(void) {
    return (uint8_t)(0x02 | (streaming ? CdlStatRead : 0));
}

int CdControl(uint8_t com, uint8_t *param, uint8_t *result) {
    last_command = com;
    memset(last_result, 0, sizeof(last_result));
    switch (com) {
    case CdlSetloc:
        if (param != NULL) {
            const int lba = CdPosToInt((CdlLOC *)param);
            position = lba < 0 ? 0 : lba;
        }
        break;
    case CdlReadN:
    case CdlReadS:
    case CdlPlay:
        streaming = 1;
        break;
    case CdlPause:
    case CdlStop:
    case CdlStandby:
    case CdlInit:
        streaming = 0;
        break;
    case CdlSetmode:
        if (param != NULL) {
            drive_mode = param[0];
        }
        break;
    case CdlGetlocP: {
        CdlLOC absolute;
        CdIntToPos(position, &absolute);
        last_result[0] = 1;                 /* track */
        last_result[1] = 1;                 /* index */
        last_result[2] = to_bcd(position / (75 * 60));
        last_result[3] = to_bcd((position / 75) % 60);
        last_result[4] = to_bcd(position % 75);
        last_result[5] = absolute.minute;
        last_result[6] = absolute.second;
        last_result[7] = absolute.sector;
        break;
    }
    default:
        break;
    }
    if (com != CdlGetlocP) {
        last_result[0] = status_byte();
    }
    if (result != NULL) {
        memcpy(result, last_result, sizeof(last_result));
    }
    return 1;
}

int CdControlF(uint8_t com, uint8_t *param) { return CdControl(com, param, NULL); }
int CdControlB(uint8_t com, uint8_t *param, uint8_t *result) { return CdControl(com, param, result); }

int CdSync(int mode, uint8_t *result) {
    (void)mode;                             /* every command has completed by now */
    if (result != NULL) {
        memcpy(result, last_result, sizeof(last_result));
    }
    return image != NULL ? CdlComplete : CdlDiskError;
}

int CdReady(int mode, uint8_t *result) {
    (void)mode;
    if (result != NULL) {
        memcpy(result, last_result, sizeof(last_result));
    }
    return CdlDataReady;
}

int CdRead(int sectors, uint32_t *buf, int mode) {
    uint8_t *out = (uint8_t *)buf;
    int i;

    (void)mode;
    read_failed = 0;
    for (i = 0; i < sectors; i++) {
        if (psx_cd_read_sector((uint32_t)(position + i), out + (size_t)i * SECTOR_DATA) != 0) {
            read_failed = 1;
            break;
        }
    }
    position += sectors;
    return 1;
}

/* The read finished in CdRead: 0 sectors remain, or -1 if one could not be read. */
int CdReadSync(int mode, uint8_t *result) {
    (void)mode;
    if (result != NULL) {
        memcpy(result, last_result, sizeof(last_result));
    }
    return read_failed ? -1 : 0;
}

int CdReadBreak(void) { return 1; }
int CdFlush(void) { return 1; }
int CdLastCom(void) { return last_command; }

int psx_cd_position(void) { return position; }
int psx_cd_streaming(void) { return streaming; }

/* One vertical blank: the head moves while the drive streams. */
static void vblank(void) {
    if (!streaming) {
        return;
    }
    rate_accumulator += (drive_mode & CdlModeSpeed) ? 2500 : 1250;
    position += rate_accumulator / 1000;
    rate_accumulator %= 1000;
}

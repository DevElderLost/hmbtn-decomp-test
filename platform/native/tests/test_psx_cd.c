#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libcd.h>
#include <libetc.h>
#include "psx_cd.h"
#include "psx_libgpu.h"
#include "psx_platform.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

/* A small disc: HELLO.TXT at LBA 20, \STR\01.XA at LBA 21..23 (3000 bytes). */
#define SECTORS 24
static uint8_t disc[SECTORS][2048];

static void put32(uint8_t *p, uint32_t v) {            /* both-endian, ECMA-119 7.3.3 */
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
    p[4] = (uint8_t)(v >> 24); p[5] = (uint8_t)(v >> 16); p[6] = (uint8_t)(v >> 8); p[7] = (uint8_t)v;
}

static int record(uint8_t *at, uint32_t extent, uint32_t size, int dir, const char *id, int id_len) {
    const int len = 33 + id_len + ((id_len % 2) == 0 ? 1 : 0);
    memset(at, 0, (size_t)len);
    at[0] = (uint8_t)len;
    put32(at + 2, extent);
    put32(at + 10, size);
    at[25] = dir ? 2 : 0;
    at[28] = 1; at[31] = 1;                             /* volume sequence number */
    at[32] = (uint8_t)id_len;
    memcpy(at + 33, id, (size_t)id_len);
    return len;
}

static void build_disc(void) {
    uint8_t *pvd = disc[16], *root = disc[18], *str = disc[19];
    int at, i;

    memset(disc, 0, sizeof(disc));
    pvd[0] = 1;
    memcpy(pvd + 1, "CD001", 5);
    pvd[6] = 1;
    record(pvd + 156, 18, 2048, 1, "\0", 1);
    disc[17][0] = 255;
    memcpy(disc[17] + 1, "CD001", 5);

    at = 0;
    at += record(root + at, 18, 2048, 1, "\0", 1);
    at += record(root + at, 18, 2048, 1, "\1", 1);
    at += record(root + at, 19, 2048, 1, "STR", 3);
    at += record(root + at, 20, 6, 0, "HELLO.TXT;1", 11);
    at = 0;
    at += record(str + at, 19, 2048, 1, "\0", 1);
    at += record(str + at, 18, 2048, 1, "\1", 1);
    at += record(str + at, 21, 3000, 0, "01.XA;1", 7);

    memcpy(disc[20], "hello\n", 6);
    for (i = 0; i < 3000; i++) {
        disc[21 + i / 2048][i % 2048] = (uint8_t)(i * 7 + 3);
    }
}

static void write_image(const char *path, int raw) {
    FILE *f = fopen(path, "wb");
    int s;
    for (s = 0; s < SECTORS; s++) {
        if (raw) {      /* sync, header, MODE2 form 1 subheader, 2048 data, EDC/ECC */
            uint8_t head[24] = {0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0};
            uint8_t tail[280] = {0};
            head[12] = 0; head[13] = 2; head[14] = (uint8_t)s; head[15] = 2;
            head[16] = head[20] = 0; head[17] = head[21] = 0; head[18] = head[22] = 8; head[19] = head[23] = 0;
            fwrite(head, 1, 24, f);
            fwrite(disc[s], 1, 2048, f);
            fwrite(tail, 1, 280, f);
        } else {
            fwrite(disc[s], 1, 2048, f);
        }
    }
    fclose(f);
}

static jmp_buf fatal_jump;
static const char *fatal_message;
static void fatal_hook(const char *message) {
    fatal_message = message;
    longjmp(fatal_jump, 1);
}

static void run_checks(const char *path) {
    CdlFILE file;
    CdlLOC loc;
    static uint32_t buf[3 * 512];
    uint8_t result[8];
    int i;

    CHECK(psx_cd_open(path) == 0 && psx_cd_is_open());
    CHECK(CdInit() == 1);

    /* Names: by path, any case, with or without the version. */
    CHECK(CdSearchFile(&file, "\\STR\\01.XA;1") == &file);
    CHECK(file.size == 3000 && CdPosToInt(&file.pos) == 21 && strcmp(file.name, "01.XA") == 0);
    CHECK(CdSearchFile(&file, "\\str\\01.xa") == &file && file.size == 3000);
    CHECK(CdSearchFile(&file, "HELLO.TXT;1") == &file && file.size == 6 && CdPosToInt(&file.pos) == 20);
    CHECK(CdSearchFile(&file, "\\STR") == NULL);                 /* a directory is not a file */
    CHECK(CdSearchFile(&file, "\\STR\\NOPE.XA;1") == NULL);
    CHECK(CdSearchFile(&file, "\\HELLO.TXT\\X") == NULL);

    /* MSF positions: sector 0 is 00:02:00. */
    CdIntToPos(0, &loc);
    CHECK(loc.minute == 0 && loc.second == 0x02 && loc.sector == 0);
    CdIntToPos(75 * 60 * 12 + 75 * 3 + 9 - 150, &loc);
    CHECK(loc.minute == 0x12 && loc.second == 0x03 && loc.sector == 0x09);
    for (i = 0; i < 300000; i += 977) {
        CdIntToPos(i, &loc);
        CHECK(CdPosToInt(&loc) == i);
    }

    /* The game's own read sequence: set the location, read, wait. */
    CdSearchFile(&file, "\\STR\\01.XA;1");
    CHECK(CdControl(CdlSetloc, (uint8_t *)&file.pos, 0) == 1);
    CHECK(CdRead(1, buf, 0x80) == 1);
    CHECK(CdReadSync(0, 0) == 0);
    CHECK(((uint8_t *)buf)[0] == 3 && psx_cd_position() == 22);
    CHECK(CdRead(1, buf, 0x80) == 1 && CdReadSync(0, 0) == 0);   /* the head carries on */
    CHECK(((uint8_t *)buf)[0] == (uint8_t)(2048 * 7 + 3) && ((uint8_t *)buf)[951] == (uint8_t)(2999 * 7 + 3));
    CHECK(psx_cd_position() == 23);
    CdControl(CdlSetloc, (uint8_t *)CdIntToPos(5000, &loc), 0);
    CdRead(1, buf, 0x80);
    CHECK(CdReadSync(0, 0) == -1);                               /* past the end of the disc */

    /* Streaming advances 75 sectors a second (1.25 per frame), 150 at double speed. */
    {
        uint8_t mode = CdlModeSpeed;
        int before, n;
        CdControl(CdlSetloc, (uint8_t *)CdIntToPos(100, &loc), 0);
        CdControl(CdlSetmode, &mode, 0);
        CdControl(CdlReadS, 0, 0);
        CHECK(CdLastCom() == CdlReadS && psx_cd_streaming());
        CHECK(CdSync(1, result) == CdlComplete && (result[0] & CdlStatRead));
        before = psx_cd_position();
        for (n = 0; n < 60; n++) {
            VSync(0);
        }
        CHECK(psx_cd_position() - before == 150);
        CdControl(CdlGetlocP, 0, 0);
        CdSync(1, result);
        CHECK(CdPosToInt((CdlLOC *)&result[5]) == psx_cd_position());
        CdControl(CdlPause, 0, 0);
        CHECK(!psx_cd_streaming());
    }
    psx_cd_close();
}

int main(void) {
    build_disc();
    write_image("/tmp/hmbtn_test_cooked.iso", 0);
    write_image("/tmp/hmbtn_test_raw.bin", 1);

    run_checks("/tmp/hmbtn_test_cooked.iso");     /* 2048 bytes a sector */
    run_checks("/tmp/hmbtn_test_raw.bin");        /* 2352 bytes a sector, MODE2 */

    CHECK(psx_cd_open("/tmp/does-not-exist.iso") != 0);
    {   /* a file the disc does not have ends the game's polling loop with a message */
        CdlFILE file;
        psx_fatal_hook = fatal_hook;
        CHECK(psx_cd_open("/tmp/hmbtn_test_cooked.iso") == 0);
        if (setjmp(fatal_jump) == 0) {
            int tries;
            for (tries = 0; tries < 1000; tries++) {
                if (CdSearchFile(&file, "\\MISSING.BIN;1") != NULL) {
                    break;
                }
            }
            CHECK(0 && "the polling loop should have stopped");
        } else {
            CHECK(strstr(fatal_message, "MISSING.BIN") != NULL);
        }
        psx_cd_close();
    }
    {   /* a non-ISO file is refused */
        FILE *f = fopen("/tmp/hmbtn_test_junk.bin", "wb");
        char junk[4096] = {0};
        fwrite(junk, 1, sizeof(junk), f);
        fclose(f);
        CHECK(psx_cd_open("/tmp/hmbtn_test_junk.bin") != 0 && !psx_cd_is_open());
    }
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("psx_cd: ok\n");
    return 0;
}

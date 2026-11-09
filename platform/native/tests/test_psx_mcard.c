#include <stdio.h>
#include <string.h>
#include <libmcrd.h>
#include <libmcx.h>
#include "psx_mcard.h"

static int failures;
#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL line %d: %s\n", __LINE__, #cond);                   \
            failures++;                                                      \
        }                                                                    \
    } while (0)

#define CARD "/tmp/hmbtn_test_card.mcr"

static long sync_result(void) {
    long cmd = -1, result = -1;
    CHECK(MemCardSync(0, &cmd, &result) == 1);
    return result;
}

static int frame_checksum_ok(const unsigned char *card, int frame) {
    unsigned char x = 0;
    int i;
    for (i = 0; i < 127; i++) {
        x ^= card[frame * 128 + i];
    }
    return x == card[frame * 128 + 127];
}

static void test_probe_and_format(void) {
    long cmd = 0, result = 0;
    static unsigned char image[PSX_MCARD_SIZE];
    FILE *f;
    int i;

    remove(CARD);
    /* Slot 2 has no card; slot 1 gets a brand new, blank one. */
    CHECK(MemCardSync(0, &cmd, &result) == -1);                  /* nothing pending */
    CHECK(MemCardExist(0x10) == 1 && sync_result() == McErrCardNotExist);
    CHECK(psx_mcard_attach(0, CARD) == 0);
    CHECK(MemCardExist(0x00) == 1 && sync_result() == McErrNewCard);
    CHECK(MemCardAccept(0x00) == 1 && sync_result() == McErrNotFormat);
    CHECK(MemCardAccept(0x00) == 1 && sync_result() == McErrNotFormat);
    {
        struct DIRENTRY dir[3];
        long files = -1;
        CHECK(MemCardGetDirentry(0, "*", dir, &files, 0, 3) == McErrNotFormat && files == 0);
        CHECK(MemCardCreateFile(0, "X", 1) == McErrNotFormat);
        CHECK(MemCardGetDirentry(0x10, "*", dir, &files, 0, 3) == McErrCardNotExist);
    }
    CHECK(MemCardFormat(0x00) == McErrNone && psx_mcard_formatted(0));
    CHECK(MemCardAccept(0x00) == 1 && sync_result() == McErrNone);

    /* The file on disk is a card in the console's format. */
    f = fopen(CARD, "rb");
    CHECK(f != NULL && fread(image, 1, sizeof(image), f) == sizeof(image));
    fclose(f);
    CHECK(image[0] == 'M' && image[1] == 'C');
    for (i = 0; i < 64; i++) {
        CHECK(frame_checksum_ok(image, i) || (i >= 36 && i < 63 ? 1 : 0));
    }
    CHECK(memcmp(image, image + 63 * 128, 128) == 0);
    for (i = 1; i <= 15; i++) {
        CHECK(image[i * 128] == 0xA0);
    }
}

static void test_files(void) {
    static unsigned long data[0x6000 / 4], back[0x6000 / 4];
    unsigned char *bytes = (unsigned char *)data;
    struct DIRENTRY dir[15];
    long files = -1;
    int i;

    for (i = 0; i < 0x6000; i++) {
        bytes[i] = (unsigned char)(i * 13 + (i >> 8));
    }
    CHECK(MemCardCreateFile(0, "BASLUS-01115SAVE1", 3) == McErrNone);
    CHECK(MemCardCreateFile(0, "BASLUS-01115SAVE1", 1) == McErrAlreadyExist);
    CHECK(MemCardCreateFile(0, "BASLUS-01115SAVE2", 2) == McErrNone);

    CHECK(MemCardGetDirentry(0, "BASLUS-01115*", dir, &files, 0, 15) == McErrNone && files == 2);
    CHECK(strcmp(dir[0].name, "BASLUS-01115SAVE1") == 0 && dir[0].size == 0x6000 && dir[0].head == 1);
    CHECK(strcmp(dir[1].name, "BASLUS-01115SAVE2") == 0 && dir[1].size == 0x4000);
    CHECK(MemCardGetDirentry(0, "BASLUS-01115SAVE?", dir, &files, 1, 15) == McErrNone && files == 1 &&
          strcmp(dir[0].name, "BASLUS-01115SAVE2") == 0);
    CHECK(MemCardGetDirentry(0, "BASLUS-01115SAVE1", dir, &files, 0, 1) == McErrNone && files == 1);
    CHECK(MemCardGetDirentry(0, "NOTHING*", dir, &files, 0, 15) == McErrNone && files == 0);

    /* Write the whole file, read it back, and a range that crosses an 8 KB block boundary. */
    CHECK(MemCardWriteFile(0, "BASLUS-01115SAVE1", data, 0, 0x6000) == 1 && sync_result() == McErrNone);
    memset(back, 0, sizeof(back));
    CHECK(MemCardReadFile(0, "BASLUS-01115SAVE1", back, 0, 0x6000) == 1 && sync_result() == McErrNone);
    CHECK(memcmp(data, back, 0x6000) == 0);
    memset(back, 0, sizeof(back));
    CHECK(MemCardReadFile(0, "BASLUS-01115SAVE1", back, 0x1F80, 0x100) == 1 && sync_result() == McErrNone);
    CHECK(memcmp(back, bytes + 0x1F80, 0x100) == 0);
    CHECK(MemCardReadFile(0, "NOSUCHFILE", back, 0, 128) == 1 && sync_result() == McErrFileNotExist);
    CHECK(MemCardReadFile(0, "BASLUS-01115SAVE2", back, 0x3F80, 0x100) == 1 && sync_result() == McErrFileNotExist);

    /* It survives detaching and attaching the file again, and the other file is untouched. */
    psx_mcard_detach(0);
    CHECK(MemCardAccept(0) == 1 && sync_result() == McErrCardNotExist);
    CHECK(psx_mcard_attach(0, CARD) == 0);
    CHECK(MemCardExist(0) == 1 && sync_result() == McErrNewCard);
    memset(back, 0, sizeof(back));
    CHECK(MemCardReadFile(0, "BASLUS-01115SAVE1", back, 0, 0x6000) == 1 && sync_result() == McErrNone);
    CHECK(memcmp(data, back, 0x6000) == 0);
    CHECK(MemCardReadFile(0, "BASLUS-01115SAVE2", back, 0, 0x4000) == 1 && sync_result() == McErrNone);
    for (i = 0; i < 0x4000; i++) {                      /* a new file reads as zeros */
        if (((unsigned char *)back)[i] != 0) {
            break;
        }
    }
    CHECK(i == 0x4000);

    /* Space: 5 of 15 blocks are used, so 11 more do not fit, 10 do. */
    CHECK(MemCardCreateFile(0, "BIG", 11) == McErrBlockFull);
    CHECK(MemCardCreateFile(0, "BIG", 10) == McErrNone);
    CHECK(MemCardCreateFile(0, "ONEMORE", 1) == McErrBlockFull);

    /* Deleting frees its blocks; the data of the other files stays. */
    CHECK(MemCardDeleteFile(0, "BIG") == McErrNone);
    CHECK(MemCardDeleteFile(0, "BIG") == McErrFileNotExist);
    CHECK(MemCardGetDirentry(0, "*", dir, &files, 0, 15) == McErrNone && files == 2);
    CHECK(MemCardCreateFile(0, "ONEMORE", 1) == McErrNone);
    CHECK(MemCardReadFile(0, "BASLUS-01115SAVE1", back, 0, 0x6000) == 1 && sync_result() == McErrNone);
    CHECK(memcmp(data, back, 0x6000) == 0);

    CHECK(MemCardUnformat(0) == McErrNone && !psx_mcard_formatted(0));
}

static void test_pocketstation(void) {
    long cmd = -1, result = -1;
    CHECK(McxCardType(0) == 0);
    CHECK(McxSync(0, &cmd, &result) == 1 && result == McxErrNoCard);
}

int main(void) {
    test_probe_and_format();
    test_files();
    test_pocketstation();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("psx_mcard: ok\n");
    return 0;
}

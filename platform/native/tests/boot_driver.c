/* Runs the game's own main() headless on a disc image and reports where it stops. A game that
 * boots far enough to ask the disc for its first file has set up graphics, sound, the CD, the
 * pad and the memory card correctly. */

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "psx_cd.h"
#include "psx_libgpu.h"
#include "psx_mcard.h"
#include "psx_mem.h"
#include "psx_platform.h"

extern int hmbtn_game_main(void);

static jmp_buf out;
static const char *why;
static char why_text[256];      /* the message lives on psx_fatal's stack, which longjmp abandons */
static int frames;

static void fatal(const char *message) {
    strncpy(why_text, message, sizeof(why_text) - 1);
    why = why_text;
    longjmp(out, 1);
}

static void blank(void) {
    if (++frames > 100000) {
        why = "frame limit";
        longjmp(out, 2);
    }
}

int main(int argc, char **argv) {
    const char *expect = argc > 2 ? argv[2] : NULL;

    if (psx_mem_init() != 0) {
        puts("FAIL: no console RAM");
        return 2;
    }
    if (argc < 2 || psx_cd_open(argv[1]) != 0) {
        puts("FAIL: no disc");
        return 2;
    }
    psx_mcard_attach(0, "/tmp/hmbtn_boot_card.mcr");
    psx_fatal_hook = fatal;
    psx_vsync_hook = blank;
    if (setjmp(out) == 0) {
        hmbtn_game_main();
        why = "main returned";
    }
    printf("stopped after %d vblanks: %s\n", frames, why);
    if (expect != NULL && (strstr(why, expect) == NULL || frames < 10)) {
        printf("FAIL: expected a stop at \"%s\" after at least 10 vblanks\n", expect);
        return 1;
    }
    return 0;
}

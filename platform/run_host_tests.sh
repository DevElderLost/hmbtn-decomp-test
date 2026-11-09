#!/bin/sh
# Host tests for the platform layer. Needs a C compiler and python3; SDL2's headers, if
# installed, add a compile check of the front end. Run from anywhere.
#
#   touch layout, controller shim, GTE, CD, memory card: also under ASan + UBSan
#   address space, GPU, compat render:  UBSan only (they map RAM at 0x80000000, which ASan
#                                       keeps for its shadow memory)
#   game sources:                       compile check against the compat headers
#   boot smoke test:                    the game's own main() on a disc with no game files
set -eu
cd "$(dirname "$0")/native"
CC=${CC:-cc}
out=${TMPDIR:-/tmp}/hmbtn-host-tests
mkdir -p "$out"
warn="-Wall -Wextra -Werror -O1 -g -fno-sanitize-recover=all"
asan="-std=c99 -D_POSIX_C_SOURCE=200809L $warn -fsanitize=address,undefined -I. -I../compat"
ubsan="-std=c99 -D_POSIX_C_SOURCE=200809L $warn -fsanitize=undefined -I. -I../compat"
compat="-std=gnu11 -D_POSIX_C_SOURCE=200809L $warn -funsigned-char -I../compat/override -I../compat/psyq -I../compat -I."

run() {
    name=$1
    shift
    "$@" -o "$out/$name"
    "$out/$name"
}

run test_touch_pad $CC $asan tests/test_touch_pad.c touch_pad.c
run test_psx_pad $CC $asan tests/test_psx_pad.c psx_pad.c
run test_psx_gte $CC $asan tests/test_psx_gte.c psx_gte.c
run test_psx_mcard $CC $compat -fsanitize=address,undefined tests/test_psx_mcard.c psx_mcard.c
run test_psx_cd $CC $compat -fsanitize=address,undefined tests/test_psx_cd.c psx_cd.c psx_libgpu.c psx_libgte.c psx_gpu.c psx_gte.c psx_mem.c psx_platform.c -lm
run test_psx_mem $CC $ubsan tests/test_psx_mem.c psx_mem.c
run test_psx_gpu $CC $ubsan tests/test_psx_gpu.c psx_gpu.c psx_mem.c
run test_compat_render $CC $compat -fsanitize=undefined tests/test_compat_render.c psx_libgte.c psx_libgpu.c psx_gte.c psx_gpu.c psx_mem.c -lm

sh ../check_game_sources.sh
sh tests/boot_smoke.sh

if command -v sdl2-config >/dev/null 2>&1; then
    # shellcheck disable=SC2046
    $CC -std=c99 -Wall -Wextra -Werror -fsyntax-only -I. -I../compat $(sdl2-config --cflags) main_sdl.c
    $CC -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -funsigned-char -I../compat/override -I../compat/psyq -I../compat -I. demo_cube.c
    echo "main_sdl.c, demo_cube.c: compile against SDL2 $(sdl2-config --version)"
fi

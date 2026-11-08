#!/bin/sh
# Host tests for the platform layer. Needs only a C compiler (and, for the last two steps,
# SDL2's headers if installed). Run from anywhere.
#
#   touch layout, controller shim, GTE: also under ASan + UBSan
#   address space, GPU, compat render:  UBSan only (they map RAM at 0x80000000, which ASan
#                                       keeps for its shadow memory)
#   game sources:                       compile check against the compat headers
set -eu
cd "$(dirname "$0")/native"
CC=${CC:-cc}
out=${TMPDIR:-/tmp}/hmbtn-host-tests
mkdir -p "$out"
warn="-Wall -Wextra -Werror -O1 -g -fno-sanitize-recover=all"
asan="-std=c99 -D_POSIX_C_SOURCE=200809L $warn -fsanitize=address,undefined -I. -I../compat"
ubsan="-std=c99 -D_POSIX_C_SOURCE=200809L $warn -fsanitize=undefined -I. -I../compat"
compat="-std=gnu11 -D_POSIX_C_SOURCE=200809L $warn -fsanitize=undefined -funsigned-char -I../compat/override -I../compat/psyq -I../compat -I."

$CC $asan tests/test_touch_pad.c touch_pad.c -o "$out/test_touch_pad"
"$out/test_touch_pad"

$CC $asan tests/test_psx_pad.c psx_pad.c -o "$out/test_psx_pad"
"$out/test_psx_pad"

$CC $asan tests/test_psx_gte.c psx_gte.c -o "$out/test_psx_gte"
"$out/test_psx_gte"

$CC $ubsan tests/test_psx_mem.c psx_mem.c -o "$out/test_psx_mem"
"$out/test_psx_mem"

$CC $ubsan tests/test_psx_gpu.c psx_gpu.c psx_mem.c -o "$out/test_psx_gpu"
"$out/test_psx_gpu"

$CC $compat tests/test_compat_render.c psx_libgte.c psx_libgpu.c psx_gte.c psx_gpu.c psx_mem.c -lm -o "$out/test_compat_render"
"$out/test_compat_render"

sh ../check_game_sources.sh

if command -v sdl2-config >/dev/null 2>&1; then
    # shellcheck disable=SC2046
    $CC -std=c99 -Wall -Wextra -Werror -fsyntax-only -I. -I../compat $(sdl2-config --cflags) main_sdl.c
    $CC -std=gnu11 -Wall -Wextra -Werror -fsyntax-only -funsigned-char -I../compat/override -I../compat/psyq -I../compat -I. demo_cube.c
    echo "main_sdl.c, demo_cube.c: compile against SDL2 $(sdl2-config --version)"
fi

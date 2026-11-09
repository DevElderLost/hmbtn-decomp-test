#!/bin/sh
# Compile every game source, link with the platform libraries, and run the game's main() on a
# disc that has no game files. The game must boot and then ask for A_FILE.HDT.
set -eu
root=$(cd "$(dirname "$0")/../../.." && pwd)
cd "$root"
CC=${CC:-gcc}
out=${TMPDIR:-/tmp}/hmbtn-boot-smoke
rm -rf "$out"
mkdir -p "$out/obj"

inc=$(grep -oE 'src/[A-Za-z0-9_/]+' CMakeLists.txt | sort -u | while read -r d; do [ -d "$d" ] && printf -- '-I%s ' "$d"; done)
flags="-std=gnu11 -c -w -O1 -funsigned-char -DHMBTN_NATIVE -D_LANGUAGE_C -D__CHAR_UNSIGNED__ -Iplatform/compat/override -Iplatform/compat/psyq -Iplatform/compat -Iplatform/native $inc -Isrc"

# One compile per source, in parallel. The game's main() is renamed; overlay_regions.c and
# text.c are replaced as the build does (see platform/android/app/jni/CMakeLists.txt).
sed -E 's/^static char (text_fmt_(dec|hex|int)\[4\])/char \1/' src/main/text/text.c > "$out/text.c"
{
    find src -name '*.c' | sort | grep -v -e 'src/main/overlay_regions.c' -e 'src/main/text/text.c'
    echo "$out/text.c"
    ls platform/native/psx_*.c platform/native/overlay_native.c
    echo platform/native/tests/boot_driver.c
} > "$out/sources"
jobs=$(nproc 2>/dev/null || echo 2)
# shellcheck disable=SC2086
xargs -P "$jobs" -I{} sh -c '
    src="{}"; obj="'"$out"'/obj/$(echo "$src" | tr "/" "_" | sed "s/\.c$/.o/")"
    extra=""
    [ "$src" = src/main/main.c ] && extra="-Dmain=hmbtn_game_main"
    [ "$src" = "'"$out"'/text.c" ] && extra="-Isrc/main/text"
    '"$CC $flags"' $extra "$src" -o "$obj"' < "$out/sources"
$CC "$out"/obj/*.o -lm -o "$out/boot"

python3 - "$out/empty.iso" <<'PY'
import struct, sys
def both32(v): return struct.pack('<I', v) + struct.pack('>I', v)
def rec(extent, size, isdir, ident):
    n = len(ident); l = 33 + n + (1 if n % 2 == 0 else 0)
    r = bytearray(l); r[0] = l; r[2:10] = both32(extent); r[10:18] = both32(size)
    r[25] = 2 if isdir else 0; r[28] = 1; r[31] = 1; r[32] = n; r[33:33 + n] = ident
    return bytes(r)
secs = [bytes(2048) for _ in range(20)]
pvd = bytearray(2048); pvd[0] = 1; pvd[1:6] = b'CD001'; pvd[6] = 1
pvd[156:156 + 34] = rec(18, 2048, True, b'\0')[:34]
secs[16] = bytes(pvd)
root = bytearray(2048); at = 0
for r in (rec(18, 2048, True, b'\0'), rec(18, 2048, True, b'\1')):
    root[at:at + len(r)] = r; at += len(r)
secs[18] = bytes(root)
open(sys.argv[1], 'wb').write(b''.join(secs))
PY
"$out/boot" "$out/empty.iso" 'A_FILE.HDT'

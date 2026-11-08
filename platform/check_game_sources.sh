#!/bin/sh
# Compile-check every source file of the game against the compat headers, with the host
# compiler. Needs only gcc. Prints how many files compile and fails if the set of files that
# do not changes: native/game_stage3_files.txt lists the ones still waiting for the CD, SPU
# and memory-card libraries.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
CC=${CC:-gcc}
tmp=${TMPDIR:-/tmp}/hmbtn-game-check
rm -rf "$tmp"
mkdir -p "$tmp"

inc=$(grep -oE 'src/[A-Za-z0-9_/]+' CMakeLists.txt | sort -u | while read -r d; do [ -d "$d" ] && printf -- '-I%s ' "$d"; done)
flags="-std=gnu11 -fsyntax-only -funsigned-char -w -Iplatform/compat/override -Iplatform/compat/psyq -Iplatform/compat -Iplatform/native $inc -Isrc"

: > "$tmp/failed"
total=0
for f in $(find src -name '*.c' | sort); do
    total=$((total + 1))
    src=$f
    if [ "$f" = src/main/text/text.c ]; then
        # The original declares three tables extern in text.h and then defines them static,
        # which the 1990s compiler accepted and a current one rejects. Define them plainly.
        src="$tmp/text.c"
        sed -E 's/^static char (text_fmt_(dec|hex|int)\[4\])/char \1/' "$f" > "$src"
    fi
    # shellcheck disable=SC2086
    if ! $CC $flags "$src" > /dev/null 2>&1; then
        echo "$f" >> "$tmp/failed"
    fi
done
failed=$(wc -l < "$tmp/failed" | tr -d ' ')
echo "game sources: $((total - failed)) of $total compile against the compat headers"

if ! diff -u platform/native/game_stage3_files.txt "$tmp/failed" > "$tmp/diff"; then
    echo "The set of files that do not compile changed (expected list: platform/native/game_stage3_files.txt):"
    cat "$tmp/diff"
    exit 1
fi

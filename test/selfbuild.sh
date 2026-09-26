#!/bin/bash
# acc's own sources, compiled by acc as they are built for the Agon, and
# linked: what test/size.sh does to measure acc against agondev, with the
# sources as they are now rather than at the revision it pins.
#
# The Agon build has pieces agondev's toolchain supplies and acc's does
# not -- src/marks.s is assembly, which acc does not take -- and each one
# has to leave the C it stands in for in place when the compiler is acc, or
# the next move of size.sh's revision finds acc can no longer build itself.
# Linking is the question: a function declared and not defined is only an
# error there.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
export ASAN_OPTIONS=detect_leaks=0
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cp src/*.c src/*.h "$tmp/"
printf '#define ACC_BUILD 0\n' > "$tmp/acc_build.h"

objs=
for f in "$tmp"/*.c; do
    o="${f%.c}.o"
    if ! err=$("$ACC" -c "$f" -o "$o" -Iinclude -I"$tmp" -DAGONDEV 2>&1); then
        printf '  FAIL %s does not compile\n%s\n' "$(basename "$f")" "$err"
        exit 1
    fi
    objs="$objs $o"
done
if ! err=$("$ACC" $objs bin/libc.a -o "$tmp/acc.bin" 2>&1); then
    printf '  FAIL acc does not link\n%s\n' "$err"
    exit 1
fi
echo "  acc builds itself for the Agon: $(stat -c%s "$tmp/acc.bin") bytes"

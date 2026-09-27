#!/bin/bash
# What fits in the Agon's heap: acc as built for the Agon, compiling on the
# Agon the headers an Agon program includes.
#
# <agon/vdp.h> pulls in seven headers of its own, and with the image taking
# 64 KB up front no program that included it could be compiled on the
# machine. These are held: each combination below compiles, and the widest
# builds in one step and runs.
#
# Every header acc has, in one file; and eleven an Agon program might
# include, built in one step and run.
#
# And aed, if ~/code/aed is there (AED_SRC to point elsewhere): a real
# program of 23 files, compiled a file at a time. AED_OVER is how many may
# run out of memory: none. And zap's eleven files, from its checkout at
# ZAP_REV, none of which may either.
#
#   test/headers.sh [acc.bin]           # default bin/acc.bin
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${1:-bin/acc.bin}
AED_SRC=${AED_SRC:-$HOME/code/aed/src}
AED_OVER=0
ZAP=${ZAP:-$HOME/code/zap}
ZAP_REV=${ZAP_REV:-v1.1.0}

emu_available || exit 77
[ -f "$ACC" ] || { echo "no $ACC -- run make -f Makefile.agon" >&2; exit 2; }
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }
if pgrep -f '^[^ ]*agon-cli-emulator .*--sdcard' >/dev/null 2>&1; then
    echo "another emulator is running -- stop it first" >&2
    exit 2
fi

sd=$(emu_card); host=$(mktemp -d); trap 'rm -rf "$sd" "$host"' EXIT

cp "$ACC" "$sd/bin/acc.bin"
mkdir -p "$sd/lib/acc"
cp bin/libc.a "$sd/lib/acc/"
cp -r include "$sd/lib/acc/include"

# One file per combination, named for it.
combo() {
    local name=$1 h
    shift
    for h in "$@"; do
        echo "#include <$h>"
    done > "$sd/$name.c"
    echo 'int main(void) { return 0; }' >> "$sd/$name.c"
    printf 'echo FILE %s\r\ntry acc -c %s.c -o %s.o\r\n' "$name" "$name" "$name" \
        >> "$sd/autoexec.txt"
}

: > "$sd/autoexec.txt"
combo vdp agon/vdp.h
combo mosvdp agon/mos.h agon/vdp.h
combo stdvdp stdio.h agon/vdp.h

cat > "$sd/five.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <agon/mos.h>
#include <agon/vdp.h>

int main(void)
{
    char *p = malloc(32);

    strcpy(p, "five");
    printf("%s headers, %d\n", p, (int) strlen(p));
    mos_putstring("in one step\r\n");
    return 0;
}
EOF
printf 'echo FILE five\r\ntry acc five.c\r\ntry five\r\n' >> "$sd/autoexec.txt"

# Every header, then the eleven.
(cd include && find . -name '*.h' | sed 's|^\./||' | sort) | while read -r h; do
    echo "#include <$h>"
done > "$sd/allh.c"
echo 'int main(void) { return 0; }' >> "$sd/allh.c"
printf 'echo FILE allh\r\ntry acc -c allh.c -o allh.o\r\n' >> "$sd/autoexec.txt"

cat > "$sd/eleven.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <agon/mos.h>
#include <agon/vdp.h>
#include <agon/keyboard.h>
#include <agon/timer.h>

int main(void)
{
    char *p = malloc(8);

    strcpy(p, "ok");
    printf("eleven %s %d %d\n", p, (int) sqrt(49.0), toupper('a'));
    return 0;
}
EOF
printf 'echo FILE eleven\r\ntry acc eleven.c\r\ntry eleven\r\n' >> "$sd/autoexec.txt"

naed=0
if [ -d "$AED_SRC" ]; then
    mkdir -p "$sd/aed"
    cp "$AED_SRC"/*.c "$AED_SRC"/*.h "$sd/aed/"
    for f in "$AED_SRC"/*.c; do
        b=$(basename "$f" .c)
        printf 'echo FILE aed/%s\r\ntry acc -c aed/%s.c -o aed/%s.o\r\n' \
            "$b" "$b" "$b" >> "$sd/autoexec.txt"
        naed=$((naed + 1))
    done
fi

# And zap, the assembler, at the tag test/size.sh takes it from, its eleven
# files compiled as test/size.sh compiles them.
nzap=0
if git -C "$ZAP" rev-parse -q --verify "$ZAP_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$sd/zap"
    git -C "$ZAP" archive "$ZAP_REV" src | tar -x -C "$sd/zap" --strip-components=1
    rm -f "$sd/zap/zmalloc.c" "$sd/zap/zmalloc.h"
    for f in "$sd"/zap/*.c; do
        b=$(basename "$f" .c)
        printf 'echo FILE zap/%s\r\ntry acc -c zap/%s.c -o zap/%s.o -DAGONDEV\r\n' \
            "$b" "$b" "$b" >> "$sd/autoexec.txt"
        nzap=$((nzap + 1))
    done
fi

ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=600 emu_run "$sd" -z -u > "$host/console.txt" 2>&1
tr -d '\r' < "$host/console.txt" > "$host/out.txt"

# Each error line, with the file it was compiling.
awk '/^FILE /{f=$2; next} /error/{print f ": " $0}' "$host/out.txt" > "$host/errors.txt"

pass=0; fail=0
held() {
    if grep -q "^$2: " "$host/errors.txt"; then
        printf '  FAIL %-40s %s\n' "$1" "$(grep "^$2: " "$host/errors.txt" | head -1)"
        fail=$((fail + 1))
    else
        printf '  ok   %s\n' "$1"; pass=$((pass + 1))
    fi
}

held "<agon/vdp.h> compiles"                vdp
held "<agon/mos.h> and <agon/vdp.h>"        mosvdp
held "<stdio.h> and <agon/vdp.h>"           stdvdp
held "five headers in one step"             five
if grep -qxF "five headers, 4" "$host/out.txt" && grep -qxF "in one step" "$host/out.txt"; then
    printf '  ok   %s\n' "and the program runs"; pass=$((pass + 1))
else
    printf '  FAIL %-40s %s\n' "and the program runs" "no output"; fail=$((fail + 1))
fi

held "every header at once"                allh
held "eleven headers in one step"           eleven
if grep -qxF "eleven ok 7 65" "$host/out.txt"; then
    printf '  ok   %s\n' "and that program runs"; pass=$((pass + 1))
else
    printf '  FAIL %-40s %s\n' "and that program runs" "no output"; fail=$((fail + 1))
fi

if [ "$naed" -gt 0 ]; then
    over=$(grep -c '^aed/' "$host/errors.txt")
    if [ "$over" -le "$AED_OVER" ]; then
        printf '  ok   %-40s %s\n' "aed on the Agon" "$((naed - over)) of $naed files compile"
        pass=$((pass + 1))
    else
        printf '  FAIL %-40s %s\n' "aed on the Agon" \
            "$over of $naed run out, more than $AED_OVER"
        fail=$((fail + 1))
    fi
    grep '^aed/' "$host/errors.txt" | sed 's/^/         /'
else
    echo "  [no aed at $AED_SRC: skipped]"
fi

if [ "$nzap" -gt 0 ]; then
    over=$(grep -c '^zap/' "$host/errors.txt")
    if [ "$over" -eq 0 ]; then
        printf '  ok   %-40s %s\n' "zap on the Agon" "$nzap of $nzap files compile"
        pass=$((pass + 1))
    else
        printf '  FAIL %-40s %s\n' "zap on the Agon" "$over of $nzap run out"
        fail=$((fail + 1))
    fi
    grep '^zap/' "$host/errors.txt" | sed 's/^/         /'
else
    echo "  [no zap at $ZAP ($ZAP_REV): skipped]"
fi

echo "  $pass passed, $fail failed"
[ $fail -eq 0 ]

#!/bin/bash
# The VDP calls that read an answer back, against the VDP itself.
#
# test/agonlib.sh holds each call to the bytes libagon sends, and cannot
# run the ones that wait for the VDP's answer: its programs catch what they
# send, so nothing ever answers. These run on the full emulator with the
# VDP's own firmware (emu_run_gui in test/emu.sh), where the answers come.
# Each program in test/vdpreal/ writes what it found to res.txt on the card
# and stops the machine; what it wrote has to be test/vdpreal/<name>.want.
#
#   test/vdpreal.sh                every program
#   test/vdpreal.sh screen         only these
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
emu_available >/dev/null 2>&1 || exit 77
[ -x "$EMU/fab-agon-emulator" ] || { echo "  [no fab-agon-emulator: the VDP read-back checks are skipped]"; exit 77; }

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

if [ $# -gt 0 ]; then
    srcs=(); for n in "$@"; do srcs+=("test/vdpreal/$n.c"); done
else
    srcs=(test/vdpreal/*.c)
fi

for src in "${srcs[@]}"; do
    name=$(basename "$src" .c)

    if ! err=$("$ACC" -c "$src" -o "$tmp/p.o" -Iinclude 2>&1) \
       || ! err=$("$ACC" "$tmp/p.o" "$LIB" -o "$tmp/p.bin" -x 2>&1); then
        printf '  FAIL %-12s %s\n' "$name" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); continue
    fi
    sd=$(emu_card)
    cp "$tmp/p.bin" "$sd/bin/p.bin"
    printf 'bin/p\r\n' > "$sd/autoexec.txt"
    emu_run_gui "$sd" -z -u
    status=$?
    tr -d '\r' < "$sd/res.txt" > "$tmp/got" 2>/dev/null
    rm -rf "$sd"

    if [ "$status" -ne 42 ]; then
        printf '  FAIL %-12s the program did not finish (status %d); it wrote:\n' "$name" "$status"
        sed 's/^/         /' "$tmp/got" | head -20
        fail=$((fail + 1))
    elif diff -u "test/vdpreal/$name.want" "$tmp/got" > "$tmp/diff"; then
        printf '  ok   %-12s %d lines\n' "$name" "$(wc -l < "$tmp/got")"
        pass=$((pass + 1))
    else
        printf '  FAIL %-12s:\n' "$name"
        sed 's/^/         /' "$tmp/diff" | head -40
        fail=$((fail + 1))
    fi
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

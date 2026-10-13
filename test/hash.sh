#!/bin/bash
# src/hash.s, the name hash written by hand for the Agon, against the C in
# src/hash.h that the host build uses.
#
# A wrong hash places names badly and nothing else: every program still
# compiles, a little slower, and no other test would see it. So the two are
# built into one program with agondev, as acc.bin's own sources are, and run
# on the emulator over every one-character name and thousands of others
# (test/test_hash_agon.c).
#
# Needs agondev and the emulator. Skips (77) without them.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "  [no agondev: the hash on the Agon skipped]"; exit 77; }
emu_available || exit 77

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc -Isrc
        -isystem $AGONDEV/include -target ez80-none-elf -Oz -Wa,-march=ez80+full"
# shellcheck disable=SC2086
$CC $CFLAGS -c test/test_hash_agon.c -o "$tmp/t.o" \
    && "$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full src/hash.s -o "$tmp/h.o" \
    || { echo "  FAIL the hash does not build with agondev"; exit 1; }
"$AGONDEV/bin/ez80-none-elf-ld" -defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000 \
    -defsym=_has_exit_handler=0 -T "$AGONDEV/config/linker.conf" --oformat binary \
    -o "$tmp/hash.bin" "$tmp/t.o" "$tmp/h.o" -L"$AGONDEV/lib" -lagon \
    || { echo "  FAIL the hash does not link with agondev"; exit 1; }

sd=$(emu_card); trap 'rm -rf "$tmp" "$sd"' EXIT
cp "$tmp/hash.bin" "$sd/bin/hashtest.bin"
printf 'hashtest\r\n' > "$sd/autoexec.txt"
ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=120 emu_run "$sd" -z -u 2>&1 | tr -d '\r' > "$tmp/out"

if grep -q 'hash: [0-9]* checks, 0 failed' "$tmp/out"; then
    echo "  the hash on the Agon: $(grep -o '[0-9]* checks' "$tmp/out"), none failed"
else
    echo "  FAIL the hash on the Agon"
    grep -E 'FAIL|checks' "$tmp/out" | head -10
    exit 1
fi

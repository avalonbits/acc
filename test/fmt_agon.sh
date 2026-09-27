#!/bin/bash
# src/fmt.c as agondev compiles it, run on the Agon.
#
# test/test_fmt.c holds fmt_vsnprintf to the C library's snprintf on the
# host. That cannot see what agondev's clang does to it: it miscompiled
# reading a long from the va_list -- the top byte read nine bytes on rather
# than three -- which every %lu then printed. So the same test is built here
# with agondev's flags and run on the emulator, against libagon's snprintf.
#
# Needs agondev and the emulator. Skips (77) without them.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "  [no agondev: fmt check on the Agon skipped]"; exit 77; }
emu_available || exit 77

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc -Isrc
        -isystem $AGONDEV/include -target ez80-none-elf -Oz -Wa,-march=ez80+full"
# shellcheck disable=SC2086
$CC $CFLAGS -c test/test_fmt.c -o "$tmp/t.o" && $CC $CFLAGS -c src/fmt.c -o "$tmp/f.o" \
    || { echo "  FAIL fmt does not build with agondev"; exit 1; }
"$AGONDEV/bin/ez80-none-elf-ld" -defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000 \
    -defsym=_has_exit_handler=0 -T "$AGONDEV/config/linker.conf" --oformat binary \
    -o "$tmp/fmt.bin" "$tmp/t.o" "$tmp/f.o" -L"$AGONDEV/lib" -lagon \
    || { echo "  FAIL fmt does not link with agondev"; exit 1; }

sd=$(emu_card); trap 'rm -rf "$tmp" "$sd"' EXIT
cp "$tmp/fmt.bin" "$sd/bin/fmttest.bin"
printf 'fmttest\r\n' > "$sd/autoexec.txt"
ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=120 emu_run "$sd" -z -u 2>&1 | tr -d '\r' > "$tmp/out"

if grep -q 'fmt: [0-9]* checks, 0 failed' "$tmp/out"; then
    echo "  fmt on the Agon: $(grep -o '[0-9]* checks' "$tmp/out"), none failed"
else
    echo "  FAIL fmt on the Agon"
    grep -E 'FAIL|checks' "$tmp/out" | head -10
    exit 1
fi

#!/bin/bash
# Runs an Agon binary and exits with the status the program reported.
#
#   test/agon.sh <program.bin>
#
# A program says how it went by writing a byte to IO port 0, which stops the
# emulator with that byte as its exit status. That is the whole result channel
# for now: no C library, no linker, nothing to print with.
#
# -u here, and only here: this is a correctness check, so the only thing that
# matters is that the program runs to the end. Nothing that times anything may
# use it -- see test/bench.sh.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

emu_available >/dev/null 2>&1 || exit 77
[ $# -eq 1 ] || { echo "usage: $0 <program.bin>" >&2; exit 2; }

sd=$(emu_card); trap 'rm -rf "$sd"' EXIT
cp "$1" "$sd/bin/p.bin"
printf 'bin/p\r\n' > "$sd/autoexec.txt"

ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-60} emu_run "$sd" -z -u >/dev/null 2>&1

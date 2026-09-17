#!/bin/bash
# Runs an Agon binary and exits with the status the program reported.
#
#   test/agon.sh <program.bin>
#
# A program says how it went by writing a byte to IO port 0, which stops the
# emulator with that byte as its exit status. That is the whole result channel
# for now: no C library, no linker, nothing to print with.
#
# The sdcard is built fresh every time. A card that accumulates files stops
# booting -- thirty-two of them was enough -- and the failures look exactly
# like the program being wrong.
set -u

EMU=${ACC_EMU:-$HOME/code/fab-agon-emulator}
BIN=$EMU/target/release/agon-cli-emulator
MOS=${ACC_MOS:-$EMU/sdcard/MOS.bin}

[ -x "$BIN" ] || { echo "no emulator at $BIN (set ACC_EMU)" >&2; exit 77; }
[ -f "$MOS" ] || { echo "no MOS.bin at $MOS (set ACC_MOS)" >&2; exit 77; }
[ $# -eq 1 ] || { echo "usage: $0 <program.bin>" >&2; exit 2; }

sd=$(mktemp -d); trap 'rm -rf "$sd"' EXIT
mkdir -p "$sd/bin"
cp "$MOS" "$sd/MOS.bin"
cp "$1" "$sd/bin/p.bin"
printf 'p\r\n' > "$sd/autoexec.txt"

(cd "$EMU" && timeout "${ACC_EMU_TIMEOUT:-60}" "$BIN" --sdcard "$sd" -z -u >/dev/null 2>&1)

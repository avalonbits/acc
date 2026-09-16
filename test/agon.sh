#!/bin/bash
# Runs an Agon binary under fab-agon-emulator and reports what it printed.
#
#   test/agon.sh <program.bin> [more.bin ...]
#
# The first binary is the one run; any others are copied alongside it so a
# program can call them. Output is whatever the program wrote; the exit status
# is the program's own, carried out through the emulator.
#
# A program signals its status by writing to IO port 0, which stops the
# emulator with that value as the exit code. exit_ok and exit_fail below are
# the two-instruction programs that do it, built here rather than taken from
# the emulator's own sdcard: the copies there are moslets linked at 0xb0000,
# and a moslet in /bin does not run.
set -uo pipefail
cd "$(dirname "$0")/.."

EMU=${ACC_EMU:-$HOME/code/fab-agon-emulator}
BIN=$EMU/target/release/agon-cli-emulator
MOS=${ACC_MOS:-$EMU/sdcard/MOS.bin}
TIMEOUT=${ACC_EMU_TIMEOUT:-30}

[ -x "$BIN" ] || { echo "SKIP: no emulator at $BIN (set ACC_EMU)" >&2; exit 77; }
[ -f "$MOS" ] || { echo "SKIP: no MOS.bin at $MOS (set ACC_MOS)" >&2; exit 77; }
[ $# -ge 1 ] || { echo "usage: $0 <program.bin> [more.bin ...]" >&2; exit 2; }

prog=$(basename "$1" .bin)
sd=$(mktemp -d)
trap 'rm -rf "$sd"' EXIT
mkdir -p "$sd/bin"
cp "$MOS" "$sd/MOS.bin"
for f in "$@"; do cp "$f" "$sd/bin/"; done

# Both spin for about 100ms of emulated time before signalling. The VDP runs
# on its own thread and the shutdown races it: without the delay the last of
# the program's output is lost, and about one run in two came back empty.
DELAY="06 03 21 00 00 00 2b 7c b5 20 fb 10 f5"
python3 test/moshdr.py exit_ok.bin   "$sd/bin/exit_ok.bin"   $DELAY af d3 00 c9
python3 test/moshdr.py exit_fail.bin "$sd/bin/exit_fail.bin" $DELAY 3e 01 d3 00 c9

# CRLF: MOS reads autoexec.txt as DOS text.
printf '%s\r\nexit_ok\r\n' "$prog" > "$sd/autoexec.txt"

# stdout only. The program's output and MOS's chatter are both on stdout; the
# emulator's own diagnostics go to stderr, including the panic its shutdown
# raises when it races the VDP thread. That panic is harmless -- the delay in
# exit_ok is what keeps output from being lost -- but it is written from
# another thread, so a line-based filter catches it only sometimes: about one
# run in five leaked a torn "thread '" into the comparison.
out=$(cd "$EMU" && timeout "$TIMEOUT" "$BIN" --sdcard "$sd" -z -u 2>/dev/null)
status=$?
[ $status -eq 124 ] && { echo "TIMEOUT after ${TIMEOUT}s" >&2; exit 124; }

# Strip the emulator's and MOS's own chatter, leaving the program's output.
#
# The panic is the emulator's shutdown racing its VDP thread. It is harmless
# and says nothing about the program -- the delay in exit_ok is what keeps the
# output itself from being lost -- but it arrives on the same stream, so it
# has to come out here or every comparison against expected output fails.
#
# tr drops control bytes as well: MOS emits a stray 0x80 during boot, which is
# not whitespace and so survives a blank-line filter and shows up as a
# mismatched first line.
printf '%s\n' "$out" \
  | sed -e '/Tom.s Fake VDP/d' -e '/unknown packet VDU/d' \
        -e '/Agon Console8 MOS Version/d' -e '/Emulator shutdown triggered/d' \
  | tr -cd '\11\12\15\40-\176' \
  | sed -e '/^[[:space:]]*$/d'

exit $status

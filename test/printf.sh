#!/bin/bash
# printf, against the printf on the machine running the test.
#
# There is no oracle for what a program prints the way there is for what it
# returns -- the return-42 tests read one byte back through a port and say
# nothing about the characters. So this one reads the console: the same
# program is built for the Agon and for the host, both are run, and the text
# has to be the same line for line.
#
# The host's C library is the reference. What that pins is everything about
# printf that is rules rather than machinery -- the padding, the precisions,
# the signs, the hex digits -- against an implementation that has had rather
# more eyes on it than this one. test/printf/format.c stays inside the
# values where a 24-bit int and a 32-bit long still agree with the host's.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}
CC=${CC:-cc}
SRC=test/printf/format.c

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
[ -f "$LIB" ] || { echo "$LIB missing -- run make"; exit 2; }
command -v "$CC" >/dev/null 2>&1 || { echo "no host compiler (set CC)"; exit 77; }
emu_available >/dev/null 2>&1 || exit 77

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# The reference, from the host.
"$CC" -w -o "$tmp/host" "$SRC" || { echo "  FAIL the host build failed"; exit 1; }
"$tmp/host" > "$tmp/want" || { echo "  FAIL the host program failed"; exit 1; }

# And the same program on the Agon.
if ! err=$("$ACC" -c "$SRC" -o "$tmp/p.o" -Iinclude 2>&1); then
    printf '  FAIL %s\n' "$(printf '%s' "$err" | head -1)"; exit 1
fi
if ! err=$("$ACC" "$tmp/p.o" "$LIB" -o "$tmp/p.bin" 2>&1); then
    printf '  FAIL %s\n' "$(printf '%s' "$err" | head -1)"; exit 1
fi

sd=$(emu_card)
cp "$tmp/p.bin" "$sd/bin/p.bin"
printf 'bin/p\r\n' > "$sd/autoexec.txt"
ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-120} emu_run "$sd" -z -u > "$tmp/raw" 2>&1
rm -rf "$sd"

# What the program printed sits between MOS's banner and the six digits the
# startup stub prints on the way out. The carriage returns MOS's console adds
# are dropped: the host writes a line feed alone for a '\n' and this is not a
# test of which bytes a console wants.
tr -d '\r' < "$tmp/raw" \
  | sed -n '/MOS Version/,/^[0-9a-fA-F]\{6\}$/p' \
  | sed '1d; $d' \
  | sed '/./,$!d' > "$tmp/got"

if [ ! -s "$tmp/got" ]; then
    echo "  FAIL the program printed nothing -- the emulator's capture:"
    sed 's/^/         /' "$tmp/raw" | head -20
    exit 1
fi

if diff -u "$tmp/want" "$tmp/got" > "$tmp/diff"; then
    printf '  %d lines match the host\n' "$(wc -l < "$tmp/want")"
    exit 0
fi

echo "  FAIL printf does not agree with the host:"
sed 's/^/         /' "$tmp/diff" | head -40
exit 1

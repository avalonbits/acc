#!/bin/bash
# The runtime's float arithmetic, run on the Agon, against the host's IEEE
# single precision: test/floatrt/check.c walks the same operands on both and
# hashes the results a block at a time. A block that differs names the
# pairs to look at; rerun with DUMP=<block> to print that block's results
# on both sides and see which operation and operands disagree.
#
#   test/floatrt.sh                 # every block
#   DUMP=12 test/floatrt.sh         # block 12, result by result
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}
CC=${CC:-cc}
SRC=test/floatrt/check.c

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
[ -f "$LIB" ] || { echo "$LIB missing -- run make"; exit 2; }
command -v "$CC" >/dev/null 2>&1 || { echo "no host compiler (set CC)"; exit 77; }
emu_available >/dev/null 2>&1 || exit 77

export ASAN_OPTIONS=detect_leaks=0
flags=()
[ -n "${DUMP:-}" ] && flags=(-DDUMP="$DUMP" -DBLOCKS=$((DUMP + 1)))

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# x86-64's float arithmetic is SSE, rounded to single precision at every
# step as the Agon's is, and never held wider.
"$CC" -O1 "${flags[@]}" -o "$tmp/host" "$SRC" \
    || { echo "  FAIL the host build failed"; exit 1; }
"$tmp/host" > "$tmp/want" || { echo "  FAIL the host program failed"; exit 1; }

if ! err=$("$ACC" -c "$SRC" -o "$tmp/f.o" -Iinclude "${flags[@]}" 2>&1); then
    printf '  FAIL %s\n' "$(printf '%s' "$err" | head -1)"; exit 1
fi
if ! err=$("$ACC" "$tmp/f.o" "$LIB" -o "$tmp/f.bin" 2>&1); then
    printf '  FAIL %s\n' "$(printf '%s' "$err" | head -1)"; exit 1
fi

sd=$(emu_card)
cp "$tmp/f.bin" "$sd/bin/fcheck.bin"
printf 'try fcheck\r\n' > "$sd/autoexec.txt"
ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-180} emu_run "$sd" -z -u > "$tmp/raw" 2>&1
rm -rf "$sd"

tr -d '\r' < "$tmp/raw" | grep -a '^[0-9][0-9]* ' > "$tmp/got"

if [ ! -s "$tmp/got" ]; then
    echo "  FAIL the program printed nothing -- the emulator's capture:"
    sed 's/^/         /' "$tmp/raw" | head -20
    exit 1
fi

if diff -u "$tmp/want" "$tmp/got" > "$tmp/diff"; then
    printf '  %d blocks of float results match the host\n' "$(wc -l < "$tmp/want")"
    exit 0
fi

echo "  FAIL the float runtime does not agree with the host:"
sed 's/^/         /' "$tmp/diff" | head -40
exit 1

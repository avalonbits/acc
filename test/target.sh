#!/bin/bash
# Compiles every case with acc as built for the Agon, on the Agon, and requires
# each image to be byte for byte what the host build of acc makes of it.
#
#   test/target.sh [acc.bin]            # default bin/acc.bin
#
# Every other test runs acc on the host, where an int is 32 bits and an
# unsigned holds what the Agon's cannot. Code written for the target's widths
# -- a value assembled a byte at a time, a 24-bit accumulator that has to stop
# before it carries out -- is exactly the code those tests cannot see going
# wrong, because on the host it does not. Here it is the Agon's acc that does
# the compiling, and the host's is the reference, so a disagreement between
# the two builds is a failure whichever of them is at fault.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${1:-bin/acc.bin}

emu_available || exit 77
[ -f "$ACC" ] || { echo "no $ACC -- run make -f Makefile.agon" >&2; exit 2; }
[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }
if pgrep -f '^[^ ]*agon-cli-emulator .*--sdcard' >/dev/null 2>&1; then
    echo "another emulator is running -- stop it first" >&2
    exit 2
fi

sd=$(emu_card); host=$(mktemp -d); trap 'rm -rf "$sd" "$host"' EXIT
cp "$ACC" "$sd/bin/acc.bin"

# The machine is stopped by a program built with -x, as in bench.sh, compiled
# by the host acc so that a broken candidate cannot leave the run hanging.
echo 'int main(void) { return 0; }' > "$host/stop.c"
bin/acc "$host/stop.c" -o "$sd/bin/stop.bin" -x >/dev/null || exit 2

# Short names, because the output's name goes into the image's header and has
# to be the same on both sides; and one line of autoexec per input, so that
# the card boots once for all of them.
: > "$sd/autoexec.txt"
n=0
for src in test/cases/*.c test/bench/*.c; do
    n=$((n + 1))
    id=$(printf 't%03d' "$n")
    cp "$src" "$sd/$id.c"
    cp "$src" "$host/$id.c"
    echo "$src" > "$host/$id.src"
    printf 'acc %s.c -o %s.bin\r\n' "$id" "$id" >> "$sd/autoexec.txt"
done
printf 'stop\r\n' >> "$sd/autoexec.txt"

ACC_EMU_TIMEOUT=${ACC_TARGET_TIMEOUT:-900} emu_run "$sd" -z -u > "$host/console.txt"

pass=0; fail=0
for c in "$host"/t*.c; do
    id=$(basename "$c" .c)
    src=$(cat "$host/$id.src")
    (cd "$host" && ASAN_OPTIONS=detect_leaks=0 "$OLDPWD/bin/acc" "$id.c" -o "$id.bin" >/dev/null 2>&1)
    if [ ! -f "$sd/$id.bin" ]; then
        printf '  FAIL %-34s the Agon made no image\n' "$src"; fail=$((fail + 1))
    elif ! cmp -s "$sd/$id.bin" "$host/$id.bin"; then
        printf '  FAIL %-34s the two builds disagree\n' "$src"; fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
done

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

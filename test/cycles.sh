#!/bin/bash
# That the cycle-counting build counts, and counts the same twice.
#
# bench.sh reads its figure from `Cycles:` when the binary prints one, and a
# timer that was never started, that overflowed, or that some other code
# also used would still print a number -- one that means nothing and that
# the benchmark would report to a decimal place. So: build it, compile one
# program with it twice, and require a count above zero and the two within
# 16384 cycles, and a count for an input that takes longer than two passes
# of the timer. Not exactly equal: an interrupt that arrives during one
# compile and not the other -- MOS's vertical blank, which the emulator paces
# by the host's clock -- adds its handler's cycles, and under MOS 3 the first
# of two compiles costs about 7,000 more than the second, every time. A timer
# that was misused is out by millions, not thousands.
#
# Needs agondev and the emulator. Skips (77) without them.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
[ -x "$AGONDEV/bin/ez80-none-elf-clang" ] || { echo "  [no agondev: cycles check skipped]"; exit 77; }
make -s -f Makefile.agon CYCLES=1 >/dev/null || exit 1

out=$(ACC_BIN=bin/acc-cycles.bin test/bench.sh 2 test/cases/010_return.c 2>/dev/null)
line=$(printf '%s\n' "$out" | grep ' 010_return.c ')
count=$(printf '%s\n' "$line" | sed -n 's/.*runs  *\([0-9]*\) cycles each.*/\1/p')
spread=$(printf '%s\n' "$line" | sed -n 's/.*, spread \([0-9]*\) .*/\1/p')
if [ "${count:-0}" -le 0 ] || [ -z "$spread" ] || [ "$spread" -gt 16384 ]; then
    echo "  FAIL cycles: expected a count within 16384 cycles twice, got:"
    printf '%s\n' "$out"
    exit 1
fi
echo "  cycles: $count for 010_return.c, within $spread of each other"

# And past more than one pass of the timer, which is 16.7 million cycles --
# past two, which the flag alone cannot count: it says the timer came round,
# not how often, so every pass but the last is counted by polling between
# declarations. A thousand small functions, generated here rather than taken
# from the benchmark, whose inputs get faster: matrix.c was past one pass
# when this was written and under it a week later.
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
awk 'BEGIN {
    for (i = 0; i < 1000; i++)
        printf "int step_%d(int a, int b) { return a * %d + b - %d; }\n", i, i % 7 + 1, i % 5;
    print "int main(void) { return step_0(40, 2); }";
}' > "$tmp/long.c"
out=$(ACC_BIN=bin/acc-cycles.bin test/bench.sh 1 "$tmp/long.c" 2>/dev/null)
line=$(printf '%s\n' "$out" | grep ' long.c ')
count=$(printf '%s\n' "$line" | sed -n 's/.*runs  *\([0-9]*\) cycles each.*/\1/p')
if [ "${count:-0}" -le 33554432 ]; then
    echo "  FAIL cycles: expected long.c counted past two passes of the timer, got:"
    printf '%s\n' "$out"
    exit 1
fi
echo "  cycles: $count for long.c, past two passes of the timer"

# And the emulator's own count (2037657 on, which test/emu.sh runs): the build
# starts it with a write to port 0x40 and prints it with one to 0x41, and
# bench.sh takes it over the timer's. It has to agree with the timer to
# within the timer's two ticks and the few instructions between the reads.
. test/emu.sh
sd=$(emu_card)
cp bin/acc-cycles.bin "$sd/bin/acc.bin"
mkdir -p "$sd/lib/acc"
cp bin/libc.a bin/rt.a "$sd/lib/acc/"        # the runtime, as the release puts it
cp test/bench/names.c "$sd/in.c"
echo 'int main(void) { return 0; }' > "$tmp/stop.c"
bin/acc "$tmp/stop.c" -o "$sd/bin/stop.bin" -x >/dev/null || exit 1
# Twice, and the first of each read: stopping the machine drops the console
# line still in flight, which is the last one, as bench.sh says.
printf 'acc in.c -o out.bin\r\nacc in.c -o out.bin\r\nstop\r\n' > "$sd/autoexec.txt"
out=$(ACC_EMU_TIMEOUT=120 emu_run "$sd" -z -u 2>&1)
rm -rf "$sd"
emu=$(printf '%s\n' "$out" | sed -n 's/.*Debug OUT(0x41): \([0-9]*\) CPU cycles.*/\1/p' | head -1)
timer=$(printf '%s\n' "$out" | sed -n 's/.*Cycles: \([0-9]*\).*/\1/p' | head -1)
if [ -z "$emu" ]; then
    echo "  FAIL cycles: the emulator printed no count for port 0x41 (test/emu.sh"
    echo "       runs one that keeps it: fab-agon-emulator 2037657 or later)"
    exit 1
fi
diff=$((emu > timer ? emu - timer : timer - emu))
if [ -z "$timer" ] || [ "$diff" -gt 1024 ]; then
    echo "  FAIL cycles: the emulator counted ${emu:-none} and the timer ${timer:-none}"
    exit 1
fi
echo "  cycles: $emu for names.c by the emulator, $timer by the timer"

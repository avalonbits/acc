#!/bin/bash
# That the cycle-counting build counts, and counts the same twice.
#
# bench.sh reads its figure from `Cycles:` when the binary prints one, and a
# timer that was never started, that overflowed, or that some other code
# also used would still print a number -- one that means nothing and that
# the benchmark would report to a decimal place. So: build it, compile one
# program with it twice, and require a count above zero and the two within
# two ticks of the timer, 512 cycles. Not exactly equal: an interrupt that
# arrives during one compile and not the other -- MOS's vertical blank, which
# the emulator paces by the host's clock -- adds its handler's cycles.
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
if [ "${count:-0}" -le 0 ] || [ -z "$spread" ] || [ "$spread" -gt 512 ]; then
    echo "  FAIL cycles: expected a count within 512 cycles twice, got:"
    printf '%s\n' "$out"
    exit 1
fi
echo "  cycles: $count for 010_return.c, within $spread of each other"

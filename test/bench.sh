#!/bin/bash
# How long acc takes to compile test/bench/big.c, on the Agon.
#
#   test/bench.sh [runs]        # default 10
#
# Measured on the target because the host is not a proxy for it: the same
# change can look like a 1.4x win here and a 3.2x win there, and host counts
# have called the largest win of a set a regression.
#
# No -u. Unthrottled, the guest's clock() stops tracking the work it did and
# the number means nothing. Wall clock from outside is no use either -- it
# measures the emulator, not the Agon -- so what is read is the line acc prints
# for itself.
#
# acc's clock counts hundredths and one compile is a fifth of a second, so the
# run is repeated and the reported times summed: the tick boundary falls
# somewhere different each time, so the quantisation averages out rather than
# accumulating.
#
# Keep the host otherwise idle. MOS's clock comes from emulated VBLANK on
# another thread, so a loaded host inflates the reading, and two emulators time
# each other's work.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC_BIN:-bin/acc.bin}
SRC=${ACC_BENCH_SRC:-test/bench/big.c}
RUNS=${1:-10}

emu_available || exit 77
[ -f "$ACC" ] || { echo "no $ACC -- run make -f Makefile.agon" >&2; exit 2; }

# Matched with the --sdcard it is always started with, so that a shell whose
# command line merely mentions the emulator does not count as one running.
if pgrep -f 'agon-cli-emulator .*--sdcard' >/dev/null 2>&1; then
    echo "another emulator is running -- stop it first" >&2
    exit 2
fi

sd=$(emu_card); trap 'rm -rf "$sd"' EXIT
cp "$ACC" "$sd/bin/acc.bin"
cp "$SRC" "$sd/in.c"
: > "$sd/autoexec.txt"
for _ in $(seq "$RUNS"); do printf 'acc in.c -o out.bin\r\n' >> "$sd/autoexec.txt"; done

out=$(ACC_EMU_TIMEOUT=${ACC_BENCH_TIMEOUT:-900} emu_run "$sd" -z)

times=$(printf '%s' "$out" | sed -n 's/.*Done in \([0-9]*\)\.\([0-9][0-9]\) seconds.*/\1\2/p')
n=$(printf '%s\n' "$times" | grep -c .)

# The emulator has no way to stop itself, so it is killed by the timeout and
# autoexec may have looped. Only the first RUNS readings are this measurement.
if [ "$n" -lt "$RUNS" ]; then
    echo "only $n of $RUNS runs reported -- the compile failed or timed out" >&2
    printf '%s\n' "$out" | grep -i error >&2
    exit 1
fi

total=$(printf '%s\n' "$times" | head -n "$RUNS" | awk '{t+=$1} END {print t}')
printf '%-16s %2d runs  %d.%02d s total  %d.%03d s each\n' \
    "$(basename "$ACC")" "$RUNS" \
    $((total / 100)) $((total % 100)) \
    $((total / RUNS / 100)) $((total * 10 / RUNS % 1000))

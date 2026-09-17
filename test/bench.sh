#!/bin/bash
# How long acc takes to compile test/bench/big.c, on the Agon.
#
#   test/bench.sh [runs] [source.c ...]     # default 10, all of test/bench
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
RUNS=${1:-10}
shift 2>/dev/null

# Every input by default, so that adding a feature to the compiler and not to
# the benchmark shows up as an input that is not there rather than as a
# measurement that quietly stops covering it. if/else/while were measured as
# costing nothing for a while, on a program with no if and no while in it.
if [ $# -gt 0 ]; then
    SRCS="$*"
else
    SRCS=$(echo test/bench/*.c)
fi
[ -n "${ACC_BENCH_SRC:-}" ] && SRCS=$ACC_BENCH_SRC

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

# MOS runs autoexec and then sits at the prompt: the emulator has no way to
# stop itself, so without this every measurement burned the whole timeout and
# grew a capture file for the length of it. A program built with -x writes its
# result to IO port 0, which is what stops the machine, so the card ends by
# running one. It is compiled by the host acc, not the one being measured, so
# that a broken candidate cannot leave the run hanging.
[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }
echo 'int main(void) { return 0; }' > "$sd/stop.c"
bin/acc "$sd/stop.c" -o "$sd/bin/stop.bin" -x >/dev/null || exit 2
rm -f "$sd/stop.c"

# Which keywords the compiler knows, against which ones any input uses. This
# is the check that was missing: if/else/while were added to the compiler and
# not to the benchmark, so the benchmark went on reporting a number that could
# not see them, and the feature measured as free on a program that never used
# it. A keyword the benchmark never compiles is a keyword whose code is not
# being measured.
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

missing=
for kw in $(sed -n 's/.*keyword("\([a-z]*\)".*/\1/p' src/lex.c); do
    grep -qE "(^|[^A-Za-z_])$kw([^A-Za-z_0-9]|\$)" $SRCS || missing="$missing $kw"
done

# And the operators. Keywords alone missed the comparisons, which are not
# keywords. Whether an operator counts is decided by compiling one -- an
# operator acc rejects has no code to measure -- rather than by reading the
# source, which cannot tell an operator the lexer knows from one the code
# generator implements.
#
# Globbing off while this runs, or `*` matches the working directory and the
# note lists the contents of the repo.
set -f
# Searched with the comments stripped. Every input opens with one, so a bare
# `*` or `/` matched the `/*` of a comment and every input looked to use both.
for src in $SRCS; do
    sed 's|//.*||' "$src" | tr '\n' '\001' | sed 's=/\*[^\*]*\*\+\([^/\*][^\*]*\*\+\)*/= =g' \
        | tr '\001' '\n' >> "$tmp/code.c"
done

check_op() {
    printf '%s\n' "$2" > "$tmp/op.c"
    bin/acc "$tmp/op.c" -o "$tmp/op.bin" -x >/dev/null 2>&1 || return 0
    grep -qF -- "$1" "$tmp/code.c" || missing="$missing $1"
}
for op in + - '*' / % '&' '|' '^' '<<' '>>' '<' '>' '<=' '>=' '==' '!='; do
    check_op "$op" "int main(void) { int a = 1; int b = 2; return a $op b; }"
done
for op in - '~' '!'; do
    check_op "$op" "int main(void) { int a = 1; return $op a; }"
done
set +f

[ -z "$missing" ] || echo "note: no benchmark input uses:$missing" >&2

status=0
total_all=0

for SRC in $SRCS; do
    [ -f "$SRC" ] || { echo "no such input: $SRC" >&2; status=1; continue; }

    # The benchmark is also a test. A miscompiled input would be timed just as
    # happily as a correct one, and the number would mean nothing; every input
    # is written to return 42, and this says so before the clock is read.
    if ! bin/acc "$SRC" -o "$sd/check.bin" -x >/dev/null 2>&1; then
        echo "$(basename "$SRC"): the host acc cannot compile it" >&2
        status=1; continue
    fi
    test/agon.sh "$sd/check.bin" >/dev/null 2>&1; v=$?
    if [ "$v" -ne 42 ] && [ "$v" -ne 77 ]; then
        echo "$(basename "$SRC"): returns $v, not 42 -- not timing a miscompile" >&2
        status=1; continue
    fi
    rm -f "$sd/check.bin"

    cp "$SRC" "$sd/in.c"

    # One compile more than is read. Halting the machine drops whatever the
    # console still has in flight, which is reliably the last line; the spare
    # one flushes the ones that count. Only the first RUNS are the measurement.
    : > "$sd/autoexec.txt"
    for _ in $(seq $((RUNS + 1))); do printf 'acc in.c -o out.bin\r\n' >> "$sd/autoexec.txt"; done
    printf 'stop\r\n' >> "$sd/autoexec.txt"

    out=$(ACC_EMU_TIMEOUT=${ACC_BENCH_TIMEOUT:-600} emu_run "$sd" -z)

    times=$(printf '%s' "$out" | sed -n 's/.*Done in \([0-9]*\)\.\([0-9][0-9]\) seconds.*/\1\2/p')
    n=$(printf '%s\n' "$times" | grep -c .)
    if [ "$n" -lt "$RUNS" ]; then
        echo "$(basename "$SRC"): only $n of $RUNS runs reported" >&2
        printf '%s\n' "$out" | grep -i error >&2
        status=1; continue
    fi

    total=$(printf '%s\n' "$times" | head -n "$RUNS" | awk '{t+=$1} END {print t}')
    total_all=$((total_all + total))
    printf '%-16s %-14s %2d runs  %d.%02d s  %d.%03d s each\n' \
        "$(basename "$ACC")" "$(basename "$SRC")" "$RUNS" \
        $((total / 100)) $((total % 100)) \
        $((total / RUNS / 100)) $((total * 10 / RUNS % 1000))
done

printf '%-16s %-14s %2d runs  %d.%02d s\n' \
    "$(basename "$ACC")" "(all)" "$RUNS" $((total_all / 100)) $((total_all % 100))

exit $status

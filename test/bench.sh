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

# The eZ80 in an Agon Light runs at 18.432 MHz. Speed is reported as cycles
# per byte of source as well as seconds, because that is the figure that can
# be compared against zap's and against a reading taken on a different input.
CLOCK=18432000

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

# Matched with the --sdcard it is always started with, and anchored to the
# start of the command line, so that only a process whose executable is the
# emulator counts. Unanchored, a shell whose command merely contained this
# pattern -- an edit to this very line, say -- counted as an emulator running,
# and the benchmark refused to start.
#
# One that has just finished can still be exiting when the next run starts --
# two benchmarks back to back tripped this repeatedly -- so it is given a few
# seconds to go before this gives up.
for _ in 1 2 3 4 5 6 7 8 9 10; do
    pgrep -f '^[^ ]*agon-cli-emulator .*--sdcard' >/dev/null 2>&1 || break
    sleep 1
done
if pgrep -f '^[^ ]*agon-cli-emulator .*--sdcard' >/dev/null 2>&1; then
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

# Searched with the comments stripped. Every input opens with one, so a bare
# `*` or `/` matched the `/*` of a comment and every input looked to use both
# -- and the word "long" in a sentence covered the keyword.
for src in $SRCS; do
    sed 's|//.*||' "$src" | tr '\n' '\001' | sed 's=/\*[^\*]*\*\+\([^/\*][^\*]*\*\+\)*/= =g' \
        | tr '\001' '\n' >> "$tmp/code.c"
done

missing=
for kw in $(sed -n 's/.*keyword("\([a-z]*\)".*/\1/p' src/lex.c); do
    grep -qE "(^|[^A-Za-z_])$kw([^A-Za-z_0-9]|\$)" "$tmp/code.c" || missing="$missing $kw"
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

# And the ones acc gained after the list above was written, which it did not
# know to look for: the benchmark went on measuring none of them, and the
# first input written to cover pointers found three code-generation bugs
# that the older inputs could never have reached.
for op in '&&' '||'; do
    check_op "$op" "int main(void) { int a = 1; int b = 2; return a $op b; }"
done
for op in '+=' '-=' '*=' '/=' '%=' '&=' '|=' '^=' '<<=' '>>='; do
    check_op "$op" "int main(void) { int a = 1; a $op 2; return a; }"
done
for op in '++' '--'; do
    check_op "$op" "int main(void) { int a = 1; a$op; return a; }"
done
check_op '?' "int main(void) { int a = 1; return a ? 2 : 3; }"
check_op '[' "int main(void) { int a[2]; a[0] = 1; return a[0]; }"

# Forms rather than operators: what they look like has no one spelling, so
# each is a pattern, and each is only asked for once acc compiles it.
check_form() {
    printf '%s\n' "$3" > "$tmp/op.c"
    bin/acc "$tmp/op.c" -o "$tmp/op.bin" -x >/dev/null 2>&1 || return 0
    grep -qE -- "$2" "$tmp/code.c" || missing="$missing $1"
}
check_form "globals" '^(int|char|short|long|unsigned|signed|float|double)[^(]*[;=[]' \
    "int g; int main(void) { return g; }"
check_form "pointers" '(char|short|int|long|float|void) +\*+ *[A-Za-z_]' \
    "int main(void) { int a = 1; int *p = &a; return *p; }"
check_form "&variable" '(^|[^&])&[A-Za-z_]' \
    "int main(void) { int a = 1; int *p = &a; return *p; }"
check_form "hex" '0[xX][0-9a-fA-F]' "int main(void) { return 0x2a; }"
check_form "octal" '(^|[^0-9A-Za-z_.])0[0-7]+([^0-9A-Za-z_.]|$)' \
    "int main(void) { return 052; }"
check_form "strings" '"[^"]*"' \
    'int main(void) { char *s = "x"; return s[0]; }'
check_form "chars" "'[^']+'" \
    "int main(void) { return 'a'; }"
# A declaration indented past a function's own, which is one in an inner block.
check_form "block-declarations" '^        +(int|char|short|long|unsigned|float|double) [A-Za-z_]' \
    "int main(void) { { int x = 1; return x; } }"
check_op '->' "struct s { int m; }; int main(void) { struct s v, *p = &v; p->m = 1; return p->m; }"
check_form "members" '[A-Za-z_0-9)]\.[A-Za-z_]' \
    "struct s { int m; }; int main(void) { struct s v; v.m = 1; return v.m; }"
check_form "casts" '\((unsigned |signed )?(char|short|int|long|float|double|void) *\**\) *[A-Za-z_(0-9]' \
    "int main(void) { long a = 1; return (int) a; }"
set +f

[ -z "$missing" ] || echo "note: no benchmark input uses:$missing" >&2

status=0
total_all=0
bytes_all=0
cycles_all=0

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
    bytes=$(stat -c%s "$SRC")
    bytes_all=$((bytes_all + bytes))

    # A build made with CYCLES=1 also says how many cycles each compile took,
    # counted by the eZ80's own timer. Where it does, that is the figure: the
    # seconds come from a clock the emulator keeps on another thread and
    # wander by a few percent between sittings, and the count does not.
    cycles=$(printf '%s' "$out" | sed -n 's/.*Cycles: \([0-9][0-9]*\).*/\1/p' | head -n "$RUNS")
    if [ "$(printf '%s\n' "$cycles" | grep -c .)" -eq "$RUNS" ]; then
        csum=$(printf '%s\n' "$cycles" | awk '{t+=$1} END {printf "%d", t}')
        spread=$(printf '%s\n' "$cycles" | sort -n | sed -n '1p;$p' | paste -sd' ' | awk '{print $2 - $1}')
        cycles_all=$((cycles_all + csum))
        printf '%-16s %-14s %2d runs  %d cycles each, spread %d  %d.%d cycles/byte\n' \
            "$(basename "$ACC")" "$(basename "$SRC")" "$RUNS" \
            $((csum / RUNS)) "$spread" $((csum / (RUNS * bytes))) \
            $((csum * 10 / (RUNS * bytes) % 10))
        continue
    fi

    # Cycles per byte of source, which is the figure to compare against zap's.
    # The readings are hundredths of a second for RUNS compiles, so
    #   cycles/byte = total/100/RUNS * CLOCK / bytes
    # and CLOCK/100 is exact, which keeps this in integers.
    printf '%-16s %-14s %2d runs  %d.%02d s  %d.%03d s each  %d cycles/byte\n' \
        "$(basename "$ACC")" "$(basename "$SRC")" "$RUNS" \
        $((total / 100)) $((total % 100)) \
        $((total / RUNS / 100)) $((total * 10 / RUNS % 1000)) \
        $((total * (CLOCK / 100) / (RUNS * bytes)))
done

if [ "$cycles_all" -gt 0 ]; then
    printf '%-16s %-14s %2d runs  %*s%d.%d cycles/byte\n' \
        "$(basename "$ACC")" "(all)" "$RUNS" 21 "" \
        $((cycles_all / (RUNS * bytes_all))) \
        $((cycles_all * 10 / (RUNS * bytes_all) % 10))
    exit $status
fi

printf '%-16s %-14s %2d runs  %d.%02d s%*s%d cycles/byte\n' \
    "$(basename "$ACC")" "(all)" "$RUNS" \
    $((total_all / 100)) $((total_all % 100)) 17 "" \
    $((bytes_all == 0 ? 0 : total_all * (CLOCK / 100) / (RUNS * bytes_all)))

exit $status

#!/bin/bash
# How long acc takes, on the Agon: from the command being typed to the
# prompt coming back.
#
#   test/bench.sh [runs] [source.c ...]     # default 10, all of test/bench
#
# What is timed is the whole of an invocation but what MOS costs any
# command: acc's loading, its startup, the compile or the link, writing the
# output, and exit. Two small programs on the card start and stop the
# emulator's cycle counter (IO ports 0x40 and 0x41), one on each side of the
# command; the same pair around a program that does nothing is timed too,
# and taken off -- which takes off the pair and MOS's own cost with it. That
# is the same for every program, 147 ms of MOS 3.0.2 finding and starting
# one and coming back, and nothing acc does can change it; it is reported
# once, as (MOS). A count is exact
# and does not care how fast the host runs the machine, so the emulator
# runs unthrottled (-u). That needs an emulator that counts (fab-agon-emulator
# 2037657 on: point ACC_EMU at a build of it), and without one there is no
# figure.
#
# The emulator reads the card without the time a real SD card takes: a
# program of 240 KB loads in the same cycles as one of 268 bytes. So the
# figure is what an Agon would take with a card that cost nothing to read,
# and a real one takes that and the reading of acc.bin besides.
#
# After the inputs, the same for whole programs: hello.c compiled to an
# object, linked, and built in one step, and zap linked from its objects --
# reported in milliseconds, since what they cost is not a function of any
# one file's size.
#
# The rest of this note is about a build made with CYCLES=1, which counts
# only its own compile, from after its arguments to before "Done in": that
# figure leaves out loading, startup and exit, and is what this script
# reported before. It is still what a counting build gives.
#
# Measured on the target because the host is not a proxy for it: the same
# change can look like a 1.4x win here and a 3.2x win there, and host counts
# have called the largest win of a set a regression.
#
# No -u for a build that only reports seconds. Unthrottled, the guest's
# clock() stops tracking the work it did and the number means nothing. Wall
# clock from outside is no use either -- it measures the emulator, not the
# Agon -- so what is read is the line acc prints for itself.
#
# A build made with CYCLES=1 counts cycles, and a count does not care how
# fast the host runs the machine: the eZ80's timer and the emulator's own
# counter are both stepped by the instructions it executes. So a counting
# build runs with -u, many times faster. The emulator's count is exact and
# is taken when there is one (fab-agon-emulator 2037657 on, which prints it
# for a write to IO port 0x41 -- point ACC_EMU at a build of it); otherwise
# the timer's, to 256 cycles. Throttled, a count comes out about 0.04% higher,
# from the extra interrupts that land when the machine runs at its real
# speed.
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
ALL_INPUTS=0
if [ $# -gt 0 ]; then
    SRCS="$*"
else
    SRCS=$(echo test/bench/*.c)
    ALL_INPUTS=1
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
mkdir -p "$sd/lib/acc"                  # where the Agon build looks
cp -r include "$sd/lib/acc/include"
cp bin/libc.a bin/rt.a "$sd/lib/acc/"            # and the runtime every program calls

# A counting build says so in its own bytes: the format it reports with.
# Any other is timed whole, between the two programs that start and stop
# the emulator's count.
speed=-u
whole=
if ! grep -aq 'Cycles: ' "$ACC"; then
    whole=1
    for port in 40 41; do
        printf '#include <ez80f92.h>\nint main(void) { io_out(0x%s, 0); return 0; }\n' \
            "$port" > "$sd/t.c"
        bin/acc -Iinclude "$sd/t.c" bin/libc.a -o "$sd/bin/tick$port.bin" >/dev/null \
            || { echo "cannot build the counter's switches" >&2; exit 2; }
    done
    printf 'int main(void) { return 0; }\n' > "$sd/t.c"
    bin/acc "$sd/t.c" -o "$sd/bin/nop.bin" >/dev/null || exit 2
    rm -f "$sd/t.c"
fi

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

# One translation unit of a real program, alongside the generated inputs.
#
# The generated ones are the same shapes over and over, at the sizes and name
# lengths real C has, and what they cannot be is a real program's proportions:
# how much of a file is declarations against statements, how deep its
# expressions go, how many of the names in a header it happens not to use.
# zap is the program acc exists to build on the machine, so zap is where that
# is taken from.
#
# Not committed here: zap is under the GPL and acc is not, and a benchmark
# input is no reason to mix them. It is taken from a checkout, at a tag rather
# than at whatever is checked out, so that the input is the same bytes today
# as last week -- and left out, with a note, when there is no checkout to take
# it from. It is a unit and not a program, so it is compiled and not run.
ZAP=${ZAP:-$HOME/code/zap}
ZAP_REV=${ZAP_REV:-v1.0.3}
ZAP_UNIT=${ZAP_UNIT:-buf_reader.c}
UNITS=

if [ -z "${ACC_BENCH_SRC:-}" ] && [ $# -eq 0 ] \
   && git -C "$ZAP" rev-parse -q --verify "$ZAP_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$tmp/zap"
    if git -C "$ZAP" archive "$ZAP_REV" src | tar -x -C "$tmp/zap" 2>/dev/null \
       && test/bench/amalgamate.py "$tmp/zap/src" "$ZAP_UNIT" include \
            > "$tmp/zap-$ZAP_UNIT" 2>/dev/null; then
        SRCS="$SRCS $tmp/zap-$ZAP_UNIT"
        UNITS="$tmp/zap-$ZAP_UNIT"
    else
        echo "note: $ZAP $ZAP_REV has no $ZAP_UNIT -- the real-code input is" \
             "left out" >&2
    fi
else
    [ -n "$UNITS" ] || echo "note: no zap at $ZAP ($ZAP_REV) -- the real-code" \
        "input is left out" >&2
fi

# And acc's own, which is the other real program on this machine -- and the
# one that decides whether it can build itself.
#
# It is here in the repository rather than in a checkout somewhere else, so
# it is always there to take. Taken at a fixed commit all the same, and for
# the same reason zap is: an input that moved every time the compiler did
# would make every figure a comparison of two different things. Moving it on
# is a deliberate act, the way bumping ZAP_REV is.
#
# What it brings that nothing else here does is the shapes a compiler is
# written in: switches with a case a line, tables of pointers, functions that
# take a struct apart a field at a time -- and the keywords the generated
# inputs have no reason to use.
ACC_REV=${ACC_REV:-a348474}
ACC_UNIT=${ACC_UNIT:-sym.c}

if [ -z "${ACC_BENCH_SRC:-}" ] && [ $# -eq 0 ] \
   && git rev-parse -q --verify "$ACC_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$tmp/acc"
    if git archive "$ACC_REV" src include | tar -x -C "$tmp/acc" 2>/dev/null \
       && test/bench/amalgamate.py "$tmp/acc/src" "$ACC_UNIT" "$tmp/acc/include" \
            > "$tmp/acc-$ACC_UNIT" 2>/dev/null; then
        SRCS="$SRCS $tmp/acc-$ACC_UNIT"
        UNITS="$UNITS $tmp/acc-$ACC_UNIT"
    else
        echo "note: $ACC_REV has no src/$ACC_UNIT -- acc's own source is" \
             "left out" >&2
    fi
else
    case " $UNITS " in
      *acc-*) ;;
      *) echo "note: no acc at $ACC_REV -- acc's own source is left out" >&2 ;;
    esac
fi

# Searched with the comments stripped. Every input opens with one, so a bare
# `*` or `/` matched the `/*` of a comment and every input looked to use both
# -- and the word "long" in a sentence covered the keyword.
# A small program compiled as it stands, reading its headers from
# /lib/acc/include as it would on the machine: the other real-code inputs
# have theirs folded in, and so never open a header at all. See
# test/bench/headers/agon.c.
if [ -z "${ACC_BENCH_SRC:-}" ] && [ $# -eq 0 ]; then
    SRCS="$SRCS test/bench/headers/agon.c"
    UNITS="$UNITS test/bench/headers/agon.c"
fi

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
check_form "prototypes" '^(static |extern )?[a-z][a-z ]*[ *][a-z_0-9]+\(.*\);$' \
    "int f(int); int main(void) { return f(1); } int f(int x) { return x; }"
check_form "casts" '\((unsigned |signed )?(char|short|int|long|float|double|void) *\**\) *[A-Za-z_(0-9]' \
    "int main(void) { long a = 1; return (int) a; }"
set +f

[ -z "$missing" ] || echo "note: no benchmark input uses:$missing" >&2

# Milliseconds at the Agon's clock, to a tenth: `ms <cycles>`.
ms() {
    printf '%d.%d' $(($1 / (CLOCK / 1000))) $(($1 * 10 / (CLOCK / 1000) % 10))
}

# What starts every card run timed whole: the two switches with nothing
# between them, and then around a program that does nothing.
WHOLE_START='tick40\r\ntick41\r\ntick40\r\nnop\r\ntick41\r\n'

# The counts of such a run: the second of those is taken off each of the
# next RUNS, which leaves what the command cost past what any command does.
whole_counts() {
    printf '%s' "$1" \
        | sed -n 's/.*Debug OUT(0x41): \([0-9][0-9]*\) CPU cycles.*/\1/p' \
        | awk -v runs="$RUNS" 'NR == 1 { next } NR == 2 { base = $1; next }
                               NR <= runs + 2 { print $1 - base }'
}

# And what MOS costs any command: the second of them less the first.
mos_count() {
    printf '%s' "$1" \
        | sed -n 's/.*Debug OUT(0x41): \([0-9][0-9]*\) CPU cycles.*/\1/p' \
        | awk 'NR == 1 { a = $1 } NR == 2 { print $1 - a; exit }'
}

status=0
total_all=0
bytes_all=0
cycles_all=0
estimated=

for SRC in $SRCS; do
    [ -f "$SRC" ] || { echo "no such input: $SRC" >&2; status=1; continue; }

    # The benchmark is also a test. A miscompiled input would be timed just as
    # happily as a correct one, and the number would mean nothing; every
    # generated input is written to return 42, and this says so before the
    # clock is read. A unit has no main to run, so what is asked of it is that
    # it compiles -- which is the whole of what is being timed.
    case " $UNITS " in
      *" $SRC "*) unit=1 ;;
      *)          unit=0 ;;
    esac
    if [ "$unit" = 1 ]; then
        if ! bin/acc -c "$SRC" -o "$sd/check.o" -Iinclude >/dev/null 2>&1; then
            echo "$(basename "$SRC"): the host acc cannot compile it" >&2
            status=1; continue
        fi
        rm -f "$sd/check.o"
    else
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
    fi

    cp "$SRC" "$sd/in.c"

    # One compile more than is read. Halting the machine drops whatever the
    # console still has in flight, which is reliably the last line; the spare
    # one flushes the ones that count. Only the first RUNS are the measurement.
    # A unit is compiled to an object, and every run to an object of its own:
    # acc records in one what it was made from, and a second compile over an
    # unchanged source says it is up to date and does no work at all. Timed
    # over one name, the first run was the compile and the rest were that
    # answer -- which is what a spread of nine million cycles was saying.
    : > "$sd/autoexec.txt"
    rm -f "$sd"/out*.o
    # shellcheck disable=SC2059
    [ -z "$whole" ] || printf "$WHOLE_START" >> "$sd/autoexec.txt"
    for i in $(seq $((RUNS + 1))); do
        [ -z "$whole" ] || printf 'tick40\r\n' >> "$sd/autoexec.txt"
        if [ "$unit" = 1 ]; then
            printf 'acc -c in.c -o out%s.o\r\n' "$i" >> "$sd/autoexec.txt"
        else
            printf 'acc in.c -o out.bin\r\n' >> "$sd/autoexec.txt"
        fi
        [ -z "$whole" ] || printf 'tick41\r\n' >> "$sd/autoexec.txt"
    done
    printf 'stop\r\n' >> "$sd/autoexec.txt"

    out=$(ACC_EMU_TIMEOUT=${ACC_BENCH_TIMEOUT:-600} emu_run "$sd" -z $speed)

    times=$(printf '%s' "$out" | sed -n 's/.*Done in \([0-9]*\)\.\([0-9][0-9]\) seconds.*/\1\2/p')
    n=$(printf '%s\n' "$times" | grep -c .)
    if [ "$n" -lt "$RUNS" ]; then
        echo "$(basename "$SRC"): only $n of $RUNS runs reported" >&2
        printf '%s\n' "$out" | grep -i error >&2
        status=1; continue
    fi

    total=$(printf '%s\n' "$times" | head -n "$RUNS" | awk '{t+=$1} END {print t}')
    total_all=$((total_all + total))
    # The bytes acc reads: an input that includes headers is counted with
    # each of them once, as their guards have it -- which is the size of
    # the input with them folded in.
    if grep -q '^[[:space:]]*#[[:space:]]*include' "$SRC"; then
        bytes=$(test/bench/amalgamate.py "$(dirname "$SRC")" "$(basename "$SRC")" \
                include | wc -c)
    else
        bytes=$(stat -c%s "$SRC")
    fi

    # A build made with CYCLES=1 also says how many cycles each compile took,
    # counted by the eZ80's own timer. Where it does, that is the figure: the
    # seconds come from a clock the emulator keeps on another thread and
    # wander by a few percent between sittings, and the count does not.
    if [ -n "$whole" ]; then
        cycles=$(whole_counts "$out")
    else
        cycles=$(printf '%s' "$out" \
            | sed -n 's/.*Debug OUT(0x41): \([0-9][0-9]*\) CPU cycles.*/\1/p' | head -n "$RUNS")
        [ "$(printf '%s\n' "$cycles" | grep -c .)" -eq "$RUNS" ] \
            || cycles=$(printf '%s' "$out" | sed -n 's/.*Cycles: \([0-9][0-9]*\).*/\1/p' | head -n "$RUNS")
    fi
    if [ "$(printf '%s\n' "$cycles" | grep -c .)" -eq "$RUNS" ]; then
        csum=$(printf '%s\n' "$cycles" | awk '{t+=$1} END {printf "%d", t}')
        spread=$(printf '%s\n' "$cycles" | sort -n | sed -n '1p;$p' | paste -sd' ' | awk '{print $2 - $1}')
        cycles_all=$((cycles_all + csum))
        bytes_all=$((bytes_all + bytes))
        printf '%-16s %-14s %2d runs  %d cycles each, %s ms, spread %d  %d.%d cycles/byte\n' \
            "$(basename "$ACC")" "$(basename "$SRC")" "$RUNS" \
            $((csum / RUNS)) "$(ms $((csum / RUNS)))" "$spread" $((csum / (RUNS * bytes))) \
            $((csum * 10 / (RUNS * bytes) % 10))
        continue
    fi

    # Unthrottled, the seconds are the host's and not the Agon's.
    if [ -n "$speed" ]; then
        echo "$(basename "$SRC"): no count -- the emulator has to be one that" \
             "counts (ACC_EMU)" >&2
        status=1; continue
    fi

    # Cycles per byte of source, which is the figure to compare against zap's.
    # The readings are hundredths of a second for RUNS compiles, so
    #   cycles/byte = total/100/RUNS * CLOCK / bytes
    # and CLOCK/100 is exact, which keeps this in integers.
    #
    # This is where an input lands when the binary does not count cycles,
    # or its count could not be read: the seconds are what is left. They come from a clock the emulator keeps on
    # another thread and wander by a few percent, so the aggregate says when
    # any of it was arrived at this way -- otherwise a number that is mostly
    # exact reads as if it were entirely so.
    bytes_all=$((bytes_all + bytes))
    cycles_all=$((cycles_all + total * (CLOCK / 100)))
    estimated="$estimated $(basename "$SRC")"
    printf '%-16s %-14s %2d runs  %d.%02d s  %d.%03d s each  %d cycles/byte\n' \
        "$(basename "$ACC")" "$(basename "$SRC")" "$RUNS" \
        $((total / 100)) $((total % 100)) \
        $((total / RUNS / 100)) $((total * 10 / RUNS % 1000)) \
        $((total * (CLOCK / 100) / (RUNS * bytes)))
done

# Whole programs, timed whole: hello.c to an object, hello.o linked,
# hello.c built in one step, and zap linked from the objects the host acc
# makes of it -- the objects being what a link reads, whoever made them.
# Only when every input was asked for, as the real-code ones are.
program() {    # program <name> <setup line> <command, with %s for the run>
    local name=$1 setup=$2 cmd=$3 i out cycles csum

    # shellcheck disable=SC2059
    printf "%s\r\n$WHOLE_START" "$setup" > "$sd/autoexec.txt"
    for i in $(seq $((RUNS + 1))); do
        # shellcheck disable=SC2059
        printf "tick40\r\n$cmd\r\ntick41\r\n" "$i" >> "$sd/autoexec.txt"
    done
    printf 'stop\r\n' >> "$sd/autoexec.txt"
    out=$(ACC_EMU_TIMEOUT=${ACC_BENCH_TIMEOUT:-600} emu_run "$sd" -z -u)
    cycles=$(whole_counts "$out")
    if [ "$(printf '%s\n' "$cycles" | grep -c .)" -ne "$RUNS" ] \
       || printf '%s' "$out" | grep -q 'error:'; then
        echo "$name: no count, or an error:" >&2
        printf '%s\n' "$out" | grep -a 'error' >&2
        status=1
        return
    fi
    csum=$(printf '%s\n' "$cycles" | awk '{t+=$1} END {printf "%d", t}')
    printf '%-16s %-14s %2d runs  %d cycles each, %s ms\n' \
        "$(basename "$ACC")" "$name" "$RUNS" $((csum / RUNS)) "$(ms $((csum / RUNS)))"
    if [ "$name" = "hello -c" ]; then
        mos=$(mos_count "$out")
        printf '%-16s %-14s %2d runs  %d cycles each, %s ms, in every figure but taken off\n' \
            "$(basename "$ACC")" "(MOS)" 1 "$mos" "$(ms "$mos")"
    fi
}

if [ -n "$whole" ] && [ -z "${ACC_BENCH_SRC:-}" ] && [ "$ALL_INPUTS" = 1 ]; then
    printf '#include <stdio.h>\nint main(void) { printf("hello\\n"); return 0; }\n' \
        > "$sd/hello.c"
    program "hello -c" "" "acc -c hello.c -o h%s.o"
    program "hello link" "acc -c hello.c -o hello.o" "acc hello.o -o h%s.bin"
    program "hello" "" "acc hello.c -o h%s.bin"
    if [ -d "$tmp/zap/src" ]; then
        zobjs= n=0
        for f in "$tmp/zap/src"/*.c; do
            [ "$(basename "$f")" = zmalloc.c ] && continue
            n=$((n + 1))
            bin/acc -c "$f" -o "$sd/z$n.o" -Iinclude -I"$tmp/zap/src" -DAGONDEV \
                >/dev/null 2>&1 || { echo "zap: the host acc cannot compile $f" >&2
                                      status=1; zobjs=; break; }
            zobjs="$zobjs z$n.o"
        done
        [ -z "$zobjs" ] || program "zap link" "" "acc$zobjs -o zap%s.bin"
    fi
fi

if [ "$bytes_all" -gt 0 ]; then
    printf '%-16s %-14s %2d runs  %*s%d.%d cycles/byte\n' \
        "$(basename "$ACC")" "(all)" "$RUNS" 21 "" \
        $((cycles_all / (RUNS * bytes_all))) \
        $((cycles_all * 10 / (RUNS * bytes_all) % 10))
    [ -z "$estimated" ] || echo "note: from seconds rather than a count:$estimated" >&2
fi

exit $status

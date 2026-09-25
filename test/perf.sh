#!/bin/bash
# How fast the code acc generates runs, and how big it is, against agondev.
#
#   test/perf.sh [program ...]     # default: every test/perf/*.c
#
# test/bench.sh measures acc compiling; this measures what it compiled. Each
# program in test/perf does a piece of work -- sorting, checksums, a sieve,
# matrices, text, linked structures, an interpreter, 64-bit and floating
# arithmetic -- from a seed neither compiler can see through, and times
# only that work, by the emulator's own count of cycles: see
# test/perf/perf.h. Each is built three ways -- acc, and agondev at -Oz and
# at -O2 -- run, and has to print the same check line every time; a program
# whose builds disagree is reported, not timed. Unless it says in its opening
# comment what the answer is -- `expect:`, worked out apart from either
# compiler -- when each build is held to that: acc's has to give it, and an
# agondev build that does not is marked, and left out of the mean.
#
# The count needs fab-agon-emulator 2037657 or later, which prints it for a
# write to IO port 0x41 (see test/bench.sh); point ACC_EMU at one. It is
# stepped by the instructions run, so the emulator runs unthrottled.
#
# Sizes are of the whole image, library and startup included, which is
# what goes on the card; the startups differ by a few hundred bytes. The
# ratio columns are acc's over agondev -Oz's: lower is better, 1.00 is even.
set -u
cd "$(dirname "$0")/.."
. test/emu.sh

emu_available || exit 77
[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }
AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "no agondev at $AGONDEV" >&2; exit 2; }
CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc
        -isystem $AGONDEV/include -target ez80-none-elf -Wa,-march=ez80+full -w"

if [ $# -gt 0 ]; then
    PROGS="$*"
else
    PROGS=$(echo test/perf/*.c)
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export ASAN_OPTIONS=detect_leaks=0

for f in start shim; do
    "$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full \
        "test/conformance/refkit/$f.s" -o "$work/$f.o" || exit 2
done

# build <source> <acc|Oz|O2> <image>
build() {
    local src=$1 how=$2 out=$3

    if [ "$how" = acc ]; then
        bin/acc -c "$src" -o "$out.o" -Iinclude -Itest/perf >/dev/null 2>&1 &&
            bin/acc "$out.o" bin/libc.a -o "$out" >/dev/null 2>&1
    else
        $CC $CFLAGS -$how -Itest/perf -c "$src" -o "$out.o" 2>/dev/null &&
            "$AGONDEV/bin/ez80-none-elf-ld" --oformat binary -Ttext=0x40000 \
                -e _start --defsym __stack=0xB0000 --defsym ___heaptop=0xAC000 \
                --defsym ___heapbot=_end -o "$out" "$work/start.o" \
                "$work/shim.o" "$out.o" -L"$AGONDEV/lib" -lagon >/dev/null 2>&1
    fi
}

# Whether a run is over: its check line printed, which comes after the
# count. Not MOS's prompt, which agondev's build does not get back to --
# its startup leaves the machine to start again, and the autoexec with it.
perf_done() {
    grep -q '^check [0-9]' "$1"
}

# run <image>: prints `cycles check`, or nothing if it did not report both.
run() {
    local sd out cycles check

    sd=$(emu_card)
    cp "$1" "$sd/bin/p.bin"
    printf 'bin/p\r\n' > "$sd/autoexec.txt"
    out=$(ACC_EMU_WATCH=perf_done ACC_EMU_TIMEOUT=${PERF_TIMEOUT:-600} emu_run "$sd" -z -u 2>&1)
    rm -rf "$sd"
    cycles=$(printf '%s' "$out" | sed -n 's/.*Debug OUT(0x41): \([0-9]*\) CPU cycles.*/\1/p' | head -1)
    check=$(printf '%s' "$out" | tr -d '\r' | sed -n 's/^check \([0-9]*\).*/\1/p' | head -1)
    [ -n "$cycles" ] && [ -n "$check" ] && echo "$cycles $check"
}

printf '%-10s %12s %12s %12s %6s   %8s %8s %8s %6s\n' program \
    "acc cycles" "-Oz cycles" "-O2 cycles" ratio "acc size" "-Oz size" "-O2 size" ratio
status=0
logs=
for src in $PROGS; do
    name=$(basename "$src" .c)
    for how in acc Oz O2; do
        if ! build "$src" "$how" "$work/$name.$how.bin"; then
            echo "$name: the $how build failed" >&2
            status=1; continue 2
        fi
        r=$(run "$work/$name.$how.bin")
        if [ -z "$r" ]; then
            echo "$name: the $how build did not report its cycles and check" >&2
            status=1; continue 2
        fi
        eval "cyc_$how=${r% *} chk_$how=${r#* }"
        eval "size_$how=$(stat -c%s "$work/$name.$how.bin")"
    done
    # The answer a program says it has to give -- `expect:` in its opening
    # comment, where one has been worked out apart from either compiler --
    # or else the three builds have to agree.
    expect=$(sed -n 's/^ \* expect: \([0-9]*\).*/\1/p' "$src" | head -1)
    wrong=
    if [ -n "$expect" ]; then
        for how in acc Oz O2; do
            eval "[ \"\$chk_$how\" = \"$expect\" ]" || wrong="$wrong $how"
        done
    elif [ "$chk_acc" != "$chk_Oz" ] || [ "$chk_Oz" != "$chk_O2" ]; then
        echo "$name: the builds disagree -- acc $chk_acc, -Oz $chk_Oz, -O2 $chk_O2" >&2
        status=1; continue
    fi
    case " $wrong " in
      *" acc "*) echo "$name: acc's build gives $chk_acc, not $expect" >&2
                 status=1; continue ;;
    esac
    printf '%-10s %12d %12d %12d %6s   %8d %8d %8d %6s\n' "$name" \
        "$cyc_acc" "$cyc_Oz" "$cyc_O2" "$(awk -v a="$cyc_acc" -v b="$cyc_Oz" 'BEGIN { printf "%.2f", a / b }')" \
        "$size_acc" "$size_Oz" "$size_O2" "$(awk -v a="$size_acc" -v b="$size_Oz" 'BEGIN { printf "%.2f", a / b }')"
    # A build that gives the wrong answer did other work than the rest, and
    # its time is no measure: said so, and left out of the mean.
    if [ -n "$wrong" ]; then
        echo "           (wrong answer from agondev$(printf ' %s' $wrong | sed 's/ \([A-Z]\)/ -\1/g'):" \
             "$chk_Oz, not $expect -- left out of the mean)"
        continue
    fi
    logs="$logs $cyc_acc/$cyc_Oz/$size_acc/$size_Oz"
done

# The geometric mean of the ratios, so that no one program's size or length
# outweighs the others.
[ -n "$logs" ] && echo "$logs" | tr ' ' '\n' | awk -F/ 'NF == 4 {
        c += log($1 / $2); s += log($3 / $4); n++
    } END { if (n) printf "%-10s %38s %6.2f   %26s %6.2f\n", "(mean)", "", exp(c / n), "", exp(s / n) }'

exit $status

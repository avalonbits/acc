#!/bin/bash
# Random programs from Csmith, compiled by acc and by agondev and run on the
# Agon, which have to print the same checksum.
#
#   test/csmith.sh [count] [seed]      # default 40 programs, a random seed
#
# test/fuzz.sh's programs are narrow expressions acc's author thought to
# write. Csmith's are whole programs -- structs, unions, bit-fields,
# pointers to pointers, 64-bit arithmetic, loops, calls -- free of
# undefined behaviour by construction, which end by printing a CRC of
# every global. Two compilers that agree about C agree about the CRC.
#
# Csmith is not part of acc: build it (github.com/csmith-project/csmith)
# and say where, with CSMITH, its csmith binary, and CSMITH_RUNTIME, the
# directory holding csmith.h -- and the safe_math.h its build generates,
# if that is elsewhere, in CSMITH_RUNTIME_BUILD. Skips (77) without them.
#
# agondev's build is the reference, made with the conformance suite's
# strict C99 flags. A program it does not build, or that
# its build does not finish in CSMITH_HANG seconds, is skipped: Csmith's
# loops are bounded but not short, and the Agon is not fast. Programs that
# disagree are kept, and where is printed; test/fuzz/reduce.py cuts one
# down.
#
# Not part of `make test`, for the reason fuzz.sh is not: the point is to
# run many.
set -u
cd "$(dirname "$0")/.."

count=${1:-40}
seed=${2:-$RANDOM}
CSMITH=${CSMITH:-}
CSMITH_RUNTIME=${CSMITH_RUNTIME:-}
CSMITH_RUNTIME_BUILD=${CSMITH_RUNTIME_BUILD:-$CSMITH_RUNTIME}
CSMITH_HANG=${CSMITH_HANG:-60}

# Kept small, for an 18 MHz machine: a few functions, shallow blocks. No
# bit-fields, which Csmith makes as wide as a 32-bit int and so wider than
# an int is here, and no qualified pointers, which is where it compares
# pointers to types C99 does not let it: both only have agondev refuse the
# program.
CSMITH_OPTIONS=${CSMITH_OPTIONS:-"--no-argc --max-funcs 4 --max-block-depth 3
    --no-bitfields --no-volatile-pointers --no-const-pointers"}

if [ ! -x "$CSMITH" ] || [ ! -f "$CSMITH_RUNTIME/csmith.h" ]; then
    echo "  [no Csmith: set CSMITH and CSMITH_RUNTIME]"
    exit 77
fi
[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }
. test/emu.sh
. test/conformance/batch.sh
emu_available || exit 77

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "no agondev at $AGONDEV" >&2; exit 2; }
CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc
        -isystem $AGONDEV/include -target ez80-none-elf -Oz -Wa,-march=ez80+full"

# The conformance suite's filter, so that a program C99 forbids is skipped
# rather than blamed on acc: Csmith compares pointers to different integer
# types now and then -- `uint32_t *` with `int32_t *` -- which 6.5.9p2 does
# not allow, acc refuses, and clang only warns about.
STRICT=$(awk '$1 == "source" { on = ($2 == "gcc-torture") }
              on && $1 == "filter" { $1 = ""; print }' test/conformance/sources.txt)

work=$(mktemp -d)
keep=$(mktemp -d "${TMPDIR:-/tmp}/acc-csmith-$seed.XXXX")
trap 'rm -rf "$work"' EXIT
export ASAN_OPTIONS=detect_leaks=0

# Csmith's runtime, with this machine's way of printing the checksum.
mkdir "$work/rt"
cp "$CSMITH_RUNTIME"/*.h "$CSMITH_RUNTIME_BUILD"/*.h "$work/rt/" 2>/dev/null
cp test/csmith/platform_generic.h "$work/rt/"

# agondev's startup and exit, as the conformance suite's reference uses.
for f in start shim; do
    "$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full \
        "test/conformance/refkit/$f.s" -o "$work/$f.o" || exit 2
done

echo "seed $seed, $count programs"
: > "$work/acc.list"
: > "$work/ref.list"
bad=0
for i in $(seq "$count"); do
    n=c$i
    "$CSMITH" --seed $((seed * 1000 + i)) $CSMITH_OPTIONS -o "$work/$n.c" \
        >/dev/null 2>&1 || { echo "  csmith failed for $n" >&2; exit 2; }

    # C99 5.2.4.1 promises 4,095 characters in a logical line, and acc
    # reads a line whole, up to 64 KB; Csmith's expressions sometimes run
    # past the first.
    if awk 'length > 4095 { found = 1 } END { exit !found }' "$work/$n.c"; then
        echo "  SKIP $n  a line longer than C99's 4,095 characters"
        continue
    fi
    if ! err=$($CC $CFLAGS $STRICT -I"$work/rt" -c "$work/$n.c" -o "$work/$n.r.o" 2>&1) ||
       ! err=$("$AGONDEV/bin/ez80-none-elf-ld" --oformat binary -Ttext=0x40000 \
               -e _start --defsym __stack=0xB0000 --defsym ___heaptop=0xAC000 \
               --defsym ___heapbot=_end -o "$work/$n.r.bin" "$work/start.o" \
               "$work/shim.o" "$work/$n.r.o" -L"$AGONDEV/lib" -lagon 2>&1); then
        echo "  SKIP $n  agondev: $(printf '%s' "$err" | grep -m1 error)"
        continue
    fi
    if ! err=$(bin/acc -c "$work/$n.c" -o "$work/$n.o" -Iinclude -I"$work/rt" 2>&1) ||
       ! err=$(bin/acc "$work/$n.o" bin/libc.a -o "$work/$n.bin" -p 2>&1); then
        echo "  FAIL $n  acc could not build it: $(printf '%s' "$err" | grep -m1 error)"
        cp "$work/$n.c" "$keep/"; bad=$((bad + 1)); continue
    fi
    echo "$n $work/$n.r.bin" >> "$work/ref.list"
    echo "$n $work/$n.bin" >> "$work/acc.list"
done

mkdir "$work/ref" "$work/acc"
BATCH_HANG=$CSMITH_HANG batch_run "$work/ref.list" "$work/ref.ran" "$work/ref" || exit 2
BATCH_HANG=$CSMITH_HANG batch_run "$work/acc.list" "$work/acc.ran" "$work/acc" || exit 2

ran=0
while read -r n want; do
    got=$(awk -v n="$n" '$1 == n { print $2 }' "$work/acc.ran")
    if [ "$want" != 00 ]; then
        echo "  SKIP $n  agondev's build: $want"
        continue
    fi
    ran=$((ran + 1))
    if [ "$got" != 00 ] || ! cmp -s "$work/ref/$n.out" "$work/acc/$n.out"; then
        echo "  FAIL $n  acc: $got $(tail -1 "$work/acc/$n.out" 2>/dev/null)," \
             "agondev: $(tail -1 "$work/ref/$n.out")"
        cp "$work/$n.c" "$keep/"; bad=$((bad + 1))
    fi
done < "$work/ref.ran"

if [ "$bad" -eq 0 ]; then
    rmdir "$keep"
    echo "  $ran programs run, all agree"
    exit 0
fi
echo "  $bad disagree, of $ran run; kept in $keep"
exit 1

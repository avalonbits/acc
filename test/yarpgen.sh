#!/bin/bash
# Random programs from YARPGen, compiled by acc and by agondev and run on the
# Agon, which have to print the same hash.
#
#   test/yarpgen.sh [count] [seed]     # default 40 programs, a random seed
#
# YARPGen writes a test as two files: func.c, a function of generated
# statements over the program's variables and arrays, and driver.c, which
# gives them their values, calls it and prints a hash of what it left. It
# aims at the optimisers of large compilers -- loops over arrays, long
# expressions over every integer type -- where Csmith aims at the language,
# and it only writes C that is valid by its own lights.
#
# By its own lights: YARPGen's model of the types is a 32-bit int, and here
# an int has 24 bits. A value it puts in an int -- `int var_2 =
# -1862638497` -- is converted, which C defines; but arithmetic it judged
# free of overflow may not be free of it here, and then the two compilers
# owe each other nothing. So a disagreement is a lead to read, not a
# verdict: kept, with where, for test/fuzz/reduce.py to cut down.
#
# YARPGen's min and max are GNU statement expressions over __typeof__. Its
# expressions have no side effects, so each is given as `(a) < (b) ? (a) :
# (b)` instead, which is the same value at the same type -- a measuring
# device, like the conformance suite's chibicc.h, and not a change to what
# is tested. The hash is printed as two halves in hex, where the driver
# prints it with %llu: agondev's printf does not have %llu, and a line that
# ends in six hex digits is what test/conformance/batch.sh reads as the
# result, so the line ends in a full stop. Alignment attributes and pragmas
# are asked not to be written, and arrays kept to two dimensions, for 448 KB
# of memory.
#
# YARPGen is not part of acc: build it (github.com/intel/yarpgen) and say
# where, with YARPGEN. Skips (77) without it. agondev's build is the
# reference, made with the conformance suite's strict C99 flags; a program
# it does not build, or whose build does not finish in YARPGEN_HANG seconds,
# is skipped. Not part of `make test`: the point is to run many.
set -u
cd "$(dirname "$0")/.."

count=${1:-40}
seed=${2:-$RANDOM}
YARPGEN=${YARPGEN:-}
YARPGEN_HANG=${YARPGEN_HANG:-60}
YARPGEN_OPTIONS=${YARPGEN_OPTIONS:-"--std=c --emit-align-attr=none
    --emit-pragmas=none --max-array-dims=2"}

if [ ! -x "$YARPGEN" ]; then
    echo "  [no YARPGen: set YARPGEN]"
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
STRICT=$(awk '$1 == "source" { on = ($2 == "gcc-torture") }
              on && $1 == "filter" { $1 = ""; print }' test/conformance/sources.txt)

work=$(mktemp -d)
keep=$(mktemp -d "${TMPDIR:-/tmp}/acc-yarpgen-$seed.XXXX")
trap 'rm -rf "$work"' EXIT
export ASAN_OPTIONS=detect_leaks=0

for f in start shim; do
    "$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full \
        "test/conformance/refkit/$f.s" -o "$work/$f.o" || exit 2
done

# The two files a test is, built by one compiler: `ref` or `acc`.
build() {
    local d=$1 which=$2 f

    if [ "$which" = ref ]; then
        for f in driver func; do
            $CC $CFLAGS $STRICT -c "$d/$f.c" -o "$d/$f.r.o" 2>&1 || return 1
        done
        "$AGONDEV/bin/ez80-none-elf-ld" --oformat binary -Ttext=0x40000 \
            -e _start --defsym __stack=0xB0000 --defsym ___heaptop=0xAC000 \
            --defsym ___heapbot=_end -o "$d/r.bin" "$work/start.o" \
            "$work/shim.o" "$d/driver.r.o" "$d/func.r.o" \
            -L"$AGONDEV/lib" -lagon 2>&1
    else
        for f in driver func; do
            bin/acc -c "$d/$f.c" -o "$d/$f.o" -Iinclude 2>&1 || return 1
        done
        bin/acc "$d/driver.o" "$d/func.o" bin/libc.a -o "$d/a.bin" -p 2>&1
    fi
}

# Where a test is kept when it disagrees: the three files it is made of.
keep_it() {
    mkdir -p "$keep/$1"
    cp "$work/$1"/driver.c "$work/$1"/func.c "$work/$1"/init.h "$keep/$1/"
}

echo "seed $seed, $count programs"
: > "$work/acc.list"
: > "$work/ref.list"
bad=0
for i in $(seq "$count"); do
    n=y$i
    mkdir "$work/$n"
    "$YARPGEN" -s $((seed * 1000 + i)) $YARPGEN_OPTIONS -o "$work/$n" \
        >/dev/null 2>&1 || { echo "  yarpgen failed for $n" >&2; exit 2; }
    python3 - "$work/$n/func.c" "$work/$n/driver.c" <<'PY'
import re, sys
p, d = sys.argv[1], sys.argv[2]
s = open(p).read()
for name, op in (('max', '>'), ('min', '<')):
    s = re.sub(r'#define %s\(a,b\) \\\n(?:.*\\\n)*.*\n' % name,
               '#define %s(a,b) ((a) %s (b) ? (a) : (b))\n' % (name, op), s)
open(p, 'w').write(s)
s = open(d).read()
s = s.replace('printf("%llu\\n", seed);',
              'printf("hash %lx %lx.\\n", (unsigned long) (seed >> 32), '
              '(unsigned long) seed);')
open(d, 'w').write(s)
PY

    # C99 5.2.4.1 promises 4,095 characters in a logical line.
    if awk 'length > 4095 { found = 1 } END { exit !found }' \
           "$work/$n"/driver.c "$work/$n"/func.c; then
        echo "  SKIP $n  a line longer than C99's 4,095 characters"
        continue
    fi
    if ! err=$(build "$work/$n" ref); then
        echo "  SKIP $n  agondev: $(printf '%s' "$err" | grep -m1 -i error)"
        continue
    fi
    if ! err=$(build "$work/$n" acc); then
        echo "  FAIL $n  acc could not build it: $(printf '%s' "$err" | grep -m1 error)"
        keep_it "$n"; bad=$((bad + 1)); continue
    fi
    echo "$n $work/$n/r.bin" >> "$work/ref.list"
    echo "$n $work/$n/a.bin" >> "$work/acc.list"
done

mkdir "$work/ref" "$work/acc"
BATCH_HANG=$YARPGEN_HANG batch_run "$work/ref.list" "$work/ref.ran" "$work/ref" || exit 2
BATCH_HANG=$YARPGEN_HANG batch_run "$work/acc.list" "$work/acc.ran" "$work/acc" || exit 2

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
        keep_it "$n"; bad=$((bad + 1))
    fi
done < "$work/ref.ran"

if [ "$bad" -eq 0 ]; then
    rmdir "$keep"
    echo "  $ran programs run, all agree"
    exit 0
fi
echo "  $bad disagree, of $ran run; kept in $keep"
exit 1

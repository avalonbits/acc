#!/bin/bash
# What a compile costs as its input grows: a source four times the size takes
# four times the work, never sixteen.
#
# Each shape is compiled at n and 4n and the work compared. perf counts it
# in instructions, which say the same thing every run; the bound is 4.6
# times, where a step quadratic in the input makes it sixteen and one only
# partly so lands in between. Without perf the time is compared instead,
# with twice four the bound, since a clock says less.
#
# The shapes are what the symbol table answers for: a block of locals each
# read where the first one was declared, so that a lookup walking back over
# the block would walk all of it; the same in blocks nested in it, each a
# scope that ends; and a name declared again in each, which asks whether
# the block has it already.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

shape() {
    python3 -c "
import sys
shape, n = sys.argv[1], int(sys.argv[2])
if shape == 'locals':
    print('int f(int a) { int x0 = a;')
    for i in range(1, n): print(' int x%d = x0 + x%d;' % (i, i - 1))
    print(' return x%d; }' % (n - 1))
elif shape == 'blocks':
    print('int f(int a) { int x0 = a;')
    for i in range(1, n): print(' int x%d = x0 + %d; { int y = x%d + x0; a += y; }' % (i, i, i))
    print(' return a; }')
" "$1" "$2"
}

counting=no
perf stat -x, -e instructions:u true 2>&1 | grep -q instructions && counting=yes

cost() {
    local count= start

    [ "$counting" = yes ] && count="perf stat -x, -o $tmp/count -e instructions:u"
    rm -f "$tmp/s.o"
    start=$(date +%s%N)
    timeout 120 $count "$ACC" -c "$1" -o "$tmp/s.o" >/dev/null 2>&1
    if [ "$counting" = yes ]; then
        grep instructions "$tmp/count" | cut -d, -f1
    else
        echo $(( ($(date +%s%N) - start) / 1000000 ))
    fi
}

# scales <shape> <n>
scales() {
    local small=$2 bound=46 small_cost big_cost

    shape "$1" "$small" > "$tmp/s1.c"
    shape "$1" $((4 * small)) > "$tmp/s4.c"
    small_cost=$(cost "$tmp/s1.c")
    big_cost=$(cost "$tmp/s4.c")
    [ "$counting" = yes ] || bound=80
    if [ -f "$tmp/s.o" ] && [ $((10 * big_cost)) -le $((bound * small_cost)) ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-38s %s, then %s\n' "$1 of $small, four times it" \
            "$small_cost" "$big_cost"
        fail=$((fail + 1))
    fi
}

scales locals 2000
scales blocks 1500

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

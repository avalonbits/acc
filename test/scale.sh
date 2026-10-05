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
#
# And what was once walked for each of many things: a switch's cases,
# each checked against those before it; a function's returns, each a jump
# to the epilogue past every jump shortened on the way; a jump past a jump,
# each asking whether anything jumped to the one jumped over; variables
# left at zero, each looked up among all of them for every export and
# every relocation the link makes; and the items of a library's member
# reached one from another, each found among every relocation.
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
elif shape == 'cases':
    print('int f(int x) { int a = 0; switch (x) {')
    for i in range(n): print(' case %d: a += %d;' % (i, i))
    print(' } return a; }')
elif shape == 'returns':
    print('int f(int x) {')
    for i in range(n): print(' if (x == %d) return %d;' % (i, i + 7))
    print(' return 0; }')
elif shape == 'over':
    print('int f(int x) { int a = 0; while (x--) {')
    for i in range(n): print(' if (a == %d) continue; a += x;' % i)
    print(' } return a; }')
elif shape == 'zeros':
    for i in range(n): print('int g%d;' % i)
    print('int main(void) { int s = 0;')
    for i in range(n): print(' s += g%d;' % i)
    print(' return s; }')
elif shape == 'chain':
    for i in range(n): print('static int c%d(int x) { return %s; }' % (i, 'x' if i == 0 else 'c%d(x + 1)' % (i - 1)))
    print('int chain(int x) { return c%d(x); }' % (n - 1))
" "$1" "$2"
}

counting=no
perf stat -x, -e instructions:u true 2>&1 | grep -q instructions && counting=yes

# cost <source>: of compiling it to an object -- or, with LINKING, of
# linking that object as a program, the link alone counted; with MEMBER, of
# linking a program that calls `chain` from a library made of it.
cost() {
    local count= start

    [ "$counting" = yes ] && count="perf stat -x, -o $tmp/count -e instructions:u"
    rm -f "$tmp/s.o" "$tmp/m.o" "$tmp/m.a" "$tmp/s.bin"
    if [ -n "${MEMBER:-}" ]; then
        "$ACC" -c "$1" -o "$tmp/m.o" >/dev/null 2>&1
        "$ACC" -a "$tmp/m.a" "$tmp/m.o" >/dev/null 2>&1
        printf 'int chain(int);\nint main(void) { return chain(1); }\n' > "$tmp/main.c"
        "$ACC" -c "$tmp/main.c" -o "$tmp/main.o" >/dev/null 2>&1
    elif [ -n "${LINKING:-}" ]; then
        "$ACC" -c "$1" -o "$tmp/m.o" >/dev/null 2>&1
    fi
    start=$(date +%s%N)
    if [ -n "${MEMBER:-}" ]; then
        timeout 120 $count "$ACC" "$tmp/main.o" "$tmp/m.a" -o "$tmp/s.bin" >/dev/null 2>&1
        cp "$tmp/s.bin" "$tmp/s.o" 2>/dev/null
    elif [ -n "${LINKING:-}" ]; then
        timeout 120 $count "$ACC" "$tmp/m.o" -o "$tmp/s.bin" >/dev/null 2>&1
        cp "$tmp/s.bin" "$tmp/s.o" 2>/dev/null
    else
        timeout 120 $count "$ACC" -c "$1" -o "$tmp/s.o" >/dev/null 2>&1
    fi
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
        printf '  FAIL %-38s %s, then %s\n' "$1${LINKING:+, linked} of $small, four times it" \
            "$small_cost" "$big_cost"
        fail=$((fail + 1))
    fi
}

scales locals 2000
scales blocks 1500
scales cases 1000
scales returns 1000
scales over 1000
scales zeros 1500
LINKING=1 scales zeros 1500
MEMBER=1 scales chain 600

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

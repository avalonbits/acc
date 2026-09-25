#!/bin/bash
# What acc writes out rather than calls for.
#
# Some operators have no instruction on this chip, and acc calls a runtime
# helper for them -- but a constant operand often makes a few instructions
# enough, and those are faster than the call and seldom bigger. Whether the
# instructions or the call came out does not show in what a program
# computes: test/cases holds that, against agondev. So it is checked here,
# in the object: an object names every helper it calls, as a symbol it
# wants, so one that does not name the helper did not call it.
#
#   test/codegen.sh
set -u
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# calls <name> <helper> <yes|no> <source>: whether the object wants it.
calls() {
    local what=$1 helper=$2 want=$3 got

    printf '%s\n' "$4" > "$tmp/c.c"
    if ! "$ACC" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1; then
        printf '  FAIL %-40s acc could not compile it\n' "$what"
        fail=$((fail + 1)); return
    fi
    got=no
    grep -aq "$helper" "$tmp/c.o" && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-40s calls %s: want %s, got %s\n' "$what" "$helper" "$want" "$got"
        fail=$((fail + 1))
    fi
}

# & with a constant: a mask of the low byte, the middle byte, both, or
# the middle byte with nothing above it -- `& 0x8000`, `& 0xffff`, which
# crc16 tests eight times a byte -- is written out. One that reaches into
# the top byte, and one with no constant, still calls.
calls "& 0x0f"                      _acc_rt_and no \
    'unsigned f(unsigned x) { return x & 0x0f; }'
calls "& 0x8000"                    _acc_rt_and no \
    'unsigned f(unsigned x) { return x & 0x8000; }'
calls "& 0xffff"                    _acc_rt_and no \
    'unsigned f(unsigned x) { return x & 0xffff; }'
calls "& 0x0ff0"                    _acc_rt_and no \
    'unsigned f(unsigned x) { return x & 0x0ff0; }'
calls "& 0x7fffff, into the top byte" _acc_rt_and yes \
    'unsigned f(unsigned x) { return x & 0x7fffff; }'
calls "& a variable"                _acc_rt_and yes \
    'unsigned f(unsigned x, unsigned y) { return x & y; }'

# << by a constant of up to eight: add hl, hl a bit at a time, a byte each,
# which is no bigger than loading the count and calling. Past eight, and
# by a variable, the helper.
calls "<< 1"                        _acc_rt_shl no \
    'unsigned f(unsigned x) { return x << 1; }'
calls "<< 8"                        _acc_rt_shl no \
    'int f(int x) { return x << 8; }'
calls "<< 9"                        _acc_rt_shl yes \
    'unsigned f(unsigned x) { return x << 9; }'
calls "<< a variable"               _acc_rt_shl yes \
    'unsigned f(unsigned x, int n) { return x << n; }'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

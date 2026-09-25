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

# * by a constant: doublings and additions, up to twelve of them, and the
# helper past that. 13 is zap's; 1030 is 0x406, ten doublings and two
# additions, exactly twelve, so a count of its bits one too many calls.
calls "* 13"                        _acc_rt_mul no \
    'unsigned f(unsigned x) { return x * 13; }'
calls "* 1030, twelve steps"        _acc_rt_mul no \
    'unsigned f(unsigned x) { return x * 1030; }'
calls "* 0x5555, too many steps"    _acc_rt_mul yes \
    'unsigned f(unsigned x) { return x * 0x5555; }'
calls "* a variable"                _acc_rt_mul yes \
    'unsigned f(unsigned x, unsigned y) { return x * y; }'

# emits <name> <hex> <yes|no> <source>: whether the object's bytes hold
# that sequence, given as hex.
emits() {
    local what=$1 want=$3 got

    printf '%s\n' "$4" > "$tmp/c.c"
    if ! "$ACC" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1; then
        printf '  FAIL %-40s acc could not compile it\n' "$what"
        fail=$((fail + 1)); return
    fi
    got=no
    od -An -v -tx1 "$tmp/c.o" | tr -d ' \n' | grep -q "$2" && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-40s emits %s: want %s, got %s\n' "$what" "$2" "$want" "$got"
        fail=$((fail + 1))
    fi
}

# A branch on an AND that keeps one byte jumps on the flags the AND left,
# rather than rebuilding the value and testing it against zero with
# ld bc, 0; or a; sbc hl, bc. A mask of two bytes still tests.
zero_test=01000000b7ed42
emits "if (x & 0x8000)"                 "$zero_test" no \
    'int f(unsigned x) { if (x & 0x8000) return 1; return 2; }'
emits "if (x & 0xff00)"                 "$zero_test" no \
    'int f(unsigned x) { if (x & 0xff00) return 1; return 2; }'
emits "if (x & 0x0ff0), two bytes"      "$zero_test" yes \
    'int f(unsigned x) { if (x & 0x0ff0) return 1; return 2; }'
emits "if (x & 0x8000) with the value kept" "$zero_test" yes \
    'int f(unsigned x) { int y; if (y = x & 0x8000) return y; return 2; }'

# And `!` straight after such an AND, or after a comparison, reads the same
# flags the other way round, rather than comparing the value with zero with
# ld de, 0; or a; sbc hl, de.
not_test=11000000b7ed52
emits "!(x & 0x40)"                     "$not_test" no \
    'int f(unsigned x) { return !(x & 0x40); }'
emits "while (!(x & 0x40))"             "$not_test" no \
    'int f(unsigned x) { while (!(x & 0x40)) x++; return x; }'
emits "!(a < b)"                        "$not_test" no \
    'int f(int a, int b) { if (!(a < b)) return 1; return 2; }'
emits "!x, of a variable"               "$not_test" yes \
    'int f(int x) { return !x; }'

# An assignment to a narrow local stores the low bytes, which converting to
# its type does not change, and converts after the store only for a value
# that is used: as a statement, or a comma's left side, it is not.
short_narrow=e5fde1                     # push hl; pop iy
char_narrow=7db7ed626f                  # ld a, l; or a; sbc hl, hl; ld l, a
emits "c = x + 1, unsigned short"       "$short_narrow" no \
    'int f(unsigned x) { unsigned short c; c = x + 1; return c; }'
emits "for (...; c = x + 1)"            "$short_narrow" no \
    'int f(unsigned x) { unsigned short c = 0; for (; x < 9; c = x + 1) x++; return c; }'
emits "c = x + 1, x at the comma"       "$short_narrow" no \
    'int f(unsigned x) { unsigned short c; return c = x + 1, c; }'
emits "unsigned short c = x + 1"        "$short_narrow" no \
    'int f(unsigned x) { unsigned short c = x + 1; return c; }'
emits "b = x + 1, unsigned char"        "$char_narrow" no \
    'int f(unsigned x) { unsigned char b; b = x + 1; return b; }'
emits "return c = x + 1, value used"    "$short_narrow" yes \
    'int f(unsigned x) { unsigned short c; return c = x + 1; }'
emits "return b = x + 1, value used"    "$char_narrow" yes \
    'int f(unsigned x) { unsigned char b; return b = x + 1; }'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

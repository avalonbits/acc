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

# &, | and ^ of values known to fit in a byte or two -- unsigned chars and
# shorts, as C promotes them -- are written out a byte at a time. An AND
# needs only one side narrow; the others need both. A signed char's sign
# fills the upper bytes, so it is not narrow.
calls "uchar ^ uchar"               _acc_rt_xor no \
    'int f(unsigned char a, unsigned char b) { return a ^ b; }'
calls "int & uchar"                 _acc_rt_and no \
    'int f(int a, unsigned char b) { return a & b; }'
calls "ushort | uchar"              _acc_rt_or no \
    'int f(unsigned short a, unsigned char b) { return a | b; }'
calls "*p & mask, bytes read"       _acc_rt_and no \
    'int f(const unsigned char *p, unsigned char m) { return (p[0] & m) | (p[1] & m); }'
calls "int | uchar"                 _acc_rt_or yes \
    'int f(int a, unsigned char b) { return a | b; }'
calls "schar | uchar"               _acc_rt_or yes \
    'int f(signed char a, unsigned char b) { return a | b; }'

# A constant on the left of a commutative operator is taken as the right
# one, so `3 * x` is the doublings and additions that `x * 3` is.
calls "3 * x"                       _acc_rt_mul no \
    'unsigned f(unsigned x) { return 3 * x; }'

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

# `&&` and `||` in a branch jump from each side rather than making a one
# or a zero -- ld hl, 1 and a jump over the zero -- and testing that. As a
# value they still make it.
logic_value='21010000\(18\|c3\)'
emits "if (a && b)"                     "$logic_value" no \
    'int f(int a, int b) { if (a && b) return 3; return 4; }'
emits "while (p < e && *p)"             "$logic_value" no \
    'char *f(char *p, char *e) { while (p < e && *p) p++; return p; }'
emits "if (a < b || b < 0), nested"     "$logic_value" no \
    'int f(int a, int b) { if ((a < b || b < 0) && a) return 3; return 4; }'
emits "return a && b, a value"          "$logic_value" yes \
    'int f(int a, int b) { return a && b; }'

# `(x & m) != 0` reads the AND's flags rather than comparing with zero.
emits "(c & 1) != 0"                    "$not_test" no \
    'int f(unsigned c) { return (c & 1) != 0; }'
emits "(c & 0x80) == 0"                 "$not_test" no \
    'int f(unsigned c) { return (c & 0x80) == 0; }'

# A narrow value converted to a type as wide is widened once, the way that
# type wants: a char read as unsigned char loads zero-filled, with no sign
# fill -- rlc l; sbc hl, hl -- first.
sign_fill=cb05ed62
emits "(unsigned char) of a char local" "$sign_fill" no \
    'unsigned f(char c) { return (unsigned char) c; }'
emits "(unsigned char) *p of a char"    "$sign_fill" no \
    'unsigned f(char *p) { return (unsigned char) *p; }'
emits "a char, as it is"                "$sign_fill" yes \
    'int f(char *p) { return *p; }'

# And a byte read or returned, branched on, is tested in A.
emits "if (b()), a _Bool"               "$zero_test" no \
    '_Bool b(void); int f(void) { if (b()) return 1; return 2; }'
emits "while (*p)"                      "$zero_test" no \
    'char *f(char *p) { while (*p) p++; return p; }'

# A call to a static inline function whose body is one return is compiled
# in place. The function is the object's first, at offset zero, so a call
# to it is call 0.
call_zero=cd000000
emits "static inline, one return"       "$call_zero" no \
    'static inline int twice(int x) { return x + x; } int f(int y) { return twice(y) + 1; }'
emits "static, not inline"              "$call_zero" yes \
    'static int twice(int x) { return x + x; } int f(int y) { return twice(y) + 1; }'
emits "a body of two statements"        "$call_zero" yes \
    'static inline int twice(int x) { x++; return x + x; } int f(int y) { return twice(y); }'
emits "a name the caller shadows"       "$call_zero" yes \
    'int k; static inline int addk(int x) { return x + k; } int f(int y) { int k = 2; return addk(y) + k; }'

# An unsigned narrow local goes straight into DE or BC, rather than
# through HL and the stack (push hl; pop de; pop hl) when HL is holding
# something: the table's address, in `t[c]`.
emits "t[c], c into DE directly"        e5d1e1 no \
    'extern const unsigned char t[256]; int f(unsigned char c) { return t[c]; }'

# A function with no frame to make has no ld hl, 0; add hl, sp; ld sp, hl.
no_frame=2100000039f9
emits "no locals, no frame set up"      "$no_frame" no \
    'int f(int x) { return x + 1; }'
emits "an array, a frame"               39f9 yes \
    'int f(int x) { int a[4]; a[x & 3] = x; return a[0]; }'

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

# An index worked out in HL, after the table's address went to DE, is
# added the other way round -- add hl, de -- rather than moved to BC
# through the stack: push hl; pop bc; ex de, hl.
emits "t[(unsigned char)*p]"            e5c1eb no \
    'extern const unsigned char t[256]; int f(const char *p) { return t[(unsigned char)*p]; }'

# A byte just loaded into A is masked there, not first moved back from L.
emits "t[c] & 1, masked in A"           7de601 no \
    'extern const unsigned char t[256]; int f(unsigned char c) { return t[c] & 1; }'

# A byte read into a char local is stored from A as it came, with no sign
# fill of a value nothing reads.
emits "c = *p, a char local"            "$sign_fill" no \
    'void h(char *); void f(char *p) { char c; c = *p; h(&c); }'

# x++ as a statement leaves no step back to the old value -- dec hl --
# after the store.
emits "x++; as a statement"             dd2f062b no \
    'int f(int x) { x++; return x; }'

# A local just stored from HL is not loaded back into HL.
emits "x = a + 1; g(x), no reload"      dd2ffddd27fd no \
    'int g(int); int f(int a) { int x; x = a + 1; return g(x); }'

# A char local read as unsigned char loads zero-filled into whichever
# register it is wanted in -- DE, beside a table's address in HL -- rather
# than into HL, with the table's address swapped out of the way first:
# ex de, hl; or a; sbc hl, hl.
emits "t[(unsigned char)c], c a local"  ebb7ed62 no \
    'extern const unsigned char t[256]; int f(char *p) { char c = *p; return t[(unsigned char)c] + c; }'

# A byte compared with a constant it could equal is compared in A: cp 10,
# not widened into HL and 10 subtracted from it. A signed one ordered has
# its top bit flipped first, xor 0x80, and the constant with it.
emits "*p == '\\n'"                     fe0a yes \
    'int f(const char *p) { return *p == 10; }'
emits "c == 10, c a char local"         fe0a yes \
    'int g(void); int f(void) { char c = g(); return c == 10; }'
emits "c < 10, signed, flipped"         ee80fe8a yes \
    'int g(void); int f(void) { signed char c = g(); if (c < 10) return 1; return 2; }'
emits "10 < *q, the constant left"      fe0b yes \
    'int f(const unsigned char *q) { return 10 < *q; }'
emits "*q == 300, out of its range"     fe2c no \
    'int f(const unsigned char *q) { return *q == 300; }'

# Two sides that cannot be negative -- unsigned chars, which promote to
# int -- are compared unsigned, which gives the same answer without the
# overflow test a signed comparison needs: ld hl, 1; jp pe.
signed_cmp=21010000ea
emits "uchar < uchar, unsigned"         "$signed_cmp" no \
    'int f(unsigned char a, unsigned char b) { return a < b; }'
emits "uchar < int, signed"             "$signed_cmp" yes \
    'int f(unsigned char a, int b) { return a < b; }'

# A constant stored through a pointer is written through the address in
# HL -- ld (hl), n, or ld de, n; ld (hl), de -- not made in HL and moved.
emits "*p = 0, a char"                  dd27063600 yes \
    'void f(char *p) { *p = 0; }'
emits "*q = 0, an int"                  11000000ed1f yes \
    'void f(int *q) { *q = 0; }'
emits "*r = 0x1234, a short"            3634233612 yes \
    'void f(short *r) { *r = 0x1234; }'

# And a three-byte value from DE through HL, not between two exchanges.
emits "*q = a + 1"                      ebed1feb no \
    'void f(int *q, int a) { *q = a + 1; }'
emits "*q = a + 1, from DE"             dd2706ed1f yes \
    'void f(int *q, int a) { *q = a + 1; }'
emits "**pp = *q"                       ebed1feb no \
    'void f(int **pp, int *q) { **pp = *q; }'
emits "**pp = *q, no BC through the stack" e5c1 no \
    'void f(int **pp, int *q) { **pp = *q; }'

# Every return goes to the one epilogue -- ld sp, ix; pop ix; ret -- at
# the end, and one that is the last thing in the function falls into it,
# with no jr +0. A function whose frame is empty has the room its
# prologue kept for setting one up taken out: no jr +4; nop; nop.
emits "three returns, one epilogue"     'ddf9dde1c9.*ddf9dde1c9' no \
    'int f(int x) { if (x < 0) return 1; if (x == 5) return 2; return 3; }'
emits "return x; last, no jr +0"        1800ddf9 no \
    'int f(int x) { if (x) x++; return x; }'
emits "no frame, no room kept for one"  18040000 no \
    'void g(int *p) { *p = 1; }'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

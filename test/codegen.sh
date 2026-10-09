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

# >> by 16 to 23 is the top byte, reached through the stack; below 16 it is
# still the helper's, and so is a variable count.
calls ">> 16 unsigned"                _acc_rt_shru no \
    'unsigned f(unsigned x) { return x >> 16; }'
calls ">> 23 unsigned"                _acc_rt_shru no \
    'unsigned f(unsigned x) { return x >> 23; }'
calls ">> 16 signed"                  _acc_rt_shrs no \
    'int f(int x) { return x >> 16; }'
calls ">> 20 signed"                  _acc_rt_shrs no \
    'int f(int x) { return x >> 20; }'
calls ">> 15"                         _acc_rt_shru yes \
    'unsigned f(unsigned x) { return x >> 15; }'
calls ">> a variable"                 _acc_rt_shrs yes \
    'int f(int x, int n) { return x >> n; }'

# >> by 1 to 8, and / by 2 to 256, are the runtime's shr and sdiv,
# a call with no count to load and no loop. An unsigned % by a power of two
# is a mask, and an unsigned / by 2^16 to 2^23 the top byte's shift.
calls ">> 2 signed"                   _acc_rt_shrs no \
    'int f(int x) { return x >> 2; }'
calls ">> 2 signed, shr"              _acc_rt_shr yes \
    'int f(int x) { return x >> 2; }'
calls ">> 8 unsigned"                 _acc_rt_shru no \
    'unsigned f(unsigned x) { return x >> 8; }'
calls ">> 9"                          _acc_rt_shru yes \
    'unsigned f(unsigned x) { return x >> 9; }'
calls "/ 4 signed"                    _acc_rt_divs no \
    'int f(int x) { return x / 4; }'
calls "/ 4 signed, sdiv"              _acc_rt_sdiv yes \
    'int f(int x) { return x / 4; }'
calls "/ 256 unsigned"                _acc_rt_divu no \
    'unsigned f(unsigned x) { return x / 256; }'
calls "/ 65536 unsigned"              _acc_rt_divu no \
    'unsigned f(unsigned x) { return x / 65536; }'
calls "/ 512 signed"                  _acc_rt_divs yes \
    'int f(int x) { return x / 512; }'
calls "/ 3"                           _acc_rt_divs yes \
    'int f(int x) { return x / 3; }'
calls "% 8 unsigned"                  _acc_rt_remu no \
    'unsigned f(unsigned x) { return x % 8; }'
calls "% 8 signed"                    _acc_rt_rems yes \
    'int f(int x) { return x % 8; }'

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

# An object's text, as hex: its code and nothing else. The header and the
# tables around it carry the compiler's build id, a checksum of its own
# source, which changes with every edit to it -- and a short pattern found
# there once read as code acc had emitted. See src/obj.c for the layout.
text_hex() {
    python3 - "$1" <<'PY2'
import sys
d = open(sys.argv[1], 'rb').read()
n3 = lambda at: d[at] | d[at + 1] << 8 | d[at + 2] << 16
at = 31 + n3(13) * 7 + n3(16) * 6 + n3(28) * 9 + n3(19) * 12 + n3(25) * 3 + n3(22)
sys.stdout.write(d[at:at + n3(7)].hex())
PY2
}

# emits <name> <hex> <yes|no> <source>: whether the object's code holds
# that sequence, given as hex.
emits() {
    local what=$1 want=$3 got

    printf '%s\n' "$4" > "$tmp/c.c"
    if ! "$ACC" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1; then
        printf '  FAIL %-40s acc could not compile it\n' "$what"
        fail=$((fail + 1)); return
    fi
    got=no
    text_hex "$tmp/c.o" | grep -q "$2" && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-40s emits %s: want %s, got %s\n' "$what" "$2" "$want" "$got"
        fail=$((fail + 1))
    fi
}

# A branch on an AND that keeps one byte jumps on the flags the AND left,
# rather than rebuilding the value and testing it against zero with
# add hl, bc; or a; sbc hl, bc. A mask of two bytes still tests.
zero_test=09b7ed42
emits "if (x & 0x8000)"                 "$zero_test" no \
    'int f(unsigned x) { if (x & 0x8000) return 1; return 2; }'
emits "if (x & 0xff00)"                 "$zero_test" no \
    'int f(unsigned x) { if (x & 0xff00) return 1; return 2; }'
emits "if (x & 0x0ff0), two bytes"      "$zero_test" yes \
    'int f(unsigned x) { if (x & 0x0ff0) return 1; return 2; }'
emits "if (x & 0x8000) with the value kept" "$zero_test" yes \
    'int f(unsigned x) { int y; if (y = x & 0x8000) return y; return 2; }'

# A constant condition is no test at all: `while (1)` falls into its body,
# `do ... while (0)` falls out, and `if (0)` jumps over. Each was ld hl, n
# and the add hl, bc; or a; sbc hl, bc of any other value.
emits "do ... while (0)"                "2100000009b7ed42" no \
    'int f(int x) { do x += 2; while (0); return x; }'
emits "while (1)"                       "2101000009b7ed42" no \
    'int f(int x) { while (1) { if (x > 9) break; x++; } return x; }'
emits "if (0)"                          "2100000009b7ed42" no \
    'int f(int x) { if (0) x++; return x; }'

# A switch on a char compares the byte in A, cp n / jr z, rather than the
# int it becomes in HL with ld de, n / or a / sbc hl, de / add hl, de, and
# has no test for a case the char cannot equal.
emits "switch (char), cp 'a'"           "fe61" yes \
    'int f(char c) { switch (c) { case 97: return 1; case 300: return 2; } return 0; }'
emits "switch (char), no ld de, 'a'"    "11610000" no \
    'int f(char c) { switch (c) { case 97: return 1; case 300: return 2; } return 0; }'
emits "switch (char), no test of 300"   "fe2c" no \
    'int f(char c) { switch (c) { case 97: return 1; case 300: return 2; } return 0; }'
# And a char just read through a pointer, which reaches the switch widened
# to an int already: switch (*p++), a lexer's every switch.
emits "switch (*p++), cp 'a'"           "fe61" yes \
    'int f(const char *p) { switch (*p++) { case 97: return 1; case 98: return *p; } return 0; }'
emits "switch (*p++), no ld de, 'a'"    "11610000" no \
    'int f(const char *p) { switch (*p++) { case 97: return 1; case 98: return *p; } return 0; }'

# And `!` straight after such an AND, or after a comparison, reads the same
# flags the other way round, rather than comparing the value with zero with
# the same add hl, bc; or a; sbc hl, bc.
not_test=09b7ed42
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
# to it is call 0 -- after its argument's push, which tells it from the
# call every function's prologue makes, which is to 0 in an object too.
call_zero=e5cd000000
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

# A function with no frame to make makes no room below IX; one with a
# frame makes it, lea hl, ix-frame / ld sp, hl.
emits "no locals, no frame set up"      'ed22..f9' no \
    'int f(int x) { return x + 1; }'
emits "an array, a frame"               'ed22..f9' yes \
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
# repair a signed comparison makes: add hl, hl / jp po, its sign put in
# the carry and turned over on an overflow.
signed_cmp=29e2
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
emits "**pp = *q, no BC through the stack" e5c1ebed0f no \
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

# A constant added to an address the link fills in goes into the slot it
# fills: arr[3] of an extern int array is ld hl, arr+9, not ld de, 9 and
# an add after it.
emits "arr[3] of an extern"             1109000019 no \
    'extern int arr[10]; int f(void) { return arr[3]; }'
emits "g.b of an extern struct"         232323 no \
    'extern struct st { int a, b; } g; int f(void) { return g.b; }'
emits "g.b: the slot says 3"            2a030000 yes \
    'extern struct st { int a, b; } g; int f(void) { return g.b; }'

# A long moves between near frame slots three bytes at a time through IY
# -- ld iy, (ix+d); ld (ix+d), iy -- not a byte at a time through A, and a
# long constant is ld iy, nn and a store.
emits "long x = a, a copy through IY"   dd3106dd3e yes \
    'long f(long a) { long x = a; return x; }'
emits "long x = a, no byte through A"   dd7e06dd77 no \
    'long f(long a) { long x = a; return x; }'
emits "long y = 100000, through IY"     fd21a08601dd3e yes \
    'long f(void) { long y = 100000L; return y; }'

# A test of a whole 24-bit value against zero adds BC and takes it away
# again, which leaves HL as it was and the flags of HL -- not a zero
# loaded into a register to subtract: ld bc, 0 or ld de, 0.
emits "if (p), a pointer"               01000000b7ed42 no \
    'int f(char *p) { if (p) return 1; return 2; }'
emits "p == 0, as a value"              11000000b7ed52 no \
    'int f(char *p) { return p == 0; }'
emits "p != 0, as a value, tested"      09b7ed42 yes \
    'int f(char *p) { return p != 0; }'

# A constant returned a second time jumps back to where the first return
# loaded it: ld hl, 0; ld a, l comes once in a _Bool function that
# returns 0 three times.
emits "return 0 three times, one load"  '210000007d.*210000007d' no \
    '_Bool f(int x) { if (x == 1) return 0; if (x == 2) return 1; if (x == 5) return 0; if (x > 9) return 1; return 0; }'

# The prologue is written out, not a call into the runtime: push ix; ld
# ix, 0; add ix, sp, and the room below made by lea hl, ix-frame / ld sp,
# hl -- or nothing after the first nine bytes for a function with no frame
# that reads IX, and nothing at all for one that does not.
emits "a frame, written out"            dde5dd21000000dd39ed22f4f9 yes \
    'int f(int x) { int a[4]; a[x] = 1; return a[0]; }'
calls "and no call to make it"          acc_rt_frameset no \
    'int f(int x) { int a[4]; a[x] = 1; return a[0]; }'
emits "no frame: its first nine bytes"  '^dde5dd21000000dd39dd27' yes \
    'int f(int x) { return x + 1; }'
emits "none read: no prologue at all"   '^21070000c9' yes \
    'int f(void) { return 7; }'

# A signed order is a jump on the carry: no jp pe, jp m and jp p on the
# sign and the overflow. Of two variables, the subtract's sign put in the
# carry, turned over where it overflowed -- sbc hl, de / add hl, hl /
# jp po / ccf -- with no register but HL, and nothing moved by 0x800000;
# against a constant, both moved, the constant already, which is shorter.
emits "x < n, signed, branch"           ea......f2 no \
    'int f(int x, int n) { if (x < n) return 1; return 2; }'
emits "x < n, by the carry"             ed5229e2 yes \
    'int f(int x, int n) { if (x < n) return 1; return 2; }'
emits "x < n, nothing moved"            000080.9 no \
    'int f(int x, int n) { if (x < n) return 1; return 2; }'
emits "x > 5, as x >= 6, moved"         01060080 yes \
    'int f(int x) { if (x > 5) return 1; return 2; }'

# A read through an address loaded as a constant is one instruction:
# ld hl, (nn) for three bytes, ld a, (nn) for one -- not the address in
# HL and ld hl, (hl) or ld a, (hl) after it. Unless something jumps in
# between: the end of a ?: that chose one of two addresses.
emits "gi, an int global"               2a000000 yes \
    'int gi; int f(void) { return gi; }'
emits "gi, not through HL"              21000000ed27 no \
    'int gi; int f(void) { return gi; }'
emits "gc, a char global"               3a000000 yes \
    'char gc; int f(void) { return gc; }'
emits "*(k ? &a : &b), read after"      21030000ed27 yes \
    'int a, b; int f(int k) { return *(k ? &a : &b); }'

# A store to a global of this file names its address -- ld (nn), hl and
# ld (nn), a -- rather than loading it into HL to store through: and a
# char from a local goes through A as it is, not widened first.
emits "gi = k, ld (nn), hl"             dd270622 yes \
    'int gi; void f(int k) { gi = k; }'
emits "gi = k, not through HL"          'dd39.*21000000' no \
    'int gi; void f(int k) { gi = k; }'
emits "gc = c, ld a, (ix+9); ld (nn), a" dd7e0932 yes \
    'char gc; void f(int k, char c) { gc = c; }'

# A for loop is its condition, its body, its step and one jump back: no
# jump over the step into the body straight after the condition's.
emits "for loop, no jump into the body" '30..18' no \
    'unsigned f(unsigned n) { unsigned s = 0, i; for (i = 0; i < n; i++) s += i; return s; }'

# A long through a pointer moves three bytes at a time through IY: ld iy,
# (hl) and a store to the slot, or the other way round -- and a value
# already in a slot of its own is stored from there, not copied first.
emits "*p, a long, through IY"          ed31dd3e yes \
    'long f(long *p) { return *p; }'
emits "*p = v, v stored from its slot"  dd3109dd3e no \
    'void g(long *p, long v) { *p = v; }'
emits "*p = v, through IY"              dd3109ed3e yes \
    'void g(long *p, long v) { *p = v; }'

# A shift of a long by whole bytes reads the variable where it is, and
# does not copy it into scratch first.
emits "a >> 8, read where a is"         dd3107dd3e yes \
    'unsigned long f(unsigned long a) { return a >> 8; }'
emits "a >> 8, no copy first"           dd3106dd3e no \
    'unsigned long f(unsigned long a) { return a >> 8; }'

# An operator whose right operand is read in place puts its answer where
# its left operand was, rather than copying that above the right.
emits "t[i] ^ (b >> 8), no copy"        dd31fcdd3ef4 no \
    'unsigned long f(unsigned long a, unsigned long b, unsigned long *t) { return t[a & 0xff] ^ (b >> 8); }'

# A local array's address in reach of a displacement is lea hl, ix+d, not
# push de; ld de, d; push ix; pop hl; add hl, de; pop de.
emits "a[i], lea hl, ix+d"              ed22 yes \
    'int f(int i) { int a[4]; a[i] = 1; return a[0]; }'
emits "a[i], not through DE"            d511 no \
    'int f(int i) { int a[4]; a[i] = 1; return a[0]; }'

# A long worked out through the runtime and assigned to a variable is
# worked out in the variable: x = x * 3 + 1 copies x nowhere, and leaves
# its answer in no scratch to be copied back.
emits "x = x * 3 + 1, x not copied"     dd3106dd3e no \
    'long f(long x) { x = x * 3L + 1L; return x; }'
emits "x = x * 3 + 1, worked on in x"   ed2206 yes \
    'long f(long x) { x = x * 3L + 1L; return x; }'

# A long tested against zero ORs its bytes in A and a branch jumps on Z,
# with no 0 or 1 made of it; and a constant on the left of a commutative
# operator goes to the right, where the pool has it, rather than into a
# slot of its own.
emits "if (x), a long, bytes ORed"      ddb6 yes \
    'int f(long x) { if (x) return 1; return 2; }'
emits "if (x), a long, no 0 or 1 made"  ddb6..21000000 no \
    'int f(long x) { if (x) return 1; return 2; }'
emits "1000 + x, no constant in a slot" fd21e80300 no \
    'long f(long x) { return 1000L + x; }'

# A condition guarding only a jump is one jump the other way: no jump on
# the condition over an unconditional one -- jr z, +2; jr.
emits "if (p[i]) continue, one jump"    280218 no \
    'int f(const char *p, int n) { int i, c = 0; for (i = 0; i < n; i++) { if (p[i]) continue; c++; } return c; }'

# A register local lives in IY: `*p++` reads at iy-1 without a copy of p --
# ld a, (iy-1) -- or writes there. Without register it is in the frame, and
# the read is not there. (Not inc iy, `fd23`, for the step: in frame code
# those bytes are a displacement of -3 and an inc hl.)
emits "register p: *p++ is (iy-1)"      fd7eff yes \
    'int f(register const char *p) { int n = 0; while (*p++) n++; return n; }'
emits "register p: *p++ = 0 at (iy-1)"  fd7[135]ff yes \
    'void f(register char *p, int n) { while (n--) *p++ = 0; }'
emits "no register: no (iy-1)"          fd7eff no \
    'int f(const char *p) { int n = 0; while (*p++) n++; return n; }'

# A byte widened into HL whose upper bytes nothing reads: the widening
# taken back and the byte used in A. A byte local &'d with a mask is read
# into A and and'd -- not cleared into HL first and moved to A -- and |'d
# with a constant byte the same, widened after. Tested, it is or a in A.
# Written through a pointer, it goes from A through the pair the pointer
# is in, ld (hl), a -- a byte local, one just read, one cast from an int --
# not widened and moved back to A for ld (de), a. A global byte compared
# with 0 is ld a, (nn) / or a; a byte compared with 0 is or a, not cp 0.
emits "x & 4 of a byte local, in A"     dd7e06e604 yes \
    'int f(unsigned char x) { return x & 4; }'
emits "not cleared into HL first"       ed62dd6e06 no \
    'int f(unsigned char x) { return x & 4; }'
emits "x | 0x20 on the byte"            dd7e06f620 yes \
    'int f(unsigned char x) { return x | 0x20; }'
emits "if (c) of a byte local, in A"    dd7e06b728 yes \
    'int f(char c) { if (c) return 1; return 2; }'
emits "not tested at 24 bits"           "$zero_test" no \
    'int f(char c) { if (c) return 1; return 2; }'
emits "*p = c by ld (hl), a"            dd7e0977 yes \
    'void f(char *p, char c) { *p = c; }'
emits "not through ld a, l"             7d12 no \
    'void f(char *p, char c) { *p = c; }'
emits "*d++ = *s++ from A"              7e12 yes \
    'void f(char *d, const char *s) { while (*s) *d++ = *s++; }'
emits "not widened between"             6fcb05 no \
    'void f(char *d, const char *s) { while (*s) *d++ = *s++; }'
emits "*p = (unsigned char) v from A"   7ddd270677 yes \
    'void f(unsigned char *p, int v) { *p = (unsigned char) v; }'
emits "not widened between"             b7ed626f no \
    'void f(unsigned char *p, int v) { *p = (unsigned char) v; }'
emits "g == 0 of a global byte, in A"   3a000000b721 yes \
    'unsigned char g; int f(void) { return g == 0; }'
emits "not widened first"               ed626f7d no \
    'unsigned char g; int f(void) { return g == 0; }'
emits "*p = x & 0x7f from A"            e67fdd270677 yes \
    'void f(char *p, int x) { *p = x & 0x7f; }'
emits "and not widened between"         e67fed62 no \
    'void f(char *p, int x) { *p = x & 0x7f; }'
# The byte a store leaves in A, widened for the assignment's value, is
# taken back again by what reads it as a byte: ++s.gen == 0 is or a.
emits "++s.gen == 0 tested in A"        77b720 yes \
    'struct st { int a; unsigned char gen; }; extern struct st state; int f(void) { if (++state.gen == 0) return 1; return 2; }'
emits "t == 0 of a byte by or a"        dd7e06b720 yes \
    'int v; int f(unsigned char t) { if (t == 0 && v > 0) return 1; return 2; }'
emits "not cp 0"                        fe00 no \
    'int v; int f(unsigned char t) { if (t == 0 && v > 0) return 1; return 2; }'

# A jump to the instruction after it is taken out: the else of x ? 1 : 0
# jumped past nothing, as did a switch's last test to its end.
emits "x ? 1 : 0, no jump past nothing"  210000001800 no \
    'int f(int x) { return x ? 1 : 0; }'
emits "if (x) g(1); else ;, nor here"    d1180021 no \
    'int g(int); int f(int x) { if (x) g(1); else ; return 2; }'
emits "nor a switch's last test"         ca100000180021 no \
    'int f(int x) { switch (x) { case 1: break; } return 0; }'

# A function with no frame ends pop ix / ret, SP never having moved; and
# one that does not read IX either has no frame at all -- a static read is
# ld hl, (nn) / ret, a call and an add call / inc hl / ret. A parameter's
# address, lea hl, ix+6, keeps it.
emits "no frame: pop ix alone"           ed27dde1c9 yes \
    'int f(int *p) { return p[3]; }'
emits "not ld sp, ix"                    ddf9 no \
    'int f(int *p) { return p[3]; }'
emits "IX unread: no frame at all"       '^2a000000c9$' yes \
    'static int counter; int f(void) { return counter; }'
emits "nor around a call"                '^cd00000023c9$' yes \
    'int g(void); int f(void) { return g() + 1; }'
emits "&a keeps it"                      '^dde5dd21000000dd39ed2206' yes \
    'int *f(int a) { return &a; }'

# x++ of a global, or through a pointer, whose value nothing reads: stepped
# where it is, no copy of the old value kept -- which took a frame for it,
# ld hl, -3 / call, and a store into it.
emits "c++; of a global, no frame"       '^2a0000002322000000c9$' yes \
    'int c; void f(void) { c++; }'
emits "p->n++; no copy kept"             dd2ffd no \
    'struct s { int n; }; void f(struct s *p) { p->n++; }'

# Signed against 0, the sign alone: add hl, hl puts it in the carry --
# not both sides moved by 0x800000, the bias loaded into BC twice.
emits "x < 0 by add hl, hl"              dd27062930 yes \
    'int f(int x) { if (x < 0) return 1; return 2; }'
emits "x >= 0 too, not the bias"         01000080 no \
    'int f(int x) { return x >= 0; }'

# A slot loaded into a register, and loaded again with nothing between that
# changes a pair or memory: `*p == ' ' || *p == '\t'` reads p once, the
# second ld a, (hl) straight after the first test's jump.
emits "*p twice, p loaded once"          7efe2028057efe09 yes \
    'int f(const char *p) { if (*p == 32 || *p == 9) return 1; return 2; }'
emits "not again"                        "28..dd2706" no \
    'int f(const char *p) { if (*p == 32 || *p == 9) return 1; return 2; }'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

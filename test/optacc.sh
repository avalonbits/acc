#!/bin/bash
# What opt-acc does that acc does not, seen in the objects each writes for
# the same source (docs/optimizer-plan.md, milestone 0):
#
# - the prologue written out, push ix; ld ix, 0; add ix, sp; ld hl, -frame;
#   add hl, sp; ld sp, hl, where acc calls acc_rt_frameset -- and cut to its
#   first nine bytes when there is no frame;
# - the local a loop uses most kept in IY, chosen by reading the body first
#   (src/prescan.c), where acc needs it declared register -- in a body with
#   a long or a float, which the SSA form's code leaves to this;
# - every function compiled a second time, from the log of the parser's
#   calls (src/genlog.c), with the same code to come out;
# - with OPTACC_SSA, functions built as SSA from that log (src/ssa.c), and
#   the others left to the classic backend with a reason.
#
# test/run.sh ACC=bin/opt-acc checks that what opt-acc writes runs; this
# checks that it is what opt-acc is meant to write.
#
#   test/optacc.sh
set -u
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
OPT=${OPT:-bin/opt-acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
[ -x "$OPT" ] || { echo "$OPT missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# An object's code as hex: see test/codegen.sh.
text_hex() {
    python3 - "$1" <<'PY2'
import sys
d = open(sys.argv[1], 'rb').read()
n3 = lambda at: d[at] | d[at + 1] << 8 | d[at + 2] << 16
at = 31 + n3(13) * 7 + n3(16) * 6 + n3(28) * 9 + n3(19) * 12 + n3(25) * 3 + n3(22)
sys.stdout.write(d[at:at + n3(7)].hex())
PY2
}

# emits <compiler> <name> <hex> <yes|no> <source>
emits() {
    local cc=$1 what=$2 want=$4 got

    printf '%s\n' "$5" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    if ! "$cc" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1; then
        printf '  FAIL %-50s %s could not compile it\n' "$what" "$cc"
        fail=$((fail + 1)); return
    fi
    got=no
    text_hex "$tmp/c.o" | grep -q "$3" && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s %s emits %s: want %s, got %s\n' "$what" "$cc" "$3" "$want" "$got"
        fail=$((fail + 1))
    fi
}

# wants <compiler> <name> <symbol> <yes|no> <source>: whether the object
# asks the link for the symbol.
wants() {
    local cc=$1 what=$2 want=$4 got

    printf '%s\n' "$5" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    "$cc" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1
    got=no
    grep -aq "$3" "$tmp/c.o" && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s %s wants %s: want %s, got %s\n' "$what" "$cc" "$3" "$want" "$got"
        fail=$((fail + 1))
    fi
}

framed='int f(int x) { int a[2]; a[x & 1] = x; return a[0]; }'
empty='int f(void) { return 7; }'
loop='int f(const char *p, int n) { int s = 0; while (n--) s += *p++; return s; }'
regloop='int f(const char *p, int n) { register int s = 0; while (n--) s += *p++; return s; }'

prologue=dde5dd21000000dd39         # push ix; ld ix, 0; add ix, sp
wants "$ACC" "a frame: acc calls the prologue"     acc_rt_frameset yes "$framed"
wants "$OPT" "a frame: opt-acc does not"           acc_rt_frameset no  "$framed"
emits "$OPT" "a frame: opt-acc writes it out"      "${prologue}21......39f9" yes "$framed"
emits "$ACC" "a frame: acc does not"               "$prologue" no "$framed"
# No frame: the load and the two after it are cut, and the body follows.
emits "$OPT" "no frame: the nine bytes, then the body" "${prologue}21070000" yes "$empty"

lea_hl_iy=ed2300                    # lea hl, iy+0: a read of the IY local
# The pre-scan chooses IY only where a body has a long or a float, which
# keeps the function to the first pass; anywhere else the SSA form's code
# is made, and chooses its own -- the first pass alone is what is checked.
longloop='long f(const char *p, int n) { long t = 1; int s = 0; while (n--) s += *p++; return s + t; }'
emits "$OPT" "a loop with a long: opt-acc keeps a local in IY" "$lea_hl_iy" yes "$longloop"
emits "$OPT" "a loop without: left to the SSA form"  "$lea_hl_iy" no "$loop"
emits "$ACC" "a loop: acc does not unasked"        "$lea_hl_iy" no  "$loop"
emits "$ACC" "a loop: acc does when told register" "$lea_hl_iy" yes "$regloop"

# Every function opt-acc compiles is replayed from its log and has to come
# out the same (src/genlog.c). The check has to be able to fail: with one
# record of the log left out of the replay, it does.
printf '%s\n' "$loop" > "$tmp/c.c"
rm -f "$tmp/c.o"                    # acc -c skips an object newer than its source
if "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s the replay did not match\n' "a loop, replayed"
    fail=$((fail + 1))
fi
rm -f "$tmp/c.o"
if err=$(GENLOG_BREAK=3 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1); then
    printf '  FAIL %-50s a record left out went unnoticed\n' "a broken replay"
    fail=$((fail + 1))
elif printf '%s' "$err" | grep -q 'internal: replaying'; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s failed another way: %s\n' "a broken replay" "$err"
    fail=$((fail + 1))
fi

# With OPTACC_SSA, a function is built as SSA and its code made from that
# (src/ssa.c); one that uses what the builder does not handle yet is left to
# the classic backend, and OPTACC_SSA_STATS says which and why.
# ssa <name> <expected line> <source>
ssa() {
    local what=$1 want=$2 got

    printf '%s\n' "$3" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    got=$(OPTACC_SSA=1 OPTACC_SSA_STATS=1 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 \
          | grep '^ssa f ' | head -1)
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s said "%s", not "%s"\n' "$what" "$got" "$want"
        fail=$((fail + 1))
    fi
}
ssa "a loop, as SSA"                "ssa f made" "$loop"
ssa "&& and ?:, as SSA"             "ssa f made" \
    'int f(int a, int b) { return a && b ? a : b; }'
ssa "a runtime-sized array, left"   "ssa f an array whose length is known when it runs" \
    'int f(int n) { char a[n]; a[0] = 1; return a[0]; }'

# The locals and parameters that stay in registers are SSA values, with a
# phi where paths join (docs/optimizer-plan.md, milestone 3): the ones whose
# address is taken stay in the frame. OPTACC_SSA_DUMP prints the form.
# phis <name> <yes|no> <source>: whether f's form has a phi
phis() {
    local what=$1 want=$2 got=no

    printf '%s\n' "$3" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    OPTACC_SSA=1 OPTACC_SSA_DUMP=1 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 \
        | grep -q '^ *phi v[0-9]* of local' && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s phi: want %s, got %s\n' "$what" "$want" "$got"
        fail=$((fail + 1))
    fi
}
phis "a loop's counter, a phi"      yes "$loop"
phis "a parameter stepped, a phi"   yes \
    'int f(int n) { int s = 0; while (n) { s += n; n--; } return s; }'
phis "a local whose address is taken, none" no \
    'void g(int *); int f(void) { int i; for (i = 0; i < 9; i++) g(&i); return i; }'
# A long where paths join is more than a phi's copies carry: the form is
# made again with the longs left in memory, the rest of it values.
ssa "a long where paths join, kept in memory" "ssa f made" \
    'long f(long n) { long s = 0; while (n) s += n--; return s; }'
phis "and no phi for it"                no \
    'long f(long n) { long s = 0; while (n) s += n--; return s; }'

# && and || in a condition jump where their answer would send the branch,
# as the classic backend does, instead of setting a 1 or a 0 and testing it.
# sets <name> <count> <source>: how many answers f's form sets
sets() {
    local what=$1 want=$2 got

    printf '%s\n' "$3" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    got=$(OPTACC_SSA=1 OPTACC_SSA_DUMP=1 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 \
          | sed -n '/ssa of f:/,/ssa of /p' | grep -c ' set ')
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s sets %s answers, not %s\n' "$what" "$got" "$want"
        fail=$((fail + 1))
    fi
}
sets "&& in an if, jumped on"       0 \
    'int f(int a, int b) { if (a && b) return 1; return 2; }'
sets "|| in a loop, jumped on"      0 \
    'int f(int *p, int n) { while (n > 0 || *p) { n--; p++; } return n; }'
sets "&& as a value, set"           2 \
    'int f(int a, int b) { return a && b; }'

# With OPTACC_REGS, OPTACC_NATIVE and OPTACC_IY, a loop's counter lives in
# BC and the comparison and the step use it there: scf; sbc hl, bc against
# the limit in IY, and inc bc -- where gen.h would copy BC out to HL first.
native='void f(char *a, unsigned n) { unsigned i; for (i = 0; i < n; i++) a[i] = 0; }'
regs() {
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 emits "$@"
}
regs "$OPT" "a comparison with BC where it is"   37ed42  yes "$native"
regs "$OPT" "a step of BC where it is"           360003  yes "$native"
emits "$OPT" "neither, from the classic backend" 37ed42  no  "$native"

# And each function made either way is kept the way that costs less to
# run: bytes, each block's counted eight times over for each loop around
# it. A loop on a counter that lives in BC is cheaper made from SSA; one
# that works a short through gen.h is cheaper as the first pass made it.
short='unsigned f(unsigned short c, int n) { while (n--) c = c & 0x8000 ? (unsigned short) (c << 1) ^ 0x1021 : (unsigned short) (c << 1); return c; }'
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 \
    ssa "a loop on BC, kept"                "ssa f made" "$native"
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 \
    ssa "a short worked by gen.h, not kept" "ssa f the first pass's code is cheaper" "$short"

# A local read before it is written reads a 0 made again, not a value
# nothing makes -- which in a register was a second owner of it.
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_PICK=0 \
    ssa "a local read unwritten"     "ssa f made" \
    'struct s { int key, next, prev; }; void f(struct s *h) { int i, k; for (i = 0; i < 2; i++) { struct s *c = h + k; c->key = i * (0xffffffffUL / 2); c->next = k + (1 - i); c->prev = k + (1 - i); } }'

# Selected with a value in a register, and every function made so (no pick):
# an unsigned char's zero test and step on its register's byte -- ld a, c;
# or a and dec c -- and p++ moved after the *p that reads the old p, so the
# two share DE and it is inc de. And a signed comparison of two values when
# DE holds one: DE pushed and popped round it, pop de; or a; sbc hl, de;
# pop de, where gen.h spilled both registers and made a 0 or 1.
byte='typedef unsigned char u8; static u8 t[256]; u8 f(const char *p, int len) { u8 h = 0; for (u8 k = (u8) len; k != 0; k--) h = t[h ^ (u8) *p++]; return h; }'
dirs='int f(const char *s, const char *want, int n) { for (int i = 0; i < n; i++) if ((s[i] | 0x20) != want[i]) return 0; return 1; }'
all() {
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
        OPTACC_PICK=0 emits "$@"
}
all "$OPT" "an unsigned char tested for 0 in its register" 79b7       yes "$byte"
all "$OPT" "and stepped there"                            0d18       yes "$byte"
all "$OPT" "p++ after *p, in place"                       6f137d     yes "$byte"
all "$OPT" "a signed comparison borrowing DE"             d1b7ed52d1 yes "$dirs"

# More functions reach the SSA form: a call compiled in place, its
# parameters locals that become values; `(*p).x`, which makes a struct of
# *p on the way to its member; and the frame laid out again without the
# locals that became values -- nine bytes of them here, where three are
# kept.
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_PICK=0 \
    ssa "a call compiled in place"   "ssa f made" \
    '__attribute__((always_inline)) static inline int sq(int x) { return x * x + 1; } int f(int a) { int s = 0; for (int i = 0; i < a; i++) s += sq(i); return s; }'
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_PICK=0 \
    ssa "a struct read for a member" "ssa f made" \
    'struct s { int a, b; }; int f(struct s *p, int i) { return p[i].b + (*p).a; }'
frame='int f(int a) { int x = a + 1, y = x * 2, z = y - 3; return z; }'
all "$OPT" "the frame without the locals made values"  21fdffff yes "$frame"
emits "$OPT" "which the first pass's has"              21f7ffff yes "$frame"

# With OPTACC_LEAF, a function that calls nothing and holds nothing wider
# than an int is made by a backend of opt-acc's own, every instruction
# selected in ssa.c. OPTACC_SSA_STATS says which: `leaf f`.
# leafs <name> <yes|no> <source>
leafs() {
    local what=$1 want=$2 got=no

    printf '%s\n' "$3" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_LEAF=1 \
        OPTACC_PICK=0 OPTACC_SSA_STATS=1 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 \
        | grep -q '^leaf f$' && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s leaf: want %s, got %s\n' "$what" "$want" "$got"
        fail=$((fail + 1))
    fi
}
leafs "a loop on bytes through a table, made here" yes "$byte"
leafs "a function that calls, made here too"      yes \
    'int g(int); int f(int a) { return g(a) + 1; }'
leafs "one that calls memcpy, not"                 no \
    'void *memcpy(void *, const void *, unsigned); void f(char *a, char *b) { memcpy(a, b, 4); }'
OPTACC_LEAF=1 all "$OPT" "a char in BC compared in A"   79ee80fec1 yes \
    'int f(char c) { return c >= 0x41 && c <= 0x5a; }'
leafs "one that holds a long, not"                no \
    'long f(long a) { return a + 1; }'
OPTACC_LEAF=1 all "$OPT" "and its counter stepped as a byte, p after it"   0dfd2318 yes "$byte"
# A byte tested through a table, as zap's character classes are: the AND
# made in A and branched on straight from its flags -- and 0x20, jr nz --
# through the inlined _Bool's conversion and the `!` of it.
OPTACC_LEAF=1 all "$OPT" "a byte's AND branched on from its flags"   e62020 yes \
    'extern const unsigned char tab[256];
__attribute__((always_inline)) static inline _Bool alpha(char c) { return (tab[(unsigned char) c] & 0x20) != 0; }
int f(const char *p) { if (!alpha(*p)) return 5; return 3; }'

# A _Bool through a pointer, made here: written as the test of what may be
# anything -- ld (hl), 0; jr z; inc (hl) -- and as it is where that is 0 or
# 1 already, a comparison's answer.
flag='struct s { int n; _Bool b; };'
leafs "a _Bool member read and written, made here" yes \
    "$flag int f(struct s *p, int a) { p->b = a; return p->b; }"
OPTACC_LEAF=1 all "$OPT" "an int written to a _Bool as its test"  3600280134 yes \
    "$flag void f(struct s *p, int a) { p->b = a; }"
OPTACC_LEAF=1 all "$OPT" "a comparison written as it is"         3600280134 no \
    "$flag void f(struct s *p, int a, int b) { p->b = a < b; }"
# Locals that stay in memory, made here: an array, a local whose address
# is taken, a struct.
leafs "a local array, made here"                 yes \
    'void g(char *); int f(int i) { char buf[8]; buf[i] = 1; g(buf); return buf[0]; }'
leafs "a local whose address is taken, made here" yes \
    'void g(int *); int f(void) { int n = 3; g(&n); return n + 1; }'
leafs "a struct local, made here"                yes \
    'struct s { int a, b; }; void g(struct s *); int f(void) { struct s x; x.a = 1; g(&x); return x.b; }'
# AND, OR and XOR of ints, made here: by the helper where all three bytes
# may be set, and a byte at a time where the code here knows two are
# enough -- what each side was masked with.
OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
OPTACC_PICK=0 wants "$OPT" "an OR of ints, by the helper"        acc_rt_or yes \
    'int f(int x, int y) { return x | y; }'
OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
OPTACC_PICK=0 wants "$OPT" "an OR of two bytes' worth, without it" acc_rt_or no \
    'int f(int x, int y) { return (x & 0xff00) | (y & 0xff); }'
# The pick keeps the first pass's code where what is made here is costlier
# to run or bigger: x * x is cheaper here, by the estimate, and bigger.
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
    ssa "cheaper but bigger, not kept" "ssa f the first pass's code is smaller" \
    'int f(int x) { return x * x; }'
# Where the leaf backend's code loses the pick, the hybrid path's is made
# and weighed too, before the first pass's is kept: a character class's
# test is the hybrid path's.
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
    ssa "the leaf backend's lost, the hybrid path's made" \
    "ssa f made, not by the leaf backend" \
    'extern const unsigned char tab[256]; _Bool f(char c) { return (tab[(unsigned char) c] & 4) != 0; }'
# A list walked with its node in IY across a call: IY kept around the call
# it lives across, and not around the one before it is made -- push iy
# once, under the arguments; p = p->next as ld iy, (iy+0); the _Bool answer
# tested in A as IY comes back, pop iy then or a; and the first argument,
# read through IY, left in HL while the parameter after it goes around it,
# ld de, (ix+12) then push de.
walk='struct n { struct n *next; const char *name; int value; };
struct n *first(struct n *, int); _Bool same(const char *, const char *);
int f(struct n *h, int k, const char *s) { struct n *p; for (p = first(h, k); p; p = p->next) if (same(p->name, s)) return p->value; return -1; }'
OPTACC_LEAF=1 all "$OPT" "p = p->next, ld iy, (iy+0)"            fd3700     yes "$walk"
OPTACC_LEAF=1 all "$OPT" "the answer tested in A as IY comes back" 'fde1b72[08]' yes "$walk"
OPTACC_LEAF=1 all "$OPT" "the next argument around the first, by DE" dd170cd5 yes "$walk"
printf '%s\n' "$walk" > "$tmp/c.c"
rm -f "$tmp/c.o"
OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_PICK=0 \
    OPTACC_LEAF=1 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1
saves=$(text_hex "$tmp/c.o" | grep -o 'fde5' | wc -l)
if [ "$saves" = 1 ]; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s saved %s times\n' "IY saved around one call of two" "$saves"
    fail=$((fail + 1))
fi

# A table indexed by a byte, as zap's character classes are: the byte kept
# in A from where it is read to its use -- not stored, since that is its
# one read -- the table's address loaded where it is used rather than
# pushed and popped round the byte, and the byte zero-extended into DE:
# ld hl, tab; ld de, 0; ld e, a; add hl, de.
table='extern const unsigned char tab[256];
__attribute__((always_inline)) static inline int space(char c) { return tab[(unsigned char) c] & 1; }
int f(const char *p, const char *e) { while (p < e && space(*p)) p++; return (int) (e - p); }'
OPTACC_LEAF=1 all "$OPT" "a table indexed by a byte kept in A" '7e21000000110000005f19' yes "$table"
OPTACC_LEAF=1 all "$OPT" "and the byte not stored"             dd77 no "$table"
# A byte local whose address is taken, read into A and compared there:
# ld a, (ix+d); cp 0x78, not widened on the way.
OPTACC_LEAF=1 all "$OPT" "a byte in memory read into A" 'dd7e..fe78' yes \
    'void g(char *); int f(void) { char c; g(&c); return c == 0x78; }'
# A global's member, (nn): the offset in the instruction and the link adding
# the address -- ld hl, (nn), ld (nn), hl, and a byte's ld a, (nn).
global='struct { unsigned char m; int a, b; } g; int f(int x) { g.a = x; return g.b + g.m; }'
OPTACC_LEAF=1 all "$OPT" "a global's member read, ld hl, (nn)"  2a040000 yes "$global"
OPTACC_LEAF=1 all "$OPT" "and written, ld (nn), hl"            22010000 yes "$global"
OPTACC_LEAF=1 all "$OPT" "a byte's, ld a, (nn)"                3a000000 yes "$global"
OPTACC_LEAF=1 all "$OPT" "an extern's, the offset for the link"  2a040000 yes \
    'extern struct { unsigned char m; int a, b; } h; int f(void) { return h.b; }'
# An unsigned byte shifted right by a constant, srl a in A, not the helper.
OPTACC_LEAF=1 all "$OPT" "a byte shifted right in A, srl a"  cb3f yes \
    'int f(unsigned char *p) { return *p >> 1; }'
# `*o++ = g(*s++)`: each step sunk past the reads of the old value, past
# the other's step and the call, so old and new share a home and the step
# is made where the value is -- o in IY, ld (hl), a then inc iy; s in its
# slot, ld hl, (ix+9); inc hl; ld (ix+9), hl -- and the one phi copy left,
# n's, made straight, not pushed and popped back into HL on the way.
steps='char g(char); void f(char *o, const char *s, int n) { while (n--) *o++ = g(*s++); }'
OPTACC_LEAF=1 all "$OPT" "o++ stepped in IY after its store"   ed230077fd23   yes "$steps"
OPTACC_LEAF=1 all "$OPT" "s++ stepped in its slot after the call" dd270923dd2f09 yes "$steps"
OPTACC_LEAF=1 all "$OPT" "one phi copy, not through the stack"  e5e1           no  "$steps"
# A step whose last reader ends its block, falling on to the next: sunk
# past it too, so o is stepped in IY after the store, not copied.
tail='char g(int); void f(char *o, int s) { if (s) *o++ = g(s); *o = 0; }'
OPTACC_LEAF=1 all "$OPT" "o++ sunk past the block's last store" ed230077fd23 yes "$tail"
# One phi copy into a slot, through HL alone: ld hl, (ix+6); ld (ix-3), hl.
copy='char *g(char *, int); char *f(char *o, int n) { char *start = o; while (n--) { char *old = o; o = g(o, n); if (!o) o = old; } return start == o ? 0 : o; }'
OPTACC_LEAF=1 all "$OPT" "one phi copy into a slot, not pushed"  dd2706dd2ffd yes "$copy"
# A long whose low three bytes are all that is kept is made here; one whose
# truth is asked -- all four bytes -- is not.
leafs "a long read and kept as an int, made here" yes \
    'void g(long *); int f(void) { long v = 0; g(&v); return (int) v; }'
leafs "a long made a _Bool, not"                 no \
    'int f(const long *p) { return (_Bool) *p; }'
# Constants written to members of the struct a pointer in IY points at:
# ld (iy+d), n for a byte, 1 for a _Bool given 5, and ld hl, n / ld
# (iy+d), hl for an int -- -2 too, which is `-` of 2 folded -- with no
# address made in HL and pushed; and the pointer stepped by the struct's
# size in place, lea iy, iy+6.
members='typedef struct { unsigned char r0, r1; _Bool b; int i; } dop;
void f(dop *op, int n) { while (n--) { op->r0 = 7; op->b = 5; op->i = -2; op++; } }'
OPTACC_LEAF=1 all "$OPT" "a byte member, ld (iy+0), 7"      fd360007   yes "$members"
OPTACC_LEAF=1 all "$OPT" "a _Bool member, ld (iy+2), 1"     fd360201   yes "$members"
OPTACC_LEAF=1 all "$OPT" "an int member, ld (iy+3), hl"     21fefffffd2f03 yes "$members"
OPTACC_LEAF=1 all "$OPT" "no address pushed and popped"     e5e1       no  "$members"
OPTACC_LEAF=1 all "$OPT" "op++ in IY, lea iy, iy+6"         ed3306     yes "$members"
OPTACC_LEAF=1 all "$OPT" "op into IY at entry, ld iy, (ix+6)" dd3106   yes "$members"
# A branch on a constant is no test in the leaf backend either: `while
# (1)` falls into its body, a macro's `do ... while (0)` out of it.
consts='int f(int x) { do x += 2; while (0); while (1) { if (x > 9) break; x++; } return x; }'
leafs "constant conditions, made here"             yes "$consts"
OPTACC_LEAF=1 all "$OPT" "no constant tested"               09b7ed42 no "$consts"
# A string literal in a function, its bytes jumped over where it is, as
# the first pass has them: made here.
leafs "a string literal, made here"               yes \
    'int puts(const char *); int f(int x) { puts(x ? "yes" : "no"); return "abc"[x]; }'
# A long constant written through a pointer: made here, the low three
# bytes and then the top one -- ld (iy+3), hl / ld (iy+6), 0x12 for a
# member through IY.
longs='typedef struct { char *name; long addr; } node; void f(node *n) { n->addr = 0x12345678; }'
leafs "a long constant written, made here"        yes "$longs"
OPTACC_LEAF=1 all "$OPT" "its bytes, ld (iy+3), hl / ld (iy+6), n" 21785634fd2f03fd360612 yes "$longs"
# Two inlined bodies whose byte parameters, a char and an unsigned char,
# share a slot: each is a value of its own, not a local in memory widened,
# stored and read back.
shared='extern const unsigned char cl[256];
static inline int sp(char c) { return cl[(unsigned char) c] & 1; }
static inline int al(unsigned char u) { return (cl[u] & 2) != 0; }
int f(const char *p, const char *e) { int n = 0; while (p < e) { n += sp(*p); n += al(*p); p++; } return n; }'
OPTACC_LEAF=1 all "$OPT" "shared-slot bytes, no widen and narrow" 6fcb05ed626f7d no "$shared"
# And each byte, read once and straight from A -- by the signed one's
# table index, and into DE by the unsigned one's -- is not stored at all.
OPTACC_LEAF=1 all "$OPT" "a byte read once from A, not stored" dd77 no "$shared"
# A local whose address a call takes, kept in a register between the
# calls: the loop steps p in BC and writes it through, inc bc / ld (ix+6),
# bc, with no read of it from memory -- and after the call, read again.
cached='extern const unsigned char cl[256]; int parse(const char **pp);
int f(const char *p, const char *e) { int n = 0; while (p < e && (cl[(unsigned char) *p] & 1)) p++;
n += parse(&p); while (p < e && (cl[(unsigned char) *p] & 1)) p++; return n + (int) (e - p); }'
OPTACC_LEAF=1 all "$OPT" "a cached p stepped and written through" 03dd0f06 yes "$cached"
OPTACC_LEAF=1 all "$OPT" "and not read in the loop"             dd270606 no "$cached"
OPTACC_LEAF=1 all "$OPT" "read again straight into BC"          dd0706 yes "$cached"
OPTACC_LEAF=1 all "$OPT" "not through HL and the stack"         dd2706e5c1 no "$cached"
# One the leaf backend cannot make is made by the hybrid path, uncached.
OPTACC_LEAF=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_PICK=0 \
    ssa "a cached local, not the leaf backend's" "ssa f made" \
    'int g(char **); int f(void) { char text[] = "abc"; char *p = text; g(&p); return p[0] + p[1]; }'
# The pick goes back to where a function began once for each way it weighs;
# where values were on the stack there -- the function before left a VLA
# prototype's bound -- each time from its own copy of them, which were
# freed twice.
OPTACC_LEAF=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
    ssa "gone back to more than once"   "ssa f made" \
    'extern int m, n; void a(void) { typedef int A3[3]; typedef A3 An3[n]; void h(An3[][m]); }
void f(void) { typedef int B[m]; void *g(B); }'
# A signed comparison moves both sides by 0x800000 through BC, saving BC
# only where a value lives there -- lists' insert_sorted has none.
sorted='struct node { int key; struct node *next; };
void f(struct node **head, struct node *n) { while (*head && (*head)->key < n->key) head = &(*head)->next; n->next = *head; *head = n; }'
OPTACC_LEAF=1 all "$OPT" "a signed compare, BC free, not saved"  c501000080 no "$sorted"
printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

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
wants "$OPT" "a frame: opt-acc writes it out"      acc_rt_frameset no  "$framed"
OPTACC_FRAME_CALL=1 wants "$OPT" "with OPTACC_FRAME_CALL, calls it" acc_rt_frameset yes "$framed"
# A function said hot has it written out, which is faster -- lea hl,
# ix-frame / ld sp, hl, where the frame is in its reach, and ld hl, -frame
# / add hl, sp / ld sp, hl where it is not.
hot='__attribute__((hot)) '
OPTACC_FRAME_CALL=1 wants "$OPT" "a hot frame: opt-acc does not call" acc_rt_frameset no  "$hot$framed"
emits "$OPT" "a hot frame: opt-acc writes it out" "${prologue}ed22..f9" yes "$hot$framed"
bigframe='int f(int x) { int a[60]; a[x & 63] = x; return a[0]; }'
emits "$OPT" "a big hot frame: ld hl, -frame"      "${prologue}21......39f9" yes "$hot$bigframe"
emits "$ACC" "a frame: acc does not"               "$prologue" no "$framed"
# No frame: the load and the two after it are cut, and the body follows.
emits "$OPT" "no hot frame: the nine bytes, then the body" "${prologue}21070000" yes "$hot$empty"

lea_hl_iy=ed2300                    # lea hl, iy+0: a read of the IY local
# The pre-scan chooses IY only where a body has a long or a float, which
# keeps the function to the first pass; anywhere else the SSA form's code
# is made, and chooses its own -- the first pass alone is what is checked.
longloop='long f(const char *p, int n) { long t = 1; int s = 0; while (n--) s += *p++; return s + t; }'
emits "$OPT" "a loop with a long: opt-acc keeps a local in IY" "$lea_hl_iy" yes "$longloop"
emits "$OPT" "a loop without: left to the SSA form"  "$lea_hl_iy" no "$loop"
# A jump in reach once the jumps between it and its target are short, two
# bytes: vi's skip_thing, whose loop's jump back is 134 bytes as written and
# 108 once the ten inside it are cut -- jr, not jp.
skipthing='extern char *end, *text; int st_test(char *p, int type, int dir, char *c); char *f(char *p, int linecnt, int dir, int type) { char c; while (st_test(p, type, dir, &c)) { if (c == 10 && --linecnt < 1) break; if (dir >= 0 && p >= end - 1) break; if (dir < 0 && p <= text) break; p += dir; } return p; }'
emits "$OPT" "a jump in reach once others are cut, jr" 1894 yes "$skipthing"
emits "$OPT" "not jp"                                  c308   no  "$skipthing"
# There a long goes three bytes at a time through DE or BC, where free,
# rather than IY with the local pushed and popped around every move.
crctable='unsigned long t[256]; void f(void) { for (unsigned n = 0; n < 256; n++) { unsigned long c = n; for (int k = 0; k < 8; k++) c = c & 1 ? 0xedb88320UL ^ (c >> 1) : c >> 1; t[n] = c; } }'
emits "$OPT" "a long beside a local in IY, kept there"   "$lea_hl_iy" yes "$crctable"
emits "$OPT" "and moved without pushing IY"             fde5 no "$crctable"
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
# (Read by the call's ld hl, -frame: OPTACC_FRAME_CALL.)
OPTACC_FRAME_CALL=1 all "$OPT" "the frame without the locals made values"  21fdffffcd yes "$frame"
OPTACC_FRAME_CALL=1 emits "$OPT" "which the first pass's has"              21f7ffffcd yes "$frame"

# With OPTACC_LEAF, a function is made by a backend of opt-acc's own, every
# instruction selected in ssa.c. OPTACC_SSA_STATS says which: `leaf f`.
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
leafs "one that holds a long, made here too"      yes \
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
# The smallest way loses to one a tenth cheaper to run: sieve's loops, the
# leaf backend's code 13 bytes smaller and a quarter slower than the hybrid
# path's, once the leaf backend could hold the long the program checks.
sieve='static char composite[16000]; extern volatile unsigned long seed; void check(unsigned long);
void f(void) { unsigned limit = 16000 - (unsigned) (seed & 1); unsigned long sum = 0; for (int pass = 0; pass < 3; pass++) { unsigned count = 0; for (unsigned i = 0; i < limit; i++) composite[i] = 0; for (unsigned i = 2; i < limit; i++) { if (composite[i]) continue; count++; for (unsigned j = i + i; j < limit; j += i) composite[j] = 1; } sum = sum * 3 + count; } check(sum); }'
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
    ssa "a smaller way a tenth costlier, not kept" \
    "ssa f made, not by the leaf backend" "$sieve"
# And lists' insert_sorted: the machine-level backend's 9 bytes smaller and
# 9.5% costlier than the leaf backend's, which is kept.
sorted_insert='struct node { int key; struct node *next; }; void f(struct node **head, struct node *n) { while (*head && (*head)->key < n->key) head = &(*head)->next; n->next = *head; *head = n; }'
OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 OPTACC_INLINE=1 \
    OPTACC_PEEP=1 OPTACC_MIR=1 \
    ssa "a smaller way a twelfth costlier, not kept" "ssa f made" "$sorted_insert"
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
# A long whose low three bytes are all that is kept is made here, read as
# an int.
leafs "a long read and kept as an int, made here" yes \
    'void g(long *); int f(void) { long v = 0; g(&v); return (int) v; }'
# One whose four bytes are wanted -- its truth, a sum, a compare -- is
# made here too, the long in a frame slot and each instruction with it in
# made by the first pass's code.
leafs "a long made a _Bool, made here too"        yes \
    'int f(const long *p) { return (_Bool) *p; }'
leafs "a long sum kept, made here too"            yes \
    'int f(const unsigned char *p, int n) { unsigned long s = 0; for (int i = 0; i < n; i++) s = s * 31 + p[i]; return (int) (s >> 8); }'
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
# A member's bytes read through a cast and a constant index -- ez80asm's
# REGSETBYTE -- read at (iy+d) too, the index in the displacement, and no
# address made in HL first.
bytes='typedef struct { unsigned char r0; int i; } dop;
int f(dop *op, int n) { int s = 0; while (n--) { s += ((const unsigned char *) &op->i)[2] & ((const unsigned char *) &op->i)[-1]; op++; } return s; }'
OPTACC_LEAF=1 all "$OPT" "a member's byte by index, ld a, (iy+3)" fd7e03 yes "$bytes"
OPTACC_LEAF=1 all "$OPT" "and by a negative one, ld a, (iy+0)"   fd7e00 yes "$bytes"
OPTACC_LEAF=1 all "$OPT" "no lea hl, iy+1 for it"                 ed2301 no  "$bytes"
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
# A signed comparison takes no register but HL -- BC is neither loaded
# nor saved around it, as moving both sides by 0x800000 did.
sorted='struct node { int key; struct node *next; };
void f(struct node **head, struct node *n) { while (*head && (*head)->key < n->key) head = &(*head)->next; n->next = *head; *head = n; }'
OPTACC_LEAF=1 all "$OPT" "a signed compare, BC free, not saved"  c501000080 no "$sorted"
# A local whose address is taken stays in memory, and is no `register`
# local: IY is free for the loop's pointer all the same.
iyfree='int g(int *); int f(const char *s, int n) { int x = 0; int c = 0; g(&x); while (n--) c += *s++; return c + x; }'
OPTACC_LEAF=1 all "$OPT" "IY beside a local in memory, inc iy"   fd23 yes "$iyfree"
# IY shared as BC is, by values live in different places: the first
# loop's pointer, loaded into it, and the second loop's counter after it.
twoloops='int f(const char *s, const char *t, int n) { int sum = 0, k = n; while (k--) sum += *s++; k = n; while (k--) sum -= *t++; return sum; }'
OPTACC_LEAF=1 all "$OPT" "IY for one loop's pointer and the next's count" 'dd3106.*fde1' yes "$twoloops"
# A short stepped or read through a pointer, by the first pass's code; and
# a frame past what (ix+d) reaches, its far locals where the arrays are:
# made here.
leafs "a short stepped through a pointer, made here" yes \
    'struct c { char tag; unsigned short count; }; int f(struct c *p, int n) { int t = 0; while (n--) { p->count++; t += p->count; p++; } return t; }'
leafs "a frame past (ix+d), made here"           yes \
    'int f(int n) { int v0 = n; int v1 = v0 + 1; int v2 = v1 + 2; int v3 = v2 + 3; int v4 = v3 + 4; int v5 = v4 + 5; int v6 = v5 + 6; int v7 = v6 + 7; int v8 = v7 + 8; int v9 = v8 + 9; int v10 = v9 + 10; int v11 = v10 + 11; int v12 = v11 + 12; int v13 = v12 + 13; int v14 = v13 + 14; int v15 = v14 + 15; int v16 = v15 + 16; int v17 = v16 + 17; int v18 = v17 + 18; int v19 = v18 + 19; int v20 = v19 + 20; int v21 = v20 + 21; int v22 = v21 + 22; int v23 = v22 + 23; int v24 = v23 + 24; int v25 = v24 + 25; int v26 = v25 + 26; int v27 = v26 + 27; int v28 = v27 + 28; int v29 = v28 + 29; int v30 = v29 + 30; int v31 = v30 + 31; int v32 = v31 + 32; int v33 = v32 + 33; int v34 = v33 + 34; int v35 = v34 + 35; int v36 = v35 + 36; int v37 = v36 + 37; int v38 = v37 + 38; int v39 = v38 + 39; int v40 = v39 + 40; int v41 = v40 + 41; int v42 = v41 + 42; int v43 = v42 + 43; int v44 = v43 + 44; int v45 = v44 + 45; int v46 = v45 + 46; int v47 = v46 + 47; int v48 = v47 + 48; int v49 = v48 + 49; int v50 = v49 + 50; int v51 = v50 + 51; int v52 = v51 + 52; int v53 = v52 + 53; int v54 = v53 + 54; int v55 = v54 + 55; int v56 = v55 + 56; int v57 = v56 + 57; int v58 = v57 + 58; int v59 = v58 + 59; int v60 = v59 + 60; int v61 = v60 + 61; int v62 = v61 + 62; int v63 = v62 + 63; int v64 = v63 + 64; int v65 = v64 + 65; int v66 = v65 + 66; int v67 = v66 + 67; int v68 = v67 + 68; int v69 = v68 + 69; int k = n, s = 0; while (k--) s += v69; return s; }'
# A function of more than 64 locals within reach -- 70 chars, and a loop's
# counter and sum declared after them -- keeps them as values: the sum is
# in IY, and the answer read from it.
c70='int f(int n) { char v0 = (char) n; char v1 = (char) (v0 + 1); char v2 = (char) (v1 + 2); char v3 = (char) (v2 + 3); char v4 = (char) (v3 + 4); char v5 = (char) (v4 + 5); char v6 = (char) (v5 + 6); char v7 = (char) (v6 + 7); char v8 = (char) (v7 + 8); char v9 = (char) (v8 + 9); char v10 = (char) (v9 + 10); char v11 = (char) (v10 + 11); char v12 = (char) (v11 + 12); char v13 = (char) (v12 + 13); char v14 = (char) (v13 + 14); char v15 = (char) (v14 + 15); char v16 = (char) (v15 + 16); char v17 = (char) (v16 + 17); char v18 = (char) (v17 + 18); char v19 = (char) (v18 + 19); char v20 = (char) (v19 + 20); char v21 = (char) (v20 + 21); char v22 = (char) (v21 + 22); char v23 = (char) (v22 + 23); char v24 = (char) (v23 + 24); char v25 = (char) (v24 + 25); char v26 = (char) (v25 + 26); char v27 = (char) (v26 + 27); char v28 = (char) (v27 + 28); char v29 = (char) (v28 + 29); char v30 = (char) (v29 + 30); char v31 = (char) (v30 + 31); char v32 = (char) (v31 + 32); char v33 = (char) (v32 + 33); char v34 = (char) (v33 + 34); char v35 = (char) (v34 + 35); char v36 = (char) (v35 + 36); char v37 = (char) (v36 + 37); char v38 = (char) (v37 + 38); char v39 = (char) (v38 + 39); char v40 = (char) (v39 + 40); char v41 = (char) (v40 + 41); char v42 = (char) (v41 + 42); char v43 = (char) (v42 + 43); char v44 = (char) (v43 + 44); char v45 = (char) (v44 + 45); char v46 = (char) (v45 + 46); char v47 = (char) (v46 + 47); char v48 = (char) (v47 + 48); char v49 = (char) (v48 + 49); char v50 = (char) (v49 + 50); char v51 = (char) (v50 + 51); char v52 = (char) (v51 + 52); char v53 = (char) (v52 + 53); char v54 = (char) (v53 + 54); char v55 = (char) (v54 + 55); char v56 = (char) (v55 + 56); char v57 = (char) (v56 + 57); char v58 = (char) (v57 + 58); char v59 = (char) (v58 + 59); char v60 = (char) (v59 + 60); char v61 = (char) (v60 + 61); char v62 = (char) (v61 + 62); char v63 = (char) (v62 + 63); char v64 = (char) (v63 + 64); char v65 = (char) (v64 + 65); char v66 = (char) (v65 + 66); char v67 = (char) (v66 + 67); char v68 = (char) (v67 + 68); char v69 = (char) (v68 + 69); int k = n, s = 0; while (k--) s += v69; return s; }'
OPTACC_LEAF=1 all "$OPT" "a sum in IY after 70 locals"  ed2300ddf9 yes "$c70"

# With OPTACC_INLINE, a static function called from one place, its address
# never taken, has its whole body read in place of the call, and is gone
# from the object -- where that leaves the caller no bigger than it and the
# body were apart, which two compiles before the one kept measure.
# inlined <name> <function> <yes|no> <source>
inlined() {
    local what=$1 want=$3 got=yes

    printf '%s\n' "$4" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    if ! OPTACC_INLINE=1 OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 \
         OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
         "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" -map "$tmp/c.map" >/dev/null 2>&1; then
        printf '  FAIL %-50s could not compile it\n' "$what"
        fail=$((fail + 1)); return
    fi
    grep -q "^$2 " "$tmp/c.map" && got=no
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s %s read in place: want %s, got %s\n' "$what" "$2" "$want" "$got"
        fail=$((fail + 1))
    fi
}
pick='static int pick(int a, int b) { if (a > b) return a; return b; }'
inlined "a static called once, read in place"     pick yes \
    "$pick int f(int x, int y) { return pick(x, y) + 1; }"
inlined "one called twice, called"                pick no \
    "$pick int f(int x, int y) { return pick(x, y) + pick(y, x); }"
inlined "one whose address is taken, called"      pick no \
    "$pick int (*fp)(int, int) = pick; int f(int x, int y) { return pick(x, y); }"
inlined "a void one, read in place"               put yes \
    'static void put(char *p, int n) { while (n--) *p++ = 0; } void f(char *q) { put(q, 4); }'
# suffix_bit, from zap: its tests are smaller as the first pass makes them,
# the loop calling it as the leaf backend does, and merged one backend
# makes both -- 88 bytes more than apart. So it is called.
sfx='enum { S_SIS = 1, S_LIS, S_SIL, S_LIL }; struct st { int adl; } state; const char *mnemonic_of(const char *s, int n);
static int suffix_bit(const char *t, int n, int adl, unsigned char *out) { const char c0 = (char) (t[0] | 0x20); const char c1 = n > 1 ? (char) (t[1] | 0x20) : 0; const char c2 = n > 2 ? (char) (t[2] | 0x20) : 0;
 if (n == 1) { if (c0 == 115) { *out = adl ? S_SIL : S_SIS; return 1; } if (c0 == 108) { *out = adl ? S_LIL : S_LIS; return 1; } return 0; }
 if (n == 2) { if (c0 != 105) return 0; if (c1 == 115) { *out = adl ? S_LIS : S_SIS; return 1; } if (c1 == 108) { *out = adl ? S_LIL : S_SIL; return 1; } return 0; }
 if (n == 3 && c1 == 105) { if (c0 == 115 && c2 == 115) { *out = S_SIS; return 1; } if (c0 == 115 && c2 == 108) { *out = S_SIL; return 1; } if (c0 == 108 && c2 == 115) { *out = S_LIS; return 1; } if (c0 == 108 && c2 == 108) { *out = S_LIL; return 1; } }
 return 0; }
const char *f(const char *s, int n, unsigned char *suffix) { int i = 1; while (i < n && s[i] != 46) i++; if (i == n) return 0; if (!suffix_bit(&s[i + 1], n - i - 1, state.adl, suffix)) return 0; return mnemonic_of(s, i); }'
inlined "one that makes its caller bigger, called" suffix_bit no "$sfx"
# What the source says decides over those: always_inline read in place
# however many call it and whatever it makes its caller -- zap asks it of
# its encoder's helpers -- and noinline never, called once or not. (Each
# call a statement of its own: one made while a value waits, as the second
# of pick(x, y) - pick(y, x), is a call however it is said.)
inlined "always_inline called twice, read in place"  pick yes \
    "${pick/static/static __attribute__((always_inline))} int f(int x, int y) { int a = pick(x, y); int b = pick(y, x); return a - b; }"
inlined "always_inline, its caller bigger, read in place" suffix_bit yes \
    "${sfx/static int suffix_bit/static __attribute__((always_inline)) int suffix_bit}"
inlined "noinline called once, called"               pick no \
    "${pick/static/static __attribute__((noinline))} int f(int x, int y) { return pick(x, y) + 1; }"
# Called in a loop, the same body makes its caller 72 bytes bigger, and is
# read in place where a caller with a call in a loop may grow that much
# (OPTACC_INLINE_LOOP): the call is made each time round. Called outside
# one, it is not, whatever the allowance; and the allowance as it is, 32
# bytes, does not stretch to 72.
sfx_body=$(printf '%s\n' "$sfx" | sed '$d')
sfx_loop="$sfx_body
int f(const char **v, int n, unsigned char *suffix) { int i, k = 0; for (i = 0; i < n; i++) { if (!suffix_bit(v[i], 3, state.adl, suffix)) continue; k++; } return k; }"
OPTACC_INLINE_LOOP=80 inlined "one called in a loop, its caller let grow"  suffix_bit yes "$sfx_loop"
OPTACC_INLINE_LOOP=80 inlined "but not where it is called once, outside"   suffix_bit no  "$sfx"
inlined "nor past the allowance"                    suffix_bit no  "$sfx_loop"
# The machine-level backend's functions ranked with the leaf backend's,
# both holding values in registers: goto_statement merged, made by it,
# takes back_to and back_in, which the leaf backend makes apart.
rk='typedef struct { int serial, mark, vm_last; } VlaBlock;
typedef struct { int at; VlaBlock *blocks; int nblocks; } Label;
extern Label *labels;
extern VlaBlock *vla_blocks;
extern int nvla_blocks, tok_line;
int label_find(int name, int line);
void fail_at(int line, const char *what);
int gen_jump(void);
void gen_jump_to(int at);
void gen_unwind(int to);
int gen_goto_hole(void);
void goto_add(int hole, int label, int line);
int next_name(void);

static int back_to(const VlaBlock *then, int nthen)
{
    int common = -1, i;

    for (i = 0; i < nvla_blocks && i < nthen; i++) {
        if (vla_blocks[i].serial != then[i].serial)
            break;
        common = i;
    }
    if (common < 0)
        return -1;
    if (then[common].mark == -1 && vla_blocks[common].mark != -1)
        return vla_blocks[common].mark;
    for (i = common + 1; i < nvla_blocks; i++)
        if (vla_blocks[i].mark != -1)
            return vla_blocks[i].mark;

    return -1;
}

static int back_in(const VlaBlock *to, int nto)
{
    int i;

    for (i = 0; i < nvla_blocks && i < nto && vla_blocks[i].serial == to[i].serial; i++)
        ;
    for (; i < nto; i++)
        if (to[i].vm_last > 0 && to[i].vm_last > to[i].serial)
            return 1;

    return 0;
}

void goto_statement(void)
{
    int line = tok_line, label = label_find(next_name(), line);

    if (labels[label].at >= 0) {
        int back = back_to(labels[label].blocks, labels[label].nblocks);

        if (back_in(labels[label].blocks, labels[label].nblocks))
            fail_at(line, "jumps into a VLA'\''s scope");
        if (back != -1)
            gen_unwind(back);
        gen_jump_to(labels[label].at);
    } else {
        goto_add(gen_goto_hole(), label, line);
    }
}'
OPTACC_MIR=1 OPTACC_PEEP=1 inlined "a leaf body in a caller MIR makes merged"  back_in yes "$rk"
# lists' insert_sorted: its loop made from its SSA form, and called from a
# function the first pass makes, one holding long longs -- smaller merged,
# and slower, the loop in the first pass's code. So it is called.
sorted_in='struct node { int key; struct node *next; }; struct node pool[8]; unsigned long seed;
static void insert_sorted(struct node **head, struct node *n) { while (*head && (*head)->key < n->key) head = &(*head)->next; n->next = *head; *head = n; }
unsigned long long f(void) { unsigned long long state = seed, check = 0; struct node *head = 0; for (int i = 0; i < 8; i++) { state = state * 1103515245UL + 12345UL; pool[i].key = (int) (state >> 12 & 0x3fff); } for (int i = 0; i < 8; i++) insert_sorted(&head, &pool[i]); for (const struct node *n = head; n; n = n->next) check = check * 3 + (unsigned long long) n->key; return check; }'
inlined "one into a caller a worse backend makes, called" insert_sorted no "$sorted_in"
# The arguments stored to the body's locals as each is made, not left on
# the stack for the next call to spill: three locals and the answer, and
# no more, in the frame.
args='int g(int); static int mix(int a, int b, int c) { if (a > b) return a - c; return b + c; } int f(int k) { return mix(g(k), g(k + 1), g(k + 2)); }'
OPTACC_FRAME_CALL=1 OPTACC_INLINE=1 emits "$OPT" "arguments stored as they are made"  21f4ffffcd yes "$args"
# A body's locals give their room back where it ends: the second body's
# take the first's, and the frame is 18 bytes, not 30.
twobodies='void touch(int *, int *); static int a(int k) { int x, y; touch(&x, &y); return x + y + k; } static int b(int k) { int u, v; touch(&u, &v); return u - v + k; } int f(int k) { int s = a(k); int t = b(k); return s + t; }'
OPTACC_FRAME_CALL=1 OPTACC_INLINE=1 all "$OPT" "two bodies' locals in the same room"  21eeffffcd yes "$twobodies"
# A slot one body gave back and another takes as another type is not
# shared as a VLA's length is: neither reads the other's, and the function
# is made from its SSA form.
retyped='int g(int); static int a(int x, int n) { int last; if (n) last = g(x); if (n) return last; return x; } static char *b(char *x, int n) { char *hit; if (n) hit = x + g(n); if (n) return hit; return x; } char *f(char *q, int n) { int s = a(n, n); char *t = b(q, s); return t; }'
OPTACC_INLINE=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
    OPTACC_LEAF=1 ssa "a slot given back, taken as another type" \
    "ssa f made, not by the leaf backend" "$retyped"

# The machine-level backend (docs/machine-ir-backend.md), with OPTACC_MIR:
# a function of ints and chars, calling others or not, is made by it --
# `mir f` -- and a function of 600 statements in a few seconds, every pass of it
# linear or n log n: its first allocator, an interference graph, took over
# an hour on gcc's pr69592, which this is.
mirs() {
    local what=$1 want=$2 got=no

    printf '%s\n' "$3" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
        OPTACC_LEAF=1 OPTACC_MIR=1 OPTACC_PICK=0 OPTACC_SSA_STATS=1 \
        timeout 20 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 \
        | grep -q '^mir f$' && got=yes
    if [ "$got" = "$want" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s mir: want %s, got %s\n' "$what" "$want" "$got"
        fail=$((fail + 1))
    fi
}
# mir <compiler> <name> <hex> <yes|no> <source>: emits, made by the machine IR.
mir() {
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
        OPTACC_LEAF=1 OPTACC_MIR=1 OPTACC_PICK=0 emits "$@"
}
# With OPTACC_MIR_SPLIT, the allocator that splits intervals: a function
# with more live than there are registers, made by it, its moves joining
# the parts checked on every path before the code is made.
printf '%s\n' 'int f(const char *a, const char *b, int n) { while (n-- > 0) if (*a++ != *b++) return 0; return 1; }' > "$tmp/c.c"
rm -f "$tmp/c.o"
if OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
    OPTACC_LEAF=1 OPTACC_MIR=1 OPTACC_MIR_SPLIT=1 OPTACC_PICK=0 OPTACC_SSA_STATS=1 \
    timeout 20 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 | grep -q '^mir f, split into [1-9]'; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s\n' "more live than registers, split"
    fail=$((fail + 1))
fi
mirs "a loop on chars, made by the machine IR"    yes \
    'int f(const char *s) { int n = 0; while (*s) if (*s++ == 32) n++; return n; }'
mirs "a string's address, made by it"             yes \
    'const char *f(int c) { if (c) return "yes"; return "no"; }'
mirs "and a static's"                               yes \
    'static int count; int *f(void) { return &count; }'
mirs "one that calls"                             yes \
    'int g(int); int f(int a) { return g(a) + 1; }'
mirs "and a value kept across the call"             yes \
    'int g(int); int f(int a) { int b = a * 3; return g(a) + b + a; }'
# The allocator, spilling everything it cannot hold, never gives up where
# a spill would do: two bytes that each must be in A, one of them spilled
# and its claim on A still counted this round -- the spills made, and the
# scan again; a byte made in A read back as an or's right, not held to A
# then; four bytes in B, C, D and E with a pair wanted, one of them
# spilled to free it; and a value made in HL read back for a copy, into
# any pair.
mirs "two bytes that must be in A, made by it" yes \
    'struct d { unsigned char r1, r2; }; unsigned char f(const struct d *op) { if (op->r1 & 4) return 0xdd; if ((op->r1 & 8) | (op->r2 & 16)) return 0xfd; return 0; }'
mirs "four bytes held with a pair wanted, made by it" yes \
    'extern const unsigned char hv[]; int f(const char *d, int n) { unsigned char bad = 0, a, b, c, e; int r; if (n < 4) return -1; a = hv[(unsigned char) d[0]]; b = hv[(unsigned char) d[1]]; c = hv[(unsigned char) d[2]]; e = hv[(unsigned char) d[3]]; bad = a | b | c | e; r = (a << 12) + (b << 8) + (c << 4) + e; return bad & 0x80 ? -1 : r; }'
mirs "a value spilled from HL read back for a copy" yes \
    'struct v { char kind; char t; int val; int ext; unsigned char x; }; extern struct v *vsp; extern int vtop; void err(const char *); char f(int depth) { if ((unsigned) vtop <= (unsigned) depth) err("x"); return (vsp - 1 - depth)->t; }'
# A block's static, its bytes laid down where the code starts, and an
# inlined body's room, made slots of this frame: made here, as the leaf
# backend makes them (test/cases/377 checks what they come to).
mirs "a block's static, made by it"            yes \
    'const char *f(int k) { static const char n[] = "statics"; return n + k; }'
mirs "an inlined body's room, made by it"      yes \
    '__attribute__((always_inline)) static inline int g(int v, int w) { return v * w + 3; } int f(int v) { return g(v + 1, v) + 1; }'
mirs "but not setjmp, which IY and BC would not survive" no \
    'int setjmp(void *); int f(void *b) { return setjmp(b); }'
mirs "nor exit, which gen_call makes in place"     no \
    'void exit(int); void f(int k) { if (k) exit(k); }'
# memcpy, memmove, memset and memchr by name: made in place, the runtime's
# routine called with its operands in registers -- BC the count, HL and DE
# or HL and A -- and what is live across it kept as round any call
# (test/cases/385 runs them).
mirs "but memcpy, made in place"                   yes \
    'void *memcpy(void *, const void *, unsigned); void f(char *d, char *s) { memcpy(d, s, 4); }'
mirs "and memset, memchr and memmove"              yes \
    'void *memset(void *, int, unsigned); void *memchr(const void *, int, unsigned); void *memmove(void *, const void *, unsigned); char *f(char *p, int n) { memset(p, 0, n); memmove(p + 1, p, n); return memchr(p, 1, n); }'
mir "$OPT" "the count in BC before the fill"           010a0000e577  yes \
    'void *memset(void *, int, unsigned); struct s { char a[10]; int n; }; void f(struct s *p, int k) { memset(p->a, k, sizeof p->a); p->n = k; }'
# And one with an initial value: its bytes laid down where the code
# starts and jumped over (test/cases/394).
mirs "a block static with a value, made by it"       yes \
    'int f(int k) { static const int t[3] = { 5, 6, 7 }; return t[k]; }'
# memset of two or more and memcpy of one or more, a count known, made in
# place -- ldir with no call (mem_in_place) -- by the first pass and the
# machine IR; a count not known, or a fill of one, still a call
# (test/cases/399 runs them).
memdecl='void *memset(void *, int, unsigned); void *memcpy(void *, const void *, unsigned);'
memfill="$memdecl struct t { char *a, *b; char c; }; void f(struct t *p) { memset(p, 0, sizeof *p); }"
emits "$OPT" "a fill of a known count in place"     e577e5d1130bedb0e1 yes "$memfill"
wants "$OPT" "and no call to acc_rt_memset"          _acc_rt_memset no "$memfill"
mir "$OPT" "in place in the machine IR too"          e577e5d1130bedb0e1 yes "$memfill"
emits "$OPT" "a copy of a known count in place"      d5edb0e1 yes \
    "$memdecl void f(char *p, const char *q) { memcpy(p, q, 5); }"
wants "$OPT" "a fill of a count not known: a call"   _acc_rt_memset yes \
    "$memdecl void f(char *p, unsigned n) { memset(p, 0, n); }"
wants "$OPT" "a fill of one: a call"                 _acc_rt_memset yes \
    "$memdecl void f(char *p) { memset(p, 0, 1); }"
# A function's strings, all laid down where its code starts behind one
# jump -- jr past "ab" and "cd" -- not each jumped over where it is read
# (test/cases/401).
mir "$OPT" "strings behind one jump" 1806616200636400 yes \
    'const char *f(int k) { return k ? "ab" : "cd"; }'
# And ones whose values hold addresses: a string of the same initial
# value, a global, a static at zero -- each moved with what it points at
# (test/cases/396).
mirs "a block static of strings, made by it"         yes \
    'const char *f(int k) { static const char *w[] = { "ab", "cd" }; return w[k]; }'
mirs "a block static with a global's address, made by it" yes \
    'int g; int *f(void) { static int *p = &g; return p; }'
mirs "a block static with a zero one's address, made by it" yes \
    'int f(void) { static int n; static int *p = &n; return ++*p; }'
# A block static that starts at zero: its room in the bss reserved again
# where the code starts, and the function made by it (test/cases/392).
mirs "a block static at zero, made by it"            yes \
    'int f(void) { static int n; return ++n; }'
# Structs by value: a call answering one, its room in the frame; one
# passed as words; one returned, copied to where the caller asked
# (test/cases/393 runs them).
keyt='typedef struct { char ch, vkey, mods; } key; key next(int); int take(int, key);'
mirs "a call answering a struct, made by it"     yes \
    "$keyt int f(int k) { key kp = next(k); return kp.ch + kp.mods; }"
mirs "a struct passed by value"                  yes \
    "$keyt extern key g; int f(void) { return take(1, g); }"
mirs "a struct returned"                         yes \
    "$keyt key f(const key *p) { return *p; }"
# Bytes C widens and the code cuts back, made as bytes: (char) (ch + 32)
# an add a, 32, and the ?: of two chars returned as a char joined as the
# byte -- not widened by rlc l / sbc hl, hl and cut again
# (test/cases/391 runs them).
fold='char f(char ch) { return (ch >= 65 && ch <= 90) ? (char) (ch + 32) : ch; }'
mir "$OPT" "(char) (ch + 32) as add a, 32"         c620       yes "$fold"
mir "$OPT" "the ?: joined as the byte"              cb05ed62   no  "$fold"
# A pair free for any: HL first, whose ld hl, (nn) and ld (nn), hl are a
# byte shorter than BC's.
mir "$OPT" "a static copied through HL"              '^2a00000022000000c9$' yes \
    'extern int a, b; void f(void) { b = a; }'
# A struct assigned through a pointer: its bytes copied with ldir, from a
# global's or another pointer's, one after another in a loop -- but not
# assigned twice over, where the first copy is a value read again
# (test/cases/380 checks what they come to).
mirs "a struct copied from a global's, made by it"   yes \
    'struct d { int a, b; char c; }; extern const struct d none; void f(struct d *p) { *p = none; }'
mirs "and from another pointer's, in a loop"       yes \
    'struct d { int a, b; char c; }; void f(struct d *p, const struct d *q, int n) { while (n--) *p++ = *q++; }'
mirs "but not assigned twice over"                 no \
    'struct d { int a, b; char c; }; void f(struct d *p, struct d *q, const struct d *r) { *p = *q = *r; }'
# A pointer copied into IY for each member read is given IY itself, the
# copies coming to nothing -- ld bc, (iy+0), not push iy / pop hl first --
# and kept there across the call with push iy around it. And a constant
# returned is made as the constant, not tested against zero again: where
# the function answers in A, a _Bool's 0 is ld a, 0 and on to the return.
members='struct s { int a, b; }; int g(int); int f(struct s *p) { return g(p->a) + p->b; }'
mir "$OPT" "a pointer in IY, read through"           fd2700     yes "$members"
mir "$OPT" "not copied there from HL"                fde5e1ed07 no  "$members"
# A loop's pointer -- written where the loop starts and again on its way
# round -- copied into IY for each member read: those copies are the
# pointer, so it can be in IY itself, stepped there by inc iy, not kept
# in the frame and loaded into IY again for every read.
looped='typedef struct { unsigned char tag, a, b; } item;
int f(const item *p, int n) { int t = 0; while (n--) { if (p->a & 1) t += p->b; else t -= p->tag; p++; } return t; }'
mir "$OPT" "a loop's pointer kept in IY, inc iy"     fd23fd23fd23 yes "$looped"
mir "$OPT" "not loaded into IY for each read"        dd31f7       no  "$looped"
# And where the pointer comes into the loop through another register --
# IY wanted for another read just then, as processInstructions reads its
# instruction's count -- it is still IY in the loop, which its reads
# outweigh the one copy at the start by far: not copied in for each.
entered='typedef struct { unsigned char a, b, c, d; } ent; typedef struct { int pad; ent *list; unsigned char n; } ins;
ins *cur; unsigned char want, other; int hits;
void f(void) { ent *l = cur->list; unsigned char k = cur->n; for (; k; k--, l++) if ((l->a & 15) == want && (l->b & 15) == other && (l->c | l->d)) { hits++; break; } }'
mir "$OPT" "a pointer entering in HL, IY in the loop" fd23fd23fd23fd23 yes "$entered"
mir "$OPT" "not copied there for each read"          e5fde1       no  "$entered"
# A call on a loop's way out -- error() and then return 0 -- is across
# nothing the loop still reads: its argument pushed and no more, not the
# pointer, the count and the sum pushed and popped around it because
# their intervals reach past it in the order the blocks are laid out.
leaving='void error(int);
int f(const char *p, int n) { int t = 0; while (n--) { if (*p == 0) { error(t); return 0; } t += *p++; } return t; }'
mir "$OPT" "nothing pushed around a call on the way out" fde5c5d5 no  "$leaving"
mir "$OPT" "but its argument"                         c5cd         yes "$leaving"
# A switch on a char, which C makes an int: its cases compared as the
# byte it is, cp n, not ld de, n / or a / sbc hl, de / add hl, de of the
# int read back from the switch's slot.
switched='int f(const char *p) { switch (*p++) { case 97: return 1; case 98: return 2; case 99: return *p; case 65: return 4; } return 0; }'
mir "$OPT" "a char's switch, cp n for its cases"     fe61         yes "$switched"
mir "$OPT" "not ld de, n and a compare of the int"  116100       no  "$switched"
# An operator whose answer is read for its low byte alone -- stored to a
# byte -- made on the bytes: a long's | and an int's << 3 into a byte
# member, or a, b and add a, a three times, with no runtime routine, and
# by this backend at all, which takes no long operator narrowed.
narrowed='typedef struct { unsigned char opcode, x; } outp; typedef struct { unsigned char reg_index; long immediate; } opnd;
void put(outp *);
void f(const opnd *op) { outp output; output.opcode = 0x40; output.opcode |= op->immediate; output.x = 3; output.opcode |= (op->immediate << 3); put(&output); }'
mirs "a long narrowed to a byte, made by it"       yes "$narrowed"
mir "$OPT" "the shift made on the byte, add a, a"    878787       yes "$narrowed"
# A switch on a long whose cases all fit in 24 bits, made by this backend:
# its value narrowed to 24 bits where its top byte only widens them --
# push hl / add hl, hl / pop hl / sbc a, a / cp e / jr z -- and its cases
# compared as 24 bits.
longsw='typedef struct { long immediate; } opnd;
int f(const opnd *op) { int y; switch (op->immediate) { case 0: y = 0; break; case 1: y = 2; break; case 2: y = 3; break; case -5: y = 9; break; default: y = 7; } return y << 3; }'
mirs "a switch on a long, made by it"             yes "$longsw"
mir "$OPT" "its value narrowed to 24 bits"           e529e19fbb2804 yes "$longsw"
# A member's byte through a cast of its address and a constant index --
# ez80asm's REGSETBYTE -- read where the member is, (iy+2), the cast and
# the index in the displacement: no address made with ld bc, 1 / add hl, bc.
regbyte='typedef struct { unsigned char tag; unsigned set; } entry; unsigned want;
int f(const entry *e, int n) { int hits = 0; while (n--) { if (((const unsigned char *) &e->set)[1] & ((const unsigned char *) &want)[2]) hits++; e++; } return hits; }'
mir "$OPT" "a member's byte by index, at (iy+2)"     'fd[4-7][6e]02' yes "$regbyte"
mir "$OPT" "its address not made, no add of 1"       0101000009   no  "$regbyte"
truth='_Bool f(int *p) { if (!p) return 0; if (*p == 3) return 1; return 0; }'
mir "$OPT" "a _Bool's constant returned as it is"     3e00         yes "$truth"
# A byte made in A -- masked, or read from a static -- and wanted there
# again before it is read: moved to another byte register, ld r, a, not
# stored to the frame and read back from it.
masked='unsigned char m1, m2;
int f(const unsigned char *p, int n) { int hits = 0; while (n--) { if ((p[0] & 0x0f) == m1 && (p[1] & 0x0f) == m2) hits++; p += 2; } return hits; }'
mir "$OPT" "a masked byte not stored to the frame"   e60fdd77     no  "$masked"
mir "$OPT" "nor a static's byte read"                3a000000dd77 no  "$masked"
mir "$OPT" "but kept in a register, ld r, a"         'e60f[4-6][7f]' yes "$masked"
# An && kept in a _Bool -- ez80asm's condmatch -- is a 0 or a 1, which is
# all its sets give it: made a _Bool it is its low byte, with no test of
# all three bytes and the truth made again (sbc hl, bc / ld a, 0 / jr z).
both='unsigned char m1, m2; int hits;
void f(const unsigned char *p, int n) { while (n--) { _Bool both = p[0] == m1 && p[1] == m2; if (p[2] & 4) { if (both || m1) hits++; } else if (both) hits += 2; p += 3; } }'
mir "$OPT" "an && kept as a _Bool, not tested again"  ed423e0028   no  "$both"
# A byte's & with a static's byte, the static read last: the static's
# byte, made in A by ld a, (nn), is the side in A -- and a, h straight
# after it -- not moved out for the other side to be brought in.
anded='unsigned char g0, g1, g2;
int f(const unsigned char *p, int n) { int hits = 0; while (n--) { if ((p[0] & g0) | (p[1] & g1) | (p[2] & g2)) hits++; p += 3; } return hits; }'
mir "$OPT" "a static's byte and'ed where it is read"   '3a000000a[0-5]' yes "$anded"
mir "$OPT" "not tested again"                   2100000009b7ed42 no "$truth"
# A struct's bytes copied by ldir, BC their count.
mir "$OPT" "the copy: ld bc, 7 / ldir"            01070000edb0 yes \
    'struct d { int a, b; char c; }; void f(struct d *p, const struct d *q) { *p = *q; }'

# DE kept across a call that answers a long: D popped back, E the answer's
# -- ld a, e / pop de / ld e, a (test/cases/382 runs it).
kept_de='unsigned long mix(unsigned long h, int x);
int g(int);
unsigned long f(const unsigned *v, int n)
{
    unsigned long h = 0;

    for (int i = 0; i < n; i++) {
        _Bool b = v[i] & 0x80;

        if (v[i] & 0x8000)
            h = mix(h, 1);
        h = mix(h, b + g(i));
    }
    return h;
}'
mir "$OPT" "a long's E kept over DE popped back"        7bd15f    yes "$kept_de"

# A switch on an int or a char: its value read once, each case compared
# and branched on -- ld de, n / or a / sbc hl, de / add hl, de, HL kept for
# the next case -- but not on a long with a case wider than 24 bits
# (test/cases/383 runs them).
swint='int f(int x) { switch (x) { case 1: return 10; case 2: return 20; case 0: return 5; } return 0; }'
mirs "a switch on an int, made by it"                yes "$swint"
mirs "and on a char"                                 yes \
    'int f(signed char c) { switch (c) { case 1: return 10; case -2: return 20; case 300: return 7; default: return 1; } }'
mirs "but not on a long with a wider case"           no \
    'int f(long x) { switch (x) { case 1: return 10; case 0x1000000: return 3; } return 0; }'
mir "$OPT" "a case: HL kept for the next"          11010000b7ed5219 yes "$swint"

# A value made before a run of calls and read once after them: kept in
# its slot -- ld (ix-3), hl once -- not pushed and popped round each call.
across='void g(int); int f(int *p) { int k = *p; g(1); g(2); g(3); g(4); return k; }'
mir "$OPT" "a value across four calls, in its slot"    dd2ffd        yes "$across"
mir "$OPT" "not pushed and popped round them"         cd000000d1c1  no  "$across"

# A join whose copies come from values in their slots: each read after
# the copies, into where it goes -- reloaded first, each into a pair of
# its own, zap's macro_subst wanted more pairs than there are and was
# refused (test/cases/386 runs it).
subst='typedef unsigned int size_t;
void *realloc(void *p, size_t n);
typedef struct { int off; unsigned char k; unsigned char len; } macmark;
typedef struct { unsigned char nparam; const char *params; char *body; int bodylen; } macro;
extern int margn[64];
int f(const macro* m, int lo, int hi, int base,
                       const macmark** mkp, const macmark* mkend,
                       char** bufp, int* capp) {
    char* out = *bufp;
    int cap = *capp;
    int len = 0;
    int cur = lo;
    const macmark* mk = *mkp;
    while (mk < mkend && mk->off < hi) {
        const int span = mk->off - cur;
        const int need = margn[base + mk->k];
        if (len + span + need + 2 > cap) {
            cap = (len + span + need + 2) * 2;
            char* grown = (char*) realloc(out, (size_t) cap);
            if (grown == ((void *) 0)) {
                return -1;
            }
            out = grown;
        }
        len += need;
        cur = mk->off + mk->len;
        mk++;
    }
    const int span = hi - cur;
    if (len + span + 2 > cap) {
        return -2;
    }
    for (int i = 0; i < span; i++) {
        out[len + i] = m->body[cur + i];
    }
    *bufp = out;
    *capp = cap;
    *mkp = mk;
    return len + span;
}'
OPTACC_CACHE_NONE=1 mirs "a join of values in their slots, made by it" yes "$subst"

# A function's address as a value: the constant, moved as the link moves
# the image, where the function is defined already; a load the link fills
# in where it is not yet (test/cases/387 runs both).
mirs "a defined function's address, made by it"   yes \
    'static int lt(int a, int b) { return a < b; } int apply(int (*)(int, int), int, int); int f(int k) { return apply(lt, k, 2); }'
mirs "and one defined further down"               yes \
    'int gt(int, int); int apply(int (*)(int, int), int, int); int f(int k) { return apply(gt, k, 2); }'

# Frames the machine-level backend lays out (test/cases/381 checks what
# they come to): spill slots shared by values never kept at once -- three
# runs of six values held across calls, a frame of nine bytes, not
# eighteen or more -- and a function of 45 locals, all of them values and given no
# room, with an inlined body's array in (ix+d)'s reach.
runs='int g(int);
void h(int, int, int, int, int, int);
void f(int k)
{
    int a = g(k), b = g(a), c = g(b), d = g(c), e = g(d), x = g(e);

    h(a, b, c, d, e, x);
    {
        int p = g(1), q = g(p), r = g(q), s = g(r), t = g(s), u = g(t);

        h(p, q, r, s, t, u);
    }
    {
        int p = g(2), q = g(p), r = g(q), s = g(r), t = g(s), u = g(t);

        h(p, q, r, s, t, u);
    }
}'
OPTACC_FRAME_CALL=1 mir "$OPT" "three runs' spills in a frame of nine bytes"  21f7ffff  yes "$runs"
mir "$OPT" "not eighteen"                                21eeffff  no  "$runs"
many='int g(int); __attribute__((always_inline)) static inline int sq(int v) { int t[2]; t[0] = v; t[1] = g(v); return t[0] * t[1]; } int f(int k) { int v1 = g(1); int v2 = g(2); int v3 = g(3); int v4 = g(4); int v5 = g(5); int v6 = g(6); int v7 = g(7); int v8 = g(8); int v9 = g(9); int v10 = g(10); int v11 = g(11); int v12 = g(12); int v13 = g(13); int v14 = g(14); int v15 = g(15); int v16 = g(16); int v17 = g(17); int v18 = g(18); int v19 = g(19); int v20 = g(20); int v21 = g(21); int v22 = g(22); int v23 = g(23); int v24 = g(24); int v25 = g(25); int v26 = g(26); int v27 = g(27); int v28 = g(28); int v29 = g(29); int v30 = g(30); int v31 = g(31); int v32 = g(32); int v33 = g(33); int v34 = g(34); int v35 = g(35); int v36 = g(36); int v37 = g(37); int v38 = g(38); int v39 = g(39); int v40 = g(40); int v41 = g(41); int v42 = g(42); int v43 = g(43); int v44 = g(44); int v45 = g(45);  return v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 + v20 + v21 + v22 + v23 + v24 + v25 + v26 + v27 + v28 + v29 + v30 + v31 + v32 + v33 + v34 + v35 + v36 + v37 + v38 + v39 + v40 + v41 + v42 + v43 + v44 + v45 +  sq(k); }'
mirs "45 locals that are values, an inlined array"  yes "$many"
# And merged: a body read in place, its room after the locals kept --
# none of the 45 -- not after all of them, past (ix+d)'s reach.
merged='int g(int); void h(int *); static void sq(int *o, int v) { int x = v; h(&x); *o = x; } int f(int k) { int v1 = g(1); int v2 = g(2); int v3 = g(3); int v4 = g(4); int v5 = g(5); int v6 = g(6); int v7 = g(7); int v8 = g(8); int v9 = g(9); int v10 = g(10); int v11 = g(11); int v12 = g(12); int v13 = g(13); int v14 = g(14); int v15 = g(15); int v16 = g(16); int v17 = g(17); int v18 = g(18); int v19 = g(19); int v20 = g(20); int v21 = g(21); int v22 = g(22); int v23 = g(23); int v24 = g(24); int v25 = g(25); int v26 = g(26); int v27 = g(27); int v28 = g(28); int v29 = g(29); int v30 = g(30); int v31 = g(31); int v32 = g(32); int v33 = g(33); int v34 = g(34); int v35 = g(35); int v36 = g(36); int v37 = g(37); int v38 = g(38); int v39 = g(39); int v40 = g(40); int v41 = g(41); int v42 = g(42); int v43 = g(43); int v44 = g(44); int v45 = g(45);  int r; sq(&r, k); return r +  v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9 + v10 + v11 + v12 + v13 + v14 + v15 + v16 + v17 + v18 + v19 + v20 + v21 + v22 + v23 + v24 + v25 + v26 + v27 + v28 + v29 + v30 + v31 + v32 + v33 + v34 + v35 + v36 + v37 + v38 + v39 + v40 + v41 + v42 + v43 + v44 + v45 +  0; }'
OPTACC_INLINE=1 mirs "45 values and a body read in place, made by it" yes "$merged"
# The room an inlined body takes is after the locals kept in memory, not
# after all the first pass declared: acc's declaration, most of whose
# locals are values, takes local_array and the rest in place.
rm -f "$tmp/k.o" "$tmp/k.map"
if OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
    OPTACC_LEAF=1 OPTACC_INLINE=1 OPTACC_PEEP=1 OPTACC_MIR=1 \
    timeout 20 "$OPT" -c test/optacc/kept.c -o "$tmp/k.o" -map "$tmp/k.map" >/dev/null 2>&1 \
    && grep -q '^declaration ' "$tmp/k.map" && ! grep -q '^local_array ' "$tmp/k.map"; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s\n' "acc's declaration, its bodies' room in reach"
    fail=$((fail + 1))
fi
# A round of the allocator that finds a register held by a value spilled
# already goes on scanning, and finds the rest of that round's spills: zap's
# assemble_line, merged, wants a dozen such rounds, and ran out at 16.
if OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
    OPTACC_LEAF=1 OPTACC_INLINE=1 OPTACC_PEEP=1 OPTACC_MIR=1 OPTACC_PICK=0 \
    OPTACC_CACHE_NONE=1 OPTACC_INLINE_LOOP=100000 OPTACC_SSA_STATS=1 \
    timeout 20 "$OPT" -c test/optacc/rounds.c -o "$tmp/r.o" 2>&1 | grep -q '^mir assemble_line$'; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s\n' "zap's assemble_line merged, made in rounds"
    fail=$((fail + 1))
fi
# A call's answer let go with (void); a _Bool made of an int or a pointer
# as an argument, 0 or 1 by its truth; and a _Bool predicate read in place
# in a loop's test, branched on as the flags that make it -- and 1 / jr z
# -- not made 0 or 1 in DE and tested again.
mirs "a call's answer cast to void, made by it"     yes \
    'int g(int); void h(void); int f(int a) { (void) g(a); (void) h(); (void) a; return a + 1; }'
mirs "a _Bool argument made of an int or a pointer" yes \
    'int g(_Bool, int); int f(int a, char *p) { return g(a, 1) + g(p, 2) + g(a & 4, 5); }'
blank='extern const unsigned char kinds[256];
__attribute__((always_inline)) static inline _Bool blank(char c) { return (kinds[(unsigned char) c] & 1) != 0; }
void g(const char *);
int f(const char *p, const char *e) { while (p < e && blank(*p)) p++; g(p); return 0; }'
mir "$OPT" "a _Bool predicate tested as the flags"      e6012803  yes "$blank"
mir "$OPT" "not made 0 or 1 first"                      e60111    no  "$blank"

# Longs in registers, E:UHL and A:UBC: a + b in line, add hl, bc / adc
# a, e / ld e, a, and nothing called; a long argument pushed as two slots
# from E:UHL, push de / push hl, and with a long answered the arguments let
# go into BC, E kept; an int made a long by its sign, push hl / add hl, hl
# / sbc a, a / pop hl / ld e, a; a comparison by the routine, its carry
# branched on, call / jr nc; and a loop's long, both of an add's operands
# made before either is put where it goes.
OPTACC_CACHE_NONE=1 mir "$OPT" "a + b of longs in line"            dd7e0f098b5fdde1c9 yes 'long f(long a, long b) { return a + b; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "a long argument as two slots"      d5e5cd000000 yes 'long g(long); long f(int x) { return g(x) + 1; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "and let go into BC, E:UHL kept"    cd000000c1c1 yes 'long g(long); long f(int x) { return g(x) + 1; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "an int made a long by its sign"    e5299fe15fdde1c9 yes 'long f(int x) { return x; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "a long compared, its carry branched on" cd00000030 yes 'int f(long a, long b) { if (a < b) return 3; return 4; }'
OPTACC_CACHE_NONE=1 mirs "a loop's long, made by it"                    yes \
    'unsigned long f(const unsigned char *p, int n) { unsigned long s = 0; while (n--) s = s * 31 + *p++; return s; }'

# Signed against a constant: against 0 the sign alone, add hl, hl;
# against another, the left moved by half the range through DE and the
# constant moved already -- no BC. &, | and ^ with a constant a byte at a
# time -- ld a, l / or 0x80 / ld l, a -- not the routine. An int's index
# scaled by adds -- push hl / pop de / add hl, hl / add hl, de -- not by a
# call of the multiply. And a _Bool read tested as the byte it is, or a,
# not widened and tested at 24 bits.
mir "$OPT" "x < 0 by its sign"                dd270629      yes 'int f(int x) { if (x < 0) return 3; return 4; }'
mir "$OPT" "x < 5 through DE"           110000801911050080 yes 'int f(int x) { if (x < 5) return 3; return 4; }'
mir "$OPT" "not through BC"             0100008009eb09eb   no  'int f(int x) { if (x < 5) return 3; return 4; }'
mir "$OPT" "x | 0x80 on its low byte"          7df6806f      yes 'unsigned f(unsigned x) { return x | 0x80; }'
mir "$OPT" "p[i] scaled by adds"               e5c12909      yes 'int f(int *p, int i) { return p[i]; }'
mir "$OPT" "not by the multiply"               01030000      no  'int f(int *p, int i) { return p[i]; }'
# A loop's counter and pointer kept in registers where the scan spills
# what holds one longest for its uses -- the weight over the length --
# not the fewest uses: the pointer stepped in BC and the counter in IY.
sum='int f(const int *a, int n) { register const int *p = a; int s = 0; while (n-- > 0) s += *p++; return s; }'
mir "$OPT" "the loop's values in registers"   030303fd2b yes "$sum"
# Signed, of two variables: the sign of the difference into the carry, and
# the carry turned over where it overflowed -- or a / sbc hl, de / add
# hl, hl / jp po -- with no bias through BC. And a pointer stepped by a
# constant number of elements, the product made: ld de, 13 / add hl, de.
sless='int f(int a, int b) { if (a < b) return 3; return 4; }'
mir "$OPT" "a < b by the sign and overflow"  b7ed5229e2   yes "$sless"
mir "$OPT" "not moved by a bias in BC"       01000080     no  "$sless"
mir "$OPT" "p + 1 of 13 bytes, the product"  110d000019   yes 'struct t { char n[10]; int v; }; struct t *f(struct t *p) { return p + 1; }'
# The same signed comparison where the leaf backend makes it, and where
# the code here selects it for the first pass's: the carry turned over on
# overflow, and no bias -- which took BC, pushed and popped where a value
# lived there, or the right side through the stack.
callless='int g(int); int f(int a, int b) { if (a < b) return g(1); return g(2); }'
OPTACC_LEAF=1 all "$OPT" "a < b in the leaf backend, by the carry" ed5229e2 yes "$callless"
OPTACC_LEAF=1 all "$OPT" "with no bias"                        01000080 no  "$callless"
all "$OPT" "a < b in the first pass's code, by the carry"     ed5229e2 yes "$callless"
all "$OPT" "with no bias there either"                        11000080 no  "$callless"
# The prologue written out, the faster -- or with OPTACC_FRAME_CALL a call
# to acc_rt_frameset, as acc's: call nn first, four bytes where writing it
# out is nine -- but written out still for a function said hot, before it
# or after its parameters.
mir "$OPT" "a prologue written out"            dde5dd21000000dd39 yes 'int f(int a) { return a + 2; }'
OPTACC_FRAME_CALL=1 mir "$OPT" "a prologue by the call" '^cd000000' yes 'int f(int a) { return a + 2; }'
OPTACC_FRAME_CALL=1 mir "$OPT" "said hot, written out"             dde5dd21000000dd39 yes '__attribute__((hot)) int f(int a) { return a + 1; }'
OPTACC_FRAME_CALL=1 mir "$OPT" "said hot after, written out"       dde5dd21000000dd39 yes 'int f(int a) __attribute__((hot)); int f(int a) { return a + 1; }'
boolread='extern _Bool b; int f(int *p) { *p = 1; if (b) return 3; return 4; }'
mir "$OPT" "a _Bool read tested as a byte"   3a000000b7      yes "$boolread"
mir "$OPT" "not widened and tested"        b7ed626f09b7ed42 no "$boolread"
# A static read by its address, ld hl, (nn), not the address loaded and
# read through; and with nothing in the frame, no frame -- the function the
# load and ret. A member of a pointer moved by a constant read with both in
# the displacement, ld a, (iy-4) -- and that byte answered from A as it
# was read, not widened at all. A parameter's pointer read at p[3] by (iy+9), the frame
# left by pop ix alone, SP never having moved.
mir "$OPT" "a static read by its address"     '^2a000000c9$' yes 'static int counter; int f(void) { return counter; }'
mir "$OPT" "not through HL"                   ed27     no  'static int counter; int f(void) { return counter; }'
vprev='struct v { int a; char k; char t; int e; }; extern struct v *vsp; char f(void) { return (vsp - 1)->t; }'
mir "$OPT" "(p - 1)->t in the displacement"   fd7efc   yes "$vprev"
mir "$OPT" "not by a subtract"                ed42     no  "$vprev"
mir "$OPT" "a byte answered from A, as read"  fd7efcc9 yes "$vprev"
mir "$OPT" "not widened twice"                7d6fcb05 no  "$vprev"
mir "$OPT" "p[3] by (iy+9), then pop ix"      fd2709dde1c9 yes 'int f(int *p) { return p[3]; }'
mir "$OPT" "no ld sp, ix"                     ddf9     no  'int f(int *p) { return p[3]; }'
# A byte widened, tested as the byte: ld a, (hl) / or a. A byte &'d with a
# constant branched on by the flags the and made, no or a after. A loop's
# pointer stepped in its own register, inc hl, not copied out and back.
# And a byte written through a pointer in HL as ld (hl), a.
mir "$OPT" "*p tested as a byte, read through HL" 7eb728 yes 'int f(const char *p) { if (*p) return 3; return 4; }'
mir "$OPT" "not widened and tested at 24 bits"    09b7ed42 no 'int f(const char *p) { if (*p) return 3; return 4; }'
mask='extern unsigned char t[]; int f(int c) { if (t[c] & 4) return 3; return 4; }'
mir "$OPT" "t[c] & 4 branched on as it is"     e60428   yes "$mask"
mir "$OPT" "not tested again"                  e604b7   no  "$mask"
count='int f(const char *s) { int n = 0; while (*s++) n++; return n; }'
mir "$OPT" "s++ in its own register"           2318     yes "$count"
mir "$OPT" "not copied out and back"           c5d1     no  "$count"
mir "$OPT" "*d = c by ld (hl), a"              77dde1c9 yes 'void f(char *d, char c) { *d = c; }'
# ++ and -- through a pointer: made here, a byte stepped where it is --
# inc (iy+3), read after it for ++x; a global's byte read before dec (hl)
# for x--; an int global's through (nn); a member pointer written back
# by (iy+3); and a pointer to a 13-byte struct stepped by 13.
mirs "++ and -- through a pointer, made by it"     yes \
    'struct s { int n; char c; int *p; }; void f(struct s *x) { x->n++; x->c--; x->p++; }'
stepc='struct s { int n; char c; }; int f(struct s *x) { return ++x->c; }'
mir "$OPT" "++x->c by inc (iy+3)"             fd3403   yes "$stepc"
mir "$OPT" "not read, stepped and written"    fd7703   no  "$stepc"
mir "$OPT" "g-- read before dec (hl)"         210000007e35 yes 'char g; int f(void) { return g--; }'
mir "$OPT" "++gi through (nn)"                2a0000002322000000 yes 'int gi; int f(void) { return ++gi; }'
mir "$OPT" "*x->p++ written back by (iy+3)"   fd1f03   yes 'struct s { int n; char *p; }; int f(struct s *x) { return *x->p++; }'
mir "$OPT" "(*pp)++ by the struct's 13 bytes" 010d000009 yes 'struct b { char pad[13]; }; struct b *f(struct b **pp) { return (*pp)++; }'
# Locals whose address is taken, in memory: made here where no way caches
# them (OPTACC_CACHE_NONE) -- but not where the type is a VLA's, whose steps
# are sizes read as it runs. A char set and stepped in its slot, ld (ix-1),
# 3 and inc (ix-1); an array laid out with the other locals, b[1] read by
# (ix-9); a struct's members at theirs, inc (ix-1) for p.tag and (ix-4) for
# p.y; and an array too big for (ix+d), its address the first pass's,
# push de / ld de, n.
OPTACC_CACHE_NONE=1 mirs "a local whose address is taken, made by it" yes \
    'void g(int *); int f(void) { int x = 5; g(&x); return x; }'
OPTACC_CACHE_NONE=1 mirs "but not with a VLA's type"      no \
    'int f(int n, int a[][n]) { return a[1][0]; }'
memchar='void g(char *); int f(void) { char c = 3; g(&c); c++; return c; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "c = 3 by ld (ix-1), 3"       dd36ff03 yes "$memchar"
OPTACC_CACHE_NONE=1 mir "$OPT" "c++ by inc (ix-1)"           dd34ff   yes "$memchar"
OPTACC_CACHE_NONE=1 mir "$OPT" "b[1] of a local array by (ix-9)" dd27f7 yes 'void g(int *); int f(void) { int b[4]; g(b); return b[1]; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "p.tag++ and p.y at their own (ix+d)" dd34ffdd27fc yes 'struct pt { int x, y; char tag; }; void g(struct pt *); int f(void) { struct pt p; g(&p); p.tag++; return p.y; }'
OPTACC_CACHE_NONE=1 mir "$OPT" "an array out of reach, as the first pass has it" d51138ffff yes 'void g(char *); int f(void) { char s[200]; g(s); return s[3]; }'
# An array laid out with the locals is in front of the spills: 124 bytes of
# it and the spills of a deep expression are past (ix+d)'s reach together,
# which the code here refuses rather than make.
# A union, its type code the one that says "look at its extension": not
# a long for being wider than an int (test/cases/384 runs one).
OPTACC_CACHE_NONE=1 mirs "a union written by its bytes, read whole"  yes \
    'int f(const unsigned char *d, int n) { union { int v; unsigned char b[3]; } u; u.v = 0; if (n > 0) u.b[0] = d[0]; if (n > 1) u.b[1] = d[1]; return u.v; }'
OPTACC_CACHE_NONE=1 mirs "spills past the reach behind an array, refused" no \
    'void g(char *); int f(int a, int b, int c, int d) { char s[124]; g(s); return (a + b) ^ ((b + c) ^ ((c + d) ^ ((d + a) ^ ((a - b) ^ ((b - c) ^ ((c - d) ^ (s[1] + a))))))); }'
# A byte answered in A, as the function answers it, not widened into HL
# and cut again: a _Bool callee's answer as it is, the call then the
# epilogue; a char's low byte; a mask of a byte; a comparison's truth, the
# 0 or 1 never rebuilt from HL.
mir "$OPT" "a _Bool callee's answer returned as it is" cd000000d1dde1c9 yes '_Bool p(int); _Bool f(int x) { return p(x); }'
mir "$OPT" "not made 0 or 1 again"                     210100007d       no  '_Bool p(int); _Bool f(int x) { return p(x); }'
mir "$OPT" "a char's answer not widened"               cb05ed626f       no  'char f(int x) { return x + 1; }'
mir "$OPT" "p[2] & 7 answered from A"                  e607dde1c9       yes 'unsigned char f(unsigned char *p) { return p[2] & 7; }'
mir "$OPT" "a < b answered as the truth it is"         210100007d       no  '_Bool f(int a, int b) { return a < b; }'
# What the pick weighs counts the frame the code here leaves out: with no
# frame, the call that makes one and the two that undo it -- a static read
# is ld hl, (nn) / ret, chosen over the first pass's eight bytes more; with
# SP kept, the ld sp, ix -- name_text's add chosen for it, where the first
# pass's is as long without it.
picked() {
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
        OPTACC_LEAF=1 OPTACC_INLINE=1 OPTACC_PEEP=1 OPTACC_MIR=1 emits "$@"
}
picked "$OPT" "no frame, chosen for it"         '^2a000000c9$' yes 'static int counter; int f(void) { return counter; }'
nametext='char arena[100]; const char *f(int ref) { return arena + ref; }'
picked "$OPT" "no ld sp, ix, chosen for it"     '19dde1c9$'    yes "$nametext"
picked "$OPT" "not the first pass's"            ddf9           no  "$nametext"
# Smaller code costlier to run by no more than an eighth is chosen: a
# static set to 1 is ld hl, 1 / ld (nn), hl / ret, nine bytes for the first
# pass's twenty-one, framed.
picked "$OPT" "smaller, an eighth costlier, chosen" '^2101000022000000c9$' yes \
    'static int flag; void f(void) { flag = 1; }'
# A phi in a block a switch's case jumps to -- the pointer stepped in one
# case and the next fallen into, as ez80asm's parse_operand has it -- is
# the machine-level backend's to copy into, its cases being branches: made
# by it, where every other way is refused, and chosen over the first pass.
casephi='typedef struct { unsigned char reg, mode, idx; } operand; void error(int);
void f(char *s, operand *op) { char *p = s; op->reg = 0; if (*p == 40) { op->mode = 1; p++; }
switch (*p++) { case 97: case 65: switch (*p++) { case 0: op->reg = 1; return; case 102: case 70: switch (*p++) { case 0: case 39: op->reg = 2; return; } break; } break;
case 98: case 66: switch (*p++) { case 0: op->reg = 3; return; case 99: case 67: if (*p == 0) { op->reg = 4; return; } break; } break;
case 105: p++; case 73: switch (*p++) { case 120: op->reg = 5; op->idx = *p; return; case 121: op->reg = 6; op->idx = *p; return; } break; }
error(*p); }'
mirs "a switch's case where paths join, made by it" yes "$casephi"
printf '%s\n' "$casephi" > "$tmp/c.c"
rm -f "$tmp/c.o"
if OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
    OPTACC_LEAF=1 OPTACC_INLINE=1 OPTACC_PEEP=1 OPTACC_MIR=1 OPTACC_SSA_STATS=1 \
    "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1 | grep -q '^ssa f made$'; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s\n' "and chosen over the first pass's code"
    fail=$((fail + 1))
fi
# Through IY or HL, a byte read straight into the register it goes to --
# ld d, (iy+0) -- not into A and copied.
cmploop='int f(const char *a, const char *b, int n) { while (n-- > 0) if (*a++ != *b++) return 0; return 1; }'
mir "$OPT" "a byte read straight into its register" fd5600 yes "$cmploop"
mir "$OPT" "not through A"                        fd7e0057 no "$cmploop"
# The steps there in place too, made in the block of the edge back: dec bc
# for n--, inc iy for a++.
mir "$OPT" "steps in place on a branch's edge"   0bfd23   yes "$cmploop"
# A loop's phi dead from its last read to the step's copy at the end: with
# lifetime holes the step has the phi's register, so len is no frame slot
# (ld (ix-1), a) and src no copy through the stack (push hl; pop de).
tokscan='typedef struct { const char *start; char term; } tok_t;
unsigned char f(tok_t *t, const char *src) { unsigned char len = 0; t->start = src; for (;;) { char c = *src; if (c == 0 || c == 44 || c == 59) break; src++; len++; if (c != 39) continue; while (*src && *src != 39) { src++; len++; } } t->term = *src; return len; }'
mir "$OPT" "a phi's hole holds its step: no slot"   dd77ff   no "$tokscan"
mir "$OPT" "nor a copy through the stack"           e5d1     no "$tokscan"
# A table's address copied for the add that wants HL for itself: made in
# BC, the register the copy takes it to, not in HL and pushed across.
tblscan='extern const unsigned char tbl[256];
unsigned char f(char *src) { unsigned char n = 0; while (!tbl[(unsigned char) *src]) { n++; src++; } *src = 0; return n; }'
mir "$OPT" "an address made where its copy goes"    010000001a yes "$tblscan"
mir "$OPT" "not made in HL and pushed to BC"        21000000e5c1 no "$tblscan"

big=$(python3 -c "
print('unsigned f(unsigned a, unsigned *b, unsigned c) { unsigned d;')
for n in range(600): print('d = a + b[%d]; if (d < a) c++; a = d;' % n)
print('return d + c; }')")
start=$(date +%s)
printf '%s\n' "$big" > "$tmp/big.c"
rm -f "$tmp/big.o"
OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 OPTACC_LEAF=1 \
    OPTACC_MIR=1 OPTACC_PICK=0 timeout 60 "$OPT" -c "$tmp/big.c" -o "$tmp/big.o" >/dev/null 2>&1
took=$(( $(date +%s) - start ))
if [ -f "$tmp/big.o" ] && [ "$took" -le 10 ]; then
    pass=$((pass + 1))
else
    printf '  FAIL %-50s %s seconds\n' "600 statements, in seconds" "$took"
    fail=$((fail + 1))
fi

# And with every way in the pick, each shape of function four times the
# size takes four times the work, which perf counts in instructions: a
# pass quadratic in the function's size makes it sixteen, and one only
# partly so somewhere between -- 4.6 is the bound. And none of them
# declined as too much work, which is where a pass quadratic enough lands
# instead. Without perf, the time, and twice four the bound.
shape() {
    python3 -c "
import sys
shape, n = sys.argv[1], int(sys.argv[2])
if shape == 'line':
    print('int f(int a) { int x0 = a;')
    for i in range(1, n): print(' int x%d = x%d + %d;' % (i, i - 1, i % 50))
    print(' return x%d; }' % (n - 1))
elif shape == 'ifs':
    print('unsigned f(unsigned a, unsigned *b, unsigned c) { unsigned d = 0;')
    for i in range(n): print(' d = a + b[%d]; if (d < a) c++; a = d;' % (i % 64))
    print(' return d + c; }')
elif shape == 'cond':
    print('int f(int a, int b) { int s = 0;')
    for i in range(n): print(' s += a > %d ? b : s;' % i)
    print(' return s; }')
elif shape == 'calls':
    print('int g(int); int f(int a) { int s = 0;')
    for i in range(n): print(' s += g(a + %d) ^ a;' % i)
    print(' return s; }')
elif shape == 'loop':
    print('int f(int n, int *p) {')
    for i in range(30): print(' int v%d = %d;' % (i, i))
    print(' for (int i = 0; i < n; i++) {')
    for i in range(n): print('  v%d = v%d + p[i];' % (i % 30, (i + 1) % 30))
    print(' }')
    print(' return ' + ' + '.join('v%d' % i for i in range(30)) + '; }')
elif shape == 'signed':
    print('int f(int a, int *p) { int s = 0;')
    for i in range(n): print(' if (p[%d] < a) s += p[%d]; a = a - s;' % (i % 64, (i + 1) % 64))
    print(' return s; }')
elif shape == 'bytes':
    print('int f(unsigned char *p, int a) { unsigned char c = 0; int s = 0;')
    for i in range(n): print(' c = p[%d] + c; if (c == %d) s++; p[%d] = c;' % (i % 64, i % 200, (i + 3) % 64))
    print(' return s; }')
elif shape == 'cached':
    print('void g(int *); int f(int a, int *p) { int x = a; g(&x);')
    for i in range(n): print(' x = x + p[%d]; if (x < %d) x++;' % (i % 64, i))
    print(' return x; }')
" "$1" "$2"
}
counting=no
perf stat -x, -e instructions:u true 2>&1 | grep -q instructions && counting=yes
compile_cost() {
    local start

    local count=

    [ "$counting" = yes ] && count="perf stat -x, -o $tmp/count -e instructions:u"
    rm -f "$tmp/s.o"
    start=$(date +%s%N)
    OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_NATIVE=1 OPTACC_IY=1 OPTACC_HOMES=2 \
        OPTACC_LEAF=1 OPTACC_INLINE=1 OPTACC_PEEP=1 OPTACC_MIR=1 \
        OPTACC_SSA_STATS=1 timeout 120 $count \
        "$OPT" -c "$1" -o "$tmp/s.o" > "$tmp/stats" 2>&1
    if [ "$counting" = yes ]; then
        grep instructions "$tmp/count" | cut -d, -f1
    else
        echo $(( ($(date +%s%N) - start) / 1000000 ))
    fi
}
scales() {
    local small=$2 bound=46 small_cost big_cost

    shape "$1" "$small" > "$tmp/s1.c"
    shape "$1" $((4 * small)) > "$tmp/s4.c"
    small_cost=$(compile_cost "$tmp/s1.c")
    big_cost=$(compile_cost "$tmp/s4.c")
    [ "$counting" = yes ] || bound=80
    if [ -f "$tmp/s.o" ] && ! grep -q 'too big for the SSA' "$tmp/stats" \
        && [ $((10 * big_cost)) -le $((bound * small_cost)) ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-50s %s, then %s%s\n' "$1 of $small, four times it" \
            "$small_cost" "$big_cost" \
            "$(grep -o 'too big for the SSA.*' "$tmp/stats" | head -1 | sed 's/^/: /')"
        fail=$((fail + 1))
    fi
}
scales line 3000
scales ifs 4000
scales cond 4000
scales calls 4000
scales loop 4000
scales signed 2000
scales bytes 2000
scales cached 2000

# Caching only the locals a loop reads or writes, as one of the ways the
# pick weighs (OPTACC_CACHE_LOOPS makes it the first): p, whose address the
# calls take and which the loop steps, is in BC there; x, read only after
# the loop, is not cached -- caching all of them puts it in IY.
loopcache='void h(int *); void g(const char **); int f(const char *p, const char *e, int k) { int x = k; h(&x); g(&p); while (p < e && *p == 32) p++; return x * 3 + (x ^ k) + *p; }'
OPTACC_LEAF=1 OPTACC_CACHE_LOOPS=1 all "$OPT" "a loop's local cached: p into BC"  dd0706 yes "$loopcache"
OPTACC_LEAF=1 OPTACC_CACHE_LOOPS=1 all "$OPT" "and one outside loops, not"  dd31fd no  "$loopcache"
OPTACC_LEAF=1 all "$OPT" "which caching all of them puts in IY"  dd31fd yes "$loopcache"
# And the way that caches none of them (OPTACC_CACHE_NONE makes it the
# first): x read again after the call for each use, push hl / ld hl,
# (ix-3), where caching reads it once. A macro in ssa.c named as this way
# is had made every way cache.
nocache='void g(int *); int f(void) { int x = 1; g(&x); return x + x * 4; }'
OPTACC_LEAF=1 OPTACC_CACHE_NONE=1 all "$OPT" "no local cached: x read again"  e5dd27fd yes "$nocache"
OPTACC_LEAF=1 all "$OPT" "which caching reads once"  e5dd27fd no "$nocache"

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

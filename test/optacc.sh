#!/bin/bash
# What opt-acc does that acc does not, seen in the objects each writes for
# the same source (docs/optimizer-plan.md, milestone 0):
#
# - the prologue written out, push ix; ld ix, 0; add ix, sp; ld hl, -frame;
#   add hl, sp; ld sp, hl, where acc calls acc_rt_frameset -- and cut to its
#   first nine bytes when there is no frame;
# - the local a loop uses most kept in IY, chosen by reading the body first
#   (src/prescan.c), where acc needs it declared register;
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
emits "$OPT" "a loop: opt-acc keeps a local in IY" "$lea_hl_iy" yes "$loop"
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
          | grep '^ssa ' | head -1)
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
ssa "a long where paths join, left" "ssa f a long or a float where paths join" \
    'long f(long n) { long s = 0; while (n) s += n--; return s; }'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

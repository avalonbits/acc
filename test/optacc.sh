#!/bin/bash
# What opt-acc does that acc does not, seen in the objects each writes for
# the same source (docs/optimizer-plan.md, milestone 0):
#
# - the prologue written out, push ix; ld ix, 0; add ix, sp; ld hl, -frame;
#   add hl, sp; ld sp, hl, where acc calls acc_rt_frameset -- and cut to its
#   first nine bytes when there is no frame;
# - the local a loop uses most kept in IY, chosen by reading the body first
#   (src/prescan.c), where acc needs it declared register.
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

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

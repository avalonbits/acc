#!/bin/bash
# acc test suite.
#
# Every target acc can be built for gets its data model asserted here, because
# the data model is the port: the eZ80 has a 24-bit int and a 32-bit long, and
# a compiler that gets that wrong produces code that links and then misbehaves.
# _Static_assert is the right tool for it -- it is checked by the compiler
# under test, for the target under test, with no need to run the output.
set -uo pipefail
cd "$(dirname "$0")/.."

pass=0; fail=0
ok()   { pass=$((pass+1)); printf '  ok   %s\n' "$1"; }
bad()  { fail=$((fail+1)); printf '  FAIL %s\n' "$1"; [ -n "${2:-}" ] && printf '%s\n' "$2" | sed 's/^/         /'; }

# Compiles a fragment for a target and reports whether it was accepted.
# expect=ok means it must compile; expect=err means it must be rejected.
compiles() {
    local cc=$1 name=$2 expect=$3 src=$4 out
    out=$(printf '%s\n' "$src" | $cc -c -xc - -o /dev/null 2>&1)
    if [ $? -eq 0 ]; then
        [ "$expect" = ok ] && ok "$name" || bad "$name" "expected rejection, was accepted"
    else
        [ "$expect" = err ] && ok "$name" || bad "$name" "$out"
    fi
}

# ---- data model -------------------------------------------------------------
# char short int long llong ptr float double ldouble
model() {
    local cc=$1 name=$2; shift 2
    compiles "$cc" "$name" ok "
_Static_assert(sizeof(char)       == $1, \"char\");
_Static_assert(sizeof(short)      == $2, \"short\");
_Static_assert(sizeof(int)        == $3, \"int\");
_Static_assert(sizeof(long)       == $4, \"long\");
_Static_assert(sizeof(long long)  == $5, \"long long\");
_Static_assert(sizeof(void *)     == $6, \"void *\");
_Static_assert(sizeof(float)      == $7, \"float\");
_Static_assert(sizeof(double)     == $8, \"double\");
_Static_assert(sizeof(long double)== $9, \"long double\");
"
}

echo "i386 (validation build):"
[ -x bin/acc-i386 ] || { echo "  bin/acc-i386 missing -- run make"; exit 2; }
model bin/acc-i386 "data model" 1 2 4 4 8 4 4 8 12
compiles bin/acc-i386 "compiles a function" ok 'int f(int x){return x*2;}'
compiles bin/acc-i386 "rejects bad syntax" err 'int f(int x){return x*;}'

echo
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

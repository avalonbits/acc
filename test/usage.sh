#!/bin/bash
# What acc says about itself, and how it fails.
#
# A bare `acc` is a question, not a mistake: the summary, and success. -h
# is every option. Both have to fit a screen of 30 rows and 80 columns --
# the Agon's with an 8x16 font -- or the top scrolls away before it can be
# read. A command line acc does not take is the summary and 2; an error is
# 1; with -errors, either is 100. test/release.sh checks the Agon's codes.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "run make first" >&2; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok()  { printf '  ok   %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf '  FAIL %-40s %s\n' "$1" "$2"; fail=$((fail + 1)); }
run() { ASAN_OPTIONS=detect_leaks=0 "$ACC" "$@" > "$tmp/out" 2> "$tmp/err"; }

# Lines and the widest line, of what a screen would be shown.
fits() {
    local rows cols
    [ -s "$1" ] || return 1
    rows=$(tr -d '\r' < "$1" | wc -l)
    cols=$(tr -d '\r' < "$1" | awk '{ if (length > m) m = length } END { print m + 0 }')
    [ "$rows" -le "$2" ] && [ "$cols" -lt 80 ]
}

run; rc=$?
if [ $rc -eq 0 ] && grep -q 'acc -h' "$tmp/out" && [ ! -s "$tmp/err" ]; then
    ok "a bare acc is the summary, and succeeds"
else
    bad "a bare acc is the summary, and succeeds" "rc $rc"
fi
fits "$tmp/out" 10 && ok "the summary is ten lines at most" \
    || bad "the summary is ten lines at most" "$(wc -l < "$tmp/out") lines"

for flag in -h --help; do
    run $flag; rc=$?
    if [ $rc -eq 0 ] && grep -q -- '-errors <file>' "$tmp/out"; then
        ok "$flag lists the options, and succeeds"
    else
        bad "$flag lists the options, and succeeds" "rc $rc"
    fi
done
# 24, and two more for where the defaults are -- /lib/acc on the Agon, and
# on the host wherever this checkout is, so as long as its path makes it:
# they are left out of the measure.
grep -v '^Headers from ' "$tmp/out" > "$tmp/help"
fits "$tmp/help" 24 && ok "-h fits a screen" \
    || bad "-h fits a screen" "$(wc -l < "$tmp/help") lines"

run -z; rc=$?
if [ $rc -eq 2 ] && grep -q "'-z' is not an option" "$tmp/err" \
   && grep -q 'acc -h' "$tmp/err"; then
    ok "an unknown option is named, with the summary"
else
    bad "an unknown option is named, with the summary" "rc $rc"
fi

printf 'int main(void) { return nope; }\n' > "$tmp/bad.c"
run "$tmp/bad.c" -o "$tmp/bad.bin"; rc=$?
[ $rc -eq 1 ] && ok "an error fails with 1" || bad "an error fails with 1" "rc $rc"
run "$tmp/bad.c" -o "$tmp/bad.bin" -errors "$tmp/e.txt"; rc=$?
[ $rc -eq 100 ] && ok "and with -errors, 100" || bad "and with -errors, 100" "rc $rc"
run -errors "$tmp/e.txt" -z; rc=$?
[ $rc -eq 100 ] && ok "a command line with -errors, 100" \
    || bad "a command line with -errors, 100" "rc $rc"

echo "  $pass passed, $fail failed"
[ $fail -eq 0 ]

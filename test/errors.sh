#!/bin/bash
# Every program in test/errors is one acc must reject. The first line says
# what it must say about it.
#
# The differential runner cannot cover these: agondev compiles most of them
# happily, so there is nothing to compare against. What is being checked is
# that acc names the right line and the right construct, which matters most
# for the features it does not have yet -- a user meeting `*` should be told
# `*` is not supported, not be sent hunting for a missing semicolon.
set -uo pipefail
cd "$(dirname "$0")/.."

# Which compiler to drive. `make test` points this at the sanitized build, so
# a run that gets the right answer by reading freed memory still fails.
ACC=${ACC:-bin/acc}

# acc allocates and never frees: it is a one-shot process and giving the
# memory back on the way out would cost code on a target where code is the
# scarce thing. That is deliberate, so leak checking is off; everything else
# ASan and UBSan look for is on.
export ASAN_OPTIONS=detect_leaks=0

# A sanitizer report on stderr is a failure even when the answer is right,
# which is exactly the case that hid the symbol-table bug.
sanitizer_tripped() {
    case $1 in
      *"AddressSanitizer"*|*"runtime error:"*|*"UndefinedBehaviorSanitizer"*) return 0 ;;
    esac
    return 1
}

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

pass=0; fail=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

for src in test/errors/*.c; do
    name=$(basename "$src" .c)
    want=$(sed -n '1s|.*/\* expect: \(.*\) \*/.*|\1|p' "$src")
    [ -n "$want" ] || { printf '  FAIL %-18s no "expect:" line\n' "$name"; fail=$((fail+1)); continue; }

    got=$("$ACC" "$src" -o "$tmp/out.bin" -x 2>&1)
    if [ $? -eq 0 ]; then
        printf '  FAIL %-18s acc accepted it\n' "$name"
        fail=$((fail+1)); continue
    fi
    if sanitizer_tripped "$got"; then
        printf '  FAIL %-18s the sanitizer tripped\n%s\n' "$name" \
            "$(printf '%s' "$got" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    if [[ "$got" == *"$want"* ]]; then
        printf '  ok   %-18s %s\n' "$name" "$want"
        pass=$((pass+1))
    else
        printf '  FAIL %-18s want %s\n                        got  %s\n' "$name" "$want" "$got"
        fail=$((fail+1))
    fi
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

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

[ -x bin/acc ] || { echo "bin/acc missing -- run make"; exit 2; }

pass=0; fail=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

for src in test/errors/*.c; do
    name=$(basename "$src" .c)
    want=$(sed -n '1s|.*/\* expect: \(.*\) \*/.*|\1|p' "$src")
    [ -n "$want" ] || { printf '  FAIL %-18s no "expect:" line\n' "$name"; fail=$((fail+1)); continue; }

    got=$(bin/acc "$src" -o "$tmp/out.bin" -x 2>&1)
    if [ $? -eq 0 ]; then
        printf '  FAIL %-18s acc accepted it\n' "$name"
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

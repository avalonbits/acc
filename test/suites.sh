#!/bin/bash
# Runs the suites a file lists (test/suites), SUITE_JOBS at a time, and
# prints what each said, whole, in the file's order -- each, as when they
# ran one after another, until the first that fails. Those after a line
# `serial` run one after another, once the rest are done.
#
#   test/suites.sh <file>
set -uo pipefail
cd "$(dirname "$0")/.."

list=$1
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

grep -v '^#' "$list" | grep -v '^$' | awk '$0 == "serial" { s = 1; next } !s' > "$tmp/parallel"
grep -v '^#' "$list" | grep -v '^$' | awk 's; $0 == "serial" { s = 1 }' > "$tmp/serial"

n=0
while IFS= read -r cmd; do
    n=$((n+1))
    printf '%s\n%s\n' "$n" "$cmd"
done < "$tmp/parallel" | xargs -d '\n' -P "${SUITE_JOBS:-4}" -n 2 bash -c '
    bash -c "$2" > "'"$tmp"'/$1.out" 2>&1; echo $? > "'"$tmp"'/$1.rc"' _

k=0
while IFS= read -r cmd; do
    k=$((k+1))
    cat "$tmp/$k.out"
    if [ "$(cat "$tmp/$k.rc")" != 0 ]; then
        echo "  FAIL $cmd"
        exit 1
    fi
done < "$tmp/parallel"

while IFS= read -r cmd; do
    bash -c "$cmd" || { echo "  FAIL $cmd"; exit 1; }
done < "$tmp/serial"
exit 0

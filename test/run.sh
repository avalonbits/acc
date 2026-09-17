#!/bin/bash
# Compiles each program in test/cases twice -- once with acc, once with
# agondev -- runs both on the Agon, and requires the same answer.
#
# A program reports through IO port 0, so the answer is one byte. Cases are
# written to land well inside that.
set -uo pipefail
cd "$(dirname "$0")/.."

[ -x bin/acc ] || { echo "bin/acc missing -- run make"; exit 2; }

pass=0; fail=0; skip=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

for src in test/cases/*.c; do
    name=$(basename "$src" .c)

    if ! err=$(bin/acc "$src" -o "$tmp/acc.bin" 2>&1); then
        printf '  FAIL %-18s acc could not compile it\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    if ! err=$(test/oracle.sh "$src" "$tmp/ref.bin" 2>&1); then
        case $? in
          77) printf '  skip %-18s no agondev\n' "$name"; skip=$((skip+1)); continue ;;
          *)  printf '  FAIL %-18s the reference build failed\n%s\n' "$name" \
                  "$(printf '%s' "$err" | sed 's/^/         /')"
              fail=$((fail+1)); continue ;;
        esac
    fi

    test/agon.sh "$tmp/ref.bin" >/dev/null 2>&1; want=$?
    [ $want -eq 77 ] && { printf '  skip %-18s no emulator\n' "$name"; skip=$((skip+1)); continue; }
    test/agon.sh "$tmp/acc.bin" >/dev/null 2>&1; got=$?

    if [ "$got" -eq "$want" ]; then
        printf '  ok   %-18s %3d\n' "$name" "$got"
        pass=$((pass+1))
    else
        printf '  FAIL %-18s acc says %d, agondev says %d\n' "$name" "$got" "$want"
        fail=$((fail+1))
    fi
done

printf '  %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ]

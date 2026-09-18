#!/bin/bash
# Compiles each program in test/self with acc, runs it, and requires 42.
#
# The difference from run.sh is that there is no second compiler. Everything
# in test/cases has to agree with agondev, which is what makes those tests
# worth having -- and also what keeps some things out of them. Where two
# locals sit relative to each other is not a contract: acc packs them in
# declaration order and agondev does not, and it changes its mind depending
# on what else the function holds. A program that reads one local through a
# pointer to its neighbour is correct, deterministic, and has no business
# being compared against another compiler's idea of the same thing.
#
# So these are acc against itself. They still run on the Agon and still have
# to come out at 42; what they do not do is ask agondev to agree.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
export ASAN_OPTIONS=detect_leaks=0

sanitizer_tripped() {
    case $1 in
      *"AddressSanitizer"*|*"runtime error:"*|*"UndefinedBehaviorSanitizer"*) return 0 ;;
    esac
    return 1
}

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

pass=0; fail=0; skip=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

for src in test/self/*.c; do
    name=$(basename "$src" .c)

    if ! err=$("$ACC" "$src" -o "$tmp/acc.bin" -x 2>&1); then
        printf '  FAIL %-18s acc could not compile it\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi
    if sanitizer_tripped "$err"; then
        printf '  FAIL %-18s the sanitizer tripped\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    test/agon.sh "$tmp/acc.bin" >/dev/null 2>&1; got=$?
    [ $got -eq 77 ] && { printf '  skip %-18s no emulator\n' "$name"; skip=$((skip+1)); continue; }

    if [ "$got" -eq 42 ]; then
        printf '  ok   %-18s %3d\n' "$name" "$got"
        pass=$((pass+1))
    else
        printf '  FAIL %-18s returns %d, not 42\n' "$name" "$got"
        fail=$((fail+1))
    fi
done

printf '  %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
[ "$fail" -eq 0 ]

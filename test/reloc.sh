#!/bin/bash
# That acc knows every address it wrote.
#
# Each program is compiled twice: once at the address MOS loads a program at,
# and once at another one. If the relocation table names every slot in the
# image that holds an address, then the two images agree everywhere except at
# those slots, and differ there by exactly the distance between the two bases.
#
# That is the whole statement, and it is not a sample of it: a slot the
# compiler forgot to record holds an address that moved, so the byte
# comparison finds it. A slot recorded that does not hold an address fails the
# same way, from the other side. This is the check that makes an object file
# possible -- a linker can only place code whose addresses are all known.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

export ASAN_OPTIONS=detect_leaks=0

# Far enough to change the top byte of every address, so a slot that was
# missed differs in a byte rather than in a carry that might not happen.
BASE_A=040000
BASE_B=050000

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# The output's name goes in the image header, so both sides are built under
# the same basename in different directories.
mkdir -p "$tmp/a" "$tmp/b"

pass=0; fail=0

for src in ${CASES:-test/cases/*.c}; do
    [ -f "$src" ] || continue
    name=$(basename "$src" .c)

    if ! err=$("$ACC" "$src" -o "$tmp/a/x.bin" -r "$tmp/x.rel" -b "$BASE_A" -x 2>&1); then
        printf '  FAIL %-18s acc could not compile it\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi
    if ! err=$("$ACC" "$src" -o "$tmp/b/x.bin" -b "$BASE_B" -x 2>&1); then
        printf '  FAIL %-18s it would not build at %s\n%s\n' "$name" "$BASE_B" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    if why=$(test/reloc.py "$tmp/a/x.bin" "$tmp/b/x.bin" "$tmp/x.rel" \
                 "$((0x$BASE_B - 0x$BASE_A))"); then
        pass=$((pass+1))
    else
        printf '  FAIL %-18s %s\n' "$name" "$why"
        fail=$((fail+1))
    fi
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Checks that acc's own linker produces the same image as agondev's ld.
#
# acc has to link on the Agon, where there is no ld and no linker script, so
# it lays the image out itself. Byte-for-byte agreement with the reference
# toolchain is what says the layout, the relocations and the symbols agondev's
# linker script would have supplied are all right -- a program that merely
# runs proves much less, since most of an image is never reached.
#
# Both builds are given the same file name, because the name is written into
# the MOS header and would otherwise be the only difference.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
[ -x "$AGONDEV/bin/ez80-none-elf-ld" ] || { echo "  skip no agondev ld"; exit 0; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/acc" "$tmp/ld"

pass=0; fail=0
for src in test/exec/*.c; do
    name=$(basename "$src" .c)
    a=$tmp/acc/p.bin
    b=$tmp/ld/p.bin
    rm -f "$a" "$b"
    test/accld.sh "$src" "$a" >/dev/null 2>&1 || { echo "  FAIL $name (acc link)"; fail=$((fail+1)); continue; }
    ACC_USE_LD=1 test/accld.sh "$src" "$b" >/dev/null 2>&1 || { echo "  FAIL $name (ld link)"; fail=$((fail+1)); continue; }
    if cmp -s "$a" "$b"; then
        pass=$((pass+1)); printf '  ok   %s (%s bytes, identical)\n' "$name" "$(stat -c%s "$a")"
    else
        fail=$((fail+1))
        printf '  FAIL %s: %s differing bytes (acc %s, ld %s)\n' "$name" \
            "$(cmp -l "$a" "$b" 2>/dev/null | wc -l)" "$(stat -c%s "$a")" "$(stat -c%s "$b")"
        cmp -l "$a" "$b" 2>/dev/null | head -5 \
            | awk '{printf "         off=0x%x acc=0x%02x ld=0x%02x\n", $1-1, strtonum("0"$2), strtonum("0"$3)}'
    fi
done

printf '  %d identical, %d differing\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

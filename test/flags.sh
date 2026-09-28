#!/bin/bash
# A conditional branch whose flags were written after the compare that set
# them, in the assembly agondev makes of acc itself.
#
# `x = accept(TK_ASSIGN)` -- a compare, a call for one arm and a zero for the
# other -- came out of clang as
#
#     sbc  hl, de         ; tok - TK_ASSIGN, which sets the zero flag
#     or   a, a           ; and this clears the carry
#     sbc  hl, hl         ; ... so this is hl = 0, and the zero flag is set
#     call z, _accept_next
#
# The zero for the arm that is not taken was scheduled between the compare
# and the call, and both of its instructions write the flag the call reads:
# `sbc hl, hl` with the carry clear leaves zero, which sets it. The call is
# therefore made whatever the token was, and every global array declared
# with no initial value went looking for a '{' that was not there.
#
# Nothing on the host can see this -- the host build is correct -- and the
# only thing acc's own output shows is a diagnostic about the wrong token.
# So the shape is looked for here. `or a, a` and `sbc hl, hl` in front of a
# branch on the zero flag is a branch that is always taken: whatever the
# comparison was, the flag the branch reads came from the zero.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "  [no agondev: flag check skipped]"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

fail=0
checked=0
# Every source the Agon build compiles, as Makefile.agon lists them.
for src in $(sed -n 's/^SRCS = //p; /^       src/p' Makefile.agon | tr -d '\\'); do
    "$CC" -mllvm -z80-gas-style -nostdinc -Isrc -isystem "$AGONDEV/include" \
        -target ez80-none-elf -DAGONDEV -Oz -S -o "$tmp/out.s" "$src" || exit 1

    # The last label seen is reported with the branch, so that a hit names
    # the function to look at rather than a line of a file nobody keeps.
    found=$(awk -v file="$(basename "$src")" '
        /^_[A-Za-z0-9_.]+:/ { fn = substr($1, 1, length($1) - 1) }
        /(or|and|xor)[ \t]+a, a/ { clear = NR; next }
        clear == NR - 1 && /sbc[ \t]+hl, hl/ { zero = NR; next }
        zero == NR - 1 && /(call|jp|jr)[ \t]+n?z,/ {
            printf "  FAIL %s: %s branches on a flag the zero before it wrote: %s\n",
                   file, fn, $0
        }
        { clear = 0; zero = 0 }
    ' "$tmp/out.s")
    checked=$((checked + 1))
    [ -z "$found" ] || { printf '%s\n' "$found"; fail=1; }
done

if [ "$fail" -ne 0 ]; then
    exit 1
fi

echo "  $checked files checked, 0 failed"

#!/bin/bash
# The runtime helpers that must stay off acc's hottest paths.
#
# A signed compare on this target is a helper call: the eZ80 has no signed
# branch, so clang follows the subtract with `call pe, __setflag` to repair
# the sign flag after an overflow. In code that runs for every byte emitted or
# every value the parser pushes, that call is most of the cost. Nothing in
# acc's output shows it -- the bytes come out the same either way -- so it is
# checked here, in the assembly agondev makes of acc itself.
#
# `out_limit - out_put < 3`, the bounds check in front of every emitted byte,
# was one of these, and so was `vtop <= 0` in front of every value popped:
# neither can be negative, so the unsigned compare says the same thing
# without the call.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "  [no agondev: helper check skipped]"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# file, then the functions in it that must not call the helper
checks=(
    "gen.c vpush_const vpush_local vpush_reg vdrop vtype vtype_at vconst_top
           vdup ld_ix_rr push_rr pop_rr add_hl_rr sbc_hl_rr"
    "out.c out_word24"
)

fail=0
for check in "${checks[@]}"; do
    set -- $check
    file=$1; shift
    "$CC" -mllvm -z80-gas-style -nostdinc -Isrc -isystem "$AGONDEV/include" \
        -target ez80-none-elf -DAGONDEV -Oz -S -o "$tmp/out.s" "src/$file" || exit 1
    for fn in "$@"; do
        body=$(awk -v f="_$fn:" '$1 == f { on = 1; next }
                                 on && /^_[A-Za-z0-9_.]+:/ { exit }
                                 on' "$tmp/out.s")
        if [ -z "$body" ]; then
            echo "  FAIL $file: no function $fn"
            fail=1
        elif grep -q '__setflag' <<<"$body"; then
            echo "  FAIL $file: $fn makes a signed compare (__setflag)"
            fail=1
        fi
    done
done

[ $fail -eq 0 ] && echo "  helpers: the hot paths make no signed compares"
exit $fail

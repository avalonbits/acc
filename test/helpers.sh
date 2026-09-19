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

# Each check: a file, a helper, and the functions in it that must not call
# it. `-` as the helper means the function must not be called at all -- it
# has to stay inlined into its callers. `+caller` means the reverse: each
# function has to stay a call from caller, out of line, because inlined its
# locals would give the caller a frame.
#
#   __setflag  a signed compare; see above.
#   __iand     a 24-bit AND. `(unsigned char) tok - X < 3` loads all of tok
#              and masks it, which is this call: tok_low reads the byte.
#   __imulu    a multiply. An extension or a member reached by index into
#              an array of entries whose width is not a power of two.
#   __ishl     a shift. An array of two-byte entries, indexed.
checks=(
    "gen.c __setflag vpush_const vpush_local vpush_reg vdrop vtype vtype_at
           vconst_top vdup ld_ix_rr push_rr pop_rr add_hl_rr sbc_hl_rr"
    "out.c __setflag out_word24"
    "parse.c __iand expr primary postfix_statement name_operand string_value"
    "sym.c __imulu ext_bytes ext_elem ext_elem_x ext_count member_find
           member_type member_offset member_next"
    "sym.c __ishl sym_param_type sym_param_ext"
    "parse.c - starts_decl"
    "lex.c +next lex_two"
)

fail=0
for check in "${checks[@]}"; do
    set -- $check
    file=$1 helper=$2; shift 2
    "$CC" -mllvm -z80-gas-style -nostdinc -Isrc -isystem "$AGONDEV/include" \
        -target ez80-none-elf -DAGONDEV -Oz -S -o "$tmp/out.s" "src/$file" || exit 1
    for fn in "$@"; do
        if [ "${helper#+}" != "$helper" ]; then
            body=$(awk -v f="_${helper#+}:" '$1 == f { on = 1; next }
                                             on && /^_[A-Za-z0-9_.]+:/ { exit }
                                             on' "$tmp/out.s")
            if ! grep -qE "call[[:space:]]+_$fn\$" <<<"$body"; then
                echo "  FAIL $file: $fn is no longer a call from ${helper#+}"
                fail=1
            fi
            continue
        fi
        if [ "$helper" = - ]; then
            if grep -qE "call[[:space:]]+_$fn\$" "$tmp/out.s"; then
                echo "  FAIL $file: $fn is called rather than inlined"
                fail=1
            fi
            continue
        fi
        body=$(awk -v f="_$fn:" '$1 == f { on = 1; next }
                                 on && /^_[A-Za-z0-9_.]+:/ { exit }
                                 on' "$tmp/out.s")
        if [ -z "$body" ]; then
            echo "  FAIL $file: no function $fn"
            fail=1
        elif grep -q "$helper" <<<"$body"; then
            echo "  FAIL $file: $fn calls $helper"
            fail=1
        fi
    done
done

[ $fail -eq 0 ] && echo "  helpers: the hot paths call none of the runtime's slow helpers"
exit $fail

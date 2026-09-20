#!/bin/bash
# The room between the heap's top and the stack.
#
# agondev's linker script puts ___heaptop and __stack at the same address, so
# malloc hands out memory the stack is standing in and nothing says so. acc
# links with src/agon.ld instead, which is that script with the heap's top
# moved down by a reserve the stack keeps to itself.
#
# Two things are checked. That src/agon.ld is still agondev's script apart
# from those two lines, so that an update to agondev which changes anything
# else does not go unnoticed; and that the image really is linked that way,
# read from the map rather than from the script that was meant to produce it.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
THEIRS=$AGONDEV/config/linker.conf
[ -r "$THEIRS" ] || { echo "  [no agondev: heap check skipped]"; exit 77; }

fail=0
check() {
    if [ "$2" = "$3" ]; then
        printf '  ok   %-42s %s\n' "$1" "$2"
    else
        printf '  FAIL %-42s %s, want %s\n' "$1" "$2" "$3"
        fail=$((fail+1))
    fi
}

# The script, from ENTRY on: everything before it is acc's note about why
# there is a copy at all.
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
sed -n '/^ENTRY/,$p' src/agon.ld > "$tmp/ours"
sed -n '/^ENTRY/,$p' "$THEIRS"  > "$tmp/theirs"

drift=$(diff "$tmp/theirs" "$tmp/ours" | grep -E '^[<>]' | grep -vcE \
        'ACC_STACK_RESERVE|___heaptop')
check "src/agon.ld is agondev's, bar the heap's top" "$drift" "0"

# And what the link actually did. The map is written beside the image.
make -s -f Makefile.agon >/dev/null 2>&1 || { echo "  FAIL the Agon build"; exit 1; }
value() {    # the address of a linker symbol, from the map
    awk -v s="$1" '$2 == s && $3 == "=" { print $1; exit }' bin/acc.map
}
top=$(printf '%d' "$(value ___heaptop)")
stack=$(printf '%d' "$(value __stack)")
check "the heap stops short of the stack" "$((stack - top))" "16384"
check "the stack is still at the top of RAM" "$stack" "$((0x40000 + 0x70000))"

printf '  %d properties checked, %d failed\n' 3 "$fail"
[ "$fail" -eq 0 ]

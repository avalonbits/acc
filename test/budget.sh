#!/bin/bash
# What acc.bin, as agondev builds it, may cost: its size, the heap it
# leaves, and the calls to the runtime's helpers its code makes.
#
# On the Agon every byte of acc.bin is a byte of heap, and the heap decides
# which programs acc can compile. So these only go one way: a change that
# makes acc.bin smaller lowers them to what it measures, and one that makes
# it bigger fails here until someone decides it is worth the bytes and says
# so by raising them.
#
# A helper call is four bytes and more at every site that makes one; the
# counts are of sites in the assembly agondev makes of acc's sources:
#
#   __setflag  a signed compare (`i < n` on ints), repaired after the
#              subtract; `i != n`, or unsigned, needs none.
#   __imulu    an index scaled by an entry that is not a power of two wide.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

IMAGE_MAX=239100        # bytes of acc.bin
HEAP_MIN=188223         # bytes from ___heapbot to ___heaptop
SETFLAG_MAX=538
IMULU_MAX=301

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "  [no agondev: budget check skipped]"; exit 77; }

make -s -f Makefile.agon >/dev/null 2>&1 || { echo "  FAIL the Agon build"; exit 1; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
for f in $(sed -n 's/^SRCS = //p; /^       src/p' Makefile.agon | tr -d '\\'); do
    # shellcheck disable=SC2046
    "$CC" $(make -s -f Makefile.agon cflags) \
        -S "$f" -o "$tmp/$(basename "$f" .c).s" || exit 1
done
cat "$tmp"/*.s > "$tmp/all.s"

image=$(stat -c%s bin/acc.bin)
heap=$(awk '$2 == "___heapbot" { b = strtonum($1) }
            $2 == "___heaptop" { t = strtonum($1) }
            END { print t - b }' bin/acc.map)
setflag=$(grep -c 'call[[:space:]]*pe, __setflag' "$tmp/all.s")
imulu=$(grep -c 'call[[:space:]]*__imulu' "$tmp/all.s")

fail=0
at_most() {
    if [ "$2" -le "$3" ]; then
        printf '  ok   %-28s %7d, at most %d\n' "$1" "$2" "$3"
    else
        printf '  FAIL %-28s %7d, more than %d\n' "$1" "$2" "$3"; fail=1
    fi
}
at_least() {
    if [ "$2" -ge "$3" ]; then
        printf '  ok   %-28s %7d, at least %d\n' "$1" "$2" "$3"
    else
        printf '  FAIL %-28s %7d, less than %d\n' "$1" "$2" "$3"; fail=1
    fi
}
at_most  "acc.bin" "$image" "$IMAGE_MAX"
at_least "the heap" "$heap" "$HEAP_MIN"
at_most  "signed compares (__setflag)" "$setflag" "$SETFLAG_MAX"
at_most  "multiplies (__imulu)" "$imulu" "$IMULU_MAX"
exit $fail

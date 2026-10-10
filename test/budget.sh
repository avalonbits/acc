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
#   __imulu    an index scaled by an entry that is not a power of two wide,
#              and the two of lvalue.c's exact_divide, which are meant.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

IMAGE_MAX=242780        # bytes of acc.bin
HEAP_MIN=184753         # bytes from ___heapbot to ___heaptop
SETFLAG_MAX=515
IMULU_MAX=299

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

# agondev makes a 0 in HL with or a / sbc hl, hl, which sets the flags --
# and has put that between a test and the jump or call that reads it, so
# that the test was lost: vstore_indirect's `if (val_number(...))
# force_into` became a call z that was always taken, and the Agon's acc
# disagreed with the host's (test/target.sh). Counted: a conditional on
# the zero or the carry flag whose nearest flag-setter before it, within
# its block and with no call between, is sbc hl, hl.
lost=$(python3 - "$tmp/all.s" <<'PY'
import re, sys
sets = re.compile(r'\s*(add|adc|sub|sbc|and|or|xor|cp|neg|rl|rr|sla|sra|srl|bit|'
                  r'cpl|scf|ccf|tst|daa|call|inc\s+[a-l(]|dec\s+[a-l(])\b')
cond = re.compile(r'\s*(jp|jr|call|ret)\s+(z|nz|c|nc)\b')
lines = open(sys.argv[1]).read().split('\n')
n = 0
for i, line in enumerate(lines):
    if not cond.match(line):
        continue
    j = i - 1
    while j >= 0 and not lines[j].rstrip().endswith(':') and not sets.match(lines[j]):
        j -= 1
    if j >= 0 and re.match(r'\s*sbc\s+hl,\s*hl', lines[j]):
        n += 1
print(n)
PY
)

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
at_most  "flags lost to sbc hl, hl" "$lost" 0
exit $fail

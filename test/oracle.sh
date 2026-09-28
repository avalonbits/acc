#!/bin/bash
# Builds the reference answer for a test, with agondev.
#
#   test/oracle.sh <source.c> <out.bin>
#
# There is no oracle for C that gives byte-identical output the way ez80asm
# does for an assembler -- two compilers may emit different code and both be
# right. What there is instead is a reference *answer*: compile the same
# source with agondev, run it, and require acc's program to report the same
# thing. That pins the semantics this target actually has -- a 24-bit int's
# wraparound, the rounding of a division -- to the implementation that
# defines them, rather than to what seems reasonable.
#
# agondev's crt0 is deliberately not linked: it clears bss, runs initialisers
# and starts stdio, and it brings its own header. test/oracle/start.s supplies
# a startup that reports the same way acc's does. libagon is still linked,
# because agondev's compiler calls it for arithmetic the chip cannot do.
set -u

AGONDEV=${AGONDEV:-$HOME/agondev}
here=$(cd "$(dirname "$0")" && pwd)

[ -x "$AGONDEV/bin/ez80-none-elf-clang" ] || { echo "no agondev (set AGONDEV)" >&2; exit 77; }
[ $# -eq 2 ] || { echo "usage: $0 <source.c> <out.bin>" >&2; exit 2; }

src=$1; out=$2
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

"$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full "$here/oracle/start.s" -o "$tmp/start.o" || exit 1
"$AGONDEV/bin/ez80-none-elf-clang" -mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc \
    -isystem "$AGONDEV/include" \
    -target ez80-none-elf -Oz -Wa,-march=ez80+full \
    -c "$src" -o "$tmp/t.o" || exit 1
"$AGONDEV/bin/ez80-none-elf-ld" --oformat binary -Ttext=0x40000 -e _start \
    -o "$out" "$tmp/start.o" "$tmp/t.o" -L"$AGONDEV/lib" -lagon || exit 1

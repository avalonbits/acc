#!/bin/bash
# A program built the way agondev builds one: its compiler at its flags, its
# own startup (crt0 in libagon, which sets sys_vars and handles arguments),
# its linker script and libagon -- the flags and the order are those of
# agondev's config/makefile.inc, so that what libagon does here is what it
# does for anyone who uses it.
#
#   test/agonref.sh <source.c> <out.bin>
#
# Needs agondev. Exits 77 without it.
set -u

AGONDEV=${AGONDEV:-$HOME/agondev}
[ -x "$AGONDEV/bin/ez80-none-elf-clang" ] || { echo "no agondev (set AGONDEV)" >&2; exit 77; }
[ $# -eq 2 ] || { echo "usage: $0 <source.c> <out.bin>" >&2; exit 2; }

src=$1; out=$2
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

"$AGONDEV/bin/ez80-none-elf-clang" -mllvm -z80-gas-style -mllvm -z80-print-zero-offset \
    -nostdinc -isystem "$AGONDEV/include" -target ez80-none-elf -DAGONDEV -Oz \
    -Wa,-march=ez80+full -fno-threadsafe-statics -w \
    -c "$src" -o "$tmp/t.o" || exit 1
"$AGONDEV/bin/ez80-none-elf-ld" -defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000 \
    -defsym=_has_exit_handler=0 -T "$AGONDEV/config/linker.conf" --oformat binary \
    -o "$out" "$tmp/t.o" -L"$AGONDEV/lib" -lagon || exit 1

#!/bin/bash
# Compiles and links a C file into a runnable Agon binary, with acc alone.
#
#   test/accld.sh <source.c> <out.bin>
#
# acc does the whole job: compile, link against libagon.a, and write the flat
# MOS image. agondev is still needed for the library itself -- acc does not
# have a C library of its own -- but not for any step of the build.
#
# ACC_USE_LD=1 links with agondev's ld instead, through its linker script.
# That is how the two are compared: test/run.sh builds the same source both
# ways and expects the same bytes.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
[ -d "$AGONDEV/lib" ] || { echo "SKIP: no agondev (set AGONDEV)" >&2; exit 77; }
[ $# -eq 2 ] || { echo "usage: $0 <source.c> <out.bin>" >&2; exit 2; }

src=$1; out=$2
obj=${out%.bin}.o

if [ "${ACC_USE_LD:-0}" = 1 ]; then
    [ -x "$AGONDEV/bin/ez80-none-elf-ld" ] || { echo "SKIP: no agondev ld" >&2; exit 77; }
    bin/acc -c "$src" -o "$obj" || exit 1
    # The defsyms are the ones agondev's own makefile passes; its linker.conf
    # reads them to place the stack and the heap.
    "$AGONDEV/bin/ez80-none-elf-ld" \
        -defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000 \
        -defsym=_has_exit_handler=0 \
        -T "$AGONDEV/config/linker.conf" --oformat binary \
        -o "$out" "$obj" -L"$AGONDEV/lib" -lagon || exit 1
    "$AGONDEV/bin/agondev-setname" "$out" >/dev/null
else
    bin/acc -nostdlib -o "$out" "$src" -L"$AGONDEV/lib" -lagon || exit 1
fi

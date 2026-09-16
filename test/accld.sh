#!/bin/bash
# Compiles a C file with acc and links it into a runnable Agon binary.
#
#   test/accld.sh <source.c> <out.bin>
#
# The link is done by agondev's ld against libagon.a. acc will grow its own
# linker and flat-binary writer -- it has to, to run on the Agon at all -- but
# using agondev's until then means the code generator can be tested on its
# own, and a fault can be bisected by linking acc output against agondev
# output in the same program.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
[ -x "$AGONDEV/bin/ez80-none-elf-ld" ] || { echo "SKIP: no agondev (set AGONDEV)" >&2; exit 77; }
[ $# -eq 2 ] || { echo "usage: $0 <source.c> <out.bin>" >&2; exit 2; }

src=$1; out=$2
obj=${out%.bin}.o

bin/acc -c "$src" -o "$obj" || exit 1

# The defsyms are the ones agondev's own makefile passes; linker.conf reads
# them to place the stack and the heap and to pick the argument-processing
# entry point.
"$AGONDEV/bin/ez80-none-elf-ld" \
    -defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000 \
    -defsym=_has_exit_handler=0 \
    -defsym=_parse_option=___arg_processing \
    -T "$AGONDEV/config/linker.conf" --oformat binary \
    -o "$out" "$obj" -L"$AGONDEV/lib" -lagon || exit 1

"$AGONDEV/bin/agondev-setname" "$out" >/dev/null

#!/bin/bash
# The Agon build keeps the sources it packs.
#
# Makefile.agon compiles each file from a copy with its messages packed
# (obj/agon/pp/*.c), made by one run that a stamp stands for. Left as
# intermediates, make deleted each copy once its object was built, and kept
# the stamp: the next build that had to remake one object -- after
# src/acc_build.h changed, say -- found the stamp current and the copy gone,
# and failed with "no such file obj/agon/pp/obj.c".
#
# Checked in a copy of the tree, building one object there, so that nothing
# in this one is touched.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
[ -x "$AGONDEV/bin/ez80-none-elf-clang" ] || { echo "  [no agondev: the packed-source check is skipped]"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
cp -r src Makefile.agon "$tmp/"

if ! (cd "$tmp" && make -s -f Makefile.agon AGONDEV="$AGONDEV" obj/agon/obj.o) >/dev/null 2>&1; then
    echo "  FAIL agonpp: obj/agon/obj.o does not build"
    exit 1
fi
if [ -f "$tmp/obj/agon/pp/obj.c" ]; then
    echo "  ok   the packed source is kept once its object is built"
else
    echo "  FAIL the packed source obj/agon/pp/obj.c was deleted once its object was built"
    exit 1
fi

# A tree built before the copies were kept: one gone, its stamp current.
rm -f "$tmp/obj/agon/pp/obj.c" "$tmp/obj/agon/obj.o"
if (cd "$tmp" && make -s -f Makefile.agon AGONDEV="$AGONDEV" obj/agon/obj.o) >/dev/null 2>&1; then
    echo "  ok   a copy already gone is packed again"
else
    echo "  FAIL a copy already gone, with its stamp current, is not packed again"
    exit 1
fi

# And the rebuild that used to fail: the object out of date, the stamp not.
touch "$tmp/src/acc_build.h"
if (cd "$tmp" && make -s -f Makefile.agon AGONDEV="$AGONDEV" obj/agon/obj.o) >/dev/null 2>&1; then
    echo "  ok   obj.o builds again after acc_build.h changes"
else
    echo "  FAIL obj.o does not build again after acc_build.h changes"
    exit 1
fi

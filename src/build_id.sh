#!/bin/sh
# What this acc is, as one number: a checksum of its own source.
#
# An object records it, and a later compile compares: an object made by a
# different compiler is out of date however untouched the program's own files
# are. Without this, changing the code generator and building again left every
# object saying it was still current, which during acc's own development is
# the common case rather than the rare one.
#
# From the source rather than from the binary, so that the host build and the
# Agon build of the same source come out with the same number -- they produce
# the same objects, and an object carried from one to the other has to be
# current on both. test/target.sh compares the two builds' objects byte for
# byte, so if these two ever disagreed it would say so.
#
# The generated header is left out, or the number would depend on itself.
# What is not covered is the flags acc was built with: a build at the wrong
# optimisation level is a different compiler that this cannot tell apart.
#
# Kept under 2^23 so that it is a positive number in an int of three bytes,
# which is what the Agon has and what the object file holds it in.
set -eu
cd "$(dirname "$0")/.."
ls src/*.c src/*.h | grep -v '^src/acc_build\.h$' | sort | xargs cat \
    | cksum | awk '{ printf "%d\n", $1 % 8388608 }'

#!/bin/bash
# Builds the release zip: unzip it at the root of an SD card and everything
# lands where acc on the Agon looks for it.
#
#   bin/acc.bin              MOS searches /bin, so `acc` works as a command
#   lib/acc/libc.a           the C library, linked into every program
#   lib/acc/rt.a             acc's runtime, which every program calls
#   lib/acc/include/         its headers, searched for every #include
#
# The library is the host build's: the host and Agon builds of acc produce
# the same objects byte for byte, which test/target.sh checks.
#
# Usage: ./mkrelease.sh [version]   (default: the version acc reports)
set -euo pipefail
cd "$(dirname "$0")"

VERSION=${1:-$(sed -n 's/.*ACC_VERSION "\(.*\)".*/\1/p' src/version.h)}
if [ -z "$VERSION" ]; then
    echo "mkrelease: no version given and none found in src/version.h" >&2

    exit 1
fi

if ! command -v zip >/dev/null; then
    echo "mkrelease: zip is not installed" >&2

    exit 1
fi

# The version in the binary has to be the version on the tin.
REPORTED=$(sed -n 's/.*ACC_VERSION "\(.*\)".*/\1/p' src/version.h)
if [ "$REPORTED" != "$VERSION" ]; then
    echo "mkrelease: src/version.h says $REPORTED but the zip would say $VERSION" >&2

    exit 1
fi

# Built fresh, not whatever is lying in bin/.
make >/dev/null
make -f Makefile.agon >/dev/null

for f in bin/acc.bin bin/libc.a bin/rt.a; do
    if [ ! -f "$f" ]; then
        echo "mkrelease: $f does not exist" >&2

        exit 1
    fi
done

OUT="acc-$VERSION.zip"
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT

mkdir -p "$STAGE/bin" "$STAGE/lib/acc"
cp bin/acc.bin "$STAGE/bin/"
cp bin/libc.a bin/rt.a "$STAGE/lib/acc/"
cp -r include "$STAGE/lib/acc/include"

rm -f "$OUT"
(cd "$STAGE" && zip -q -r -X "$OLDPWD/$OUT" .)

echo "$OUT"
unzip -l "$OUT" | sed -n '4,$p' | head -n -2

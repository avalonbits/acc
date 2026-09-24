#!/bin/bash
# Every VDP call's bytes, played into the VDP firmware itself.
#
#   test/vdpsync.sh <captured.txt>
#
# test/agonlib.sh holds each call to what libagon sends; this holds what is
# sent to what the VDP takes -- which is the only check there is for the
# calls libagon does not have. The captured lines come from a VDP test
# program's vdp.txt (test/agonlib/capture.h); test/vdpsync/replay.cpp sends
# each command into the VDP, built for the host the way the Fab Agon
# Emulator builds it, and requires the general poll after it to be answered.
#
# Needs a fab-agon-emulator source tree (FAB_EMU, default
# ~/code/fab-agon-emulator) whose src/vdp/vdp-console8 is an agon-vdp that
# builds with -DUSERSPACE. Exits 77 without one.
set -uo pipefail
cd "$(dirname "$0")/.."

EMU=${FAB_EMU:-$HOME/code/fab-agon-emulator}
VDPDIR=$EMU/src/vdp
[ $# -eq 1 ] || { echo "usage: $0 <captured.txt>" >&2; exit 2; }
if [ ! -d "$VDPDIR/userspace-vdp-gl/src" ] || [ ! -d "$VDPDIR/vdp-console8/video" ]; then
    echo "  [no fab-agon-emulator source at $EMU: the VDP replay is skipped]"
    exit 77
fi

out=$(mktemp -d); trap 'rm -rf "$out"' EXIT
if ! make -C "$VDPDIR" rust_glue.o vdp-console8.o userspace-vdp-gl/vdp_gl.a \
        > "$out/build.log" 2>&1; then
    echo "  FAIL the VDP would not build:"
    grep -m5 "error:" "$out/build.log" | sed 's/^/       /'
    exit 1
fi
(cd "$VDPDIR" && ${CXX:-c++} -Wall -O2 -std=c++17 -DUSERSPACE -I. -I.. -I./dispdrivers \
    -I./userspace-vdp-gl/src -I./userspace-vdp-gl/src/userspace-platform \
    -I./userspace-vdp-gl/src/dispdrivers \
    -I./userspace-vdp-gl/src/userspace-platform/matrix -I./vdp-console8/video \
    -o "$out/replay" "$OLDPWD/test/vdpsync/replay.cpp" \
    rust_glue.o vdp-console8.o userspace-vdp-gl/src/vdp-gl.a -pthread) || exit 1
"$out/replay" "$1"

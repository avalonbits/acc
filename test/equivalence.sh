#!/bin/bash
# Compares acc-i386's output against pristine tinycc's, byte for byte.
#
# This is the safety net for changes to the shared core. acc's i386 target is
# unchanged tinycc semantics, so any change that is supposed to be a port and
# not a behaviour change must leave every generated object identical. A diff
# here is a real finding: it is how the ctype_t widening was caught leaving a
# variable uninitialised in gv(), which no amount of "it still compiles" would
# have shown.
#
# It needs a pristine tinycc, for the reference compiler and for the corpus.
# The tree is cloned once into test/ref and reused; set ACC_REF_TCC to point
# at an existing checkout instead.
set -uo pipefail
cd "$(dirname "$0")/.."

REF=${ACC_REF_TCC:-test/ref}
UPSTREAM=https://github.com/TinyCC/tinycc.git
# The commit acc was vendored from, so the comparison is against the same
# code and not against whatever upstream has moved to.
PIN=$(sed -n 's/^commit: *//p' docs/UPSTREAM)

[ -x bin/acc-i386 ] || { echo "bin/acc-i386 missing -- run make"; exit 2; }

if [ ! -x "$REF/tcc" ]; then
    if [ ! -d "$REF" ]; then
        command -v git >/dev/null || { echo "SKIP: no git and no $REF"; exit 0; }
        echo "cloning pristine tinycc into $REF ..."
        git clone -q "$UPSTREAM" "$REF" 2>/dev/null || { echo "SKIP: clone failed (offline?)"; exit 0; }
        git -C "$REF" checkout -q "$PIN" 2>/dev/null || echo "  warning: could not check out $PIN, using default branch"
    fi
    echo "building pristine tinycc ..."
    ( cd "$REF" && ./configure --cpu=i386 >/dev/null 2>&1 && make -j8 tcc >/dev/null 2>&1 )
    [ -x "$REF/tcc" ] || { echo "SKIP: could not build reference tcc"; exit 0; }
fi

P=$REF/tcc
A=bin/acc-i386
# -nostdinc with a stub libc: the corpus wants <stdio.h> and friends, and the
# host's glibc headers do not survive a 32-bit cross tcc. The stubs declare
# what the corpus calls and nothing else; they are never linked, only parsed.
INC="-nostdinc -Itest/libcstub"

same=0; diff=0; skip=0; mism=0
for f in "$REF"/tests/tests2/*.c "$REF"/tests/*.c "$REF"/examples/*.c; do
    [ -f "$f" ] || continue
    $P -c "$f" -o /tmp/acc_ref.o  $INC -I"$REF/include" >/dev/null 2>&1; s1=$?
    $A -c "$f" -o /tmp/acc_new.o  $INC -Iinclude        >/dev/null 2>&1; s2=$?
    if [ $s1 -ne $s2 ]; then
        mism=$((mism+1)); echo "  ACCEPT/REJECT DIFFERS: $(basename "$f") (ref=$s1 acc=$s2)"; continue
    fi
    [ $s1 -ne 0 ] && { skip=$((skip+1)); continue; }
    if cmp -s /tmp/acc_ref.o /tmp/acc_new.o; then
        same=$((same+1))
    else
        diff=$((diff+1)); echo "  OBJECT DIFFERS: $(basename "$f")"
    fi
done
rm -f /tmp/acc_ref.o /tmp/acc_new.o

echo "  $same identical, $diff differing, $mism accept/reject mismatches, $skip rejected by both"
[ "$diff" -eq 0 ] && [ "$mism" -eq 0 ] && [ "$same" -gt 100 ]

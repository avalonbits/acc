#!/bin/bash
# zap, built by opt-acc with each of its backends made to take every
# function it can, assembling BBC BASIC: the same bytes as zap built by acc.
#
# The case suite and the conformance suites held while zap, built with the
# leaf backend on every function, lost the address of every `call.lis`: a
# program the size of zap has shapes the suites do not. It takes seconds.
#
#   test/optacc-zap.sh                  # needs ~/code/zap (ZAP) at v1.1.0
set -u
cd "$(dirname "$0")/.."
. test/emu.sh

ZAP=${ZAP:-$HOME/code/zap}
ZAP_REV=${ZAP_REV:-v1.1.0}
BASIC=test/corpus/Z_PRG_Agon-bbc-basic-v/tests

emu_available || exit 77
[ -x bin/acc ] && [ -x bin/opt-acc ] || { echo "run make first" >&2; exit 2; }
git -C "$ZAP" rev-parse -q --verify "$ZAP_REV^{commit}" >/dev/null 2>&1 \
    || { echo "  [no zap at $ZAP ($ZAP_REV): skipped]"; exit 77; }

z=$(mktemp -d); trap 'rm -rf "$z"' EXIT
mkdir -p "$z/src" "$z/basic"
git -C "$ZAP" archive "$ZAP_REV" src | tar -x -C "$z/src" --strip-components=1
rm -f "$z/src/zmalloc.c"                # zap's measuring shim, a link
git -C "$ZAP" archive "$ZAP_REV" "$BASIC" | tar -x -C "$z/basic" --strip-components=4
pass=0; fail=0

# assemble <name> <compiler> [VAR=value...]: zap built so, BBC BASIC
# assembled with it; prints the output's md5, or why there is none.
assemble() {
    local name=$1 cc=$2 objs= f o sd
    shift 2
    mkdir -p "$z/$name"
    for f in "$z/src"/*.c; do
        o="$z/$name/$(basename "$f" .c).o"
        env "$@" "$cc" -c "$f" -o "$o" -Iinclude -I"$z/src" -DAGONDEV >/dev/null 2>&1 \
            || { echo "no build of $(basename "$f")"; return; }
        objs="$objs $o"
    done
    "$cc" $objs bin/libc.a -o "$z/$name.bin" >/dev/null 2>&1 || { echo "no link"; return; }
    sd=$(emu_card)
    cp "$z/$name.bin" "$sd/bin/zap.bin"
    cp "$z/basic"/* "$sd/"
    printf 'try zap bbcbasicvez.s out.bin\r\n' > "$sd/autoexec.txt"
    ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=180 emu_run "$sd" -z -u >/dev/null 2>&1
    if [ -f "$sd/out.bin" ]; then md5sum < "$sd/out.bin" | cut -c1-8; else echo "no output"; fi
    rm -rf "$sd"
}

want=$(assemble acc bin/acc)
case $want in
  no*) echo "  FAIL zap built by acc: $want"; exit 1 ;;
esac

every='OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_IY=1 OPTACC_NATIVE=1 OPTACC_HOMES=2 OPTACC_PICK=0'
check() {
    local what=$1 got
    shift
    got=$(assemble "$@")
    if [ "$got" = "$want" ]; then
        printf '  ok   %s\n' "$what"; pass=$((pass + 1))
    else
        printf '  FAIL %-44s %s, not %s\n' "$what" "$got" "$want"; fail=$((fail + 1))
    fi
}
# shellcheck disable=SC2086
check "zap by opt-acc, as it is"               opt bin/opt-acc
# shellcheck disable=SC2086
check "zap by opt-acc, SSA on every function"  hybrid bin/opt-acc $every
# shellcheck disable=SC2086
check "zap by opt-acc, its own backend on all" leaf bin/opt-acc $every OPTACC_LEAF=1

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

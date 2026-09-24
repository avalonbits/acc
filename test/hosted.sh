#!/bin/bash
# The hosted library, against the C library of the machine running the test.
#
# Each program in test/hosted/ is built for the Agon and for the host, both
# are run, and what they print has to be the same line for line -- the way
# test/printf.sh holds printf to the host's. The host's library is the
# reference for everything that is rules rather than machinery: where
# strtol stops, what mbrtowc makes of a byte that is not UTF-8, which
# characters iswpunct says yes to.
#
# The programs print only what the two machines should agree on. A value
# that depends on the width of a type -- LONG_MAX, what 24 bits wrap to --
# is printed as a comparison against the macro that names it, so it reads
# 1 on both. The host runs in its C locale, which is the only one the Agon
# has.
#
#   test/hosted.sh                  every program
#   test/hosted.sh errno wchar      only these
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}
CC=${CC:-cc}

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
[ -f "$LIB" ] || { echo "$LIB missing -- run make"; exit 2; }
command -v "$CC" >/dev/null 2>&1 || { echo "no host compiler (set CC)"; exit 77; }
emu_available >/dev/null 2>&1 || exit 77

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

if [ $# -gt 0 ]; then
    srcs=(); for n in "$@"; do srcs+=("test/hosted/$n.c"); done
else
    srcs=(test/hosted/*.c)
fi

for src in "${srcs[@]}"; do
    name=$(basename "$src" .c)

    # The reference, from the host.
    if ! "$CC" -std=c99 -w -o "$tmp/host" "$src" -lm 2> "$tmp/ccerr"; then
        printf '  FAIL %-12s the host build failed\n' "$name"
        sed 's/^/         /' "$tmp/ccerr"
        fail=$((fail + 1)); continue
    fi
    # In a directory of its own, since some of them make files.
    mkdir -p "$tmp/run"
    (cd "$tmp/run" && LC_ALL=C "$tmp/host" > "$tmp/want" 2>&1)

    # And the same program on the Agon.
    if ! err=$("$ACC" -c "$src" -o "$tmp/p.o" -Iinclude 2>&1) \
       || ! err=$("$ACC" "$tmp/p.o" "$LIB" -o "$tmp/p.bin" 2>&1); then
        printf '  FAIL %-12s %s\n' "$name" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); continue
    fi

    sd=$(emu_card)
    cp "$tmp/p.bin" "$sd/bin/p.bin"
    printf 'p\r\n' > "$sd/autoexec.txt"
    ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-120} emu_run "$sd" -z -u > "$tmp/raw" 2>&1
    rm -rf "$sd"

    # Between MOS's banner and the six digits the startup stub prints on
    # the way out, without the carriage returns MOS's console adds.
    tr -d '\r' < "$tmp/raw" \
      | sed -n '/MOS Version/,/^[0-9a-fA-F]\{6\}$/p' \
      | sed '1d; $d' \
      | sed '/./,$!d' > "$tmp/got"

    if diff -u "$tmp/want" "$tmp/got" > "$tmp/diff"; then
        printf '  ok   %-12s %d lines match the host\n' "$name" \
            "$(wc -l < "$tmp/want")"
        pass=$((pass + 1))
    else
        printf '  FAIL %-12s does not agree with the host:\n' "$name"
        sed 's/^/         /' "$tmp/diff" | head -30
        fail=$((fail + 1))
    fi
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

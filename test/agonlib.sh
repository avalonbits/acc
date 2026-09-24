#!/bin/bash
# The Agon's own library -- <agon/mos.h>, <agon/vdp.h> and the rest --
# against libagon's.
#
# Each program in test/agonlib/ is built twice, once by acc against
# bin/libc.a and once by agondev against libagon (test/agonref.sh), and each
# is run on the emulator under MOS 3.0.2. What they print has to be the same
# line for line. libagon is the reference for what each call does: the
# registers it sets, what it makes of what MOS answers, and -- for the VDP
# -- the bytes it sends, which the programs capture by defining putch and
# mos_puts themselves. Where acc has a call libagon does not, the program
# says what it should answer under `#ifdef __ACC__`... and libagon's build
# prints the same thing from a table, so that both still agree line for
# line; see the programs.
#
#   test/agonlib.sh                every program
#   test/agonlib.sh mos vdp        only these
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
emu_available >/dev/null 2>&1 || exit 77
[ -x "${AGONDEV:-$HOME/agondev}/bin/ez80-none-elf-clang" ] || { echo "  [no agondev: the libagon comparison is skipped]"; exit 77; }

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

if [ $# -gt 0 ]; then
    srcs=(); for n in "$@"; do srcs+=("test/agonlib/$n.c"); done
else
    srcs=(test/agonlib/*.c)
fi

# run <bin> <out>: what the program printed, without the banner, the prompt
# after it, or the six digits acc's startup prints on the way out.
run() {
    local sd
    sd=$(emu_card)
    cp "$1" "$sd/bin/p.bin"
    printf 'bin/p\r\n' > "$sd/autoexec.txt"
    ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-120} emu_run "$sd" -z -u 2>&1 \
      | tr -d '\r' \
      | sed -n '/MOS Version/,$p' \
      | sed '1d; /^\/ \*/,$d' \
      | sed '/./,$!d' \
      | sed '${/^[0-9A-F]\{6\}$/d}' > "$2"
    # And what it wrote to vdp.txt, which is where the VDP tests put what
    # they caught: see test/agonlib/capture.h.
    if [ -f "$sd/vdp.txt" ]; then
        echo "--- vdp.txt" >> "$2"
        tr -d '\r' < "$sd/vdp.txt" >> "$2"
    fi
    rm -rf "$sd"
}

for src in "${srcs[@]}"; do
    name=$(basename "$src" .c)

    if ! err=$(test/agonref.sh "$src" "$tmp/ref.bin" 2>&1); then
        printf '  FAIL %-12s agondev would not build it: %s\n' "$name" \
            "$(printf '%s' "$err" | grep -m1 error)"
        fail=$((fail + 1)); continue
    fi
    if ! err=$("$ACC" -c "$src" -o "$tmp/p.o" -Iinclude 2>&1) \
       || ! err=$("$ACC" "$tmp/p.o" "$LIB" -o "$tmp/acc.bin" 2>&1); then
        printf '  FAIL %-12s %s\n' "$name" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); continue
    fi
    run "$tmp/ref.bin" "$tmp/want"
    run "$tmp/acc.bin" "$tmp/got"

    if [ ! -s "$tmp/want" ]; then
        printf '  FAIL %-12s libagon'"'"'s build printed nothing\n' "$name"
        fail=$((fail + 1))
    elif diff -u "$tmp/want" "$tmp/got" > "$tmp/diff"; then
        printf '  ok   %-12s %d lines match libagon\n' "$name" "$(wc -l < "$tmp/want")"
        pass=$((pass + 1))
    else
        printf '  FAIL %-12s does not agree with libagon:\n' "$name"
        sed 's/^/         /' "$tmp/diff" | head -80
        fail=$((fail + 1))
    fi
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

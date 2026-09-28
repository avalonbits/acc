#!/bin/bash
# kbuf keeps every key a busy program is sent, as libagon's does.
#
# test/keyboard/kbuf.c starts a buffer and then does not poll it until MOS
# has counted all the keys it is waiting for; only then does it read the
# buffer out. Keys come in on the emulator's stdin, which it turns into a key
# packet down and one up for each character of a line and then of a return
# -- so "abc" is eight packets, a return being the last two.
#
#   - a buffer longer than that gives all eight, in the order they came;
#   - a buffer of four gives the first four, and drops what came after, as a
#     full ring does;
#   - typed twice into a buffer of four, read out in between, the second
#     round's first four come out too: reading goes round the end of the
#     ring and back to its start;
#   - emptied with kbuf_clear before it is read, it gives nothing.
#
# A kbuf that reads the system variables instead of being called on each
# packet sees only the last of them, and fails both.
#
# With agondev there, the same program is built against libagon's kbuf and
# held to the same answers first: that is what says the keys arrive as this
# expects, so that a failure of acc's is acc's.
#
# Skips (77) without the emulator.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

emu_available || exit 77
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }

AGONDEV=${AGONDEV:-$HOME/agondev}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
bin/acc test/keyboard/kbuf.c -I include bin/libc.a -o "$tmp/acc.bin" >/dev/null \
    || { echo "  FAIL keyboard: kbuf.c does not compile"; exit 1; }
fail=0

# run <program> <args>: its GOT lines, one to a round, with "abc" typed
# each time it says READY -- once the buffer is there to take the keys.
run() {
    local sd fifo cap hold emu typed=0

    sd=$(emu_card); fifo=$(mktemp -u); cap=$(mktemp)
    cp "$1" "$sd/bin/kbuf.bin"
    printf 'kbuf %s\r\n' "$2" > "$sd/autoexec.txt"
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" & hold=$!
    (cd "$EMU" && exec timeout 60 ./agon-cli-emulator --sdcard "$sd" \
        --mos "$EMU_MOS" -z < "$fifo" > "$cap" 2>&1) & emu=$!

    while kill -0 "$emu" 2>/dev/null && ! grep -q EVENTS "$cap"; do
        if [ "$(grep -c READY "$cap")" -gt "$typed" ]; then
            printf 'abc\n' > "$fifo"
            typed=$((typed + 1))
        fi
        sleep 0.1
    done
    kill "$emu" "$hold" 2>/dev/null
    wait "$emu" "$hold" 2>/dev/null
    tr -d '\r' < "$cap" | grep '^GOT' | sed 's/^GOT *//' | paste -sd'|'
    rm -rf "$sd" "$fifo" "$cap"
}

check() {      # check <who> <program> <args> <what is wanted> <what it is>
    local got

    got=$(run "$2" "$3")
    if [ "$got" = "$4" ]; then
        echo "  ok   $1, $5: $4"
    else
        echo "  FAIL $1, $5: wanted '$4', got '$got'"
        fail=1
    fi
}

cases() {      # cases <who> <program>
    check "$1" "$2" '16 8 1' "$all" 'a buffer of 16'
    check "$1" "$2" '4 8 1' "$first" 'a buffer of 4'
    check "$1" "$2" '4 8 2' "$first|$first" 'a buffer of 4, twice'
    check "$1" "$2" '16 8 1 clear' '' 'cleared'
}

all='61d 61u 62d 62u 63d 63u 0dd 0du'
first='61d 61u 62d 62u'

if [ -x "$AGONDEV/bin/ez80-none-elf-clang" ]; then
    "$AGONDEV/bin/ez80-none-elf-clang" -mllvm -z80-gas-style \
        -mllvm -z80-print-zero-offset -target ez80-none-elf -Oz -nostdinc \
        -isystem "$AGONDEV/include" -Wa,-march=ez80+full -w \
        -c test/keyboard/kbuf.c -o "$tmp/kbuf.o" \
    && "$AGONDEV/bin/ez80-none-elf-ld" -defsym=RAM_START=0x40000 \
        -defsym=RAM_SIZE=0x70000 -defsym=_has_exit_handler=0 \
        -T "$AGONDEV/config/linker.conf" --oformat binary \
        -o "$tmp/agondev.bin" "$tmp/kbuf.o" -L"$AGONDEV/lib" -lagon \
    || { echo "  FAIL keyboard: libagon's build of kbuf.c failed"; exit 1; }
    cases libagon "$tmp/agondev.bin"
    [ "$fail" = 0 ] || { echo "  FAIL keyboard: the harness, not acc"; exit 1; }
fi

cases acc "$tmp/acc.bin"

exit $fail

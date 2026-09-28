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
#     full ring does.
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

# run <program> <length>: its GOT line, with "abc" typed once it is READY.
run() {
    local sd fifo cap hold emu line=

    sd=$(emu_card); fifo=$(mktemp -u); cap=$(mktemp)
    cp "$1" "$sd/bin/kbuf.bin"
    printf 'kbuf %s 8\r\n' "$2" > "$sd/autoexec.txt"
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" & hold=$!
    (cd "$EMU" && exec timeout 60 ./agon-cli-emulator --sdcard "$sd" \
        --mos "$EMU_MOS" -z < "$fifo" > "$cap" 2>&1) & emu=$!

    # The keys go in only once the buffer is there to take them.
    while kill -0 "$emu" 2>/dev/null && ! grep -q READY "$cap"; do
        sleep 0.1
    done
    printf 'abc\n' > "$fifo"
    while kill -0 "$emu" 2>/dev/null && ! grep -q EVENTS "$cap"; do
        sleep 0.1
    done
    kill "$emu" "$hold" 2>/dev/null
    wait "$emu" "$hold" 2>/dev/null
    line=$(tr -d '\r' < "$cap" | grep -m1 '^GOT')
    rm -rf "$sd" "$fifo" "$cap"
    echo "$line"
}

check() {      # check <who> <program> <length> <what is wanted>
    local got

    got=$(run "$2" "$3")
    if [ "$got" = "GOT $4" ]; then
        echo "  ok   $1, a buffer of $3: $4"
    else
        echo "  FAIL $1, a buffer of $3: wanted '$4', got '${got#GOT }'"
        fail=1
    fi
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
    check libagon "$tmp/agondev.bin" 16 "$all"
    check libagon "$tmp/agondev.bin" 4 "$first"
    [ "$fail" = 0 ] || { echo "  FAIL keyboard: the harness, not acc"; exit 1; }
fi

check acc "$tmp/acc.bin" 16 "$all"
check acc "$tmp/acc.bin" 4 "$first"

exit $fail

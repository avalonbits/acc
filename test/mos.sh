#!/bin/bash
# What a program acc compiles owes MOS when it returns.
#
# MOS keeps something of its own in IY and wants it back. A program that
# returns having changed it takes the machine down -- and takes it down after
# it has run and printed its answer, which is about the most confusing moment
# for that to happen: the output is right there on the screen and the machine
# reboots underneath it, over and over, because autoexec runs again.
#
# The backend uses IY as its own scratch, so a program will have changed it.
# The entry stub is the one place that can put it back whatever the program
# did, and does.
#
# Every other test runs programs built with -x, which stop the machine rather
# than returning to MOS, so none of them goes near this.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
emu_available >/dev/null 2>&1 || exit 77

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# returns <name> <six hex digits it should print> <source>
returns() {
    local what=$1 want=$2 sd out banners printed

    printf '%s' "$3" > "$tmp/p.c"
    if ! err=$("$ACC" "$tmp/p.c" -o "$tmp/p.bin" 2>&1); then
        printf '  FAIL %-34s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi

    sd=$(emu_card)
    cp "$tmp/p.bin" "$sd/bin/p.bin"
    printf 'p\r\n' > "$sd/autoexec.txt"
    out=$(ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-30} emu_run "$sd" -z -u 2>&1)
    rm -rf "$sd"

    # One banner: the machine booted once and stayed up. More than one means
    # it reset and autoexec ran again, which is what a clobbered IY looks
    # like from outside.
    banners=$(printf '%s' "$out" | grep -c 'MOS Version')
    # The stub prints its digits in capitals, so the wanted value is read
    # the same way rather than written twice.
    printed=$(printf '%s' "$out" | grep -oE '^[0-9a-fA-F]{6}' | head -1 \
              | tr 'a-f' 'A-F')
    want=$(printf '%s' "$want" | tr 'a-f' 'A-F')

    if [ "$banners" != 1 ]; then
        printf '  FAIL %-34s the machine booted %s times\n' "$what" "$banners"
        fail=$((fail + 1))
    elif [ "$printed" != "$want" ]; then
        printf '  FAIL %-34s printed "%s", want "%s"\n' "$what" "$printed" "$want"
        fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
}

returns "the simplest program there is" 000007 \
'int main(void) { return 7; }
'
# Through a pointer, which is where the backend reaches for IY: it pushes the
# pointer into IY to read two bytes without disturbing what the allocator is
# holding.
returns "a program that uses IY itself" 000029 \
'short values[3] = { 20, 21, 0 };

int main(void) {
    short *p = values;

    return p[0] + p[1];
}
'
# And a call through a pointer, which goes through IY as well.
returns "a call through a pointer" 00002a \
'int twice(int n) { return n + n; }

int main(void) {
    int (*f)(int) = twice;

    return f(21);
}
'
# Something with a little of everything, so that IY has been through the mill.
returns "a program that does some work" 000177 \
'int table[16];

int fill(int n) {
    int i;

    for (i = 0; i < n; i++)
        table[i] = i * 3;

    return n;
}

int main(void) {
    char text[4];
    int total = 0, i;           /* 3 * (0 + 1 + ... + 15) = 360, and 15 more */

    fill(16);
    for (i = 0; i < 16; i++)
        total += table[i];
    text[0] = 15;

    return total + text[0];
}
'

# exit does not return: it puts the stack back where main was called on and
# carries on just after the call, so the tail below runs as though main had
# returned and MOS gets its machine back in the same state. Everything in
# between -- five frames of it -- is skipped, epilogues and all.
#
# Which is what the reboot check above is here for: when this was first
# written the two cells exit reads were addressed as an offset into the bss,
# and an object compiled on its own has no idea where the link will put them.
# It read two addresses out of somewhere else's variables, jumped to whatever
# was in them and the machine came up again. Repeatedly.
returns "exit from five frames down" 00002a \
'void exit(int status);

static short scratch[2] = { 1, 2 };

static void f5(void) { exit(40 + scratch[1]); }
static void f4(void) { f5(); }
static void f3(void) { f4(); }
static void f2(void) { f3(); }
static void f1(void) { f2(); }

int main(void) {
    f1();

    return 9;
}
'
# And straight from main, where there is nothing to unwind past.
returns "exit from main itself" 000007 \
'void exit(int status);

int main(void) {
    exit(7);

    return 9;
}
'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

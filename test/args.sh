#!/bin/bash
# What main is handed: argc and argv, as MOS passes them.
#
# MOS enters a program with HL pointing at what was typed after the command's
# name. The entry stub saves it, and the routine acc emits at the end of the
# image turns it into the array C says main takes -- with the name MOS did
# not pass put in front as argv[0], which is the name the program was built
# under.
#
# None of that can be tested by compiling alone, and none of it by the
# return-42 cases either: those run with no command line at all. So this runs
# programs through MOS with arguments after them and reads what they made of
# them, the way test/mos.sh reads a return value.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
emu_available >/dev/null 2>&1 || exit 77

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# What every program here is compiled with: a comparison, since these run
# with no library.
PRELUDE='static int same(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }

    return *a == 0 && *b == 0;
}
'

# runs <name> <six hex digits it should print> <what follows the command>
#      <source>
runs() {
    local what=$1 want=$2 tail=$3 sd out banners printed

    printf '%s%s' "$PRELUDE" "$4" > "$tmp/p.c"
    if ! err=$("$ACC" "$tmp/p.c" -o "$tmp/p.bin" 2>&1); then
        printf '  FAIL %-34s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi

    sd=$(emu_card)
    cp "$tmp/p.bin" "$sd/bin/p.bin"
    printf 'p%s\r\n' "$tail" > "$sd/autoexec.txt"
    out=$(ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-30} emu_run "$sd" -z -u 2>&1)
    rm -rf "$sd"

    banners=$(printf '%s' "$out" | grep -c 'MOS Version')
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

# Three arguments, and the name in front of them. 42.
runs "three arguments and the name" 00002a ' one two three' \
'int main(int argc, char **argv) {
    int r = 0;

    if (argc == 4) r++;
    if (same(argv[0], "p.bin")) r++;
    if (same(argv[1], "one")) r++;
    if (same(argv[2], "two")) r++;
    if (same(argv[3], "three")) r++;

    return r * 8 + 2;
}
'

# None at all: argc is 1 and argv[0] is still there.
runs "no arguments" 00002a '' \
'int main(int argc, char **argv) {
    int r = 0;

    if (argc == 1) r++;
    if (same(argv[0], "p.bin")) r++;

    return r * 20 + 2;
}
'

# More than one space between them, and trailing spaces, which the walk skips
# rather than turning into empty arguments.
runs "spaces between the arguments" 00002a '   a    bb  ' \
'int main(int argc, char **argv) {
    int r = 0;

    if (argc == 3) r++;
    if (same(argv[1], "a")) r++;
    if (same(argv[2], "bb")) r++;

    return r * 13 + 3;
}
'

# argv is an array of pointers and the strings are separate: writing through
# one does not reach the next.
runs "the arguments are strings of their own" 00002a ' abc de' \
'int main(int argc, char **argv) {
    int r = 0;

    if (argc == 3) r++;
    if (argv[1][3] == 0) r++;          /* a zero of its own, not a space */
    if (argv[2][2] == 0) r++;
    argv[1][0] = 122;
    if (same(argv[1], "zbc") && same(argv[2], "de")) r++;

    return r * 10 + 2;
}
'

# A main that takes nothing still runs: the stub hands it two values it does
# not read, and takes them back itself.
runs "a main that takes no arguments" 00002a ' one two' \
'int main(void) { return 42; }
'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

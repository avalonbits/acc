#!/bin/bash
# Random programs, compiled by acc and by agondev and run on the Agon, which
# have to agree.
#
#   test/fuzz.sh [count] [seed]          # default 40 programs, a random seed
#
# The test cases are written by hand, and check what their author thought
# of. This checks what nobody did: test/fuzz/narrow.py writes programs of
# random expressions over narrow values held in locals and behind pointers,
# and each has to come out the same under both compilers. Its first run found
# three code-generation bugs in minutes that the cases had never reached.
#
# Not part of `make test`: a program costs two builds and two boots of the
# emulator, about a second, and the point is to run many. Run it after a
# change to the code generator, with a count in the hundreds.
#
# agondev's own build of a program occasionally hangs. When it does, the
# program is compiled for the host by cc instead -- every value stays inside a
# 24-bit int, so the host's 32 bits give the same answer -- and acc has to
# agree with that.
#
# Programs that disagree are kept, and where is printed; test/fuzz/reduce.py
# cuts one down to what is needed to show it.
set -u
cd "$(dirname "$0")/.."

count=${1:-40}
seed=${2:-$RANDOM}

[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }
. test/emu.sh
emu_available || exit 77

work=$(mktemp -d)
keep=$(mktemp -d "${TMPDIR:-/tmp}/acc-fuzz-$seed.XXXX")
trap 'rm -rf "$work"' EXIT
export ASAN_OPTIONS=detect_leaks=0

python3 test/fuzz/narrow.py "$work" "$count" "$seed" || exit 2
echo "seed $seed, $count programs"

# What the host makes of it, as the low byte of main's result.
host_answer() {
    sed 's/int main(void)/int fuzz_main(void)/' "$1" > "$work/host.c"
    printf '#include <stdio.h>\nint main(void) { printf("%%d\\n", fuzz_main() & 255); return 0; }\n' \
        >> "$work/host.c"
    cc -std=c99 -fsigned-char -w -o "$work/host" "$work/host.c" && "$work/host"
}

bad=0
for src in "$work"/n*.c; do
    name=$(basename "$src" .c)

    if ! err=$(bin/acc "$src" -o "$work/a.bin" -x 2>&1); then
        echo "  FAIL $name  acc could not compile it: $(printf '%s' "$err" | head -1)"
        cp "$src" "$keep/"; bad=$((bad + 1)); continue
    fi
    test/agon.sh "$work/a.bin" >/dev/null 2>&1; got=$?

    want=
    if test/oracle.sh "$src" "$work/r.bin" >/dev/null 2>&1; then
        test/agon.sh "$work/r.bin" >/dev/null 2>&1; want=$?
        [ "$want" = 124 ] && want=     # agondev's build hung
    fi
    [ -n "$want" ] || want=$(host_answer "$src") || want=

    if [ -z "$want" ]; then
        echo "  SKIP $name  no reference answer"
        continue
    fi
    if [ "$got" != "$want" ]; then
        echo "  FAIL $name  acc says $got, the reference $want"
        cp "$src" "$keep/"; bad=$((bad + 1))
    fi
done

if [ "$bad" -eq 0 ]; then
    rmdir "$keep"
    echo "  $count programs, all agree"
    exit 0
fi
echo "  $bad of $count disagree; kept in $keep"
exit 1

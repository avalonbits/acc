#!/bin/bash
# test/run.sh itself: that it reports cases in their own order though it runs
# them several at a time, counts a failure among them, and takes agondev's
# answer from bin/answers only under the reference program and the MOS that
# gave it.
#
# Eleven cases, so that an order taken from the names of their directories
# -- 1, 10, 11, 2 -- rather than their numbers shows. Each computes its answer
# through a volatile, so that no two of agondev's programs are the same; one
# comes out at 41 in both compilers, which run.sh has to call a failure.
#
# Needs agondev and the emulator. Skips (77) without them.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

[ -x bin/acc ] || { echo "bin/acc missing -- run make"; exit 2; }
emu_available >/dev/null 2>&1 || { echo "  [no emulator: the runner check is skipped]"; exit 77; }
[ -x "${AGONDEV:-$HOME/agondev}/bin/ez80-none-elf-clang" ] \
    || { echo "  [no agondev: the runner check is skipped]"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

check() {
    if eval "$2"; then
        echo "  ok   $1"
    else
        echo "  FAIL $1"; fail=1
    fi
}

cases=
for i in $(seq 11); do
    if [ "$i" -eq 4 ]; then answer=41; else answer=42; fi
    printf 'volatile int v = %d;\nint main(void) { return (v * 1000 + %d) & 255; }\n' \
        "$i" "$(( (answer - i * 1000 % 256 + 256) % 256 ))" > "$tmp/c$i.c"
    cases="$cases $tmp/c$i.c"
done

out=$(CASES=$cases ACC_ANSWERS=$tmp/answers RUN_JOBS=4 test/run.sh); rc=$?
check "a failing case fails the run" '[ $rc -ne 0 ]'
check "ten passed, one failed" \
    'printf "%s\n" "$out" | grep -q "^  10 passed, 1 failed, 0 skipped$"'
check "the one at 41 is the failure" \
    'printf "%s\n" "$out" | grep -q "FAIL c4 .*both say 41"'
order=$(printf '%s\n' "$out" | sed -n 's/^  [a-zA-Z]* *\(c[0-9]*\) .*/\1/p' | tr '\n' ' ')
check "reported in the order given" '[ "$order" = "c1 c2 c3 c4 c5 c6 c7 c8 c9 c10 c11 " ]'
check "ten answers kept, and only 42s" \
    '[ "$(cat "$tmp"/answers/* | grep -c "^42$")" -eq 10 ] && [ "$(ls "$tmp/answers" | wc -l)" -eq 10 ]'

# Every kept answer made wrong: a case run again has to take its answer from
# there -- and only when the reference program and the MOS are the same.
for f in "$tmp"/answers/*; do echo 7 > "$f"; done
out=$(CASES=$tmp/c1.c ACC_ANSWERS=$tmp/answers test/run.sh)
check "a kept answer is the one used" \
    'printf "%s\n" "$out" | grep -q "FAIL c1 .*agondev says 7"'

printf 'volatile int v = 12;\nint main(void) { return (v * 1000 + %d) & 255; }\n' \
    "$(( (42 - 12000 % 256 + 256) % 256 ))" > "$tmp/c12.c"
out=$(CASES=$tmp/c12.c ACC_ANSWERS=$tmp/answers test/run.sh)
check "another reference is not given it" \
    'printf "%s\n" "$out" | grep -q "^  ok   c12 "'

# The same MOS from another place, which is another key. The emulator wants
# its map beside it, under the same name.
mkdir "$tmp/mos"
cp "$EMU_MOS" "${EMU_MOS%.bin}.map" "$tmp/mos/"
out=$(CASES=$tmp/c1.c ACC_ANSWERS=$tmp/answers ACC_EMU_MOS=$tmp/mos/$(basename "$EMU_MOS") test/run.sh)
check "another MOS is not given it" \
    'printf "%s\n" "$out" | grep -q "^  ok   c1 "'

exit $fail

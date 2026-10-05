#!/bin/bash
# A command line of more than 15 words, on the Agon -- and the words of a
# file named @file among them, for more than MOS's line holds.
#
# agondev's startup splits what MOS passes into 16 words at most -- the
# program's name and 15 more -- and drops the rest without a word, which
# lost a link the last of its objects. acc splits the line again itself
# when that happens (agon_split in parse.c). Here the Agon's acc links
# twenty objects, and must write what the host's acc writes of them; and
# does it again with its console sent to a file, a redirection it must not
# take for an object, and a quoted name. And the same objects from a file,
# @objs.lnk, its words on lines ended CR LF and LF, one after a tab, one in
# quotes; a compile whose options are in a file; and a library made of the
# objects a file names.
#
#   test/manyargs.sh [acc.bin]          # default bin/acc.bin
#
# Needs the emulator. Skips (77) without it.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${1:-bin/acc.bin}
[ -f "$ACC" ] || { echo "$ACC missing -- run make -f Makefile.agon"; exit 2; }
[ -x bin/acc ] || { echo "bin/acc missing -- run make"; exit 2; }
emu_available >/dev/null 2>&1 || { echo "  [no emulator: many-arguments check skipped]"; exit 77; }

sd=$(emu_card); tmp=$(mktemp -d); trap 'rm -rf "$sd" "$tmp"' EXIT
fail=0
N=20

# f1 .. f20, one to an object, and a main that calls each.
objs=
calls=
for i in $(seq $N); do
    echo "int f$i(void) { return $i; }" > "$tmp/f$i.c"
    bin/acc -c "$tmp/f$i.c" -o "$sd/f$i.o" >/dev/null || exit 2
    objs="$objs f$i.o"
    calls="$calls + f$i()"
done
decls=$(for i in $(seq $N); do printf 'int f%d(void); ' "$i"; done)
echo "$decls int main(void) { return 0$calls; }" > "$tmp/m.c"
bin/acc -c "$tmp/m.c" -o "$sd/m.o" >/dev/null || exit 2
mkdir -p "$sd/a b"
cp "$sd/f$N.o" "$sd/a b/last.o"

# What the host makes of the same objects, under the same names.
(cd "$sd" && "$OLDPWD/bin/acc" m.o $objs -o many.bin >/dev/null) || exit 2
mv "$sd/many.bin" "$tmp/many.bin"
(cd "$sd" && "$OLDPWD/bin/acc" m.o ${objs% f$N.o} "a b/last.o" -o many.bin >/dev/null) || exit 2
mv "$sd/many.bin" "$tmp/quoted.bin"

# The words in files: the link, a compile's options, a library's objects.
{
    printf 'm.o\r\n'
    for o in ${objs% f$N.o}; do printf '%s\n' "$o"; done
    printf '\t"a b/last.o"\n'
} > "$sd/objs.lnk"
cp "$tmp/m.c" "$sd/m.c"
printf -- '-c m.c\n-o three/m.o\n' > "$sd/cc.lnk"
printf '%s\n' $objs > "$sd/lib.lnk"
(cd "$sd" && "$OLDPWD/bin/acc" @objs.lnk -o many.bin >/dev/null) || exit 2
cmp -s "$sd/many.bin" "$tmp/quoted.bin" \
    && echo "  ok   @file on the host links what the line does" \
    || { echo "  FAIL @file on the host links what the line does"; fail=1; }
rm -f "$sd/many.bin"
(cd "$sd" && "$OLDPWD/bin/acc" -a "$tmp/lib.a" $objs >/dev/null) || exit 2

cp "$ACC" "$sd/bin/acc.bin"
mkdir -p "$sd/lib/acc"
cp bin/libc.a bin/rt.a "$sd/lib/acc/"        # the runtime, as the release puts it
echo 'int main(void) { return 0; }' > "$tmp/stop.c"
bin/acc "$tmp/stop.c" -o "$sd/bin/stop.bin" -x >/dev/null || exit 2
mkdir -p "$sd/one" "$sd/two" "$sd/three"
{
    printf 'try acc m.o%s -o one/many.bin\r\n' "$objs"
    printf 'try acc m.o%s "a b/last.o" -o two/many.bin > log.txt\r\n' "${objs% f$N.o}"
    printf 'try acc @objs.lnk -o three/many.bin\r\n'
    printf 'try acc @cc.lnk\r\n'
    printf 'try acc -a three/lib.a @lib.lnk\r\n'
    printf 'stop\r\n'
} > "$sd/autoexec.txt"
out=$(ACC_EMU_TIMEOUT=180 emu_run "$sd" -z -u 2>&1 | tr -d '\r')

check() {     # check <what> <made> <want>
    if [ -f "$2" ] && cmp -s "$2" "$3"; then
        echo "  ok   $1"
    else
        echo "  FAIL $1"
        printf '%s\n' "$out" | grep -a 'error' | sed 's/^/         /'
        fail=1
    fi
}
check "$((N + 4)) words on the Agon link what the host links" "$sd/one/many.bin" "$tmp/many.bin"
check "and with a quoted name and a redirection" "$sd/two/many.bin" "$tmp/quoted.bin"
check "@objs.lnk on the Agon links what the line does" "$sd/three/many.bin" "$tmp/quoted.bin"
(cd "$sd" && "$OLDPWD/bin/acc" -c m.c -o "$tmp/m.o" >/dev/null) || exit 2
check "@cc.lnk compiles with the options it holds" "$sd/three/m.o" "$tmp/m.o"
check "-a @lib.lnk makes the library the line does" "$sd/three/lib.a" "$tmp/lib.a"
if grep -q 'Done in' "$sd/log.txt" 2>/dev/null; then
    echo "  ok   the redirection took the console"
else
    echo "  FAIL the redirection took the console"; fail=1
fi

exit $fail

#!/bin/bash
# The release as the Agon sees it: a card laid out the way mkrelease.sh lays
# out the zip, and the commands the README shows, run on the emulator by the
# Agon build of acc. It finds <stdio.h> in /lib/acc/include and printf in
# /lib/acc/libc.a without being told, and reports its version.
#
#   test/release.sh [acc.bin]           # default bin/acc.bin
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${1:-bin/acc.bin}

emu_available || exit 77
[ -f "$ACC" ] || { echo "no $ACC -- run make -f Makefile.agon" >&2; exit 2; }
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }
if pgrep -f '^[^ ]*agon-cli-emulator .*--sdcard' >/dev/null 2>&1; then
    echo "another emulator is running -- stop it first" >&2
    exit 2
fi

sd=$(emu_card); host=$(mktemp -d); trap 'rm -rf "$sd" "$host"' EXIT

cp "$ACC" "$sd/bin/acc.bin"
mkdir -p "$sd/lib/acc"
cp bin/libc.a bin/rt.a "$sd/lib/acc/"
cp -r include "$sd/lib/acc/include"

echo 'int main(void) { return 0; }' > "$host/stop.c"
bin/acc "$host/stop.c" -o "$sd/bin/stop.bin" -x >/dev/null || exit 2

cat > "$sd/hello.c" <<'EOF'
#include <stdio.h>
#include "greet.h"

int main(int argc, char **argv)
{
    printf("%s, %s\n", GREETING, argc > 1 ? argv[1] : "world");
    return 0;
}
EOF
cat > "$sd/one.c" <<'EOF'
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("one step, %s\n", argc > 1 ? argv[1] : "world");
    return 0;
}
EOF
# The README's obey example, as it is written there.
printf 'int square(int n);\n' > "$sd/util.h"
printf '#include "util.h"\n\nint square(int n)\n{\n    return n * n;\n}\n' \
    > "$sd/util.c"
printf '#include <stdio.h>\n#include "util.h"\n\nint main(void)\n{\n    printf("7 squared is %%d\\n", square(7));\n    return 0;\n}\n' \
    > "$sd/main.c"
printf 'acc -c main.c\r\nacc -c util.c\r\nacc main.o util.o -o prog.bin\r\n' \
    > "$sd/build.obey"
# main.c edited, for the second run: only it is compiled again.
sed 's/7 squared/Seven squared/' "$sd/main.c" > "$sd/main2.c"
# One step with <agon/mos.h>, whose declarations fill the heap: the link
# reads the library while the compile's tables are all still there.
printf '#include <agon/mos.h>\n\nint main(void)\n{\n    mos_putstring("mos in one step\\r\\n");\n    return 0;\n}\n' \
    > "$sd/mosone.c"

# The files acc writes besides the program -- a compile's map, a link's
# map, and -r's offsets -- for a program big enough to fill the heap, which
# is when a link ran out of memory on the Agon and left its map half
# written. Each has to be what the host writes, byte for byte.
mkdir -p "$sd/maps" "$host/maps"
cp test/cases/305_byte_compare.c "$sd/maps/t.c"
cp test/cases/305_byte_compare.c "$host/maps/t.c"

# A compile that fails, in an obey file: the line after it must not run,
# and MOS must not add a message of its own. It stops the autoexec too, so
# it is the last line, and the run ends at the prompt rather than at stop.
printf 'int main(void) { return nope; }\n' > "$sd/bad.c"
printf 'acc bad.c\r\necho not reached\r\n' > "$sd/bad.obey"
mkdir -p "$sd/common"
echo '#define GREETING "hello"' > "$sd/common/greet.h"

cat > "$sd/autoexec.txt" <<'EOF'
acc
acc -h
acc -v
acc -c hello.c -o hello.o -I common
acc hello.o -o hello.bin
hello Agon
acc -c hello.c -o hello.o -I common
acc one.c
one Agon
acc mosone.c
mosone
acc -c maps/t.c -o maps/t.o -map maps/t.map
acc maps/t.o -o maps/t.bin -map maps/l.map
acc -r maps/x.txt maps/t.c -o maps/u.bin
obey build.obey
prog
delete main.c
rename main2.c main.c
obey build.obey
prog
obey bad.obey
EOF
sed -i 's/$/\r/' "$sd/autoexec.txt"

ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=120 emu_run "$sd" -z -u > "$host/console.txt" 2>&1
tr -d '\r' < "$host/console.txt" > "$host/out.txt"

pass=0; fail=0
expect() {
    if grep -qxF -- "$2" "$host/out.txt"; then
        printf '  ok   %s\n' "$1"; pass=$((pass + 1))
    else
        printf '  FAIL %-40s no line "%s"\n' "$1" "$2"; fail=$((fail + 1))
    fi
}

expect_not() {
    if grep -qF -- "$2" "$host/out.txt"; then
        printf '  FAIL %-40s a line "%s"\n' "$1" "$2"; fail=$((fail + 1))
    else
        printf '  ok   %s\n' "$1"; pass=$((pass + 1))
    fi
}

version=$(sed -n 's/.*ACC_VERSION "\(.*\)".*/\1/p' src/version.h)
build=$(sed -n 's/.*ACC_BUILD \([0-9]*\).*/\1/p' src/acc_build.h)
expect "a bare acc is the summary"          "  acc -h                      every option"
expect "-h names the default places"        "Headers from /lib/acc/include, the library /lib/acc/libc.a."
expect "a failed compile says why"          "bad.c:1:25: error: 'nope' is not declared"
expect_not "and stops the obey file"        "not reached"
expect_not "MOS adds no internal error"     "Internal error"
expect_not "nor an invalid command"         "Invalid command"
expect "acc -v on the Agon"                  "acc $version (build $build)"
expect "a program linked with the defaults"  "hello, Agon"
expect "a current object is left alone"      "hello.o is up to date"
expect "one step, named after the source"    "one step, Agon"
expect "one step with <agon/mos.h>"          "mos in one step"
expect "the README's obey build"             "7 squared is 49"
expect "run again, an unchanged file is kept" "util.o is up to date"
expect "and the edited one compiled again"   "Seven squared is 49"

(cd "$host" && "$OLDPWD/bin/acc" -c maps/t.c -o maps/t.o -map maps/t.map \
    && "$OLDPWD/bin/acc" maps/t.o -o maps/t.bin -map maps/l.map \
    && "$OLDPWD/bin/acc" -r maps/x.txt maps/t.c -o maps/u.bin) >/dev/null 2>&1 \
    || { echo "  FAIL the host's maps"; exit 2; }
for f in t.map l.map x.txt t.bin u.bin; do
    if cmp -s "$sd/maps/$f" "$host/maps/$f"; then
        printf '  ok   %-40s\n' "$f as the host writes it"; pass=$((pass + 1))
    else
        printf '  FAIL %-40s %s\n' "$f as the host writes it" \
            "$([ -f "$sd/maps/$f" ] && echo differs || echo missing)"
        fail=$((fail + 1))
    fi
done

echo "  $pass passed, $fail failed"
[ $fail -eq 0 ] || { sed 's/^/    | /' "$host/out.txt" | tail -15; exit 1; }

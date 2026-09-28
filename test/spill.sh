#!/bin/bash
# A compile writes its image to a file as it goes, and gets the same bytes.
#
# The image of a compile goes to `<output>~` a function at a time (out_flush),
# so that a file of a hundred kilobytes of code need not be held whole on
# the Agon. Three things then reach back into what is in the file, and each
# is checked here against an acc that never writes to a file at all -- its
# image starts too big to fill -- which must give the same object and the
# same program, byte for byte:
#
#   - leaving out a file's unused static functions, which cuts bytes out of
#     the middle of the file and moves every address in it (spill_cut);
#   - a variable given room in the bss and then a value, whose uses are
#     read back to be moved (out_resident, through gen_bss_move);
#   - a const written again with its value (out_resident, through out_seek).
#
# A counting acc (ACC_TABLE_STATS) says each case did reach its path; and no
# `~` file is left behind.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
SRCS=$(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\')
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -DACC_TABLE_STATS -Isrc -o "$tmp/count" $SRCS \
    || { echo "  FAIL the counting acc does not build"; exit 1; }
mkdir -p "$tmp/whole"
cp src/*.c src/*.h "$tmp/whole/"
sed -i 's/^int out_start_cap = 4096;/int out_start_cap = 1 << 24;/' "$tmp/whole/out.c"
grep -q '^int out_start_cap = 1 << 24;' "$tmp/whole/out.c" \
    || { echo "  FAIL spill: cannot make the acc that holds its image whole"; exit 1; }
# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -I"$tmp/whole" -o "$tmp/whole/acc" \
    $(printf '%s\n' $SRCS | sed "s|^src/|$tmp/whole/|") \
    || { echo "  FAIL the acc that holds its image whole does not build"; exit 1; }

body() {       # body <n> <statement>: a function body n statements long
    local i
    for i in $(seq "$1"); do printf '    %s\n' "$2"; done
}

# Statics, used and not, among functions big enough to go to the file.
{
    for i in $(seq 12); do
        printf 'static int s%d(int x) {\n%s    return x;\n}\n' "$i" "$(body 60 'x = x * 3 + 1;')"
        printf 'int f%d(int x) {\n%s    return x%s;\n}\n' "$i" "$(body 60 'x = x * 5 + 2;')" \
            "$([ $((i % 3)) = 0 ] && printf ' + s%d(x)' "$i")"
    done
    echo 'int main(void) { return f1(1) + f3(2) + f6(3) != 0; }'
} > "$tmp/cut.c"

# A variable in the bss, used across a function, then given a value.
{
    echo 'int a[3];'
    printf 'int big(int x) {\n%s    return x;\n}\n' "$(body 400 'x = x * 3 + a[1];')"
    echo 'int a[3] = {1, 2, 3};'
    echo 'int main(void) { return big(1) != 0; }'
} > "$tmp/bss.c"

# A const declared, used across a function, and then given its value.
{
    echo 'const int k;'
    printf 'int big(int x) {\n%s    return x + k;\n}\n' "$(body 400 'x = x * 3 + 7;')"
    echo 'const int k = 5;'
    echo 'int main(void) { return big(1) != 0; }'
} > "$tmp/again.c"

check() {      # check <case> <what> <field>: the path was reached, the bytes agree
    local c=$1 what=$2 field=$3 counts n
    counts=$("$tmp/count" -c "$tmp/$c.c" -o "$tmp/$c-count.o" 2>&1 >/dev/null |
             sed -n 's/^spill //p')
    n=$(printf '%s\n' "$counts" | awk -v f="$field" '{ for (i = 2; i <= NF; i += 2) if ($i == f) print $(i - 1) }')
    "$tmp/whole/acc" -c "$tmp/$c.c" -o "$tmp/$c-whole.o" >/dev/null 2>&1
    bin/acc -c "$tmp/$c.c" -o "$tmp/$c.o" >/dev/null 2>&1
    (cd "$tmp" && "$tmp/whole/acc" "$c.c" -o "p.bin" >/dev/null 2>&1 && mv p.bin "$c-whole.bin")
    (cd "$tmp" && "$OLDPWD/bin/acc" "$c.c" -o "p.bin" >/dev/null 2>&1 && mv p.bin "$c.bin")
    if [ "${n:-0}" -lt 1 ]; then
        echo "  FAIL spill: $c did not reach $what ($counts)"; fail=1
    elif ! cmp -s -i 7 "$tmp/$c.o" "$tmp/$c-whole.o"; then
        echo "  FAIL spill: $c's object is not what an acc holding its image writes"; fail=1
    elif ! cmp -s "$tmp/$c.bin" "$tmp/$c-whole.bin"; then
        echo "  FAIL spill: $c's program is not what an acc holding its image writes"; fail=1
    else
        echo "  ok   $what: $counts"
    fi
}
check cut "the unused functions cut out of the file" cuts
check bss "a variable moved out of the bss, read back" reads
check again "a const given its value again, read back" reads

if ls "$tmp"/*~ >/dev/null 2>&1; then
    echo "  FAIL spill: a ~ file was left behind: $(ls "$tmp"/*~)"; fail=1
fi
exit $fail

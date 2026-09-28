#!/bin/bash
# A link's calls to what comes after them, thousands of them, in little room.
#
# Linking acc itself on the Agon made six thousand fixups, each a call or
# an address waiting on something placed later, and ran out of memory
# keeping them. Now:
#
#   - the fixups are kept in blocks of 256 (fixup_at), not in one array
#     doubling towards them, and the bss's slots likewise (bss_fixup);
#   - a link lets go of the relocations of what has gone to its file, which
#     it never looks at again;
#   - a fixup whose slot is in the file becomes, in its own place, an
#     addition the file gets as it is swept at the end (out_add_later),
#     rather than a patch kept beside it;
#   - a reference to a variable in the bss of an object already placed is
#     filled as it is placed, and is one of the bss's slots, not a fixup.
#
# Here sixteen objects make 12,800 calls to 800 functions in another placed
# after them, each checked by the program, and a host acc none of whose
# allocations may be over 48 KB (test/failalloc.c) links them, as bin/acc
# does. A counting acc (ACC_TABLE_STATS) says the calls in the file became
# additions, and that a variable's uses made after it was placed made no
# fixups. The program runs on the Agon when the emulator is there.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
SRCS=$(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\')
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -Isrc -o "$tmp/small" $SRCS test/failalloc.c \
    -Wl,--wrap=fopen,--wrap=fwrite,--wrap=malloc,--wrap=calloc,--wrap=realloc \
    || { echo "  FAIL the limited acc does not build"; exit 1; }
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -DACC_TABLE_STATS -Isrc -o "$tmp/count" $SRCS \
    || { echo "  FAIL the counting acc does not build"; exit 1; }

K=16 N=800
{
    for i in $(seq 0 $((N - 1))); do echo "int g$i(void) { return $i; }"; done
    echo 'int shared[64];'
} > "$tmp/b.c"
objs=
for k in $(seq 0 $((K - 1))); do
    {
        for i in $(seq 0 $((N - 1))); do echo "int g$i(void);"; done
        echo "int a$k(void) {"
        echo '    int s = 0;'
        for i in $(seq 0 $((N - 1))); do echo "    s += g$i() * $((k + 1));"; done
        echo "    return s == $(( (N - 1) * N / 2 * (k + 1) ));"
        echo '}'
    } > "$tmp/a$k.c"
    objs="$objs a$k.o"
done
{
    for k in $(seq 0 $((K - 1))); do echo "int a$k(void);"; done
    echo 'int main(void) {'
    echo -n '    return (1'
    for k in $(seq 0 $((K - 1))); do echo -n " & a$k()"; done
    echo ') ? 42 : 1;'
    echo '}'
} > "$tmp/m.c"
{
    echo 'extern int shared[64];'
    echo 'int c(void) {'
    for i in $(seq 200); do echo "    shared[$((i % 64))]++;"; done
    echo '    return shared[1];'
    echo '}'
} > "$tmp/c.c"
for f in b m c $(seq -f 'a%g' 0 $((K - 1))); do
    bin/acc -c "$tmp/$f.c" -o "$tmp/$f.o" >/dev/null || exit 2
done

# The calls, linked in little room, and as bin/acc links them -- under the
# same name, which a program's header holds.
mkdir -p "$tmp/want" "$tmp/got"
(cd "$tmp" && "$OLDPWD/bin/acc" m.o $objs b.o -o want/p.bin -x >/dev/null) || exit 2
if ! out=$(cd "$tmp" && FAILALLOC_OVER=49152 FAILALLOC_FROM_START=1 ./small m.o $objs b.o \
               -o got/p.bin -x 2>&1); then
    echo "  FAIL $((K * N)) calls forward did not link with no allocation over 48 KB"
    printf '%s\n' "$out" | grep error | sed 's/^/         /'
    fail=1
elif ! cmp -s "$tmp/got/p.bin" "$tmp/want/p.bin"; then
    echo "  FAIL linked in 48 KB pieces, $((K * N)) calls forward are not what bin/acc links"
    fail=1
else
    echo "  ok   $((K * N)) calls forward linked with no allocation over 48 KB"
fi

# The ones in the file, additions and not patches.
read -r fx pt ad <<< "$(cd "$tmp" && ./count m.o $objs b.o -o n.bin 2>&1 >/dev/null |
    sed -n 's/^fixups \([0-9]*\) patches \([0-9]*\) adds \([0-9]*\)$/\1 \2 \3/p')"
if [ "${fx:-0}" -ge $((K * N)) ] && [ "${ad:-0}" -ge $((K * N * 9 / 10)) ] \
   && [ $((pt - ad)) -lt 100 ]; then
    echo "  ok   of $fx fixups, $ad became additions to the file and $((pt - ad)) patches"
else
    echo "  FAIL of ${fx:-?} fixups, ${ad:-?} became additions to the file and $((${pt:-0} - ${ad:-0})) patches"
    fail=1
fi

# A variable's uses: after it is placed, none are fixups; before, each is.
before=$(cd "$tmp" && ./count c.o m.o $objs b.o -o n.bin 2>&1 >/dev/null | sed -n 's/^fixups \([0-9]*\).*/\1/p')
after=$(cd "$tmp" && ./count m.o $objs b.o c.o -o n.bin 2>&1 >/dev/null | sed -n 's/^fixups \([0-9]*\).*/\1/p')
if [ -n "$before" ] && [ -n "$after" ] && [ $((before - after)) -ge 200 ]; then
    echo "  ok   200 uses of a variable placed before them made no fixups ($after against $before)"
else
    echo "  FAIL 200 uses of a variable placed before them: ${after:-?} fixups against ${before:-?}"
    fail=1
fi

# And the program gives the answer every call is checked for.
if [ "$fail" = 0 ] && [ -x test/agon.sh ]; then
    test/agon.sh "$tmp/got/p.bin" >/dev/null 2>&1; v=$?
    if [ "$v" = 42 ]; then
        echo "  ok   the program's $((K * N)) calls each reach their function"
    elif [ "$v" != 77 ]; then
        echo "  FAIL the program returns $v, not 42"; fail=1
    fi
fi
exit $fail

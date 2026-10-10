#!/bin/bash
# A link writes its image to the file as it goes.
#
# The image of a link is the whole program, and held until the end it was
# most of what a link of zap needed and more than the Agon had. Between
# objects the link writes what it has to the file (out_flush) once it holds
# 8 KB, which a small program's whole image fits in; what is
# filled in after that waits as a patch, made in the file when the link
# ends. Checked here, with a host acc that reports what it held
# (test/peak.c, test/linkheap.sh) and one that counts what waited:
#
#   - four objects of 30,000 bytes each cost the link less heap than two of
#     them would: the image in memory is one object, not the program. The
#     acc measured holds a compile's image to 16 MB before it writes any
#     (out_flush_at), as a link must not wait for: on the Agon a compile
#     waits for 32 KB, and a link that did held that and an object besides;
#   - a slot whose symbol was placed before it is filled as it is placed,
#     and neither waits as a fixup nor becomes a patch;
#   - a link that fails after writing some of the file leaves no file.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
SRCS=$(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\')
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

mkdir -p "$tmp/late"
cp src/*.c src/*.h "$tmp/late/"
sed -i 's/^int out_flush_at = 2048;/int out_flush_at = 1 << 24;/' "$tmp/late/image.c"
grep -q '^int out_flush_at = 1 << 24;' "$tmp/late/image.c" \
    || { echo "  FAIL linkstream: cannot make an acc whose compile holds its image"; exit 1; }
# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -I"$tmp/late" -o "$tmp/peak" \
    $(printf '%s\n' $SRCS | sed "s|^src/|$tmp/late/|") test/peak.c \
    -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free \
    || { echo "  FAIL the measuring acc does not build"; exit 1; }
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -DACC_TABLE_STATS -Isrc -o "$tmp/count" $SRCS \
    || { echo "  FAIL the counting acc does not build"; exit 1; }

compile() {    # compile <name> <source text>
    printf '%s\n' "$2" > "$tmp/$1.c"
    bin/acc -c "$tmp/$1.c" -o "$tmp/$1.o" >/dev/null \
        || { echo "  FAIL linkstream: $1.c does not compile"; exit 1; }
}

# The heap: four objects of N bytes, against a program of almost nothing.
N=30000
for i in 1 2 3 4; do
    compile "b$i" "const char big$i[$N] = {1};"
done
compile m 'extern const char big1[], big2[], big3[], big4[];
int main(void) { return big1[0] + big2[0] + big3[0] + big4[0]; }'
compile s 'int main(void) { return 5; }'
big=$(cd "$tmp" && ./peak m.o b1.o b2.o b3.o b4.o -o m.bin 2>&1 >/dev/null |
      sed -n 's/^peak //p')
small=$(cd "$tmp" && ./peak s.o -o s.bin 2>&1 >/dev/null | sed -n 's/^peak //p')
if [ -z "$big" ] || [ -z "$small" ]; then
    echo "  FAIL linkstream: no count"; fail=1
elif [ $((big - small)) -lt $((2 * N)) ]; then
    echo "  ok   four objects of $N bytes cost the link $((big - small)) more bytes of heap"
else
    echo "  FAIL four objects of $N bytes cost the link $((big - small)) more" \
         "bytes of heap, as much as two of them"
    fail=1
fi

# The fixups: 200 calls to f, after f and before it.
calls=$(printf 'f() + %.0s' $(seq 200))
compile f 'int f(void) { return 1; }'
compile u "int f(void);
int main(void) { return ${calls}0; }"
compile pad "const char pad[$N] = {1};"
counts() {     # counts <objects...>: "<fixups> <patches>" when they end
    (cd "$tmp" && ./count "$@" -o c.bin 2>&1 >/dev/null |
     sed -n 's/^fixups \([0-9]*\) patches \([0-9]*\).*/\1 \2/p')
}
read -r fx pt <<< "$(counts f.o pad.o u.o)"
if [ "${fx:-999}" -lt 10 ] && [ "${pt:-999}" -lt 10 ]; then
    echo "  ok   200 calls to f placed before them: $fx fixups, $pt patches"
else
    echo "  FAIL 200 calls to f placed before them: ${fx:-?} fixups, ${pt:-?} patches"
    fail=1
fi
read -r fx pt <<< "$(counts u.o pad.o f.o)"
if [ "${fx:-0}" -ge 200 ] && [ "${pt:-0}" -ge 200 ]; then
    echo "  ok   200 calls to f placed after them: $fx fixups, $pt patches"
else
    echo "  FAIL 200 calls to f placed after them: ${fx:-?} fixups, ${pt:-?} patches"
    fail=1
fi

# A link that fails once the file is begun.
compile x "int missing(void);
const char pad2[$N] = {1};
int main(void) { return missing(); }"
if bin/acc "$tmp/x.o" "$tmp/pad.o" -o "$tmp/x.bin" >/dev/null 2>&1; then
    echo "  FAIL linkstream: a link with a name missing succeeded"; fail=1
elif [ -e "$tmp/x.bin" ]; then
    echo "  FAIL a link that failed left x.bin behind"; fail=1
else
    echo "  ok   a link that failed leaves no file"
fi

exit $fail

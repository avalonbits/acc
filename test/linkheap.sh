#!/bin/bash
# A link holds an object's text only in the image it is copied into.
#
# The text is most of an object, and read whole it was held twice at once
# -- in the object and in the image -- which on the Agon was what a link of
# zap ran out of. The link reads what comes before the text and copies the
# text straight from the file into the image. Here a host acc that reports
# the most heap it held at once (test/peak.c) links an object of 120,000
# bytes of text, and an object of almost none: the first may hold more by
# what the image needs for the text and no more. The image grows by
# doubling, and holds the old block and the new at once as it grows, so
# that is under twice the text; read whole, the object's text comes on top.
set -uo pipefail
cd "$(dirname "$0")/.."

N=120000
CC=${CC:-cc}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -Isrc -o "$tmp/acc" \
    $(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\') \
    test/peak.c -Wl,--wrap=malloc,--wrap=calloc,--wrap=realloc,--wrap=free \
    || { echo "  FAIL the measuring acc does not build"; exit 1; }

printf 'const char big[%d] = {1};\nint main(void) { return big[5]; }\n' "$N" \
    > "$tmp/big.c"
printf 'int main(void) { return 5; }\n' > "$tmp/small.c"

peak() {    # the most the link of $1.o held
    "$tmp/acc" -c "$tmp/$1.c" -o "$tmp/$1.o" >/dev/null 2>&1 \
        || { echo "  FAIL linkheap: $1.c does not compile" >&2; return 1; }
    "$tmp/acc" "$tmp/$1.o" -o "$tmp/$1.bin" 2>&1 >/dev/null | sed -n 's/^peak //p'
}
big=$(peak big) && small=$(peak small) || exit 1
if [ -z "$big" ] || [ -z "$small" ]; then
    echo "  FAIL linkheap: no count"; exit 1
fi
more=$((big - small))
if [ "$more" -lt $((2 * N)) ]; then
    echo "  linkheap: $N bytes of text cost the link $more more bytes of heap"
else
    echo "  FAIL linkheap: $N bytes of text cost the link $more more bytes of heap," \
         "twice the text or more"
    exit 1
fi

#!/bin/bash
# The object writer asks for no block the size of the relocations.
#
# On the Agon, vi.c compiled and then ran out of memory writing its object:
# the heap was not full, but the compile had left it in pieces, and the
# writer asked for one block of three bytes a relocation -- 12 KB for vi.c
# -- which no piece was big enough for. The writer now walks the calls out
# of the file beside the relocations (reloc_call in obj.c) and holds
# nothing that long. Here a host acc whose allocations of more than 8 KB
# fail once the object is open (test/failalloc.c) has to write the object
# of a file with 3,000 calls out of it, and write it as the real one does.
#
# And with nothing over 4 KB: the file's image went to `<output>~` as it
# was made (out_flush), and copying it out is done through a piece of 8 KB
# or, when there is no block that big, of one (piece in out.c).
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -Isrc -o "$tmp/acc" \
    $(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\') \
    test/failalloc.c -Wl,--wrap=fopen,--wrap=fwrite,--wrap=malloc,--wrap=calloc,--wrap=realloc \
    || { echo "  FAIL the limited acc does not build"; exit 1; }

{
    echo 'void g(void);'
    echo 'void f(void) {'
    for _ in $(seq 3000); do echo '    g();'; done
    echo '}'
} > "$tmp/t.c"

bin/acc -c "$tmp/t.c" -o "$tmp/want.o" >/dev/null || exit 2
if ! out=$(FAILALLOC_OVER=8192 "$tmp/acc" -c "$tmp/t.c" -o "$tmp/t.o" 2>&1); then
    echo "  FAIL objmem: writing the object of 3000 calls out asked for more than 8 KB at once"
    printf '%s\n' "$out" | grep error | sed 's/^/         /'
    exit 1
fi
if ! cmp -s -i 7 "$tmp/t.o" "$tmp/want.o"; then
    echo "  FAIL objmem: the object differs from the one bin/acc writes"
    exit 1
fi
if ! out=$(FAILALLOC_OVER=4096 "$tmp/acc" -c "$tmp/t.c" -o "$tmp/t4.o" 2>&1) \
   || ! cmp -s -i 7 "$tmp/t4.o" "$tmp/want.o"; then
    echo "  FAIL objmem: with no block over 4 KB, the object was not written as bin/acc writes it"
    printf '%s\n' "$out" | grep error | sed 's/^/         /'
    exit 1
fi
echo "  objmem: 3000 calls out written with no block over 8 KB, and over 4 KB"

#!/bin/bash
# A function's tables, given back when it is done.
#
# The jumps, the cuts, the pool and the arrays of a function are empty again
# when the next one begins, and kept at the size the biggest function made
# them, they held that much of the Agon's heap to the end of the file. Here a
# host acc built to say what they hold compiles one function of three
# thousand branches and then a small one, to an object: at the end the
# tables have to be back to the size a small function needs.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -DACC_TABLE_STATS -Isrc -o "$tmp/acc" \
    $(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\') \
    || { echo "  FAIL the counting acc does not build"; exit 1; }

{
    echo 'int big(int x) {'
    i=0
    while [ $i -lt 3000 ]; do echo "    if (x == $i) x++;"; i=$((i + 1)); done
    echo '    return x;'
    echo '}'
    echo 'int small(int x) { return x + 1; }'
} > "$tmp/t.c"

out=$("$tmp/acc" -c "$tmp/t.c" -o "$tmp/t.o" 2>&1 >/dev/null)
bytes=$(printf '%s\n' "$out" | sed -n 's/^tables //p')
if [ -z "$bytes" ]; then
    echo "  FAIL forget: no count ($out)"; exit 1
elif [ "$bytes" -le 4096 ]; then
    echo "  forget: after a function of 3000 branches the tables hold $bytes bytes"
else
    echo "  FAIL forget: after a function of 3000 branches the tables hold $bytes bytes"
    exit 1
fi

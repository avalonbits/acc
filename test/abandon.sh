#!/bin/bash
# A compile or a library that fails once its file is open leaves no file.
#
# On the Agon, vi.c ran out of memory while its object was being written,
# and left the object there with nothing in it; the link after it then said
# the object was "too short to be an object", which is not what went wrong.
# Here a host acc that fails once it has opened a file to write -- its next
# allocation, file opened to read or write (test/failalloc.c) -- compiles a
# file and makes a library, and each has to
# fail and leave nothing behind -- while the same two, with nothing
# failing, make both.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -Isrc -o "$tmp/acc" \
    $(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\') \
    test/failalloc.c -Wl,--wrap=fopen,--wrap=fwrite,--wrap=malloc,--wrap=calloc,--wrap=realloc \
    || { echo "  FAIL the failing acc does not build"; exit 1; }

echo 'int f(void) { return 1; }' > "$tmp/a.c"
echo 'int g(void) { return 2; }' > "$tmp/b.c"
bin/acc -c "$tmp/a.c" -o "$tmp/a.o" >/dev/null || exit 2
bin/acc -c "$tmp/b.c" -o "$tmp/b.o" >/dev/null || exit 2

check() {     # check <what> <file> <command...>
    local what=$1 file=$2
    shift 2
    if "$@" >/dev/null 2>&1; then
        echo "  FAIL $what: it did not fail"; fail=1
    elif [ -e "$file" ]; then
        echo "  FAIL $what: $(basename "$file") is left behind, $(stat -c%s "$file") bytes"
        fail=1
    else
        echo "  ok   $what: failed, and left nothing"
    fi
}
check "a compile" "$tmp/c.o" "$tmp/acc" -c "$tmp/a.c" -o "$tmp/c.o"
check "a library" "$tmp/l.a" "$tmp/acc" -a "$tmp/l.a" "$tmp/a.o" "$tmp/b.o"

# And the same with nothing failing, so that the two above failed for the
# reason they were meant to.
if bin/acc -c "$tmp/a.c" -o "$tmp/c.o" >/dev/null && [ -s "$tmp/c.o" ] \
   && bin/acc -a "$tmp/l.a" "$tmp/a.o" "$tmp/b.o" >/dev/null && [ -s "$tmp/l.a" ]; then
    echo "  ok   and with nothing failing, both are made"
else
    echo "  FAIL with nothing failing, the compile or the library was not made"
    fail=1
fi

exit $fail

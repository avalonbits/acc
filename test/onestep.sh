#!/bin/bash
# A program from a source in one step, with the objects and libraries named
# beside it linked after it, and the output named after the input when -o
# is not given.
#
# The one-step image has to be the image the two steps make: compile to an
# object, then link. A library is read only while something waits on a
# name, so a library nothing needs can be anything at all.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }
ACC=$(cd "$(dirname "$ACC")" && pwd)/$(basename "$ACC")
LIB=$PWD/bin/libc.a
INC=$PWD/include

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok()  { printf '  ok   %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf '  FAIL %-44s %s\n' "$1" "$2"; fail=$((fail + 1)); }
acc() { ASAN_OPTIONS=detect_leaks=0 "$ACC" "$@" >/dev/null 2>&1; }

cat > "$tmp/hello.c" <<'EOF'
#include <stdio.h>

int main(void)
{
    printf("hello\n");
    return 0;
}
EOF
printf 'int foo(void);\nint main(void) { return foo(); }\n' > "$tmp/calls.c"
printf 'int foo(void) { return 42; }\n' > "$tmp/foo.c"
printf 'int main(void) { return 0; }\n' > "$tmp/plain.c"
echo 'this is not a library' > "$tmp/junk.a"

# Each build in a directory of its own, since an image holds its own name.
mkdir -p "$tmp/one" "$tmp/two"
cp "$tmp/hello.c" "$tmp/one/"; cp "$tmp/hello.c" "$tmp/two/"
(cd "$tmp/two" && acc -c hello.c -o hello.o -I"$INC" \
    && acc hello.o "$LIB" -o hello.bin) || exit 2
if (cd "$tmp/one" && acc hello.c "$LIB" -o hello.bin -I"$INC") \
   && cmp -s "$tmp/one/hello.bin" "$tmp/two/hello.bin"; then
    ok "a source and a library, in one step"
else
    bad "a source and a library, in one step" "no image, or not the two steps' image"
fi

rm -rf "$tmp/one"/*; rm -rf "$tmp/two"/*
cp "$tmp/calls.c" "$tmp/foo.c" "$tmp/one/"; cp "$tmp/calls.c" "$tmp/foo.c" "$tmp/two/"
(cd "$tmp/two" && acc -c foo.c -o foo.o && acc -c calls.c -o calls.o \
    && acc calls.o foo.o -o calls.bin) || exit 2
if (cd "$tmp/one" && acc -c foo.c -o foo.o && acc calls.c foo.o -o calls.bin) \
   && cmp -s "$tmp/one/calls.bin" "$tmp/two/calls.bin"; then
    ok "a source and an object, in one step"
else
    bad "a source and an object, in one step" "no image, or not the two steps' image"
fi

printf 'int nowhere(void);\nint main(void) { return nowhere(); }\n' > "$tmp/nowhere.c"
if acc "$tmp/nowhere.c" -o "$tmp/nolib.bin"; then
    bad "a call nothing defines is still refused" "it compiled"
else
    ok "a call nothing defines is still refused"
fi

# Every program waits on the runtime, which rt.a beside the library has;
# with it named first, nothing is left waiting for the one after it.
if acc "$tmp/plain.c" "$(dirname "$LIB")/rt.a" "$tmp/junk.a" -o "$tmp/junk.bin"; then
    ok "a library nothing needs is not read"
else
    bad "a library nothing needs is not read" "the compile failed"
fi
if acc "$tmp/hello.c" "$tmp/junk.a" -o "$tmp/junk2.bin" -I"$INC"; then
    bad "a library something needs is read" "junk.a was passed over"
else
    ok "a library something needs is read"
fi

if acc -c "$tmp/calls.c" "$tmp/one/foo.o" -o "$tmp/c.o"; then
    bad "-c with an object is refused" "it compiled"
else
    ok "-c with an object is refused"
fi

# Named after the input, beside it, when -o is not given.
mkdir -p "$tmp/a.dir"
cp "$tmp/plain.c" "$tmp/a.dir/prog.c"
cp "$tmp/foo.c" "$tmp/a.dir/foo.c"
acc "$tmp/a.dir/prog.c"
[ -f "$tmp/a.dir/prog.bin" ] && ok "a source makes <name>.bin" \
    || bad "a source makes <name>.bin" "no $tmp/a.dir/prog.bin"
acc -c "$tmp/a.dir/foo.c"
[ -f "$tmp/a.dir/foo.o" ] && ok "-c makes <name>.o" \
    || bad "-c makes <name>.o" "no $tmp/a.dir/foo.o"
cp "$tmp/calls.c" "$tmp/a.dir/calls.c"
acc -c "$tmp/a.dir/calls.c"
acc "$tmp/a.dir/calls.o" "$tmp/a.dir/foo.o"
[ -f "$tmp/a.dir/calls.bin" ] && ok "a link is named after its first input" \
    || bad "a link is named after its first input" "no $tmp/a.dir/calls.bin"
cp "$tmp/plain.c" "$tmp/a.dir/noext"
acc "$tmp/a.dir/noext"
[ -f "$tmp/a.dir/noext.bin" ] && ok "a name with no extension gains one" \
    || bad "a name with no extension gains one" "no $tmp/a.dir/noext.bin"

echo "  $pass passed, $fail failed"
[ $fail -eq 0 ]

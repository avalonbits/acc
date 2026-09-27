#!/bin/bash
# What acc does without being told: -v, and the include directory and library
# a build can name (the Agon build names /lib/acc/include and
# /lib/acc/libc.a, where the release puts them).
#
# The host build names neither, so this builds a host acc that names the
# repository's include/ and bin/libc.a, and checks that it finds them, that
# -I is still looked in first, and that naming the library as well changes
# nothing. test/release.sh checks the Agon build's own paths on the Agon.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok()  { printf '  ok   %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf '  FAIL %-40s %s\n' "$1" "$2"; fail=$((fail + 1)); }

SRC="src/obj.c src/lex.c src/float.c src/sym.c src/gen.c src/out.c src/parse.c"
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -Isrc -o "$tmp/acc" $SRC \
    -DACC_INCLUDE_DIR="\"$PWD/include\"" -DACC_LIBC="\"$PWD/bin/libc.a\"" \
    || exit 2
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -Isrc -o "$tmp/acc-nolib" $SRC \
    -DACC_INCLUDE_DIR="\"$PWD/include\"" -DACC_LIBC="\"$tmp/none/libc.a\"" \
    || exit 2

version=$(sed -n 's/.*ACC_VERSION "\(.*\)".*/\1/p' src/version.h)
build=$(sed -n 's/.*ACC_BUILD \([0-9]*\).*/\1/p' src/acc_build.h)
for flag in -v --version; do
    got=$(bin/acc $flag 2>&1); rc=$?
    want="acc $version (build $build)"
    if [ $rc -eq 0 ] && [ "$(printf '%s' "$got" | tr -d '\r')" = "$want" ]; then
        ok "$flag prints the version"
    else
        bad "$flag prints the version" "got '$got' ($rc), want '$want'"
    fi
done

cat > "$tmp/hello.c" <<'EOF'
#include <stdio.h>

int main(void)
{
    printf("hello\n");
    return 0;
}
EOF

# The host build has no default: <stdio.h> is not found without -I.
if bin/acc -c "$tmp/hello.c" -o "$tmp/nodefault.o" >/dev/null 2>&1; then
    bad "the host build names no include directory" "<stdio.h> was found"
else
    ok "the host build names no include directory"
fi

# The reference: everything named. An image holds its own name, so every
# link below writes p.bin and the answer is moved aside.
bin/acc -c "$tmp/hello.c" -o "$tmp/ref.o" -Iinclude >/dev/null || exit 2
bin/acc "$tmp/ref.o" bin/libc.a -o "$tmp/p.bin" >/dev/null || exit 2
mv "$tmp/p.bin" "$tmp/ref.bin"

if "$tmp/acc" -c "$tmp/hello.c" -o "$tmp/hello.o" >/dev/null 2>&1; then
    ok "headers come from the default directory"
else
    bad "headers come from the default directory" "<stdio.h> was not found"
fi

rm -f "$tmp/p.bin"
if "$tmp/acc" "$tmp/hello.o" -o "$tmp/p.bin" >/dev/null 2>&1 \
   && cmp -s "$tmp/p.bin" "$tmp/ref.bin"; then
    ok "a link takes the default library"
else
    bad "a link takes the default library" "no image, or a different one"
fi

rm -f "$tmp/p.bin"
if "$tmp/acc" "$tmp/hello.o" bin/libc.a -o "$tmp/p.bin" >/dev/null 2>&1 \
   && cmp -s "$tmp/p.bin" "$tmp/ref.bin"; then
    ok "naming the library as well changes nothing"
else
    bad "naming the library as well changes nothing" "no image, or a different one"
fi

# -I first: a <stdio.h> of its own is the one read.
mkdir -p "$tmp/mine"
echo '#define MINE 1' > "$tmp/mine/stdio.h"
printf '#include <stdio.h>\n#ifndef MINE\n#error the default was read first\n#endif\nint main(void) { return 0; }\n' \
    > "$tmp/first.c"
if "$tmp/acc" -c "$tmp/first.c" -o "$tmp/first.o" -I "$tmp/mine" >/dev/null 2>&1; then
    ok "-I is looked in before the default"
else
    bad "-I is looked in before the default" "the default was read first"
fi

# With the library missing, a program that needs nothing from it links.
printf 'int main(void) { return 0; }\n' > "$tmp/plain.c"
if "$tmp/acc-nolib" -c "$tmp/plain.c" -o "$tmp/plain.o" >/dev/null 2>&1 \
   && "$tmp/acc-nolib" "$tmp/plain.o" -o "$tmp/plain.bin" >/dev/null 2>&1; then
    ok "a missing default library is passed over"
else
    bad "a missing default library is passed over" "the link failed"
fi

echo "  $pass passed, $fail failed"
[ $fail -eq 0 ]

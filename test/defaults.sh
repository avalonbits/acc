#!/bin/bash
# What acc does without being told: -v, and the include directory and library
# a build can name (the Agon build names /lib/acc/include and
# /lib/acc/libc.a, where the release puts them).
#
# The host build names bin/libc.a, with acc's runtime beside it, and the
# repository's include/. So does the acc this builds, and it checks that
# it finds them, that -I is still looked in first, and that naming the
# library as well changes nothing. test/release.sh checks the Agon build's
# own paths on the Agon.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok()  { printf '  ok   %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf '  FAIL %-40s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# The compiler's sources, as the Makefile lists them.
SRC=$(sed -n 's/^SRC *= //p; /^           src/p' Makefile | tr -d '\\')
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -Isrc -o "$tmp/acc" $SRC \
    -DACC_INCLUDE_DIR="\"$PWD/include\"" -DACC_LIBC="\"$PWD/bin/libc.a\"" \
    || exit 2
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -Isrc -o "$tmp/acc-nolib" $SRC \
    -DACC_INCLUDE_DIR="\"$PWD/include\"" -DACC_LIBC="\"$tmp/none/libc.a\"" \
    || exit 2

echo 'this is not a library' > "$tmp/junk.a"
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -Isrc -o "$tmp/acc-junk" $SRC \
    -DACC_INCLUDE_DIR="\"$PWD/include\"" -DACC_LIBC="\"$tmp/junk.a\"" \
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

# The host build's default is the repository's include/, as the Agon's is
# /lib/acc/include: <stdio.h> is found without -I.
if bin/acc -c "$tmp/hello.c" -o "$tmp/hostdefault.o" >/dev/null 2>&1; then
    ok "the host build finds its headers without -I"
else
    bad "the host build finds its headers without -I" "<stdio.h> was not found"
fi

# The reference: everything named, in the order a link reads them by
# default -- the runtime, then the C library. An image holds its own name,
# so every link below writes p.bin and the answer is moved aside.
bin/acc -c "$tmp/hello.c" -o "$tmp/ref.o" -Iinclude >/dev/null || exit 2
bin/acc "$tmp/ref.o" bin/rt.a bin/libc.a -o "$tmp/p.bin" >/dev/null || exit 2
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
if "$tmp/acc" "$tmp/hello.o" bin/rt.a bin/libc.a -o "$tmp/p.bin" >/dev/null 2>&1 \
   && cmp -s "$tmp/p.bin" "$tmp/ref.bin"; then
    ok "naming the library as well changes nothing"
else
    bad "naming the library as well changes nothing" "no image, or a different one"
fi

# One step, from the source: headers and library both by default.
mkdir -p "$tmp/one"
cp "$tmp/hello.c" "$tmp/one/"
if (cd "$tmp/one" && "$tmp/acc" hello.c -o p.bin >/dev/null 2>&1) \
   && cmp -s "$tmp/one/p.bin" "$tmp/ref.bin"; then
    ok "one step from a source takes the defaults"
else
    bad "one step from a source takes the defaults" "no image, or a different one"
fi

# The default library is read for a program that calls nothing else: the
# runtime is in it, and a multiply of two ints is a call into it. (A
# prologue is written out, and calls nothing.)
printf 'int main(int argc, char **argv) { return argc * argc; }\n' > "$tmp/plain.c"
if "$tmp/acc-junk" "$tmp/plain.c" -o "$tmp/pj.bin" >/dev/null 2>&1; then
    bad "a program that calls nothing else reads it" "junk.a was passed over"
else
    ok "a program that calls nothing else reads it"
fi
out=$("$tmp/acc-nolib" "$tmp/plain.c" -o "$tmp/pn.bin" 2>&1)
if printf '%s' "$out" | grep -q "acc's runtime and is in libc.a"; then
    ok "without it, the runtime is said to be missing"
else
    bad "without it, the runtime is said to be missing" "some other error"
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

# The default directory is not an option: an object made with it is the same
# file as one made without it, so objects carry between the Agon and a PC.
printf 'int main(int argc, char **argv) { return argc * argc; }\n' > "$tmp/plain.c"
bin/acc -c "$tmp/plain.c" -o "$tmp/plain-ref.o" >/dev/null || exit 2
if "$tmp/acc" -c "$tmp/plain.c" -o "$tmp/plain.o" >/dev/null 2>&1 \
   && cmp -s "$tmp/plain.o" "$tmp/plain-ref.o"; then
    ok "the default directory leaves an object alone"
else
    bad "the default directory leaves an object alone" "the objects differ"
fi
rm -f "$tmp/plain.o"

# A header from the default directory is recorded by its name there, not by
# the path the directory has -- /lib/acc/include on the Agon, include/ here
# -- so an object that includes <stdio.h> is the same file wherever it was
# made, and one made in one place is up to date in the other until the
# header there changes.
mkdir -p "$tmp/moved"
cp -r include "$tmp/moved/include"
# shellcheck disable=SC2086
"$CC" -O1 -fsigned-char -Isrc -o "$tmp/acc-moved" $SRC \
    -DACC_INCLUDE_DIR="\"$tmp/moved/include\"" -DACC_LIBC="\"$PWD/bin/libc.a\"" \
    || exit 2
bin/acc -c "$tmp/hello.c" -o "$tmp/here.o" >/dev/null || exit 2
"$tmp/acc-moved" -c "$tmp/hello.c" -o "$tmp/there.o" >/dev/null 2>&1
if cmp -s "$tmp/here.o" "$tmp/there.o"; then
    ok "an object with <stdio.h> in it is the same made from either place"
else
    bad "an object with <stdio.h> in it is the same made from either place" \
        "the objects differ"
fi
if "$tmp/acc-moved" -c "$tmp/hello.c" -o "$tmp/here.o" 2>&1 | grep -q "up to date"; then
    ok "and one made here is up to date there"
else
    bad "and one made here is up to date there" "it was compiled again"
fi
echo '/* changed */' >> "$tmp/moved/include/stdio.h"
if "$tmp/acc-moved" -c "$tmp/hello.c" -o "$tmp/here.o" 2>&1 | grep -q "up to date"; then
    bad "until the header there changes" "it was still up to date"
else
    ok "until the header there changes"
fi

# With the library missing, a link of an object says the runtime is.
"$tmp/acc-nolib" -c "$tmp/plain.c" -o "$tmp/plain.o" >/dev/null 2>&1
out=$("$tmp/acc-nolib" "$tmp/plain.o" -o "$tmp/plain.bin" 2>&1)
if printf '%s' "$out" | grep -q "acc's runtime and is in libc.a"; then
    ok "a link without the library says the runtime is missing"
else
    bad "a link without the library says the runtime is missing" "some other error"
fi

echo "  $pass passed, $fail failed"
[ $fail -eq 0 ]

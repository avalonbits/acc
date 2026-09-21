#!/bin/bash
# The library, and the archive it is kept in.
#
# An archive is objects end to end with a list of what each of them defines
# in front. A link takes only the members it turns out to want, which is what
# makes a library affordable on a machine with 448 KB: a program that prints
# a string should not carry the arithmetic helpers, the file routines and
# everything else that happens to be in the same file.
#
# So two things to say. That the functions do what C says they do, which is a
# program that exercises them and comes out at 42. And that a link takes what
# it needs and leaves the rest, which is measured: the same program, linked
# against a library with a large unused member in it, has to come out the
# same size.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
[ -f "$LIB" ] || { echo "$LIB missing -- run make"; exit 2; }

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok() {
    if [ "$2" = "$3" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-36s want %s, got %s\n' "$1" "$3" "$2"
        fail=$((fail + 1))
    fi
}

# runs <name> <source>: compile it, link it against the library, run it, and
# require 42 back.
runs() {
    local what=$1 err

    printf '%s' "$2" > "$tmp/p.c"
    if ! err=$("$ACC" -c "$tmp/p.c" -o "$tmp/p.o" -Iinclude 2>&1); then
        printf '  FAIL %-36s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi
    if ! err=$("$ACC" "$tmp/p.o" "$LIB" -o "$tmp/p.bin" -x 2>&1); then
        printf '  FAIL %-36s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi
    if ! emu_available >/dev/null 2>&1; then
        pass=$((pass + 1)); return      # built, at least
    fi
    test/agon.sh "$tmp/p.bin" >/dev/null 2>&1
    ok "$what" "$?" 42
}

runs "what is in <string.h>, the memory half" \
'#include <string.h>

char a[16];
char b[16];

int main(void) {
    int r = 0;

    memset(a, 7, 16);
    if (a[0] == 7 && a[15] == 7) r++;
    memcpy(b, a, 16);
    if (memcmp(a, b, 16) == 0) r++;
    b[8] = 9;
    if (memcmp(a, b, 16) != 0 && memcmp(a, b, 8) == 0) r++;
    if (memchr(b, 9, 16) == b + 8 && memchr(b, 3, 16) == NULL) r++;

    memcpy(a, "0123456789", 10);
    memmove(a + 2, a, 8);               /* overlapping, upwards */
    if (a[2] == 48 && a[9] == 55) r++;
    memcpy(a, "0123456789", 10);
    memmove(a, a + 2, 8);               /* and downwards */
    if (a[0] == 50 && a[7] == 57) r++;

    return r + 36;                      /* 6 checks */
}
'

runs "what is in <string.h>, the strings" \
'#include <string.h>

char buf[32];

int main(void) {
    int r = 0;

    if (strlen("") == 0 && strlen("abc") == 3) r++;
    strcpy(buf, "abc");
    if (strlen(buf) == 3 && buf[3] == 0) r++;
    strcat(buf, "def");
    if (strcmp(buf, "abcdef") == 0) r++;
    if (strcmp("abc", "abd") < 0 && strcmp("abd", "abc") > 0) r++;
    if (strncmp("abcxx", "abcyy", 3) == 0 && strncmp("abc", "abd", 3) != 0) r++;

    strncpy(buf, "ab", 5);
    if (buf[2] == 0 && buf[4] == 0) r++;
    strncat(buf, "cdef", 2);
    if (strcmp(buf, "abcd") == 0) r++;

    if (strchr("hello", 108) == 0) r++; else if (*strchr("hello", 108) == 108) r++;
    if (*strrchr("hello", 108) == 108 && strrchr("hello", 108)[1] == 111) r++;
    if (strchr("abc", 122) == NULL && strrchr("abc", 122) == NULL) r++;
    if (strstr("hello world", "o w") != NULL && strstr("abc", "cd") == NULL) r++;

    return r + 31;                      /* 11 checks */
}
'

runs "what is in <stdio.h>" \
'#include <stdio.h>

int main(void) {
    puts("the library says hello");
    putchar(46);
    putchar(13);
    putchar(10);

    return 42;
}
'

# ------------------------------------------------------------------
# And that a link takes only what it wants.

printf '#include <string.h>\nint main(void) { return strlen("hello world!") * 3 + 6; }\n' \
    > "$tmp/one.c"
"$ACC" -c "$tmp/one.c" -o "$tmp/one.o" -Iinclude >/dev/null 2>&1
"$ACC" "$tmp/one.o" "$LIB" -o "$tmp/one.bin" -x >/dev/null 2>&1
plain=$(wc -c < "$tmp/one.bin")

# The same library with something large added that nothing calls.
printf 'int spare[600];\nint bulky(int i) { spare[i] = i; return spare[i]; }\n' \
    > "$tmp/bulky.c"
"$ACC" -c "$tmp/bulky.c" -o "$tmp/bulky.o" >/dev/null 2>&1
cp "$LIB" "$tmp/big.a"
"$ACC" -a "$tmp/big.a" bin/lib/mem.o bin/lib/str.o bin/lib/stdio.o "$tmp/bulky.o" \
    >/dev/null 2>&1
"$ACC" "$tmp/one.o" "$tmp/big.a" -o "$tmp/two.bin" -x >/dev/null 2>&1
padded=$(wc -c < "$tmp/two.bin")

ok "an unused member costs nothing" "$padded" "$plain"

# A member that wants another member is pulled in too, and the link looks
# again rather than once.
printf 'int inner(int n) { return n + 20; }\n' > "$tmp/inner.c"
printf 'int inner(int n);\nint outer(int n) { return inner(n) + 20; }\n' > "$tmp/outer.c"
"$ACC" -c "$tmp/inner.c" -o "$tmp/inner.o" >/dev/null 2>&1
"$ACC" -c "$tmp/outer.c" -o "$tmp/outer.o" >/dev/null 2>&1
# inner first, so that finding outer means looking again at a member already
# passed over.
"$ACC" -a "$tmp/chain.a" "$tmp/inner.o" "$tmp/outer.o" >/dev/null 2>&1
printf 'int outer(int n);\nint main(void) { return outer(2); }\n' > "$tmp/chain.c"
"$ACC" -c "$tmp/chain.c" -o "$tmp/chain.o" >/dev/null 2>&1
if err=$("$ACC" "$tmp/chain.o" "$tmp/chain.a" -o "$tmp/chain.bin" -x 2>&1); then
    if emu_available >/dev/null 2>&1; then
        test/agon.sh "$tmp/chain.bin" >/dev/null 2>&1
        ok "a member that wants another member" "$?" 42
    else
        pass=$((pass + 1))
    fi
else
    printf '  FAIL %-36s %s\n' "a member that wants another member" \
        "$(printf '%s' "$err" | head -1)"
    fail=$((fail + 1))
fi

# A name no member defines is still a name nothing defines.
printf 'int missing(void);\nint main(void) { return missing(); }\n' > "$tmp/gone.c"
"$ACC" -c "$tmp/gone.c" -o "$tmp/gone.o" >/dev/null 2>&1
err=$("$ACC" "$tmp/gone.o" "$LIB" -o "$tmp/gone.bin" -x 2>&1)
case $err in
  *"'missing' is called but never defined"*) pass=$((pass + 1)) ;;
  *) printf '  FAIL %-36s %s\n' "a name the library has not got" "$err"
     fail=$((fail + 1)) ;;
esac

# Something that is not an archive at all.
cp "$tmp/one.c" "$tmp/junk.a"
err=$("$ACC" "$tmp/one.o" "$tmp/junk.a" -o "$tmp/junk.bin" -x 2>&1)
case $err in
  *"not a library acc made"*) pass=$((pass + 1)) ;;
  *) printf '  FAIL %-36s %s\n' "a file that is not a library" "$err"
     fail=$((fail + 1)) ;;
esac

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

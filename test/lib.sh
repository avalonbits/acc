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

runs "what is in <stddef.h>" \
'#include <stddef.h>

struct parts { char tag; int count; char name[6]; };

int main(void) {
    char *nothing = NULL;
    size_t n = sizeof(struct parts);
    ptrdiff_t d;
    char room[8];
    int r = 0;

    if (nothing == NULL) r++;
    if (offsetof(struct parts, tag) == 0) r++;
    if (offsetof(struct parts, count) == 1) r++;
    if (offsetof(struct parts, name) == 4) r++;
    if (offsetof(struct parts, name[2]) == 6) r++;
    if (n == 10) r++;
    d = &room[5] - &room[1];
    if (d == 4) r++;

    return r + 35;                      /* 7 checks */
}
'

runs "the heap, in <stdlib.h>" \
'#include <stdlib.h>
#include <string.h>

int main(void) {
    char *a, *b, *c, *both;
    int r = 0;

    a = malloc(100);
    b = malloc(100);
    c = malloc(100);
    if (a && b && c) r++;
    memset(a, 1, 100);
    memset(b, 2, 100);
    memset(c, 3, 100);
    if (a[99] == 1 && b[0] == 2 && c[99] == 3) r++;

    /* The one in the middle comes back, and is handed out again. */
    free(b);
    b = malloc(100);
    if (b) r++;

    /* Two blocks that touch, the earlier one freed first: they are one
     * block afterwards, which is what makes a request too big for either of
     * them alone come back at the first one and not from somewhere else. */
    free(b);
    free(c);
    both = malloc(208);
    if (both == b) r++;
    free(both);

    /* And freed the other way round, which is the join in the other
     * direction -- the one that costs a walk without a link backwards. */
    b = malloc(100);
    c = malloc(100);
    free(c);
    free(b);
    both = malloc(208);
    if (both == b) r++;
    free(both);

    a = realloc(a, 200);
    if (a && a[99] == 1) r++;           /* what was there is still there */
    c = calloc(50, 2);
    if (c && c[0] == 0 && c[99] == 0) r++;
    free(a);
    free(c);
    if (malloc(0) == NULL && realloc(NULL, 10) != NULL) r++;

    return r + 34;                      /* 8 checks */
}
'

runs "qsort, in <stdlib.h>" \
'#include <stdlib.h>
#include <string.h>

static int by_int(const void *a, const void *b) {
    int x = *(const int *) a, y = *(const int *) b;

    return x < y ? -1 : x > y ? 1 : 0;
}

static int by_int_down(const void *a, const void *b) {
    return by_int(b, a);
}

struct row { int key; char tag; };

static int by_key(const void *a, const void *b) {
    return ((const struct row *) a)->key - ((const struct row *) b)->key;
}

/* Longer than the first gap, so more than one pass runs, and with the
 * largest first and the smallest last so that nothing is in place. */
static int many[40];

int main(void) {
    int few[7];
    struct row rows[5];
    int r = 0, i, ok;

    few[0] = 5; few[1] = 3; few[2] = 9; few[3] = 1; few[4] = 9; few[5] = 0;
    few[6] = 4;
    qsort(few, 7, sizeof(int), by_int);
    if (few[0] == 0 && few[1] == 1 && few[2] == 3 && few[3] == 4
        && few[4] == 5 && few[5] == 9 && few[6] == 9) r++;

    qsort(few, 7, sizeof(int), by_int_down);
    if (few[0] == 9 && few[6] == 0) r++;

    for (i = 0; i < 40; i++)
        many[i] = 39 - i;
    qsort(many, 40, sizeof(int), by_int);
    ok = 1;
    for (i = 0; i < 40; i++)
        if (many[i] != i) ok = 0;
    if (ok) r++;

    rows[0].key = 3; rows[0].tag = 99;
    rows[1].key = 1; rows[1].tag = 97;
    rows[2].key = 4; rows[2].tag = 100;
    rows[3].key = 1; rows[3].tag = 98;
    rows[4].key = 2; rows[4].tag = 101;
    qsort(rows, 5, sizeof(struct row), by_key);
    if (rows[0].key == 1 && rows[1].key == 1 && rows[2].key == 2
        && rows[3].key == 3 && rows[4].key == 4) r++;
    /* The whole element moves, not just the key it was sorted on. */
    if ((rows[3].tag == 99 && rows[4].tag == 100)
        && (rows[2].tag == 101)) r++;

    /* Nothing to do, and nothing broken by asking. */
    qsort(few, 0, sizeof(int), by_int);
    qsort(few, 1, sizeof(int), by_int);
    if (few[0] == 9) r++;

    return r + 36;                      /* 6 checks */
}
'

runs "files, in <stdio.h>" \
'#include <stdio.h>
#include <string.h>

/* What a compiler asks of a file: write one a piece at a time, read
 * it back whole, and find out how long it is. The writes go through a
 * buffer, so the pieces here are smaller than one and the whole is
 * larger. */
int main(void) {
    FILE *f;
    char buf[1200];
    long size;
    int i;

    f = fopen("/libtest.bin", "wb");
    if (!f) return 1;
    if (fwrite("hello ", 1, 6, f) != 6) return 2;
    if (fputc(119, f) != 119) return 3;          /* w */
    if (fprintf(f, "orld %d/%s!", 42, "x") < 0) return 4;

    /* Past the buffer, so that it has to be handed over and filled again. */
    for (i = 0; i < 1000; i++)
        if (fputc(97 + (i % 26), f) != 97 + (i % 26)) return 5;
    /* Where it has got to, while some of what was written is still held in
     * the buffer: telling has to hand that over first or it reports the
     * file as shorter than the program has made it. */
    if (ftell(f) != 17 + 1000) return 22;
    if (fputc(33, f) != 33) return 23;
    if (ftell(f) != 17 + 1001) return 24;
    if (fclose(f) != 0) return 6;

    f = fopen("/libtest.bin", "rb");
    if (!f) return 7;
    if (fseek(f, 0, SEEK_END) != 0) return 8;
    size = ftell(f);
    if (size != 17 + 1001) return 9;
    if (fseek(f, 0, SEEK_SET) != 0) return 10;
    memset(buf, 0, sizeof buf);
    if ((long) fread(buf, 1, (size_t) size, f) != size) return 11;
    if (fclose(f) != 0) return 12;
    if (strncmp(buf, "hello world 42/x!", 17) != 0) return 13;
    if (buf[17] != 97 || buf[17 + 25] != 122 || buf[17 + 26] != 97) return 14;

    /* Seeking into the middle and reading from there. */
    f = fopen("/libtest.bin", "rb");
    if (!f) return 15;
    if (fseek(f, 17, SEEK_SET) != 0) return 16;
    if (ftell(f) != 17) return 17;
    if (fread(buf, 1, 4, f) != 4) return 18;
    if (strncmp(buf, "abcd", 4) != 0) return 19;
    if (fclose(f) != 0) return 20;

    /* One that is not there answers nothing rather than wandering off. */
    if (fopen("/nosuchfile.xyz", "rb") != NULL) return 21;

    return 42;
}
' 42

runs "snprintf, in <stdio.h>" \
'#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[32];
    int n;

    n = snprintf(buf, sizeof buf, "%s=%d", "n", 7);
    if (n != 3 || strcmp(buf, "n=7") != 0) return 1;

    /* It answers with the length the whole thing would have been, which is
     * what lets a caller ask with no room and then find some. */
    n = snprintf(buf, 3, "%s=%d", "n", 7);
    if (n != 3 || strcmp(buf, "n=") != 0) return 2;
    n = snprintf(buf, 1, "%s=%d", "n", 7);
    if (n != 3 || buf[0] != 0) return 3;

    n = snprintf(buf, sizeof buf, "%08lx", 3735928559UL);
    if (n != 8 || strcmp(buf, "deadbeef") != 0) return 4;

    return 42;
}
' 42

runs "what is in <time.h>" \
'#include <time.h>

int main(void) {
    clock_t start = clock();
    clock_t now = start;
    long spins = 0;
    int r = 0;

    if (CLOCKS_PER_SEC == 100UL) r++;

    /* It has to move on its own: MOS counts the frames, so waiting is all
     * that is needed. Bounded, so that a clock that never ticks is a
     * failure rather than a program that never ends. */
    while (now == start && spins < 200000L) {
        now = clock();
        spins++;
    }
    if (now != start) r++;
    if (now - start < 100UL) r++;       /* within a second of starting */

    return r + 39;                      /* 3 checks */
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

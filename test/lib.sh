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
    printf '%s' "$2" > "$tmp/p.c"
    runs_file "$1" "$tmp/p.c"
}

# The same, for a program already in a file. A program with a character
# constant in it cannot be written inside the single quotes above -- there is
# no way to put a single quote in a single-quoted shell string -- so those
# are written with a heredoc and handed over by name.
runs_file() {
    local what=$1 err

    [ "$2" = "$tmp/p.c" ] || cp "$2" "$tmp/p.c"
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

# abort, which is the one function here that does not come back. It is a
# member of its own: the rest of <stdlib.h> is the heap, and a program that
# gives up should not be given an allocator for it. So two things are asked
# -- that it stops the program with the status it says it does, from
# wherever it is called, and that calling it costs nothing but itself.
printf '#include <stdlib.h>\nstatic int deep(int n) { if (!n) abort(); return deep(n - 1); }\nint main(void) { deep(4); return 7; }\n' > "$tmp/ab.c"
if "$ACC" -c "$tmp/ab.c" -o "$tmp/ab.o" -Iinclude >/dev/null 2>&1 \
   && "$ACC" "$tmp/ab.o" "$LIB" -o "$tmp/ab.bin" -x >/dev/null 2>&1; then
    if emu_available >/dev/null 2>&1; then
        test/agon.sh "$tmp/ab.bin" >/dev/null 2>&1
        ok "abort, from five frames down" "$?" 134
    else
        pass=$((pass + 1))
    fi
else
    printf '  FAIL %-36s %s\n' "abort, from five frames down" "it did not build"
    fail=$((fail + 1))
fi

# The same program with the abort taken out: what abort costs is abort, and
# not the heap that shares a header with it.
printf 'static int deep(int n) { if (!n) return 0; return deep(n - 1); }\nint main(void) { deep(4); return 7; }\n' > "$tmp/nab.c"
"$ACC" -c "$tmp/nab.c" -o "$tmp/nab.o" >/dev/null 2>&1
"$ACC" "$tmp/nab.o" "$LIB" -o "$tmp/nab.bin" -x >/dev/null 2>&1
with=$(wc -c < "$tmp/ab.bin"); without=$(wc -c < "$tmp/nab.bin")
if [ "$((with - without))" -lt 64 ]; then
    pass=$((pass + 1))
else
    printf '  FAIL %-36s abort costs %d bytes, so it brought the heap\n' \
        "abort brings nothing with it" "$((with - without))"
    fail=$((fail + 1))
fi

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

# exit, which the compiler emits rather than the library defining: it puts
# the stack back where main was called on and carries on from just after the
# call, so the status arrives at the stub's tail as though main had returned
# it. Here rather than with the other cases because it is the link that this
# is about -- the two cells it reads through are a name, and a file compiled
# on its own has not seen the stub that gives them room. Addressed as an
# offset into the bss instead, the object read two addresses out of whatever
# the link happened to put at that offset and jumped to one of them.
runs "exit, in <stdlib.h>" \
'#include <stdlib.h>
#include <string.h>

static void deeper(int n, const char *text) {
    if (n == 0)
        exit(atoi(text));

    deeper(n - 1, text);
}

int main(void) {
    char *text = malloc(8);

    strcpy(text, "42");
    deeper(5, text);

    return 9;
}
'

cat > "$tmp/ctype.c" <<'CTYPE'
#include <ctype.h>

int main(void) {
    int r = 0, c, letters = 0, digits = 0, spaces = 0, printing = 0;

    if (isalpha('a') && isalpha('Z') && !isalpha('0') && !isalpha(' ')) r++;
    if (isdigit('7') && !isdigit('a') && isxdigit('f') && isxdigit('F')) r++;
    if (isspace(' ') && isspace('\n') && isspace('\t') && !isspace('x')) r++;
    if (ispunct('!') && ispunct('~') && !ispunct('a') && !ispunct(' ')) r++;
    if (iscntrl(0) && iscntrl(127) && !iscntrl(' ')) r++;
    if (isprint(' ') && !isprint('\n') && isgraph('!') && !isgraph(' ')) r++;
    if (isblank(' ') && isblank('\t') && !isblank('\n')) r++;
    if (isalnum('a') && isalnum('9') && !isalnum('-')) r++;
    if (islower('q') && !islower('Q') && isupper('Q') && !isupper('q')) r++;

    /* Changing case, and leaving alone what has none. */
    if (tolower('A') == 'a' && toupper('z') == 'Z') r++;
    if (tolower('5') == '5' && toupper('5') == '5' && tolower('a') == 'a') r++;

    /* EOF is none of them, which is what the int in the signature is for. */
    if (!isalpha(-1) && !isdigit(-1) && !isprint(-1) && !iscntrl(-1)) r++;

    /* How many of the 256 bytes each says yes to, which pins the ranges
     * rather than the handful of characters above. Nothing past 127 is any
     * of these things in the only locale there is. */
    for (c = 0; c < 256; c++) {
        letters += isalpha(c) != 0;
        digits += isdigit(c) != 0;
        spaces += isspace(c) != 0;
        printing += isprint(c) != 0;
    }
    if (letters == 52 && digits == 10) r++;
    if (spaces == 6 && printing == 95) r++;

    /* Each is a function, so each has an address. */
    {
        int (*f)(int) = isdigit;

        if (f('3') && !f('x')) r++;
    }

    return r + 27;                      /* 15 checks */
}
CTYPE
runs_file "what is in <ctype.h>" "$tmp/ctype.c"

cat > "$tmp/assert.c" <<'ASSERT'
#include <assert.h>

static int side_effects;

static int bump(void) { return ++side_effects; }

int main(void) {
    assert(1);
    assert(bump() == 1);                /* evaluated, and only once */
    assert(side_effects == 1);

    return 42;
}
ASSERT
runs_file "assert that holds, in <assert.h>" "$tmp/assert.c"

# NDEBUG turns the macro into nothing at all, which C99 asks for and which is
# why <assert.h> has no include guard round the macro: the header is included
# again here with NDEBUG on and has to give the other definition.
cat > "$tmp/ndebug.c" <<'NDEBUG'
#include <assert.h>
#define NDEBUG
#include <assert.h>

static int side_effects;

static int bump(void) { return ++side_effects; }

int main(void) {
    assert(bump() == 99);               /* false, and not evaluated either */
    if (side_effects != 0)
        return 1;

    return 42;
}
NDEBUG
runs_file "assert with NDEBUG, in <assert.h>" "$tmp/ndebug.c"

# One that fails, which cannot go through runs_file: it stops the program
# with the status abort gives and not with 42.
cat > "$tmp/af.c" <<'AF'
#include <assert.h>

int main(void) {
    int n = 1;

    assert(n == 2);

    return 42;
}
AF
if ! err=$("$ACC" -c "$tmp/af.c" -o "$tmp/af.o" -Iinclude 2>&1) \
   || ! err=$("$ACC" "$tmp/af.o" "$LIB" -o "$tmp/af.bin" -x 2>&1); then
    printf '  FAIL %-36s %s\n' "an assert that fails" \
        "$(printf '%s' "$err" | head -1)"
    fail=$((fail + 1))
elif ! emu_available >/dev/null 2>&1; then
    pass=$((pass + 1))
else
    test/agon.sh "$tmp/af.bin" >/dev/null 2>&1
    ok "an assert that fails stops the program" "$?" 134
fi

# The exact half of <math.h>: nothing here is an approximation, so every
# check is an equality on the bits and not a tolerance. Cross-checked once
# against agondev's own library, which agrees with all of it but one --
# libagon's modf(-4.0) gives +0.0 where C99 asks for -0.0.
cat > "$tmp/math1.c" <<'MATH1'
#include <math.h>

static unsigned long ub(double x) { union { float f; unsigned long u; } v; v.f = (float) x; return v.u; }
static int same(double a, double b) { return ub(a) == ub(b); }

int main(void) {
    int r = 0;
    double ip;

    /* trunc, floor, ceil, round: every combination of sign and fraction. */
    if (same(trunc(2.7f), 2.0f) && same(trunc(-2.7f), -2.0f)
        && same(trunc(0.5f), 0.0f) && same(trunc(-0.5f), -0.0f)) r++;
    if (same(floor(2.7f), 2.0f) && same(floor(-2.7f), -3.0f)
        && same(floor(-0.5f), -1.0f) && same(floor(3.0f), 3.0f)) r++;
    if (same(ceil(2.1f), 3.0f) && same(ceil(-2.1f), -2.0f)
        && same(ceil(0.5f), 1.0f) && same(ceil(-3.0f), -3.0f)) r++;
    if (same(round(2.5f), 3.0f) && same(round(-2.5f), -3.0f)
        && same(round(2.4f), 2.0f) && same(round(-0.5f), -1.0f)) r++;

    /* rint breaks a tie to even, where round breaks it away from zero. */
    if (same(rint(2.5f), 2.0f) && same(rint(3.5f), 4.0f)
        && same(rint(-2.5f), -2.0f) && same(rint(2.4f), 2.0f)) r++;

    /* Large values are whole already and come back untouched. */
    if (same(trunc(1e20f), 1e20f) && same(floor(1e20f), 1e20f)
        && same(round(1e20f), 1e20f) && same(rint(1e20f), 1e20f)) r++;

    /* fabs and copysign, including the sign of zero. */
    if (same(fabs(-3.5f), 3.5f) && same(fabs(3.5f), 3.5f)
        && ub(fabs(-0.0f)) == ub(0.0f)) r++;
    if (same(copysign(3.5f, -1.0f), -3.5f) && same(copysign(-3.5f, 1.0f), 3.5f)
        && ub(copysign(1.0f, -0.0f)) == ub(-1.0f)) r++;

    /* frexp and ldexp, which have to undo each other. */
    {
        int e = 99;
        double m = frexp(24.0f, &e);

        if (same(m, 0.75f) && e == 5 && same(ldexp(m, e), 24.0f)) r++;
        m = frexp(-0.125f, &e);
        if (same(m, -0.5f) && e == -2) r++;
        m = frexp(0.0f, &e);
        if (same(m, 0.0f) && e == 0) r++;
    }
    if (same(ldexp(1.0f, 10), 1024.0f) && same(ldexp(3.0f, -2), 0.75f)) r++;
    if (same(scalbn(1.5f, 100), 1.5f * 1048576.0f * 1048576.0f * 1048576.0f
                                    * 1048576.0f * 1048576.0f)) r++;

    /* modf, whose fraction keeps the sign even when it is zero. */
    if (same(modf(3.75f, &ip), 0.75f) && same(ip, 3.0f)) r++;
    if (same(modf(-3.75f, &ip), -0.75f) && same(ip, -3.0f)) r++;
    if (ub(modf(-4.0f, &ip)) == ub(-0.0f) && same(ip, -4.0f)) r++;

    /* fmod: exact, and with the sign of the left. */
    if (same(fmod(7.0f, 3.0f), 1.0f) && same(fmod(-7.0f, 3.0f), -1.0f)
        && same(fmod(7.0f, -3.0f), 1.0f)) r++;
    if (same(fmod(5.5f, 2.0f), 1.5f) && same(fmod(1.0f, 4.0f), 1.0f)
        && same(fmod(9.0f, 3.0f), 0.0f)) r++;
    if (same(fmod(1e10f, 3.0f), 1.0f)) r++;     /* 10^10 is exact, and 1 mod 3 */

    /* fmax, fmin, fdim. */
    if (same(fmax(1.0f, 2.0f), 2.0f) && same(fmin(1.0f, 2.0f), 1.0f)
        && same(fdim(5.0f, 3.0f), 2.0f) && same(fdim(3.0f, 5.0f), 0.0f)) r++;

    /* What a number is. */
    if (fpclassify(1.0f) == FP_NORMAL && fpclassify(0.0f) == FP_ZERO
        && fpclassify(-0.0f) == FP_ZERO) r++;
    if (signbit(-1.0f) && !signbit(1.0f) && signbit(-0.0f)) r++;
    if (isfinite(1.0f) && !isnan(1.0f) && !isinf(1.0f) && isnormal(1.0f)) r++;

    return r + 19;              /* 23 checks */
}
MATH1
runs_file "the exact half of <math.h>" "$tmp/math1.c"

# The approximations in <math.h>, against values worked out to more digits
# than a float holds and rounded to one. The tolerances are in ulp and come
# from what these were measured to be worth -- one or two for most of them,
# more where the function itself is losing digits. test/mathvalues.py is what
# generated the tables. Run against agondev's own library instead, the same
# test fails seventy-two of these comparisons.
runs_file "the approximations in <math.h>" test/mathvalues.c

# <float.h>: each value checked against what the arithmetic does, not just
# read back. The epsilon is the gap above 1, and half of it is lost; the
# mantissa's width is the first integer a float cannot hold; FLT_ROUNDS is
# to nearest with ties to even, which a sum exactly between two floats
# shows; and the largest and smallest are the exact bit patterns.
#
# Built by agondev against its own <float.h>, the same program fails two
# checks, and they are the two ties: 1 + 2^-24 and 1 + 3 * 2^-24 do not
# come out at the even neighbour. agondev's header says FLT_ROUNDS is 1
# all the same. acc's arithmetic does round a tie to even, so here the 1
# is true -- which is also why this is a library check and not a case run
# against agondev.
cat > "$tmp/float.c" <<'FLOATH'
#include <float.h>

volatile float one = 1.0f, eps = FLT_EPSILON, big = FLT_MAX;
volatile long wide = 16777216L;

int main(void) {
    int r = 0;
    float half = eps / 2;

    if (FLT_RADIX == 2 && FLT_MANT_DIG == 24 && DBL_MANT_DIG == 24) r++;
    if (FLT_DIG == 6 && DBL_DIG == 6 && DECIMAL_DIG == 17) r++;
    if (FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128 && DBL_MIN_EXP == -125
        && DBL_MAX_EXP == 128) r++;
    if (FLT_MIN_10_EXP == -37 && FLT_MAX_10_EXP == 38
        && DBL_MIN_10_EXP == -37 && DBL_MAX_10_EXP == 38) r++;
    if (FLT_EVAL_METHOD == 0 && FLT_ROUNDS == 1) r++;
    if (LDBL_MANT_DIG == 53 && LDBL_DIG == 15 && LDBL_MIN_EXP == -1021
        && LDBL_MAX_EXP == 1024 && LDBL_MIN_10_EXP == -307
        && LDBL_MAX_10_EXP == 308) r++;

    /* The values, bit for bit, and double's are float's. */
    if (FLT_MAX == 0x1.fffffep127f && FLT_MIN == 0x1p-126f
        && FLT_EPSILON == 0x1p-23f) r++;
    if (DBL_MAX == FLT_MAX && DBL_MIN == FLT_MIN && DBL_EPSILON == FLT_EPSILON
        && sizeof (double) == sizeof (float)) r++;

    /* What they say about the arithmetic. */
    if (one + eps != one && one + half == one) r++;
    if ((float) (wide + 1) == (float) wide) r++;        /* 2^24 + 1 */
    if (one + 3 * half == one + 2 * eps) r++;           /* a tie, to even */
    if (big * 2 > big) r++;                             /* past it is inf */

    return r + 30;              /* 12 checks */
}
FLOATH
runs_file "what is in <float.h>" "$tmp/float.c"

# <iso646.h>: each word is its operator, and each check is one its
# lookalike would get wrong -- 2 bitand 1 is 0 where 2 and 1 is 1, and
# and and or have to stop at their left operand, which & and | do not.
runs "what is in <iso646.h>" \
'#include <iso646.h>

static int calls;
static int touch(int v) { calls++; return v; }

int main(void) {
    int r = 0, x = 12;

    if ((2 and 1) == 1 and (2 bitand 1) == 0) r++;
    if ((2 or 0) == 1 and (2 bitor 1) == 3) r++;
    if ((6 xor 3) == 5 and (compl 0) == -1 and (not 5) == 0) r++;
    if (3 not_eq 4 and not (3 not_eq 3)) r++;
    x and_eq 10;                        /* 8 */
    x or_eq 1;                          /* 9 */
    x xor_eq 3;                         /* 10 */
    if (x == 10) r++;
    if (not (0 and touch(1)) and (1 or touch(1)) and calls == 0) r++;

    return r + 36;              /* 6 checks */
}'

# main running off its closing brace, which C99 5.1.2.2.3 says is a return
# of 0. It returned whatever was last in HL: here that is the 126 the body
# leaves behind, which reads as a failure from a program that did nothing
# wrong. Seven of gcc's torture tests end this way.
cat > "$tmp/falloff.c" <<'FALLOFF'
int value = 42;

int main(void) {
    int x = value;

    x = x * 3;
    value = x;
}
FALLOFF
if ! err=$("$ACC" -c "$tmp/falloff.c" -o "$tmp/falloff.o" -Iinclude 2>&1) \
   || ! err=$("$ACC" "$tmp/falloff.o" "$LIB" -o "$tmp/falloff.bin" -x 2>&1); then
    printf '  FAIL %-36s %s\n' "main running off its end" \
        "$(printf '%s' "$err" | head -1)"
    fail=$((fail + 1))
elif ! emu_available >/dev/null 2>&1; then
    pass=$((pass + 1))
else
    test/agon.sh "$tmp/falloff.bin" >/dev/null 2>&1
    ok "main running off its end returns 0" "$?" 0
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

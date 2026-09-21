#!/bin/bash
# What a file's own functions cost when nothing in the file calls them.
#
# A `static` at file scope is that file's alone, so one nothing in it wanted
# is one nothing ever can want, and it has no business being in the image.
# acc cannot leave it out as it reads it -- a call may come later in the file
# -- so it writes it like any other function and takes it out again at the
# end. What that has to be is invisible: the image has to come out exactly as
# it would have if the function had not been written at all.
#
# So the checks here are mostly of the form "these two sources make the same
# bytes". The output's name goes into the image, so both sides of a pair are
# built under the same basename in different directories.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/a" "$tmp/b"
pass=0; fail=0

ok()  { pass=$((pass + 1)); }
bad() { printf '  FAIL %-38s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# same <name> <source with the extra> <source without it>
same() {
    local what=$1 err

    printf '%s' "$2" > "$tmp/a/x.c"
    printf '%s' "$3" > "$tmp/b/x.c"
    if ! err=$("$ACC" "$tmp/a/x.c" -o "$tmp/a/x.bin" -x 2>&1); then
        bad "$what" "$(printf '%s' "$err" | head -1)"; return
    fi
    if ! err=$("$ACC" "$tmp/b/x.c" -o "$tmp/b/x.bin" -x 2>&1); then
        bad "$what" "$(printf '%s' "$err" | head -1)"; return
    fi
    if cmp -s "$tmp/a/x.bin" "$tmp/b/x.bin"; then
        ok
    else
        bad "$what" "$(wc -c < "$tmp/a/x.bin") bytes against $(wc -c < "$tmp/b/x.bin")"
    fi
}

# differs <name> <source> <source>: the two are not the same image, which is
# how "it was kept" is said.
differs() {
    local what=$1 err

    printf '%s' "$2" > "$tmp/a/x.c"
    printf '%s' "$3" > "$tmp/b/x.c"
    "$ACC" "$tmp/a/x.c" -o "$tmp/a/x.bin" -x >/dev/null 2>&1
    "$ACC" "$tmp/b/x.c" -o "$tmp/b/x.bin" -x >/dev/null 2>&1
    if cmp -s "$tmp/a/x.bin" "$tmp/b/x.bin"; then
        bad "$what" "it was taken out and should have been kept"
    else
        ok
    fi
}

# runs <name> <source>: it compiles, and the Agon says 42.
runs() {
    local what=$1 err

    printf '%s' "$2" > "$tmp/a/x.c"
    if ! err=$("$ACC" "$tmp/a/x.c" -o "$tmp/a/x.bin" -x 2>&1); then
        bad "$what" "$(printf '%s' "$err" | head -1)"; return
    fi
    test/agon.sh "$tmp/a/x.bin" >/dev/null 2>&1
    case $? in
      42) ok ;;
      77) ok ;;                         # no emulator: it built, at least
      *)  bad "$what" "it said $?" ;;
    esac
}

MAIN='int main(void) { return 42; }
'

same "one nobody calls" \
"static int spare(int n) { return n * 3 + 1; }
$MAIN" "$MAIN"

same "several, and one with a string in it" \
'static int a(int n) { return n + 1; }
static const char *b(void) { return "a string only it can reach"; }
static int c(int n) { int t = 0, i; for (i = 0; i < n; i++) t += i; return t; }
'"$MAIN" "$MAIN"

# One that would have asked the library for something: the call goes with it,
# so the link does not go looking.
same "one that calls what nothing defines" \
'int nowhere(int n);
static int spare(int n) { return nowhere(n); }
'"$MAIN" "$MAIN"

# And one that would have pulled a helper into the image for its multiply.
same "one that would have wanted a helper" \
'static long spare(long a, long b) { return a * b / 3; }
'"$MAIN" "$MAIN"

# A static that only a dead static calls is kept: the walk is one pass, and
# saying so here is what stops that being a surprise.
differs "one that only a dead one calls" \
'static int inner(int n) { return n + 1; }
static int outer(int n) { return inner(n) * 2; }
'"$MAIN" \
'static int inner(int n) { return n + 1; }
'"$MAIN"

# --- and everything that has to be kept -------------------------------

runs "one that is called" \
'static int twice(int n) { return n + n; }

int main(void) { return twice(21); }
'

runs "one called before it is defined" \
'static int later(int n);

int main(void) { return later(21); }

static int later(int n) { return n + n; }
'

runs "one reached only through a pointer" \
'static int twice(int n) { return n + n; }

int main(void) { int (*f)(int) = twice; return f(21); }
'

runs "one in a table of pointers" \
'static int a(void) { return 20; }
static int b(void) { return 22; }

static int (*const table[])(void) = { a, b };

int main(void) { return table[0]() + table[1](); }
'

runs "one that calls another" \
'static int inner(int n) { return n + 1; }
static int outer(int n) { return inner(n) * 2; }

int main(void) { return outer(20); }
'

runs "a string in one that is kept" \
'static const char *name(void) { return "the answer"; }

int main(void) { return name()[4] + name()[0] - 171; }
'

# The same, through an object: what is taken out is taken out before the
# object is written, so the linker never sees it.
printf 'static int spare(int n) { return n * 3; }\nint answer(void) { return 42; }\n' \
    > "$tmp/a/lib.c"
printf 'int answer(void) { return 42; }\n' > "$tmp/b/lib.c"
"$ACC" -c "$tmp/a/lib.c" -o "$tmp/a/lib.o" >/dev/null 2>&1
"$ACC" -c "$tmp/b/lib.c" -o "$tmp/b/lib.o" >/dev/null 2>&1

# The objects themselves are not the same file -- each one records the source
# it was made from and what that source came to -- so it is the run of code
# in them that is compared, which is the three bytes at offset 7.
text_of() {
    od -An -tu1 -j7 -N3 "$1" | awk '{ print $1 + $2 * 256 + $3 * 65536 }'
}
if [ "$(text_of "$tmp/a/lib.o")" = "$(text_of "$tmp/b/lib.o")" ]; then
    ok
else
    bad "an object carries none of it" \
        "$(text_of "$tmp/a/lib.o") bytes of code against $(text_of "$tmp/b/lib.o")"
fi

printf 'int answer(void);\nint main(void) { return answer(); }\n' > "$tmp/a/main.c"
"$ACC" -c "$tmp/a/main.c" -o "$tmp/a/main.o" >/dev/null 2>&1
if "$ACC" "$tmp/a/main.o" "$tmp/a/lib.o" -o "$tmp/a/linked.bin" -x >/dev/null 2>&1; then
    test/agon.sh "$tmp/a/linked.bin" >/dev/null 2>&1
    case $? in
      42|77) ok ;;
      *) bad "an object that had one still links" "it said $?" ;;
    esac
else
    bad "an object that had one still links" "it would not link"
fi

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

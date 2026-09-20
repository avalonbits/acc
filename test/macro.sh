#!/bin/bash
# #define and #undef: a name standing for some text.
#
# Each check compiles two programs -- one using a macro, one with the macro
# written out by hand -- and requires the same image from both. That is a
# stronger statement than "it returns 42": it says the macro produced
# exactly the tokens it should have, in the right places, and nothing else.
#
# What is said when a definition is malformed is here too, since no second
# compiler is needed to ask.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# The output's name is part of the image's header, so the two sides are
# built under the same basename in different directories.
mkdir -p "$tmp/with" "$tmp/plain"

pass=0; fail=0

# same <name> <with-macros> <written-out>
same() {
    local what=$1
    printf '%s' "$2" > "$tmp/m.c"
    printf '%s' "$3" > "$tmp/p.c"

    if ! err=$("$ACC" "$tmp/m.c" -o "$tmp/with/x.bin" 2>&1); then
        printf '  FAIL %-38s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi
    if ! err=$("$ACC" "$tmp/p.c" -o "$tmp/plain/x.bin" 2>&1); then
        printf '  FAIL %-38s the hand-written one: %s\n' "$what" \
            "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi
    if ! cmp -s "$tmp/with/x.bin" "$tmp/plain/x.bin"; then
        printf '  FAIL %-38s the two images differ\n' "$what"
        fail=$((fail + 1)); return
    fi
    pass=$((pass + 1))
}

# refuses <name> <pattern> <source>
refuses() {
    local what=$1 want=$2
    printf '%s' "$3" > "$tmp/m.c"
    if err=$("$ACC" "$tmp/m.c" -o "$tmp/with/x.bin" 2>&1); then
        printf '  FAIL %-38s it was accepted\n' "$what"
        fail=$((fail + 1))
    elif ! printf '%s' "$err" | grep -q -- "$want"; then
        printf '  FAIL %-38s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
}

same "a name for a number" \
'#define SIZE 40
int main(void) { return SIZE + 2; }
' 'int main(void) { return 40 + 2; }
'

same "a name for an expression" \
'#define TWO (1 + 1)
int main(void) { return 40 + TWO; }
' 'int main(void) { return 40 + (1 + 1); }
'

same "a macro used in its own definition" \
'#define INNER 21
#define OUTER (INNER * 2)
int main(void) { return OUTER; }
' 'int main(void) { return (21 * 2); }
'

same "a definition holding several tokens" \
'#define BODY int x = 20; return x + x + 2;
int main(void) { BODY }
' 'int main(void) { int x = 20; return x + x + 2; }
'

same "an empty definition expands to nothing" \
'#define NOTHING
int main(void) { NOTHING return 42; }
' 'int main(void) { return 42; }
'

# Written with printf so that the blanks are explicit: a literal here loses
# them to the first editor that trims lines. They are kept in the stored
# text and skipped by the lexer like any other space, so what this pins is
# that they do no harm.
same "blanks at the end of a definition" \
"$(printf '#define SIZE 42 \t \nint main(void) { return SIZE; }\n')" \
'int main(void) { return 42; }
'

same "a definition ending in a comment" \
'#define SIZE 42 /* how big */
int main(void) { return SIZE; }
' 'int main(void) { return 42; }
'

same "#undef, and the name used again after it" \
'#define value 1
#undef value
int main(void) { int value = 42; return value; }
' 'int main(void) { int value = 42; return value; }
'

same "#undef of a name never defined" \
'#undef never_defined
int main(void) { return 42; }
' 'int main(void) { return 42; }
'

same "defined again after an #undef" \
'#define N 1
#undef N
#define N 42
int main(void) { return N; }
' 'int main(void) { return 42; }
'

# A macro that stands for its own name is the case that would expand for
# ever. It has to come out as the name itself, once.
same "a macro that stands for its own name" \
'#define A A
int A(void) { return 42; }
int main(void) { return A(); }
' 'int A(void) { return 42; }
int main(void) { return A(); }
'

# Each stands for the other, so expanding either one comes back to where it
# started and stops there: `X` becomes `Y` becomes `X`, which is already
# being expanded and is left alone. Every name is therefore its own answer,
# and the pair terminates instead of going round for ever.
same "two macros that stand for each other" \
'#define X Y
#define Y X
int X(void) { return 42; }
int main(void) { return X(); }
' 'int X(void) { return 42; }
int main(void) { return X(); }
'

# The name is only a macro where it is a whole token.
same "a name inside a longer one is not expanded" \
'#define N 1
int main(void) { int Nx = 42; return Nx; }
' 'int main(void) { int Nx = 42; return Nx; }
'

same "a name inside a string is not expanded" \
'#define N 1
int main(void) { char *s = "N"; return s[0] == 78 ? 42 : 0; }
' 'int main(void) { char *s = "N"; return s[0] == 78 ? 42 : 0; }
'

# Definitions from a header are in force in the file that included it.
mkdir -p "$tmp/inc"
cat > "$tmp/inc/defs.h" <<'EOF'
#define FROM_HEADER 42
EOF
cp "$tmp/inc/defs.h" "$tmp/defs.h"
same "a definition made in a header" \
'#include "defs.h"
int main(void) { return FROM_HEADER; }
' 'int main(void) { return 42; }
'

# --- what is refused --------------------------------------------------
refuses "#define with no name" "needs a name" \
'#define
int main(void) { return 0; }
'
refuses "#define of something that is not a name" "needs a name" \
'#define 3 4
int main(void) { return 0; }
'
refuses "#undef with no name" "needs a name" \
'#undef
int main(void) { return 0; }
'
refuses "#undef with more than a name" "and nothing else" \
'#define A 1
#undef A B
int main(void) { return 0; }
'
refuses "a macro with parameters, for now" "not supported yet" \
'#define f(x) (x)
int main(void) { return 0; }
'

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

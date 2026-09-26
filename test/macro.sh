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
#
# Run against the sanitized build this looks for leaks as well, and that is
# on purpose: the preprocessor allocates for every expansion and every file
# it opens, so a leak here is one that grows with the program rather than a
# one-off. The other suites turn leak checking off because acc frees
# nothing it does not have to -- a compiler runs once and exits -- but what
# this covers is the part where that reasoning does not hold.
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

# same_opts <name> <source compiled with them> <hand-written> <acc option>...
#
# The options come last and one to an argument, so that one holding a space
# -- which a macro body may -- stays one option.
#
# The options say what the source does not: `-DSIZE=42` against a file that
# begins `#define SIZE 42`. Requiring one image from both is what says the
# option made the same macro the directive would have, rather than
# something that merely answers the same.
same_opts() {
    local what=$1

    printf '%s' "$2" > "$tmp/m.c"
    printf '%s' "$3" > "$tmp/p.c"
    shift 3

    if ! err=$("$ACC" "$@" "$tmp/m.c" -o "$tmp/with/x.bin" 2>&1); then
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

# refuses_opts <name> <pattern> <source> <acc option>...
refuses_opts() {
    local what=$1 want=$2

    printf '%s' "$3" > "$tmp/m.c"
    shift 3
    if err=$("$ACC" "$@" "$tmp/m.c" -o "$tmp/with/x.bin" 2>&1); then
        printf '  FAIL %-38s it was accepted\n' "$what"
        fail=$((fail + 1))
    elif ! printf '%s' "$err" | grep -q -- "$want"; then
        printf '  FAIL %-38s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
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

# A join in the middle of a token in the body. The name a #define is about
# is read straight from the file, so that half has always worked; the body
# is gathered first, and gathering it used to leave a space behind where
# the join was -- which turned one name into two.
same "a join in the middle of a definition" \
'#define NAME ab\
cd
int main(void) { int abcd = 42; return NAME; }
' 'int main(void) { int abcd = 42; return abcd; }
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
# --- macros with parameters -------------------------------------------

same "one parameter" \
'#define TWICE(x) ((x) * 2)
int main(void) { return TWICE(21); }
' 'int main(void) { return ((21) * 2); }
'

same "two parameters" \
'#define ADD(a, b) ((a) + (b))
int main(void) { return ADD(40, 2); }
' 'int main(void) { return ((40) + (2)); }
'

same "no parameters at all" \
'#define NOW() 42
int main(void) { return NOW(); }
' 'int main(void) { return 42; }
'

# A comma inside parentheses belongs to what it is inside, or `f(a, b)` as
# an argument would arrive as two.
same "a comma inside an argument" \
'#define PICK(a, b) (b)
int main(void) { return PICK(f(1, 2), 42); }
' 'int main(void) { return (42); }
'

same "an empty argument" \
'#define SECOND(a, b) (b)
int main(void) { return SECOND(, 42); }
' 'int main(void) { return (42); }
'

same "a call spread over lines" \
'#define ADD(a, b) ((a) + (b))
int main(void) { return ADD(
    40,
    2); }
' 'int main(void) { return ((40) + (2)); }
'

# An argument is expanded before it goes in, and the result is read again,
# so a macro can be written in terms of another.
same "a macro used as an argument" \
'#define N 21
#define TWICE(x) ((x) * 2)
int main(void) { return TWICE(N); }
' 'int main(void) { return ((21) * 2); }
'

same "a macro whose body uses another" \
'#define TWICE(x) ((x) * 2)
#define QUAD(x) TWICE(TWICE(x))
int main(void) { return QUAD(10) + 2; }
' 'int main(void) { return ((((10) * 2)) * 2) + 2; }
'

same "a macro that calls itself stops" \
'#define f(x) f(x)
int f(int v) { return v; }
int main(void) { return f(42); }
' 'int f(int v) { return v; }
int main(void) { return f(42); }
'

# Without a '(' after it the name is an ordinary identifier, which is what
# lets a variable and a macro over it share a spelling.
same "the name on its own is not a use" \
'#define twice(x) ((x) * 2)
int main(void) { int twice = 42; return twice; }
' 'int main(void) { int twice = 42; return twice; }
'

same "a macro with parameters in an #if" \
'#define DOUBLE(x) ((x) * 2)
#if DOUBLE(3) == 6
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

# --- # and ## ---------------------------------------------------------

same "# makes a string of an argument" \
'#define NAME(x) #x
int main(void) { char *s = NAME(hello); return s[0] == 104 ? 42 : 0; }
' 'int main(void) { char *s = "hello"; return s[0] == 104 ? 42 : 0; }
'

# What # quotes is what was written, not what it stands for: that is the
# one place an argument is not expanded first.
same "# quotes the argument unexpanded" \
'#define N 7
#define NAME(x) #x
int main(void) { char *s = NAME(N); return s[0] == 78 ? 42 : 0; }
' 'int main(void) { char *s = "N"; return s[0] == 78 ? 42 : 0; }
'

same "## joins two names" \
'#define JOIN(a, b) a ## b
int value42 = 42;
int main(void) { return JOIN(value, 42); }
' 'int value42 = 42;
int main(void) { return value42; }
'

same "## joins two numbers" \
'#define JOIN(a, b) a ## b
int main(void) { return JOIN(4, 2); }
' 'int main(void) { return 42; }
'

# --- a variable number of arguments -----------------------------------

same "... with the rest thrown away" \
'#define FIRST(a, ...) (a)
int main(void) { return FIRST(42, 1, 2, 3); }
' 'int main(void) { return (42); }
'

same "__VA_ARGS__, commas and all" \
'#define SUM(a, ...) ((a) + rest(__VA_ARGS__))
int rest(int x, int y) { return x + y; }
int main(void) { return SUM(20, 10, 12); }
' 'int rest(int x, int y) { return x + y; }
int main(void) { return ((20) + rest(10, 12)); }
'

# --- what is refused --------------------------------------------------
refuses "too few arguments" "takes 2 arguments, not 1" \
'#define ADD(a, b) ((a)+(b))
int main(void) { return ADD(1); }
'
refuses "too many arguments" "takes 1 argument, not 2" \
'#define ONE(a) (a)
int main(void) { return ONE(1, 2); }
'
refuses "arguments that are not closed" "not closed" \
'#define ADD(a, b) ((a)+(b))
int main(void) { return ADD(1, 2; }
'
refuses "# on something that is not a parameter" "needs one of its" \
'#define BAD(x) # 3
int main(void) { return BAD(1); }
'
refuses "a parameter list with no ')'" "need a" \
'#define BAD(a, b (a)
int main(void) { return 0; }
'

# --- #error, #pragma and #line ----------------------------------------

refuses "#error says what it was given" "#error this needs a library" \
'#error this needs a library
int main(void) { return 0; }
'

# Under a group that is not taken it is not said: the program meant not to.
same "#error in a group that is skipped" \
'#ifdef NOPE
#error not said
#endif
int main(void) { return 42; }
' 'int main(void) { return 42; }
'

# C says an unknown pragma is ignored, and acc knows none.
same "#pragma is ignored" \
'#pragma once
#pragma anything at all
int main(void) { return 42; }
' 'int main(void) { return 42; }
'

same "#line sets the number" \
'#line 100
int main(void) { return __LINE__ - 58; }
' 'int main(void) { return 100 - 58; }
'

same "#line sets the file name too" \
'#line 7 "generated.c"
int main(void) { char *s = __FILE__; return s[0] == 103 ? 42 : 0; }
' 'int main(void) { char *s = "generated.c"; return s[0] == 103 ? 42 : 0; }
'

# And what it renumbers is what a diagnostic says, which is the point of it.
refuses "#line moves the diagnostics" ":50:25: error:" \
'#line 50
int main(void) { return @; }
'

refuses "#line with no number" "needs a line number" \
'#line
int main(void) { return 0; }
'
refuses "#line with a name that is not closed" "is not closed" \
'#line 3 "nope
int main(void) { return 0; }
'
refuses "#line with anything else after it" "nothing else" \
'#line 3 "a.c" and more
int main(void) { return 0; }
'

# --- __FILE__ and __LINE__ --------------------------------------------
# The compiler defines these, not the program. Their values depend on where
# they are written, so the hand-written side spells out what they should
# come to -- for __FILE__ that is the path the test itself used.

same "__LINE__ is the line it is on" \
'int a(void) { return __LINE__; }

int b(void) { return __LINE__; }
int main(void) { return a() + b() + 38; }
' 'int a(void) { return 1; }

int b(void) { return 3; }
int main(void) { return a() + b() + 38; }
'

# Inside an expansion the line is the one the macro was used on, since
# putting text back does not move the count.
same "__LINE__ inside a macro" \
'#define WHERE __LINE__
int main(void) { return WHERE + 41; }
' 'int main(void) { return 2 + 41; }
'

same "__FILE__ is the file being read" \
'int main(void) { char *s = __FILE__; return s[0] == 47 ? 42 : 0; }
' "int main(void) { char *s = \"$tmp/m.c\"; return s[0] == 47 ? 42 : 0; }
"

refuses "#define of __LINE__" "compiler's to define" \
'#define __LINE__ 5
int main(void) { return 0; }
'
refuses "#undef of __FILE__" "compiler's to define" \
'#undef __FILE__
int main(void) { return 0; }
'

same "both count as defined" \
'#if defined(__FILE__) && defined(__LINE__)
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

# --- the conditionals -------------------------------------------------
# #ifdef and #ifndef ask the same table #define fills, so they belong here.

same "#ifdef on a name that is defined" \
'#define YES 1
#ifdef YES
int main(void) { return 42; }
#else
int main(void) { return 1; }
#endif
' 'int main(void) { return 42; }
'

same "#ifndef on a name that is not" \
'#ifndef NOPE
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

same "the #else of a group not taken" \
'#ifdef NOPE
int main(void) { return 1; }
#else
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

same "#undef makes an #ifdef false" \
'#define A 1
#undef A
#ifdef A
int main(void) { return 1; }
#else
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

same "a group inside a group that is taken" \
'#define A 1
#ifdef A
#ifdef NOPE
int main(void) { return 1; }
#endif
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

# The nesting has to be counted while skipping too, or the inner #endif
# ends the outer group and the text after it comes back.
same "a group inside a group that is skipped" \
'#ifdef NOPE
#ifdef ALSO_NOPE
#endif
int main(void) { return 1; }
#endif
int main(void) { return 42; }
' 'int main(void) { return 42; }
'

# What is skipped is not lexed: `#if 0` around prose is what it is for, and
# an apostrophe in it closes nothing.
same "text that is not C inside a skipped group" \
"$(printf '#ifdef NOPE\nthis is prose, and it'"'"'s got \"unclosed quotes\n#endif\nint main(void) { return 42; }\n')" \
'int main(void) { return 42; }
'

# A comment is the one thing that is read, because one can run over the
# #endif that would otherwise end the group.
same "a comment hiding an #endif" \
'#ifdef NOPE
/* #endif */
int main(void) { return 1; }
#endif
int main(void) { return 42; }
' 'int main(void) { return 42; }
'

refuses "#endif with no #if" "without #if" \
'#endif
int main(void) { return 0; }
'
refuses "#else with no #if" "without #if" \
'#else
int main(void) { return 0; }
'
refuses "an #if left open" "without #endif" \
'#ifdef NOPE
int main(void) { return 0; }
'
refuses "#else twice" "after #else" \
'#ifdef NOPE
#else
#else
#endif
int main(void) { return 42; }
'
# --- #if on an expression ---------------------------------------------

same "arithmetic in an #if" \
'#if (2 + 3) * 4 == 20
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

# A name nothing defined is zero rather than a mistake, which is what makes
# `#if FEATURE` usable without defining FEATURE to 0 first.
same "a name nothing defined is zero" \
'#if UNDEFINED_THING
int main(void) { return 1; }
#else
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

same "defined, with and without parentheses" \
'#define A 1
#if defined A && defined(A) && !defined(NOPE)
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

same "a macro used in the condition" \
'#define WIDTH 6
#define HEIGHT 7
#if WIDTH * HEIGHT == 42
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

same "an #elif chain, the middle one taken" \
'#define V 2
#if V == 1
int main(void) { return 1; }
#elif V == 2
int main(void) { return 42; }
#elif V == 3
int main(void) { return 3; }
#endif
' 'int main(void) { return 42; }
'

same "an #elif chain falling to #else" \
'#define V 9
#if V == 1
int main(void) { return 1; }
#elif V == 2
int main(void) { return 2; }
#else
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

# Once a branch has run the rest are skipped whatever they say, so the
# condition on this one is never worked out -- it would divide by zero.
same "an #elif after a branch that ran" \
'#if 1
int main(void) { return 42; }
#elif 1/0
int main(void) { return 1; }
#endif
' 'int main(void) { return 42; }
'

same "hex, octal and character constants" \
"$(printf '#if 0x10 == 16 && 010 == 8 && %s == 65\nint main(void) { return 42; }\n#endif\n' "'A'")" \
'int main(void) { return 42; }
'

same "shifts, and the conditional operator" \
'#if (1 << 5) == 32 ? 1 : 0
int main(void) { return 42; }
#endif
' 'int main(void) { return 42; }
'

# The sides C says are not evaluated: the right of a `&&` that is already
# false, of a `||` that is already true, and the branch of a `?:` that is
# not taken. A program is entitled to guard a division that way, and a
# compiler that divides anyway refuses a condition that is well formed.
same "a divide an #if never reaches" \
'#if 0 && 1 / 0
int broken_and;
#endif
#if 1 || 1 / 0
#define FROM_OR 20
#endif
#if 1 ? 1 : 1 / 0
#define FROM_YES 20
#endif
#if 0 ? 1 / 0 : 1
#define FROM_NO 2
#endif
int main(void) { return FROM_OR + FROM_YES + FROM_NO; }
' 'int main(void) { return 20 + 20 + 2; }
'

refuses "a division by zero in an #if" "division by zero" \
'#if 1/0
#endif
int main(void) { return 42; }
'
refuses "an #if with no expression" "stops too soon" \
'#if
#endif
int main(void) { return 42; }
'
refuses "a character an #if cannot read" "has no meaning" \
'#if @
#endif
int main(void) { return 42; }
'
refuses "something left over in an #if" "left over" \
'#if 1 2
#endif
int main(void) { return 42; }
'
refuses "an #if missing a parenthesis" "needs a" \
'#if (1 + 2
#endif
int main(void) { return 42; }
'
refuses "#elif after #else" "after #else" \
'#if 0
#else
#elif 1
#endif
int main(void) { return 42; }
'
refuses "#elif with no #if" "without #if" \
'#elif 1
int main(void) { return 42; }
'
refuses "defined with no name" "needs a name" \
'#if defined()
#endif
int main(void) { return 42; }
'
refuses "#ifdef with no name" "needs a name" \
'#ifdef
int main(void) { return 0; }
'

# -trigraphs: the nine `??` sequences of C99 5.2.1.1, read as what they
# stand for before anything else -- in strings too, and before lines are
# joined, so that `??/` at a line's end joins it. pr18502-1 in gcc.dg.
same_opts "-trigraphs reads them" \
'int a??(2??) = ??< 40, 2 ??>;
??=define ADD(x, y) ((x) + ??/
(y))
int main(void) { const char *s = "??/"??!"; int b = 1 ??! 2, c = ??-0;
    return ADD(a??(0??), a??(1??)) + (s??(0??) == 34 && s??(1??) == 124 && b == 3 && c == -1 ? 0 : 1); }
' 'int a[2] = { 40, 2 };
#define ADD(x, y) ((x) + (y))
int main(void) { const char *s = "\"|"; int b = 1 | 2, c = ~0;
    return ADD(a[0], a[1]) + (s[0] == 34 && s[1] == 124 && b == 3 && c == -1 ? 0 : 1); }
' "-trigraphs"

# And without it they are what they are written as.
same_opts "without -trigraphs they are left" \
'int main(void) { return sizeof "??(" == 4 ? 42 : 1; }
' 'int main(void) { return sizeof "\?\?(" == 4 ? 42 : 1; }
'

# -D and -U: a macro made on the command line rather than in the file.
# Written out as the directive would have read it and handed to the same
# code, so what these check is that the option reaches it intact.
same_opts "-D of a name and a value" \
'int main(void) { return SIZE; }
' '#define SIZE 42
int main(void) { return SIZE; }
' "-DSIZE=42"

same_opts "-D of a name alone is 1" \
'int main(void) { return 41 + ON; }
' '#define ON 1
int main(void) { return 41 + ON; }
' "-DON"

same_opts "-D with the name apart from it" \
'int main(void) { return SIZE; }
' '#define SIZE 42
int main(void) { return SIZE; }
' "-D" "SIZE=42"

same_opts "-D of a macro with parameters" \
'int main(void) { return ADD(40, 2); }
' '#define ADD(a, b) ((a) + (b))
int main(void) { return ADD(40, 2); }
' "-DADD(a,b)=((a) + (b))"

same_opts "-D of a body with a bracket" \
'int main(void) { return P; }
' '#define P (21 * 2)
int main(void) { return P; }
' "-DP=(21 * 2)"

same_opts "-U takes one away again" \
'int main(void) {
#ifdef GONE
    return 0;
#else
    return 42;
#endif
}
' 'int main(void) { return 42; }
' "-DGONE=1" "-UGONE"

same_opts "-U of a name nothing defined" \
'int main(void) { return 42; }
' 'int main(void) { return 42; }
' "-UNEVER"

same_opts "-D after -U of the same name" \
'int main(void) { return X; }
' '#define X 42
int main(void) { return X; }
' "-UX" "-DX=42"

same_opts "-D reaches an #if, not just the code" \
'int main(void) {
#if VAL == 7
    return 42;
#else
    return 0;
#endif
}
' 'int main(void) { return 42; }
' "-DVAL=7"

refuses_opts "-D of something that is not a name" "does not begin with one" 'int main(void) { return 42; }
' "-D=1"

refuses_opts "-U of something that is not a name" "does not begin with one" 'int main(void) { return 42; }
' "-U9x"

refuses_opts "-D of a name the compiler defines" "the compiler's to define" 'int main(void) { return 42; }
' "-D__LINE__=1"

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

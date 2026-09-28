#!/bin/bash
# Puts acc, zap and the test programs on the SD card, and writes the README
# from the tests themselves.
#
#   test/card.sh [card-directory]
#
# The README is generated rather than edited because the hand-written one went
# stale twice: it claimed eleven cases when there were fourteen, four error
# programs when there were six, and it said nothing either way about comments,
# which work. Anything on the card that a reader could check against the tests
# is derived from them here.
#
# The expected values come from running each case, the same way test/run.sh
# does. There is no oracle on the Agon, so they have to be written down.
set -uo pipefail

cd "$(dirname "$0")/.."

SD=${1:-${ACC_SDCARD:-/mnt/d/agon-emu/sdcard}}
ZAP=${ACC_ZAP:-$HOME/code/zap/bin/zap.bin}

[ -d "$SD" ] || { echo "no card at $SD" >&2; exit 2; }
[ -f bin/acc.bin ] || { echo "no bin/acc.bin -- run make -f Makefile.agon" >&2; exit 2; }
[ -x bin/acc ] || { echo "no bin/acc -- run make" >&2; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

mkdir -p "$SD/bin" "$SD/testc/cases" "$SD/testc/errors"
cp bin/acc.bin "$SD/bin/acc.bin"
mkdir -p "$SD/lib/acc"
cp bin/libc.a "$SD/lib/acc/"        # the library, with the runtime every program calls
[ -f "$ZAP" ] && cp "$ZAP" "$SD/bin/zap.bin"
cp test/demos/*.c "$SD/testc/"
cp test/cases/*.c "$SD/testc/cases/"
cp test/errors/*.c "$SD/testc/errors/"

# Anything left over from a previous card: compiled output from running the
# demos, and programs that are no longer tests. The card is not the copy of
# record for any of this -- a demo left only on it was overwritten by a
# compiler output file and nobody noticed until it was read back.
rm -f "$SD"/testc/*.bin
for f in "$SD"/testc/*.c; do
    [ -f "test/demos/$(basename "$f")" ] || rm -f "$f"
done
for d in cases errors; do
    for f in "$SD"/testc/$d/*.c; do
        [ -f "test/$d/$(basename "$f")" ] || rm -f "$f"
    done
done

# Each demo says in its first lines what it prints. Check that it does, so the
# card cannot promise one number and produce another.
bad=0
for src in test/demos/*.c; do
    name=$(basename "$src")
    want=$(sed -n 's/.*Prints \([0-9A-F]\{6\}\).*/\1/p' "$src" | head -1)
    if ! bin/acc "$src" -o "$tmp/t.bin" -x >/dev/null 2>&1; then
        [ -n "$want" ] && { echo "  $name claims $want and does not compile" >&2; bad=1; }
        continue
    fi
    [ -n "$want" ] || { echo "  $name does not say what it prints" >&2; bad=1; continue; }
    test/agon.sh "$tmp/t.bin" >/dev/null 2>&1; v=$?
    [ "$v" -eq 77 ] && continue
    got=$(printf '%06X' "$v")
    [ "$got" = "$want" ] || { echo "  $name says it prints $want, prints $got" >&2; bad=1; }
done
[ "$bad" -eq 0 ] || { echo "the demos and their comments disagree -- card not written" >&2; exit 1; }

README=$SD/testc/README.txt

{
cat <<'EOF'
acc -- a C compiler that runs on the Agon
=========================================

  acc <source.c> -o <out.bin>      compile, then run out.bin
  acc <source.c> -o <out.bin> -p   and have it print what main returned
  acc <source.c> -o <out.bin> -x   report through IO port 0 instead

acc says how long it took. A compiled program gives what main returned back
to MOS, as a program agondev built does, and prints nothing of its own.
With -p it prints it first, as six hex digits:

  acc testc/add.c -o add.bin -p
  Done in 0.00 seconds
  add
  00002A

That 2A is 42. Each demo in testc/ says in its first lines what it prints
so.

What acc can do today
---------------------

  char, short, int, long, long long, _Bool, and the unsigned form of each
  float and double
  pointers to any of them, and to pointers, seven deep
  functions, parameters, locals, calls, recursion, prototypes, pointers to
    functions, and ... with va_list
  struct, union, enum, typedef, bit-fields, and arrays of any dimension --
    including ones whose length the program works out
  + - * / % and unary minus, ~
  & | ^ << >>
  assignment, and += -= *= /= %= &= |= ^= <<= >>=
  < > <= >= == !=, signed or unsigned according to the operands
  ! && ||, stopping as soon as the answer is known
  ++ -- before and after, ?:, the comma operator, sizeof, casts
  if, else, else if, while, for, do, switch, break, continue, goto
  static, extern, const, volatile, register, auto, inline
  initialisers with braces, designated initialisers, compound literals
  decimal, hex and octal constants, with the u, l and ll suffixes
  floating constants, with or without an exponent
  'c' and "text", their escapes, and __func__
  /* block */ and // line comments, anywhere a space can go

char is one byte, short two, int three, long four, and a pointer three --
agondev's widths, so a program compiled by either comes out the same. Arithmetic happens at int
width and only a store narrows, which is what C says; a long or a float
drags the other operand up to its own width instead.

float and double are both the same four bytes, IEEE-754 single, which is
what agondev makes them. Denormals, infinities and NaNs all work. What is
missing is the part of IEEE-754 about the machine rather than the numbers:
no exception flags, no rounding mode but to nearest with ties to even, and a
signalling NaN is treated as a quiet one. C99 puts all of that behind
<fenv.h> and makes it optional.

The eZ80 has an instruction for almost none of this -- no 24-bit AND, no
shift of more than one place, no multiply wider than 8x8, no divide at all,
and nothing whatever for floating point. So acc carries the routines and
puts the ones a program uses into that program's image. A program that uses
none pays nothing for them.

What it cannot do yet
---------------------

The language acc is being written towards is C99. Everything below is
missing rather than excluded.

  #include, #define    -- no preprocessor
  printf               -- no library, which is why a program prints its
                          result the way it does
  long double          -- libagon has no arithmetic for a double, so acc
                          refuses the type rather than carry half of it
  L'x' and L"text"     -- no wide characters or strings
  _Complex             -- and no imaginary numbers
  int f(a) int a; {}   -- a definition has to say its parameters' types

  a frame over 128 bytes -- which is 42 ints or 32 longs in one function.
                          (ix+d) reaches that far and no further.

Anything acc cannot do it should name and point at, rather than emit
something that misbehaves. broken.c is there to be refused.

zap is here too, for assembling by hand what acc cannot generate yet.

A value is true when it compares unequal to zero at its own type, which is
not always what its bytes say: a float is false at -0.0, and 0.5 is true
although it would convert to the integer 0. `&&=` and `||=` are not
operators in C, and acc says so.

`p + 1` is the next object and not the next byte, so `&a + 1` moves by
three for an int and by one for a char. `q - p` is the same backwards, a
count of objects rather than of bytes, and two pointers can only be
subtracted or compared when they agree about what they point at.

The test suite
--------------

EOF

ncases=$(ls test/cases/*.c | wc -l)
nerrs=$(ls test/errors/*.c | wc -l)

cat <<EOF
testc/cases holds the $ncases programs the host runs against agondev and
requires the same answer from. There is no oracle on the Agon, so what each
one should print is written down here instead:

EOF

for src in test/cases/*.c; do
    name=$(basename "$src")
    bin/acc "$src" -o "$tmp/t.bin" -x >/dev/null 2>&1 || { echo "  $name -- acc could not compile it"; continue; }
    test/agon.sh "$tmp/t.bin" >/dev/null 2>&1; v=$?
    [ "$v" -eq 77 ] && { echo "  $name -- no emulator, value unknown"; continue; }
    printf '  acc testc/cases/%-24s -o t.bin -p    t    %06X\n' "$name" "$v"
done

cat <<EOF

testc/errors holds $nerrs acc has to refuse, with the message it has to give:

EOF

for src in test/errors/*.c; do
    name=$(basename "$src")
    msg=$(bin/acc "$src" -o "$tmp/t.bin" -x 2>&1 | head -1 | sed "s|^$src|testc/errors/$name|")
    printf '  acc testc/errors/%s\n    %s\n' "$name" "$msg"
done
} > "$README"

echo "card: $SD"
printf '  bin/acc.bin  %s bytes\n' "$(stat -c%s "$SD/bin/acc.bin")"
[ -f "$SD/bin/zap.bin" ] && printf '  bin/zap.bin  %s bytes\n' "$(stat -c%s "$SD/bin/zap.bin")"
printf '  testc/cases  %s programs\n' "$(ls "$SD"/testc/cases/*.c | wc -l)"
printf '  testc/errors %s programs\n' "$(ls "$SD"/testc/errors/*.c | wc -l)"
printf '  testc/README.txt regenerated\n'

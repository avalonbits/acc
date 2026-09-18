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
  acc <source.c> -o <out.bin> -x   report through IO port 0 instead

acc says how long it took. A compiled program prints what main returned, as
six hex digits, and comes back to MOS:

  acc testc/add.c -o add.bin
  Done in 0.00 seconds
  add
  00002A

That 2A is 42.

What acc can do today
---------------------

  char, short, int, long, and the unsigned form of each
  float and double
  functions, parameters, locals, calls, recursion
  + - * / % and unary minus, ~
  & | ^ << >>
  assignment
  < > <= >= == !=, signed or unsigned according to the operands
  if, else, else if, while
  decimal and hex constants, with the u and l suffixes
  floating constants, with or without an exponent
  /* block */ and // line comments, anywhere a space can go

char is one byte, short two, int three, long four -- agondev's widths, so a
program compiled by either comes out the same. Arithmetic happens at int
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

  ! && || ?:           -- no logical or conditional operators; a condition
                          is a value, or a comparison
  for do break continue switch -- while is the only loop and the only
                          branch besides if
  (int) x              -- no casts; a conversion happens where a value is
                          assigned or passed, and nowhere else
  ++ -- += -= etc.     -- no increment, decrement or compound assignment
  pointers, arrays, structs, unions, enums
  'c' and "text"       -- no character or string constants
  variables at file scope -- everything lives in a function
  void f(void)         -- a function has to return something
  int f(int);          -- no declaration without a definition, so a function
                          must be defined above its first call unless it
                          takes and returns int
  declarations inside a block -- they go at the start of the function
  long long, long double
  #include, #define    -- no preprocessor
  printf               -- no library, which is why a program prints its
                          result the way it does

  a frame over 128 bytes -- which is 42 ints or 32 longs in one function.
                          (ix+d) reaches that far and no further.

Anything acc cannot do it should name and point at, rather than emit
something that misbehaves. broken.c is there to be refused.

zap is here too, for assembling by hand what acc cannot generate yet.

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
    printf '  acc testc/cases/%-24s -o t.bin    t    %06X\n' "$name" "$v"
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

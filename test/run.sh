#!/bin/bash
# acc test suite.
#
# Every target acc can be built for gets its data model asserted here, because
# the data model is the port: the eZ80 has a 24-bit int and a 32-bit long, and
# a compiler that gets that wrong produces code that links and then misbehaves.
# _Static_assert is the right tool for it -- it is checked by the compiler
# under test, for the target under test, with no need to run the output.
set -uo pipefail
cd "$(dirname "$0")/.."

pass=0; fail=0
ok()   { pass=$((pass+1)); printf '  ok   %s\n' "$1"; }
bad()  { fail=$((fail+1)); printf '  FAIL %s\n' "$1"; [ -n "${2:-}" ] && printf '%s\n' "$2" | sed 's/^/         /'; }

# Compiles a fragment for a target and reports whether it was accepted.
# expect=ok means it must compile; expect=err means it must be rejected.
compiles() {
    local cc=$1 name=$2 expect=$3 src=$4 out
    out=$(printf '%s\n' "$src" | $cc -c -xc - -o /dev/null 2>&1)
    if [ $? -eq 0 ]; then
        [ "$expect" = ok ] && ok "$name" || bad "$name" "expected rejection, was accepted"
    else
        [ "$expect" = err ] && ok "$name" || bad "$name" "$out"
    fi
}

# ---- data model -------------------------------------------------------------
# char short int long llong ptr float double ldouble
model() {
    local cc=$1 name=$2; shift 2
    compiles "$cc" "$name" ok "
_Static_assert(sizeof(char)       == $1, \"char\");
_Static_assert(sizeof(short)      == $2, \"short\");
_Static_assert(sizeof(int)        == $3, \"int\");
_Static_assert(sizeof(long)       == $4, \"long\");
_Static_assert(sizeof(long long)  == $5, \"long long\");
_Static_assert(sizeof(void *)     == $6, \"void *\");
_Static_assert(sizeof(float)      == $7, \"float\");
_Static_assert(sizeof(double)     == $8, \"double\");
_Static_assert(sizeof(long double)== $9, \"long double\");
"
}

echo "i386 (validation build):"
[ -x bin/acc-i386 ] || { echo "  bin/acc-i386 missing -- run make"; exit 2; }
model bin/acc-i386 "data model" 1 2 4 4 8 4 4 8 12
compiles bin/acc-i386 "compiles a function" ok 'int f(int x){return x*2;}'
compiles bin/acc-i386 "rejects bad syntax" err 'int f(int x){return x*;}'
compiles bin/acc-i386 "struct alignment unchanged" ok '
_Static_assert(sizeof(struct{char c; int i;}) == 8, "padded");
_Static_assert(_Alignof(int) == 4, "align 4");
'

echo
echo "eZ80 (Agon cross-compiler):"
[ -x bin/acc ] || { echo "  bin/acc missing -- run make"; exit 2; }
model bin/acc "data model" 1 2 3 4 8 3 4 4 8

# Every type packs to alignment 1. Verified against agondev's own compiler,
# down to `struct { char c; long long ll; }` being nine bytes -- matching it
# is what lets a struct cross between acc's output and libagon.
compiles bin/acc "everything packs to 1" ok '
_Static_assert(sizeof(struct{char c; int i;})        == 4, "char+int");
_Static_assert(sizeof(struct{char c; long l;})       == 5, "char+long");
_Static_assert(sizeof(struct{char c; long long l;})  == 9, "char+long long");
_Static_assert(sizeof(struct{char c; double d;})     == 5, "char+double");
_Static_assert(sizeof(struct{char c; short s;})      == 3, "char+short");
_Static_assert(sizeof(struct{char c; long double d;})== 9, "char+long double");
_Static_assert(_Alignof(int) == 1, "align int");
_Static_assert(_Alignof(long long) == 1, "align long long");
'

# int and long share a basic type and are told apart by VT_LONG. Getting that
# wrong makes them the same width, which is the failure this guards.
compiles bin/acc "int and long are different widths" ok '
_Static_assert(sizeof(int) != sizeof(long), "int vs long");
_Static_assert(sizeof(int[10]) == 30, "array of int");
_Static_assert(sizeof(int *) == 3, "pointer to int");
'

# A 3-byte pointer means a 3-byte size_t and ptrdiff_t. tinycc picks those
# from PTR_SIZE and only knew about 4 and 8.
compiles bin/acc "size_t and ptrdiff_t follow the pointer" ok '
typedef __SIZE_TYPE__ st; typedef __PTRDIFF_TYPE__ pt;
_Static_assert(sizeof(st) == 3, "size_t");
_Static_assert(sizeof(pt) == 3, "ptrdiff_t");
'

# agondev's own headers branch on these, so acc has to report what agondev
# reports or <stdint.h> picks the wrong types. The values on the right are
# what ez80-none-elf-clang prints for the same macros.
compiles bin/acc "predefined widths match agondev" ok '
_Static_assert(__SIZEOF_POINTER__   == 3, "pointer");
_Static_assert(__SIZEOF_INT__       == 3, "int");
_Static_assert(__SIZEOF_LONG__      == 4, "long");
_Static_assert(__SIZEOF_SIZE_T__    == 3, "size_t");
_Static_assert(__SIZEOF_PTRDIFF_T__ == 3, "ptrdiff_t");
_Static_assert(__INT_MAX__     == 8388607, "INT_MAX");
_Static_assert(__CHAR_BIT__         == 8, "CHAR_BIT");
'

compiles bin/acc "defines __ez80__ and __AGON__" ok '
#if !defined(__ez80__) || !defined(__AGON__)
#error missing target define
#endif
'

# The backend emits nothing yet. What matters until it does is that reaching
# it says so rather than producing a binary that is quietly wrong.
out=$(printf 'int f(int x){return x*2;}\n' | bin/acc -c -xc - -o /dev/null 2>&1)
case "$out" in
  *"not implemented"*) ok "codegen refuses rather than emitting wrong code" ;;
  *) bad "codegen refuses rather than emitting wrong code" "$out" ;;
esac

echo
echo "object interop with agondev:"
# acc's objects have to be readable by agondev's binutils: same ELF machine
# flags, same symbol naming, same relocation format. Without that they cannot
# be linked against libagon.a and a compiled program has no C library.
AGONDEV=${AGONDEV:-$HOME/agondev}
if [ -x "$AGONDEV/bin/ez80-none-elf-ld" ]; then
    obj=$(mktemp -d)
    # Data only -- there is no code generator yet. The pointer initialiser is
    # there on purpose: it makes the unit carry a relocation, so the test
    # covers the relocation format and not just the ELF header.
    printf 'int counter = 0x123456;\nchar msg[] = "hi";\nchar *p = msg;\n' > "$obj/t.c"
    if bin/acc -c "$obj/t.c" -o "$obj/t.o" 2>"$obj/err"; then
        ok "acc compiles a data-only unit"
    else
        bad "acc compiles a data-only unit" "$(cat "$obj/err")"
    fi
    case "$("$AGONDEV/bin/ez80-none-elf-readelf" -h "$obj/t.o" 2>&1)" in
      *EZ80*ADL*) ok "object reports EZ80/ADL machine flags" ;;
      *) bad "object reports EZ80/ADL machine flags" "readelf saw no EZ80/ADL" ;;
    esac
    # agondev names C symbols with a leading underscore.
    if "$AGONDEV/bin/ez80-none-elf-nm" "$obj/t.o" 2>/dev/null | grep -q ' _counter$'; then
        ok "symbols carry agondev's leading underscore"
    else
        bad "symbols carry agondev's leading underscore" "$("$AGONDEV/bin/ez80-none-elf-nm" "$obj/t.o" 2>&1)"
    fi
    # No warnings either: a warning here means a section agondev cannot use.
    ldout=$("$AGONDEV/bin/ez80-none-elf-ld" -r "$obj/t.o" -o "$obj/t2.o" 2>&1)
    if [ -z "$ldout" ]; then
        ok "agondev ld links it without complaint"
    else
        bad "agondev ld links it without complaint" "$ldout"
    fi
    rm -rf "$obj"
else
    printf '  skip no agondev toolchain (set AGONDEV)\n'
fi

echo
echo "Agon emulator harness:"
# Self-test: the two helper programs test/agon.sh builds are the whole
# pipeline in miniature -- the MOS header, the sdcard, autoexec, and the exit
# status carried back out through IO port 0. If these do not run, nothing that
# depends on running generated code can be believed.
helpers=$(mktemp -d)
python3 test/moshdr.py ok.bin   "$helpers/ok.bin"   06 03 21 00 00 00 2b 7c b5 20 fb 10 f5 af d3 00 c9
python3 test/moshdr.py bad.bin  "$helpers/bad.bin"  06 03 21 00 00 00 2b 7c b5 20 fb 10 f5 3e 01 d3 00 c9
test/agon.sh "$helpers/ok.bin" >/dev/null 2>&1
case $? in
  0)  ok "a program that succeeds reports success" ;;
  77) printf '  skip no emulator (set ACC_EMU)\n' ;;
  *)  bad "a program that succeeds reports success" "got status $?" ;;
esac
test/agon.sh "$helpers/bad.bin" >/dev/null 2>&1
case $? in
  1)  ok "a program that fails reports failure" ;;
  77) printf '  skip no emulator (set ACC_EMU)\n' ;;
  *)  bad "a program that fails reports failure" "expected status 1" ;;
esac
rm -rf "$helpers"

echo
printf '%d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

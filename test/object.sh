#!/bin/bash
# That an object holds everything a compile knew.
#
# Each program is built twice: straight to a program, and to an object that
# is then linked into one. The two images have to be identical, byte for
# byte. That is a strong thing to ask: the object is written with the image
# based at zero and no header, so every address in it is an offset, and the
# link puts all of them somewhere else. If the text, the symbols or the
# relocations were short of anything, the two would differ -- and where they
# differ says which.
#
# It leans on test/reloc.sh having gone first. That one says the relocations
# name every address; this one says they survive a round trip through a file
# and come back pointing at the same things.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
# The output's name goes in the image header, so both sides are built under
# the same basename in different directories.
mkdir -p "$tmp/direct" "$tmp/linked"

pass=0; fail=0

for src in ${CASES:-test/cases/*.c}; do
    name=$(basename "$src" .c)

    if ! err=$("$ACC" "$src" -o "$tmp/direct/x.bin" -x 2>&1); then
        printf '  FAIL %-18s acc could not compile it\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi
    if ! err=$("$ACC" -c "$src" -o "$tmp/x.o" 2>&1); then
        printf '  FAIL %-18s it would not compile to an object\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi
    if ! err=$("$ACC" "$tmp/x.o" -o "$tmp/linked/x.bin" -x 2>&1); then
        printf '  FAIL %-18s the object would not link\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    if cmp -s "$tmp/direct/x.bin" "$tmp/linked/x.bin"; then
        pass=$((pass+1))
    else
        printf '  FAIL %-18s the linked image differs: %s\n' "$name" \
            "$(cmp "$tmp/direct/x.bin" "$tmp/linked/x.bin" 2>&1 | head -1 |
               sed 's/^cmp: //')"
        fail=$((fail+1))
    fi
done

# ------------------------------------------------------------------
# Several objects, and what an object knows about where it came from.

ok() {
    if [ "$2" = "$3" ]; then
        pass=$((pass+1))
    else
        printf '  FAIL %-32s want %s, got %s\n' "$1" "$3" "$2"
        fail=$((fail+1))
    fi
}

work=$tmp/multi; mkdir -p "$work/one" "$work/two"

# A program split in half links to the same bytes as the same program written
# as one file -- in either order, since a call forward and a call back are
# both fixups the linker settles at the end.
cat > "$work/m.c" <<'C'
int add(int a, int b);
int main(void) { return add(40, 2); }
C
cat > "$work/n.c" <<'C'
int add(int a, int b) { return a + b; }
C
cat > "$work/whole.c" <<'C'
int add(int a, int b);
int main(void) { return add(40, 2); }
int add(int a, int b) { return a + b; }
C
"$ACC" "$work/whole.c" -o "$work/one/x.bin" -x >/dev/null 2>&1
"$ACC" -c "$work/m.c" -o "$work/m.o" >/dev/null 2>&1
"$ACC" -c "$work/n.c" -o "$work/n.o" >/dev/null 2>&1
"$ACC" "$work/m.o" "$work/n.o" -o "$work/two/x.bin" -x >/dev/null 2>&1
if cmp -s "$work/one/x.bin" "$work/two/x.bin"; then
    pass=$((pass+1))
else
    printf '  FAIL %-32s two objects differ from the one file\n' "split in two"
    fail=$((fail+1))
fi

# The other order puts add first, so main's call goes backwards instead.
cat > "$work/whole2.c" <<'C'
int add(int a, int b) { return a + b; }
int main(void) { return add(40, 2); }
C
"$ACC" "$work/whole2.c" -o "$work/one/x.bin" -x >/dev/null 2>&1
"$ACC" "$work/n.o" "$work/m.o" -o "$work/two/x.bin" -x >/dev/null 2>&1
if cmp -s "$work/one/x.bin" "$work/two/x.bin"; then
    pass=$((pass+1))
else
    printf '  FAIL %-32s the other order differs\n' "split in two"
    fail=$((fail+1))
fi

# Nothing defines add.
err=$("$ACC" "$work/m.o" -o "$work/two/x.bin" -x 2>&1)
case $err in
  *"'add' is called but never defined"*) pass=$((pass+1)) ;;
  *) printf '  FAIL %-32s %s\n' "a name nothing defines" "$err"; fail=$((fail+1)) ;;
esac

# Two objects that both define it.
err=$("$ACC" "$work/n.o" "$work/n.o" -o "$work/two/x.bin" -x 2>&1)
case $err in
  *"defined in more than one object"*) pass=$((pass+1)) ;;
  *) printf '  FAIL %-32s %s\n' "a name two objects define" "$err"; fail=$((fail+1)) ;;
esac

# Something that is not an object at all.
err=$("$ACC" "$work/whole.c" -o "$work/two/x.bin" -x 2>&1 >/dev/null; \
      cp "$work/whole.c" "$work/junk.o"; \
      "$ACC" "$work/junk.o" -o "$work/two/x.bin" -x 2>&1)
case $err in
  *"not an object acc made"*) pass=$((pass+1)) ;;
  *) printf '  FAIL %-32s %s\n' "a file that is not an object" "$err"; fail=$((fail+1)) ;;
esac

# What an object was made from, and what that buys: a second compile of the
# same bytes does nothing at all. The Agon has no clock that survives being
# turned off, so this asks about the bytes and not about the time -- which is
# why touching the file changes nothing.
inc=$tmp/inc; mkdir -p "$inc"
printf '#define BONUS 2\n' > "$inc/h.h"
printf '#include "h.h"\nint main(void) { return 40 + BONUS; }\n' > "$inc/u.c"

again() { "$ACC" -c "$inc/u.c" -o "$inc/u.o" 2>&1; }

case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "the first compile"            "$got" compiled
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "the same bytes again"         "$got" skipped
touch "$inc/u.c" "$inc/h.h"
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "touched but not changed"      "$got" skipped
printf '#define BONUS 3\n' > "$inc/h.h"
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "a header changed"             "$got" compiled
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "and unchanged again"          "$got" skipped
printf '#include "h.h"\nint main(void) { return 39 + BONUS; }\n' > "$inc/u.c"
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "the source changed"           "$got" compiled

# A same-length edit, which is what a checksum is for: a size on its own
# would call this unchanged.
printf '#define BONUS 4\n' > "$inc/h.h"
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "changed, same length"         "$got" compiled

rm -f "$inc/u.o"
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "the object deleted"           "$got" compiled

# A header reached twice -- directly and through another -- is opened twice
# and read twice. One entry in the object covers it, and its marks are of one
# reading: folded twice they would be a number no reading of that file could
# give, and the build would compile again every time.
two=$tmp/twice; mkdir -p "$two"
printf '#define A 1\n' > "$two/one.h"
printf '#include "one.h"\n#define B 2\n' > "$two/two.h"
printf '#include "one.h"\n#include "two.h"\nint main(void) { return 39 + A + B; }\n' \
    > "$two/d.c"
twice() { "$ACC" -c "$two/d.c" -o "$two/d.o" 2>&1; }

case $(twice) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "a header reached twice"       "$got" compiled
case $(twice) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "and unchanged after that"     "$got" skipped
printf '#define A 7\n' > "$two/one.h"
case $(twice) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "the twice-read header edited" "$got" compiled

# A variable one file declares and another defines. Nothing is reserved for
# it where it is only declared, so every use of it is a relocation -- and the
# whole comes out as the same program written in one piece, where the
# declaration and the definition are both in the same file and the uses
# between them are relocations for the same reason.
cat > "$work/e.c" <<'C'
extern int counter;
int bump(void) { return ++counter; }
C
cat > "$work/f.c" <<'C'
int counter = 41;
int bump(void);
int main(void) { return bump(); }
C
cat > "$work/ef.c" <<'C'
extern int counter;
int bump(void) { return ++counter; }
int counter = 41;
int main(void) { return bump(); }
C
"$ACC" "$work/ef.c" -o "$work/one/x.bin" -x >/dev/null 2>&1
"$ACC" -c "$work/e.c" -o "$work/e.o" >/dev/null 2>&1
"$ACC" -c "$work/f.c" -o "$work/f.o" >/dev/null 2>&1
"$ACC" "$work/e.o" "$work/f.o" -o "$work/two/x.bin" -x >/dev/null 2>&1
if cmp -s "$work/one/x.bin" "$work/two/x.bin"; then
    pass=$((pass+1))
else
    printf '  FAIL %-32s a shared variable differs from the one file\n' \
        "a variable two objects share"
    fail=$((fail+1))
fi

# And nothing defines it at all. With a main of its own, so that what is
# missing is the variable and not the entry point.
cat > "$work/g.c" <<'C'
extern int counter;
int main(void) { return counter; }
C
"$ACC" -c "$work/g.c" -o "$work/g.o" >/dev/null 2>&1
err=$("$ACC" "$work/g.o" -o "$work/two/x.bin" -x 2>&1)
case $err in
  *"'counter'"*"never given room"*) pass=$((pass+1)) ;;
  *) printf '  FAIL %-32s %s\n' "a variable nothing defines" "$err"
     fail=$((fail+1)) ;;
esac

# The same, compiled straight to a program rather than to an object: there is
# no linker coming, so there is nothing that could ever give it an address.
err=$("$ACC" "$work/g.c" -o "$work/two/x.bin" -x 2>&1)
case $err in
  *"'counter'"*"never given room"*) pass=$((pass+1)) ;;
  *) printf '  FAIL %-32s %s\n' "extern with no linker" "$err"
     fail=$((fail+1)) ;;
esac

# split <name> <a.c> <b.c> <whole.c>: two objects against the same program
# written as one file.
#
# The pieces are written to need none of the runtime helpers -- no multiply,
# no shift, no `&` -- because every object that uses one carries its own copy
# of the blob, so two that use them do not lay out the way one file does.
# Making the helpers a library object is what settles that; until then the
# comparison keeps clear of them.
split() {
    local what=$1
    printf '%s' "$2" > "$work/s1.c"
    printf '%s' "$3" > "$work/s2.c"
    printf '%s' "$4" > "$work/sw.c"
    if ! err=$("$ACC" "$work/sw.c" -o "$work/one/x.bin" -x 2>&1); then
        printf '  FAIL %-32s as one file: %s\n' "$what" \
            "$(printf '%s' "$err" | head -1)"
        fail=$((fail+1)); return
    fi
    if ! err=$("$ACC" -c "$work/s1.c" -o "$work/s1.o" 2>&1) \
       || ! err=$("$ACC" -c "$work/s2.c" -o "$work/s2.o" 2>&1); then
        printf '  FAIL %-32s to an object: %s\n' "$what" \
            "$(printf '%s' "$err" | head -1)"
        fail=$((fail+1)); return
    fi
    if ! err=$("$ACC" "$work/s1.o" "$work/s2.o" -o "$work/two/x.bin" -x 2>&1); then
        printf '  FAIL %-32s linking: %s\n' "$what" \
            "$(printf '%s' "$err" | head -1)"
        fail=$((fail+1)); return
    fi
    if cmp -s "$work/one/x.bin" "$work/two/x.bin"; then
        pass=$((pass+1))
    else
        printf '  FAIL %-32s differs from the one file\n' "$what"
        fail=$((fail+1))
    fi
}

# An array declared with no size at all. There is nothing to reserve room by
# and nothing to say how long it is, so every use of it is a relocation --
# which is what the cell of three bytes it used to get was standing in for.
split "an array with no size" \
'extern int table[];
int pick(void) { return table[2]; }
' \
'int table[3] = { 1, 2, 42 };
int pick(void);
int main(void) { return pick(); }
' \
'extern int table[];
int pick(void) { return table[2]; }
int table[3] = { 1, 2, 42 };
int main(void) { return pick(); }
'

# An address with an amount added, worked out before the address is known:
# the amount goes in the slot and the linker adds the address to it.
split "an address with an offset" \
'extern int arr[4];
int *p = arr + 2;
int main(void) { return *p; }
' \
'int arr[4] = { 1, 2, 42, 4 };
' \
'extern int arr[4];
int *p = arr + 2;
int main(void) { return *p; }
int arr[4] = { 1, 2, 42, 4 };
'

# An extern said inside a block, with nothing of that name at file scope: it
# introduces the name and reserves nothing, as an extern anywhere does. Its
# bytes used to go in the middle of the function, with a jump over them.
split "an extern inside a block" \
'int use(void) { extern int hidden; return hidden; }
' \
'int hidden = 42;
int use(void);
int main(void) { return use(); }
' \
'int use(void) { extern int hidden; return hidden; }
int hidden = 42;
int main(void) { return use(); }
'

# A variable one file declares const and another defines.
split "a const variable shared" \
'extern const int limit;
int under(int n) { return n < limit; }
' \
'const int limit = 50;
int under(int n);
int main(void) { return under(1) ? 42 : 0; }
' \
'extern const int limit;
int under(int n) { return n < limit; }
const int limit = 50;
int main(void) { return under(1) ? 42 : 0; }
'

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

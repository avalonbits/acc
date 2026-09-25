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

# The options that change what a compile reads. None of the files did, so
# an object that recorded only them was called up to date after a -D that
# changed what the program says.
with() { "$ACC" -c "$inc/u.c" -o "$inc/u.o" "$@" 2>&1; }

case $(with -DEXTRA=1) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "a -D given"                   "$got" compiled
case $(with -DEXTRA=1) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "the same -D again"            "$got" skipped
case $(with -DEXTRA=2) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "its value changed"            "$got" compiled
case $(with -DEXTRA=2 -UEXTRA) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "a -U after it"                "$got" compiled
case $(with -I"$inc") in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "a -I instead"                 "$got" compiled
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "and none"                     "$got" compiled
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "and none again"               "$got" skipped

# An object made by a different acc. The program's own files are untouched,
# which during acc's own development is exactly the case that matters: the
# compiler changed and everything it made is out of date.
#
# Poked rather than made by building a second compiler: byte 4 of an object is
# the low byte of which acc made it, and byte 3 is the version of the format.
poke() {                                # poke <file> <offset> <byte>
    printf "$(printf '\\%03o' "$3")" \
        | dd of="$1" bs=1 seek="$2" conv=notrunc status=none
}

case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "unchanged before the poking"  "$got" skipped
poke "$inc/u.o" 4 "$(( $(od -An -tu1 -j4 -N1 "$inc/u.o") ^ 1 ))"
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "made by another acc"          "$got" compiled
poke "$inc/u.o" 3 99
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "an older format"              "$got" compiled
case $(again) in *"up to date"*) got=skipped ;; *) got=compiled ;; esac
ok "and current once more"        "$got" skipped

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
int pick(int i) { return table[i]; }
' \
'int table[3] = { 1, 2, 42 };
int pick(int i);
int main(void) { return pick(2); }
' \
'extern int table[];
int pick(int i) { return table[i]; }
int table[3] = { 1, 2, 42 };
int main(void) { return pick(2); }
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

# A static function is this file's alone, so two objects may each have one
# of that name -- which is what a `static inline` in a header gives every
# file that includes it. Neither is offered to the other, and each calls its
# own. Not compared against the same program written as one file, because
# there it would be one function and here it is two.
cat > "$work/sa.c" <<'C'
static int helper(int n) { return n + 1; }
int a(void) { return helper(20); }
C
cat > "$work/sb.c" <<'C'
static int helper(int n) { return n + 1; }
int a(void);
int main(void) { return a() + helper(20); }
C
"$ACC" -c "$work/sa.c" -o "$work/sa.o" >/dev/null 2>&1
"$ACC" -c "$work/sb.c" -o "$work/sb.o" >/dev/null 2>&1
if err=$("$ACC" "$work/sa.o" "$work/sb.o" -o "$work/two/x.bin" -x 2>&1); then
    if emu_available >/dev/null 2>&1; then
        test/agon.sh "$work/two/x.bin" >/dev/null 2>&1
        ok "a static of the same name" "$?" 42
    else
        pass=$((pass + 1))
    fi
else
    printf '  FAIL %-32s %s\n' "a static of the same name" \
        "$(printf '%s' "$err" | head -1)"
    fail=$((fail + 1))
fi


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

# Both halves multiply by something that is not known until it runs, which is
# a call to a helper -- a constant multiplier is written out as doublings and
# would not reach the runtime at all. One copy of the runtime goes into the
# program, not one into each object, which is what lets this come out as the
# same bytes as the one file.
split "both halves use a helper" \
'int times(int n, int m) { return n * m; }
' \
'int times(int n, int m);
int main(void) { return times(3, 7); }
' \
'int times(int n, int m) { return n * m; }
int main(void) { return times(3, 7); }
'

# And that it really is a name rather than a copy: the object that multiplies
# says what it wants and does not carry the runtime, which is five kilobytes
# of it.
printf 'int times(int n, int m) { return n * m; }\n' > "$work/h.c"
"$ACC" -c "$work/h.c" -o "$work/h.o" >/dev/null 2>&1
if grep -q acc_rt_mul "$work/h.o" 2>/dev/null; then
    pass=$((pass+1))
else
    printf '  FAIL %-32s no name for the helper it uses\n' "a helper named, not copied"
    fail=$((fail+1))
fi
if [ "$(wc -c < "$work/h.o")" -lt 1024 ]; then
    pass=$((pass+1))
else
    printf '  FAIL %-32s %s bytes, so the runtime is in it\n' \
        "an object without the runtime" "$(wc -c < "$work/h.o")"
    fail=$((fail+1))
fi

# A long long, whose routines are in the part of the blob that is only laid
# down when something asks for it.
split "a helper past the split" \
'long long wide(long long n) { return n * 6; }
' \
'long long wide(long long n);
int main(void) { return (int) wide(7); }
' \
'long long wide(long long n) { return n * 6; }
int main(void) { return (int) wide(7); }
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

# A file with nothing in it, and one with only a variable at zero: neither
# has any text, and so neither has an item. The writer used to give each an
# item at 0 anyway, which is not inside the text, and the link refused the
# object as having its items out of order.
split "a file with nothing in it" \
'' \
'int main(void) { return 42; }
' \
'int main(void) { return 42; }
'

# The variable's place in the bss differs from where one file would put it,
# so this one is run rather than compared.
printf 'int shared;\n' > "$work/b1.c"
printf 'extern int shared;\nint main(void) { shared = 40; return shared + 2; }\n' > "$work/b2.c"
if "$ACC" -c "$work/b1.c" -o "$work/b1.o" >/dev/null 2>&1 \
   && "$ACC" -c "$work/b2.c" -o "$work/b2.o" >/dev/null 2>&1 \
   && "$ACC" "$work/b2.o" "$work/b1.o" -o "$work/b.bin" -x >/dev/null 2>&1; then
    test/agon.sh "$work/b.bin" >/dev/null 2>&1
    rc=$?
    if [ "$rc" -eq 42 ] || [ "$rc" -eq 77 ]; then
        pass=$((pass+1))
    else
        printf '  FAIL %-32s returned %d\n' "a file with only a variable" "$rc"
        fail=$((fail+1))
    fi
else
    printf '  FAIL %-32s did not link\n' "a file with only a variable"
    fail=$((fail+1))
fi

# -map: every function and variable the object holds, static ones too, by
# name, each at its offset and running to the next -- so that, together,
# they are the whole of the object's text. test/size.sh reads it to set
# acc's functions beside agondev's.
cat > "$tmp/mapped.c" <<'EOF'
static int counter = 3;
int shared[4] = { 1, 2, 3, 4 };
static int helper(int x) { return x * counter; }
int visible(int x) { return helper(x) + shared[x & 3]; }
EOF
if "$ACC" -c "$tmp/mapped.c" -o "$tmp/mapped.o" -map "$tmp/mapped.map" >/dev/null 2>&1; then
    names=$(awk '{ print $1 }' "$tmp/mapped.map" | sort | tr '\n' ' ')
    total=$(awk '{ t += $3 } END { print t }' "$tmp/mapped.map")
    text=$(python3 test/perf/objsize.py acc-text "$tmp/mapped.o")
    ok "-map names statics too"        "$names" "counter helper shared visible "
    ok "-map covers the whole text"    "$total" "$text"
else
    ok "-map writes a map"             "it failed" "a map"
fi

# -map on a link: where each item went. Checked against the objects: an
# item is as long in the image as its object's own map says, the items
# come one after another in the order of the image, and at the address
# the map gives an item is its object's bytes -- `twice`, which holds no
# address to be moved. The runtime is listed when the program calls it.
cat > "$tmp/la.c" <<'EOF2'
static int sq(int x) { return x * x; }
int twice(int x);
int main(void) { return sq(twice(3)) / 7; }
EOF2
printf 'int twice(int x) { return x + x; }\n' > "$tmp/lb.c"
if "$ACC" -c "$tmp/la.c" -o "$tmp/la.o" -map "$tmp/la.map" >/dev/null 2>&1 \
   && "$ACC" -c "$tmp/lb.c" -o "$tmp/lb.o" -map "$tmp/lb.map" >/dev/null 2>&1 \
   && "$ACC" "$tmp/la.o" "$tmp/lb.o" -o "$tmp/l.bin" -map "$tmp/l.map" >/dev/null 2>&1; then
    got=$(python3 - "$tmp" <<'PY'
import sys
t = sys.argv[1]

def n3(d, at):
    return d[at] | d[at + 1] << 8 | d[at + 2] << 16

def text(path):                 # an object's text: see src/obj.c
    d = open(path, 'rb').read()
    at = 31 + n3(d, 13) * 7 + n3(d, 16) * 6 + n3(d, 28) * 9 + n3(d, 19) * 12
    at += n3(d, 25) * 3 + n3(d, 22)
    return d[at:at + n3(d, 7)]

own = {}
for stem in ('la', 'lb'):
    for line in open('%s/%s.map' % (t, stem)):
        name, off, size = line.split()
        own[('%s/%s.o' % (t, stem), int(off))] = int(size)
image = open(t + '/l.bin', 'rb').read()
rows = [l.split() for l in open(t + '/l.map')]
out = []
end = 0
for at, size, name, obj, off in rows:
    at, size, off = int(at, 16), int(size), int(off)
    if at < end:
        out.append('overlap at %s' % name)
    end = at + size
    if obj != '(runtime)' and own.get((obj, off)) != size:
        out.append('%s is %d, its object says %s' % (name, size, own.get((obj, off))))
    if name == 'twice':
        body = image[at - 0x40000:at - 0x40000 + size]
        out.append('twice ' + ('matches' if body == text(obj)[off:off + size] else 'differs'))
names = [r[2] for r in rows if r[3] != '(runtime)']
out.append(' '.join(names))
out.append('runtime' if any(r[2] == 'acc_rt_mul' for r in rows) else 'no runtime')
print('; '.join(out))
PY
)
    ok "-map on a link"                "$got" "twice matches; - main twice; runtime"
else
    ok "-map on a link writes a map"   "it failed" "a map"
fi

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

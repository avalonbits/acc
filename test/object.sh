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

# A variable another file has to define: refused rather than built wrong.
printf 'extern int counter;\nint bump(void) { return ++counter; }\n' > "$work/e.c"
err=$("$ACC" -c "$work/e.c" -o "$work/e.o" 2>&1)
case $err in
  *"declared extern and never given a value here"*) pass=$((pass+1)) ;;
  *) printf '  FAIL %-32s %s\n' "extern with no definition" "$err"; fail=$((fail+1)) ;;
esac

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

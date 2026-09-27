#!/bin/bash
# #include: where a file is looked for, and what is said when it is not there.
#
# The differential tests cover what an included file *means* -- it is the
# same program however it was spelled across files, and agondev agrees. What
# they cannot cover is the searching: which directory a quoted name is tried
# in and an angled one is not, what a diagnostic from inside a header says,
# and that the line numbers go back to the includer's when it ends. Those are
# this file's job, and none of them needs an emulator.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/src/sub" "$tmp/elsewhere" "$tmp/out"

pass=0; fail=0

ok() { pass=$((pass + 1)); }
bad() { printf '  FAIL %-34s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# Compiles, and the image is made. $1 names the check, the rest are arguments.
compiles() {
    local what=$1; shift
    if out=$("$ACC" "$@" -o "$tmp/out/a.bin" 2>&1); then
        ok
    else
        bad "$what" "$(printf '%s' "$out" | head -1)"
    fi
}

# Refused, with a message matching $2.
refuses() {
    local what=$1 want=$2; shift 2
    if out=$("$ACC" "$@" -o "$tmp/out/a.bin" 2>&1); then
        bad "$what" "it was accepted"
    elif ! printf '%s' "$out" | grep -q -- "$want"; then
        bad "$what" "$(printf '%s' "$out" | head -1)"
    else
        ok
    fi
}

# --- a header beside the source, and one below it ---------------------
cat > "$tmp/src/sub/deep.h" <<'EOF'
int deeper(int x) { return x * 2; }
EOF
cat > "$tmp/src/hdr.h" <<'EOF'
#include "sub/deep.h"
int helper(int x) { return x + 1; }
EOF
cat > "$tmp/src/main.c" <<'EOF'
#include "hdr.h"
int main(void) { return deeper(helper(20)); }
EOF
compiles "a quoted header beside the source" "$tmp/src/main.c"

# The same, from a directory that is not the working one: a quoted name is
# looked for beside the file that asked for it, not beside the compiler.
compiles "a header found from another directory" "$tmp/src/main.c"

# --- angled names do not look beside the source -----------------------
cat > "$tmp/elsewhere/lib.h" <<'EOF'
int from_a_path(void) { return 7; }
EOF
cat > "$tmp/src/angled.c" <<'EOF'
#include <lib.h>
int main(void) { return from_a_path(); }
EOF
refuses "an angled name is not looked for beside the source" \
    "cannot find 'lib.h'" "$tmp/src/angled.c"
compiles "an angled name is looked for in -I" \
    "$tmp/src/angled.c" -I "$tmp/elsewhere"

# A quoted name falls back to the -I directories when it is not beside the
# file that asked for it.
cat > "$tmp/src/quoted_far.c" <<'EOF'
#include "lib.h"
int main(void) { return from_a_path(); }
EOF
compiles "a quoted name falls back to -I" \
    "$tmp/src/quoted_far.c" -I "$tmp/elsewhere"

# And the source's own directory wins over -I, so a header next to the file
# is the one that is used.
cat > "$tmp/src/lib.h" <<'EOF'
int from_a_path(void) { return 42; }
EOF
cat > "$tmp/src/which.c" <<'EOF'
#include "lib.h"
int main(void) { return from_a_path(); }
EOF
cat > "$tmp/src/direct.c" <<'EOF'
int from_a_path(void) { return 42; }
int main(void) { return from_a_path(); }
EOF

# The output's name goes into the image's header, so the two sides are built
# under the same basename in different directories to be comparable at all.
mkdir -p "$tmp/out/one" "$tmp/out/two"
"$ACC" "$tmp/src/which.c" -I "$tmp/elsewhere" -o "$tmp/out/one/x.bin" \
    >/dev/null 2>&1
"$ACC" "$tmp/src/direct.c" -o "$tmp/out/two/x.bin" >/dev/null 2>&1
if cmp -s "$tmp/out/one/x.bin" "$tmp/out/two/x.bin"; then
    ok
else
    bad "the source's own directory wins over -I" "a different header was used"
fi

# --- what a diagnostic says -------------------------------------------
cat > "$tmp/src/bad.h" <<'EOF'
int fine(void) { return 1; }
int broken(void) { return @; }
EOF
cat > "$tmp/src/usebad.c" <<'EOF'
#include "bad.h"
int main(void) { return 0; }
EOF
refuses "an error inside a header names the header" \
    "bad.h:2:27: error:" "$tmp/src/usebad.c"

# And when the header ends, the lines are the includer's again.
cat > "$tmp/src/after.c" <<'EOF'
#include "hdr.h"


int main(void) { return @; }
EOF
refuses "the line after an include is the includer's" \
    "after.c:4:25: error:" "$tmp/src/after.c"

# --- how deep it goes --------------------------------------------------
# A file that includes itself is the shortest way to the bottom.
cat > "$tmp/src/loop.h" <<'EOF'
#include "loop.h"
EOF
cat > "$tmp/src/loop.c" <<'EOF'
#include "loop.h"
int main(void) { return 0; }
EOF
refuses "a header that includes itself stops" \
    "nested more than" "$tmp/src/loop.c"

# --- the directive itself ---------------------------------------------
printf '#include "nothing_here.h"\nint main(void) { return 0; }\n' \
    > "$tmp/src/missing.c"
refuses "a file that is not there" "cannot find" "$tmp/src/missing.c"

printf '#include hdr.h\nint main(void) { return 0; }\n' > "$tmp/src/bare.c"
refuses "a name with no quotes" "needs \"a file\"" "$tmp/src/bare.c"

printf '#include "hdr.h\nint main(void) { return 0; }\n' > "$tmp/src/unclosed.c"
refuses "a name that is not closed" "is not closed" "$tmp/src/unclosed.c"

printf '#include ""\nint main(void) { return 0; }\n' > "$tmp/src/empty.c"
refuses "an empty name" "needs a file name" "$tmp/src/empty.c"

printf '#include "hdr.h" and more\nint main(void) { return 0; }\n' \
    > "$tmp/src/junk.c"
refuses "anything after the name" "and nothing else" "$tmp/src/junk.c"

printf '#nonsense\nint main(void) { return 0; }\n' > "$tmp/src/unknown.c"
refuses "a directive acc does not know" "is not a directive" \
    "$tmp/src/unknown.c"

# A '#' on a line of its own is allowed and does nothing.
printf '#\nint main(void) { return 0; }\n' > "$tmp/src/hash.c"
compiles "a '#' alone" "$tmp/src/hash.c"

# A '#' that is not first on its line is not a directive.
printf 'int main(void) { return 0 # 1; }\n' > "$tmp/src/mid.c"
refuses "a '#' in the middle of a line" "stray" "$tmp/src/mid.c"

# --- a conditional belongs to the file it is written in ---------------
# A header that leaves an #if open would otherwise swallow the rest of the
# file that included it, and one with a stray #endif would close a group
# its caller opened. Both are mistakes C does not allow, and neither shows
# up as anything sensible if it is let through.
cat > "$tmp/src/left_open.h" <<'EOF'
#ifdef NOT_DEFINED
EOF
cat > "$tmp/src/open.c" <<'EOF'
#include "left_open.h"
int main(void) { return 42; }
EOF
refuses "a header that leaves an #if open" "#endif" "$tmp/src/open.c"

cat > "$tmp/src/stray_endif.h" <<'EOF'
int from_the_header(void) { return 1; }
#endif
EOF
cat > "$tmp/src/stray.c" <<'EOF'
#define TAKEN 1
#ifdef TAKEN
#include "stray_endif.h"
int main(void) { return 42; }
#endif
EOF
refuses "a header with an #endif of its caller's" \
    "close an #if in the file" "$tmp/src/stray.c"

# What it must not do is refuse a header that balances its own, which is
# every include guard ever written -- and the same header twice.
cat > "$tmp/src/guard.h" <<'EOF'
#ifndef GUARD_H
#define GUARD_H
int guarded(void) { return 42; }
#endif
EOF
cat > "$tmp/src/guarded.c" <<'EOF'
#include "guard.h"
#include "guard.h"
int main(void) { return guarded(); }
EOF
compiles "an include guard, and the header twice" "$tmp/src/guarded.c"

# --- as deep as it goes -----------------------------------------------
# Every level holds a window and, while it runs, the handle of the file
# that included it is set aside -- MOS has few of them. A chain to the
# limit is what says both of those work.
{
    i=1
    while [ $i -lt 8 ]; do
        printf '#include "deep%d.h"\nint level%d(void) { return %d; }\n' \
            $((i + 1)) "$i" "$i" > "$tmp/src/deep$i.h"
        i=$((i + 1))
    done
    printf 'int level8(void) { return 8; }\n' > "$tmp/src/deep8.h"
}
cat > "$tmp/src/deep.c" <<'EOF'
#include "deep1.h"
int main(void) {
    return level1() + level2() + level3() + level4()
         + level5() + level6() + level7() + level8() + 6;
}
EOF
compiles "a chain as deep as it goes" "$tmp/src/deep.c"

# --- a file whose handle is still open when it includes ---------------
# The handle of the file doing the including is closed while the included
# one runs, and opened again afterwards at the byte it had reached. A small
# file never exercises that -- it has already been read to its end by the
# time the include happens -- so this one is longer than the window, which
# leaves it mid-file with the handle open, and has more to read afterwards.
#
# Without the seek back, the file carries on from its own beginning and
# what comes after the include is read twice.
# The include comes near the top: what makes the handle still be open is
# that most of the file is still to come, not that most of it has been read.
#
# The padding is function definitions rather than comments on purpose. With
# comments, a file read a second time is swallowed by whatever comment was
# half-scanned at the join and the damage hides; a definition that arrives
# twice is refused, so the mistake is seen.
{
    printf 'int before(void) { return 20; }\n'
    printf '#include "small.h"\n'
    i=0
    while [ $i -lt 600 ]; do
        printf 'int pad_%03d(void) { return %d; }\n' $i $i
        i=$((i + 1))
    done
    printf 'int after(void) { return 15; }\n'
    printf 'int main(void) { return before() + middle() + after(); }\n'
} > "$tmp/src/long.c"
cat > "$tmp/src/small.h" <<'EOF'
int middle(void) { return 7; }
EOF
compiles "an include from a file bigger than its window" "$tmp/src/long.c"

# The same program with the header written out where it was included: the
# two have to compile to the same image, which is what says the seek landed
# on the right byte rather than merely somewhere legal.
{
    sed 's|#include "small.h"|int middle(void) { return 7; }|' "$tmp/src/long.c"
} > "$tmp/src/long_flat.c"
"$ACC" "$tmp/src/long.c" -o "$tmp/out/one/z.bin" >/dev/null 2>&1
"$ACC" "$tmp/src/long_flat.c" -o "$tmp/out/two/z.bin" >/dev/null 2>&1
if cmp -s "$tmp/out/one/z.bin" "$tmp/out/two/z.bin"; then
    ok
else
    bad "the file carries on where it left off" "the images differ"
fi

# --- and that the result is the same however it was split up ----------
# The same program in one file and in three has to compile to the same
# image: an include is not allowed to change what the program means.
cat > "$tmp/src/whole.c" <<'EOF'
int deeper(int x) { return x * 2; }
int helper(int x) { return x + 1; }
int main(void) { return deeper(helper(20)); }
EOF
"$ACC" "$tmp/src/whole.c" -o "$tmp/out/one/y.bin" >/dev/null 2>&1
"$ACC" "$tmp/src/main.c" -o "$tmp/out/two/y.bin" >/dev/null 2>&1
if cmp -s "$tmp/out/one/y.bin" "$tmp/out/two/y.bin"; then
    ok
else
    bad "split across files is the same image" "the two differ"
fi

# _Pragma("once"), C99's operator form of the directive, in a header whose
# one definition would be a second one if it were read twice -- and from
# a macro, which is what the operator is for: a directive cannot come out
# of one.
cat > "$tmp/src/op_once.h" <<'EOF'
_Pragma("once")
int only_once = 20;
EOF
cat > "$tmp/src/op_macro.h" <<'EOF'
#define ONCE _Pragma("once")
ONCE
int only_once_too = 22;
EOF
cat > "$tmp/src/op_main.c" <<'EOF'
#include "op_once.h"
#include "op_once.h"
#include "op_macro.h"
#include "op_macro.h"
int main(void) { return only_once + only_once_too; }
EOF
compiles "_Pragma(\"once\") read once" "$tmp/src/op_main.c"

# A standard header declares the names C99 gives it and no others (7.1.3):
# after <stdio.h>, ptrdiff_t, wchar_t and offsetof are still the
# program's, where the headers used to bring all of <stddef.h> with them.
# <stdlib.h> and <wchar.h> are given wchar_t, and <stddef.h> itself all
# of them, so those are left out.
cat > "$tmp/src/names.c" <<'EOF'
#include <inttypes.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
typedef long ptrdiff_t;
typedef int wchar_t;
static int offsetof(int x) { return x; }
int main(void) { return offsetof((int) sizeof(ptrdiff_t) + (int) sizeof(wchar_t)) + (NULL != 0) + (int) sizeof(size_t); }
EOF
compiles "headers leave <stddef.h>'s other names" -Iinclude "$tmp/src/names.c"

# Every header acc ships says #pragma once before anything else, so a
# second #include of it is not opened at all: <agon/vdp.h>'s headers each
# include <stdint.h> and <agon/mos.h> again, and read through to their
# #endif those were a fifth of compiling a program that uses them. Except
# <assert.h>, which C has read again on every include (7.2).
once_first() {    # what comes first in the file, comments aside
    python3 -c '
import re, sys
t = re.sub(r"/\*.*?\*/", "", open(sys.argv[1]).read(), flags=re.S)
t = re.sub(r"//[^\n]*", "", t)
print(t.split("\n", 1)[0] if t.strip() == "" else t.strip().split("\n", 1)[0])
' "$1"
}
for h in $(find include -name '*.h' | sort); do
    first=$(once_first "$h")
    if [ "$h" = include/assert.h ]; then
        if grep -q 'pragma once' "$h"; then
            bad "assert.h is read again" "it says #pragma once"
        else
            ok
        fi
    elif [ "$first" = "#pragma once" ]; then
        ok
    else
        bad "#pragma once first in $h" "it starts: $first"
    fi
done

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

#!/bin/bash
# What fits in the Agon's heap, and what happens when something does not:
# acc as built for the Agon, compiling on the emulator.
#
# A program a user reported ran out of memory built in one step, with half
# the heap free: the link that followed the compile wanted blocks -- a
# library's index, its members, a symbol table doubling -- that the compile
# had left it no piece big enough for. Here:
#
#   - test/oom/tool.c, a program of the same size and shape, built in one
#     step and run;
#   - every axis of what a compile holds -- functions, one function's
#     statements, globals, strings, macros, members, a switch's cases,
#     nesting, locals, an expression, the library -- at a size it has to
#     build, in one step and as an object and a link (test/oom/gen.py makes
#     each);
#   - a program of little code reaching much of the library, its headers'
#     names compacted for the link that follows the compile in one step: at
#     80 functions it runs out of memory linking without that;
#   - each of four axes well past what fits, which has to fail as running
#     out of memory, saying so, and leave no output, no object and no file
#     of the image behind;
#   - and after all of those, a small program built and run: MOS keeps a
#     program's files open after it ends, and acc that stopped on an error
#     with its files open left none for the next one after a few.
#
# The sizes are below what fits by a margin, so that this says when a
# change takes room away; what each axis's limit is was measured, and is in
# the commit that brought this in.
#
#   test/oom.sh [acc.bin]               # default bin/acc.bin
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${1:-bin/acc.bin}

emu_available || exit 77
[ -f "$ACC" ] || { echo "no $ACC -- run make -f Makefile.agon" >&2; exit 2; }
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }

sd=$(emu_card); trap 'rm -rf "$sd"' EXIT
cp "$ACC" "$sd/bin/acc.bin"
mkdir -p "$sd/lib/acc"
cp bin/libc.a bin/rt.a "$sd/lib/acc/"
cp -r include "$sd/lib/acc/include"

pass=0; fail=0
ok()  { pass=$((pass + 1)); }
bad() { printf '  FAIL %-28s %s\n' "$1" "$2"; fail=$((fail + 1)); }

: > "$sd/autoexec.txt"
say() { printf '%s\r\n' "$@" >> "$sd/autoexec.txt"; }

# The reported shape, in one step, and run.
cp test/oom/tool.c "$sd/tool.c"
say "echo BEGIN tool" "try acc tool.c" "try tool --help" "echo END tool"

# name axis n how: how is `one` (acc it.c), `two` (acc -c it.c, then a link
# of it.o) or `over` (in one step, and it has to run out of memory).
cases="
functions 800 one
functions 800 two
longfunc 800 one
longfunc 800 two
globals 2000 one
globals 2000 two
strings 2000 one
strings 2000 two
macros 1000 one
macros 1000 two
members 3000 one
members 3000 two
switch 1000 one
switch 1000 two
nesting 127 one
nesting 127 two
locals 800 one
locals 800 two
expr 800 one
expr 800 two
libcalls 10 two
reach 80 one
macros 8000 over
longfunc 3200 over
globals 8000 over
strings 8000 over
"
while read -r axis n how; do
    [ -n "$axis" ] || continue
    name=${axis}_${n}_$how
    python3 test/oom/gen.py "$axis" "$n" > "$sd/$name.c"
    say "echo BEGIN $name"
    if [ "$how" = two ]; then
        say "try acc -c $name.c" "try acc $name.o"
    else
        say "try acc $name.c"
    fi
    say "echo END $name"
done <<<"$cases"

# And last, after every failure above: acc can still open its files.
printf '#include <stdio.h>\nint main(void) { puts("still here"); return 0; }\n' > "$sd/after.c"
say "echo BEGIN after" "try acc after.c" "try after" "echo END after"

ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=${OOM_TIMEOUT:-900} emu_run "$sd" -z -u 2>&1 \
    | tr -d '\r' > "$sd/out.txt"

# What a case printed, between its markers.
said() { sed -n "/^BEGIN $1\$/,/^END $1\$/p" "$sd/out.txt"; }

if said tool | grep -q '^tool - read records' && [ -f "$sd/tool.bin" ]; then
    ok
else
    bad "tool.c in one step" "$(said tool | grep -m1 error)"
fi

while read -r axis n how; do
    [ -n "$axis" ] || continue
    name=${axis}_${n}_$how
    out=$(said "$name")
    if [ -z "$out" ]; then
        bad "$name" "never ran: the emulator stopped before it"
    elif [ "$how" != over ]; then
        if [ -f "$sd/$name.bin" ]; then
            ok
        else
            bad "$name" "$(grep -m1 error <<<"$out")"
        fi
    elif ! grep -q 'error: out of memory for ' <<<"$out"; then
        bad "$name runs out" "it said: $(grep -m1 -E 'error|Done' <<<"$out")"
    else
        left=""
        for f in "$name.bin" "$name.o" "$name.bin~" "$name.o~"; do
            [ -e "$sd/$f" ] && left="$left $f"
        done
        if [ -n "$left" ]; then
            bad "$name leaves nothing" "left:$left"
        else
            ok
        fi
    fi
done <<<"$cases"

if said after | grep -q '^still here'; then
    ok
else
    bad "a program after the failures" "$(said after | grep -m1 error)"
fi

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

#!/bin/bash
# Variables that start at zero, which do not take the zeros in the file.
#
# `int big[1000];` was three thousand bytes of zeros to read off the card.
# Now it is nothing: the variable is given an address past the image's last
# byte, and the program clears what is there before main runs.
#
# Two things to say. What it costs in the file is measured here directly, and
# what the program does about it is read out of the image by test/bss.py --
# which routine it calls, where it clears from and how much.
#
# And then watched happening, which is the part that needs care: a program
# run on a fresh machine finds zeros whether anything cleared them or not. So
# two programs on one boot. The first fills a large area with a pattern and
# returns to MOS; the second is laid out to sit inside that area and has to
# find zeros. Without the clearing it finds the first one's pattern.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok() {
    if [ "$2" = "$3" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-36s want %s, got %s\n' "$1" "$3" "$2"
        fail=$((fail + 1))
    fi
}

# clears <name> <bytes> <source>
clears() {
    local what=$1 bytes=$2 err why

    printf '%s' "$3" > "$tmp/b.c"
    if ! err=$("$ACC" "$tmp/b.c" -o "$tmp/b.bin" -x 2>&1); then
        printf '  FAIL %-36s %s\n' "$what" "$(printf '%s' "$err" | head -1)"
        fail=$((fail + 1)); return
    fi
    if why=$(test/bss.py "$tmp/b.bin" "$bytes"); then
        pass=$((pass + 1))
    else
        printf '  FAIL %-36s %s\n' "$what" "$why"
        fail=$((fail + 1))
    fi
}

# ------------------------------------------------------------------
# What it costs in the file.

printf 'int big[1000];\nint main(void) { big[3] = 1; return big[3]; }\n' \
    > "$tmp/zero.c"
printf 'int big[1000] = { 1 };\nint main(void) { big[3] = 1; return big[3]; }\n' \
    > "$tmp/data.c"
"$ACC" "$tmp/zero.c" -o "$tmp/zero.bin" -x >/dev/null 2>&1
"$ACC" "$tmp/data.c" -o "$tmp/data.bin" -x >/dev/null 2>&1
zero=$(wc -c < "$tmp/zero.bin"); data=$(wc -c < "$tmp/data.bin")

ok "a thousand ints cost nothing"   "$(( zero < 400 ))" 1
ok "and with a value they cost it"  "$(( data - zero > 2900 ))" 1

# ------------------------------------------------------------------
# And that the program clears exactly them.

clears "nothing starts at zero" 0 \
'int main(void) { return 42; }
'
clears "one int" 3 \
'int n;
int main(void) { return n + 42; }
'
# One byte on its own, which is the shape that needs no ldir: one of bc = 0
# is sixteen megabytes on this chip, and cleared the machine out from under
# the program.
clears "one byte" 1 \
'_Bool flag;
int main(void) { return flag + 42; }
'
clears "an array" 3000 \
'int big[1000];
int main(void) { return big[7] + 42; }
'
clears "a struct and a char" 4 \
'struct box { int n; };
struct box b;
char c;
int main(void) { return b.n + c + 42; }
'
# One with a value takes its bytes in the file; the other still starts at
# zero, so only the second is cleared.
clears "one of each" 3 \
'int given = 1;
int not_given;
int main(void) { return given + not_given + 41; }
'
# A variable said twice and given a value the second time has its bytes in
# the file, because by the end of the file it had a value -- but the room the
# first mention was given in the bss stays reserved, and is cleared along
# with everything else. Three bytes of an area that is all zeros anyway, in
# exchange for every use of a variable that does start at zero folding.
clears "said twice, then given a value" 3 \
'int x;
int x = 42;
int main(void) { return x; }
'
# And one said twice and never given a value is, once.
clears "said twice and never given one" 3 \
'int x;
int x;
int main(void) { return x + 42; }
'
# Shapes whose size is not the plain one: an array of arrays, and an array of
# structs. Three bytes an int, so six ints is eighteen; six bytes a pair of
# them, so four pairs is twenty-four.
clears "an array of arrays" 18 \
'int grid[2][3];
int main(void) { return grid[1][2] + 42; }
'
clears "an array of structs" 24 \
'struct pair { int a, b; };
struct pair v[4];
int main(void) { return v[3].b + 42; }
'

# A block's static, which has no name outside its function and so no symbol
# to be reached by -- only its offset, which is all it needs.
clears "a block's static" 3 \
'int bump(void) { static int n; return ++n; }
int main(void) { bump(); return bump() + 40; }
'
# One with a value takes its bytes in the file, as any variable with a value
# does.
clears "a block's static with a value" 0 \
'int bump(void) { static int n = 20; return ++n; }
int main(void) { bump(); return bump() + 20; }
'
# Two of the same name in two functions are two variables, and get two
# places.
clears "statics in two functions" 6 \
'int a(void) { static int n; return ++n; }
int b(void) { static int n; return ++n; }
int main(void) { return a() + b() + 40; }
'
# And a big one, which is the shape the whole thing is for.
clears "a big one in a block" 4096 \
'int fill(int i) { static char buf[4096]; buf[i] = 1; return buf[i]; }
int main(void) { return fill(9) + 41; }
'

# An enum constant's value is its own, and a negative one looks exactly like
# a variable that has not been given an address yet.
clears "a negative enum constant" 0 \
'enum { BELOW = -1, ABOVE = 1 };
int main(void) { return ABOVE - BELOW + 40; }
'
# And one beside a variable that really does start at zero.
clears "a negative enum and a variable" 3 \
'enum { BELOW = -1 };
int n;
int main(void) { return n - BELOW + 41; }
'

# What another file defines is nobody's to clear here.
clears "what another file defines" 3 \
'extern int theirs;
int mine;
int main(void) { return mine + 42; }
'
# And said extern first, then defined here, it is the file's after all: it
# has a value, so it takes its bytes in the file rather than starting at zero.
clears "extern first, then given a value" 0 \
'extern int ours;
int ours = 42;
int main(void) { return ours; }
'
# Said extern first and then declared without it, with no value: this file
# does define it, and it starts at zero.
clears "extern first, then declared here" 3 \
'extern int ours;
int ours;
int main(void) { return ours + 42; }
'

# ------------------------------------------------------------------
# And watched happening, over memory that was dirty first.

if emu_available >/dev/null 2>&1; then
    # The first program's area starts at its own image's end and runs on for
    # thirty-two kilobytes; the second's is four, and starts a little later
    # because its image is a little longer. So the second sits inside the
    # first, and every byte it looks at was written by the first.
    #
    # The first returns to MOS so that the second gets to run; only the
    # second stops the machine, with its answer.
    cat > "$tmp/dirty.c" <<'C'
char area[32768];

int main(void) {
    int i;

    for (i = 0; i < 32768; i++)
        area[i] = 0x5a;

    return 0;
}
C
    cat > "$tmp/check.c" <<'C'
char area[4096];

int main(void) {
    int i, dirty = 0;

    for (i = 0; i < 4096; i++)
        if (area[i])
            dirty++;

    return dirty ? 1 : 42;
}
C
    if "$ACC" "$tmp/dirty.c" -o "$tmp/dirty.bin" >/dev/null 2>&1 \
       && "$ACC" "$tmp/check.c" -o "$tmp/check.bin" -x >/dev/null 2>&1; then
        sd=$(emu_card)
        cp "$tmp/dirty.bin" "$sd/bin/d.bin"
        cp "$tmp/check.bin" "$sd/bin/c.bin"
        printf 'd\r\nc\r\n' > "$sd/autoexec.txt"
        ACC_EMU_TIMEOUT=${ACC_EMU_TIMEOUT:-180} emu_run "$sd" -z -u >/dev/null 2>&1
        got=$?
        rm -rf "$sd"
        ok "zero, over memory that was dirty" "$got" 42
    else
        printf '  FAIL %-36s they would not compile\n' "the two programs"
        fail=$((fail + 1))
    fi
else
    echo "  [no emulator: the clearing is read, not watched]"
fi

# And the one shape that cannot be allowed: something compiled between the
# two was told where in the bss to look, and the value it has been given is
# somewhere else entirely.
printf 'int x;\nint peek(void) { return x; }\nint x = 42;\nint main(void) { return peek(); }\n' \
    > "$tmp/moved.c"
err=$("$ACC" "$tmp/moved.c" -o "$tmp/moved.bin" -x 2>&1)
case $err in
  *"is used above and given a value here"*) pass=$((pass + 1)) ;;
  *) printf '  FAIL %-36s %s\n' "used, then given a value" \
         "$(printf '%s' "$err" | head -1)"; fail=$((fail + 1)) ;;
esac

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

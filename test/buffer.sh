#!/bin/bash
# The source window, swept across every construct that could straddle it.
#
# The lexer reads the file 4 KB at a time and refills when it runs out. The
# invariant that makes that safe is that a refill only ever happens where a
# line ended, so no token is ever split by one -- and nothing in the output
# shows whether it holds. A token cut in half becomes a different token, or a
# diagnostic about a character that is really there.
#
# So the same program is compiled many times over, each with a different
# amount of comment padding in front of it, which slides the whole of it
# through the window's edge a byte at a time. Padding is comments, so every
# one of them has to produce the same image: if the edge ever takes a bite
# out of an identifier, a string, a number or a comment, one of these differs
# from the rest or fails to compile.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/ref" "$tmp/pad"

# The window's size, from the lexer. Read rather than repeated, so that
# changing it there moves this sweep with it.
cap=$(sed -n 's/^#define SRC_CAP  *\([0-9]*\).*/\1/p' src/lex.c)
[ -n "$cap" ] || { echo "  FAIL cannot find SRC_CAP in src/lex.c"; exit 1; }

# What goes after the padding: one of everything whose scan walks forward
# over characters, so that each in turn is the thing the edge lands in.
#
# A parameter's array size is kept as the text between its brackets, and
# the edge falling inside it is what says that text survives the refill.
#
# The backslash-joined name and string are here for the window in
# particular. The join is taken out of the window as it is filled, which
# moves everything after it down; sliding one across the edge is what says
# the part still to be read moves with it.
payload() {
    cat <<'EOF'
static int second_row(int n, int a[][(
                                      n
                                      )])
{
    return a[1][0];
}
/* a block comment
 * that runs over several lines and so is the one thing
 * a window's edge can fall inside
 */
int a_long_identifier_to_straddle_the_edge = 7;
int split\
_over_two_lines = 11;
char *joined = "half \
and half";
char *text_with_escapes = "a string \"with\" escapes and a \\ backslash";
double number = 1234.56789e-12;
long long wide_value = 1234567890123LL;
int shifted = 0;

int main(void) {
    int grid[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };

    shifted = a_long_identifier_to_straddle_the_edge;
    shifted >>= 1;                  // a line comment at the end of a line
    shifted = shifted == 3 ? 42 : 42;

    return number > 0.0 && wide_value > 0 && text_with_escapes[0] == 'a'
           && split_over_two_lines == 11 && joined[5] == 'a'
           && second_row(3, grid) == 4
           ? shifted : 0;
}
EOF
}

payload > "$tmp/payload.c"

# The reference: no padding at all.
cp "$tmp/payload.c" "$tmp/ref.c"
if ! "$ACC" "$tmp/ref.c" -o "$tmp/ref/out.bin" >/dev/null 2>&1; then
    echo "  FAIL the payload does not compile on its own"
    exit 1
fi

# One filler line, whose length is known, repeated to reach the offset wanted.
# 64 characters including the newline.
filler='// 0123456789012345678901234567890123456789012345678901234567'
flen=$((${#filler} + 1))

pass=0; fail=0
for pad in $(seq $((cap - 80)) $((cap + 80))); do
    lines=$((pad / flen))
    rest=$((pad - lines * flen))

    {
        for _ in $(seq $lines); do printf '%s\n' "$filler"; done
        # The remainder as one more comment line of exactly `rest` bytes,
        # newline included, so the payload starts at byte `pad`.
        if [ "$rest" -ge 3 ]; then
            printf '//'
            i=2
            while [ $i -lt $((rest - 1)) ]; do printf 'x'; i=$((i + 1)); done
            printf '\n'
        elif [ "$rest" -gt 0 ]; then
            i=0
            while [ $i -lt "$rest" ]; do printf '\n'; i=$((i + 1)); done
        fi
        payload
        # And a window's worth behind it. Without this the payload is the
        # last thing in the file, the window that holds it ends where the
        # file does, and the part of a refill that carries the unread tail
        # forward is never reached -- which is the part a join has to move
        # along with everything else.
        for _ in $(seq $((cap / flen + 2))); do printf '%s\n' "$filler"; done
    } > "$tmp/pad.c"

    if ! "$ACC" "$tmp/pad.c" -o "$tmp/pad/out.bin" >"$tmp/err" 2>&1; then
        printf '  FAIL padding %d: acc rejected it\n%s\n' "$pad" \
            "$(sed 's/^/         /' "$tmp/err")"
        fail=$((fail + 1))
        continue
    fi
    if ! cmp -s "$tmp/pad/out.bin" "$tmp/ref/out.bin"; then
        printf '  FAIL padding %d: the image differs from the unpadded one\n' \
            "$pad"
        fail=$((fail + 1))
        continue
    fi
    pass=$((pass + 1))
done

# A line longer than the window: the window grows to hold it, at the start
# of the file and across the edge after padding. Each has to compile to what
# the same terms make wrapped over many lines, since where a line breaks
# changes nothing in the image.
terms=$((cap / 4 + 8))
acc_abs=$(cd "$(dirname "$ACC")" && pwd)/$(basename "$ACC")
long_line() {
    local i=0 wrap=$1
    printf 'int x = 1'
    while [ $i -lt $terms ]; do
        printf ' + 1'
        i=$((i + 1))
        [ "$wrap" -gt 0 ] && [ $((i % wrap)) -eq 0 ] && printf '\n'
    done
    printf ';\nint main(void) { return x; }\n'
}
long_line 100 > "$tmp/ref/wrapped.c"
(cd "$tmp/ref" && "$acc_abs" wrapped.c -o out.bin >/dev/null 2>&1) \
    || { echo "  FAIL the wrapped reference does not compile"; exit 1; }
for pad in 0 $((cap - 100)); do
    {
        [ "$pad" -gt 0 ] && printf '/*%*s*/\n' "$((pad - 5))" ''
        long_line 0
    } > "$tmp/pad/wrapped.c"
    rm -f "$tmp/pad/out.bin"
    if (cd "$tmp/pad" && "$acc_abs" wrapped.c -o out.bin >"$tmp/err" 2>&1) \
       && cmp -s "$tmp/pad/out.bin" "$tmp/ref/out.bin"; then
        pass=$((pass + 1))
    else
        printf '  FAIL a line longer than the window, after %d bytes: %s\n' \
            "$pad" "$(head -1 "$tmp/err")"
        fail=$((fail + 1))
    fi
done

# Past 64 KB a line is refused rather than cut in half.
{
    printf 'int x = 1'
    head -c 70000 /dev/zero | tr '\0' ' '
    printf ';\nint main(void) { return 42; }\n'
} > "$tmp/long.c"
if "$ACC" "$tmp/long.c" -o "$tmp/pad/out.bin" >"$tmp/err" 2>&1; then
    echo "  FAIL a line longer than 64 KB was accepted"
    fail=$((fail + 1))
elif ! grep -q "a line longer than 65536" "$tmp/err"; then
    printf '  FAIL a line longer than 64 KB: %s\n' "$(cat "$tmp/err")"
    fail=$((fail + 1))
else
    pass=$((pass + 1))
fi

echo "  $pass offsets across the window's edge, $fail failed"
[ "$fail" -eq 0 ]

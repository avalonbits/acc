#!/bin/bash
# The source window, swept across every construct that could straddle it.
#
# The lexer reads the file 16 KB at a time and refills when it runs out. The
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
payload() {
    cat <<'EOF'
/* a block comment
 * that runs over several lines and so is the one thing
 * a window's edge can fall inside
 */
int a_long_identifier_to_straddle_the_edge = 7;
char *text_with_escapes = "a string \"with\" escapes and a \\ backslash";
double number = 1234.56789e-12;
long long wide_value = 1234567890123LL;
int shifted = 0;

int main(void) {
    shifted = a_long_identifier_to_straddle_the_edge;
    shifted >>= 1;                  // a line comment at the end of a line
    shifted = shifted == 3 ? 42 : 42;

    return number > 0.0 && wide_value > 0 && text_with_escapes[0] == 'a'
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

# And the one thing the window cannot do: a line with no end in it. There is
# nowhere to put the rest of such a line, so it has to be refused rather than
# quietly cut in half.
{
    printf 'int x = 1'
    i=0
    while [ $i -lt $((cap / 4 + 8)) ]; do printf ' + 1'; i=$((i + 1)); done
    printf ';\nint main(void) { return 42; }\n'
} > "$tmp/long.c"

if "$ACC" "$tmp/long.c" -o "$tmp/pad/out.bin" >"$tmp/err" 2>&1; then
    echo "  FAIL a line longer than the window was accepted"
    fail=$((fail + 1))
elif ! grep -q "a line longer than" "$tmp/err"; then
    printf '  FAIL a line longer than the window: %s\n' "$(cat "$tmp/err")"
    fail=$((fail + 1))
else
    pass=$((pass + 1))
fi

echo "  $pass offsets across the window's edge, $fail failed"
[ "$fail" -eq 0 ]

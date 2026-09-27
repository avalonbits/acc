#!/bin/bash
# What a jump costs when its target turns out to be near.
#
# The eZ80 has a two-byte relative jump that reaches 127 bytes either way,
# and most jumps in compiled code go nowhere near that far -- the end of an
# `if`, the top of a loop. Which ones do is not known when they are written,
# because the target of a forward jump is wherever the statement after it
# ends up, so acc writes them all as four-byte `jp` and shortens the ones
# that turn out to be near once the file is read.
#
# The saving is stated here as a size, and calibrated rather than asserted:
# the same loop is compiled with a short body and with a long one, and the
# same two bodies again with no loop around them. What is left after
# subtracting is what the loop's two jumps cost, and the difference between
# the two of those is the four bytes the short ones did not spend.
#
# Whether the jumps still GO to the right place is test/cases/216_jump_reach.c,
# which walks the body lengths across the distance at which one stops
# reaching, against agondev.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

ok()  { pass=$((pass + 1)); }
bad() { printf '  FAIL %-34s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# The bytes of code in an object, which is the three at offset 7.
text_of() {
    printf '%s' "$1" > "$tmp/x.c"
    if ! "$ACC" -c "$tmp/x.c" -o "$tmp/x.o" >/dev/null 2>&1; then
        echo "-1"; return
    fi
    od -An -tu1 -j7 -N3 "$tmp/x.o" | awk '{ print $1 + $2 * 256 + $3 * 65536 }'
}

# n statements, which is how the target is pushed out of reach.
body() {
    local i=0
    while [ "$i" -lt "$1" ]; do i=$((i + 1)); printf '    t += %d;\n' "$i"; done
}

near=$(body 2)
far=$(body 20)

loop()  { printf 'int f(int a) { int t = 0;\n  while (a) { a--;\n%s  }\n  return t; }\n' "$1"; }
plain() { printf 'int f(int a) { int t = 0;\n  { a--;\n%s  }\n  return t; }\n' "$1"; }

a=$(text_of "$(loop "$near")")
b=$(text_of "$(loop "$far")")
c=$(text_of "$(plain "$near")")
d=$(text_of "$(plain "$far")")

if [ "$a" -lt 0 ] || [ "$b" -lt 0 ] || [ "$c" -lt 0 ] || [ "$d" -lt 0 ]; then
    bad "a loop's two jumps" "one of them would not compile"
elif [ $(((b - d) - (a - c))) -eq 4 ]; then
    printf '  ok   %-34s %d bytes against %d\n' "a loop's two jumps" \
        $((a - c)) $((b - d))
    ok
else
    bad "a loop's two jumps" \
        "$((a - c)) bytes near, $((b - d)) far -- the difference should be 4"
fi

# And said the other way: a loop that reaches has no long jump left in it.
# The bytes are searched for the opcodes of one, which is honest here because
# the function is small enough to have no constant that could be mistaken for
# one -- the only numbers in it are a frame offset and a one.
printf '%s' "$(loop "$(body 1)")" > "$tmp/x.c"
if "$ACC" -c "$tmp/x.c" -o "$tmp/x.o" >/dev/null 2>&1; then
    n=$(od -An -tu1 -j7 -N3 "$tmp/x.o" | awk '{ print $1 + $2 * 256 + $3 * 65536 }')
    code=$(tail -c "$n" "$tmp/x.o" | od -An -tx1 -v | tr -s ' \n' ' ')
    long=$(printf '%s' "$code" | tr ' ' '\n' | grep -cE '^(c3|ca|c2|da|d2)$' || true)
    short=$(printf '%s' "$code" | tr ' ' '\n' | grep -cE '^(18|28|20|38|30)$' || true)
    if [ "$long" -eq 0 ] && [ "$short" -ge 2 ]; then
        printf '  ok   %-34s %d short, %d long\n' "nothing long is left in it" \
            "$short" "$long"
        ok
    else
        bad "nothing long is left in it" "$short short and $long long"
    fi
else
    bad "nothing long is left in it" "it would not compile"
fi

# A jump backwards to a label, which is the other way one is written: not a
# hole filled in later but an address that is already known. Any jr will
# do: `if (a) goto again;` is one conditional jump, jr nz.
back=$(text_of 'int f(int a) { int t = 0;
again:
    t += a;
    a--;
    if (a) goto again;
    return t; }
')
if [ "$back" -gt 0 ]; then
    printf '%s' 'int f(int a) { int t = 0;
again:
    t += a;
    a--;
    if (a) goto again;
    return t; }
' > "$tmp/x.c"
    "$ACC" -c "$tmp/x.c" -o "$tmp/x.o" >/dev/null 2>&1
    n=$(od -An -tu1 -j7 -N3 "$tmp/x.o" | awk '{ print $1 + $2 * 256 + $3 * 65536 }')
    if tail -c "$n" "$tmp/x.o" | od -An -tx1 -v | tr -s ' \n' ' ' \
       | tr ' ' '\n' | grep -qE '^(18|20|28|30|38)$'; then
        printf '  ok   %-34s %d bytes\n' "a goto that goes backwards" "$back"
        ok
    else
        bad "a goto that goes backwards" "it is still a long jump"
    fi
else
    bad "a goto that goes backwards" "it would not compile"
fi

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

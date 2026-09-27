#!/bin/bash
# Every program in test/errors is one acc must reject. The first line says
# what it must say about it: `expect: LINE:COLUMN: error: text`, or
# `expect: LINE: error: text` where the column is not pinned down -- though
# there has to be one -- or anything else, found as it is in what acc says,
# which is how an error in a header is named: `x.h:2:5: error: text`.
#
# The differential runner cannot cover these: agondev compiles most of them
# happily, so there is nothing to compare against. What is being checked is
# that acc names the right line and the right construct, which matters most
# for the features it does not have yet -- a user meeting `*` should be told
# `*` is not supported, not be sent hunting for a missing semicolon.
set -uo pipefail
cd "$(dirname "$0")/.."

# Which compiler to drive. `make test` points this at the sanitized build, so
# a run that gets the right answer by reading freed memory still fails.
ACC=${ACC:-bin/acc}

# acc allocates and never frees: it is a one-shot process and giving the
# memory back on the way out would cost code on a target where code is the
# scarce thing. That is deliberate, so leak checking is off; everything else
# ASan and UBSan look for is on.
export ASAN_OPTIONS=detect_leaks=0

# A sanitizer report on stderr is a failure even when the answer is right,
# which is exactly the case that hid the symbol-table bug.
sanitizer_tripped() {
    case $1 in
      *"AddressSanitizer"*|*"runtime error:"*|*"UndefinedBehaviorSanitizer"*) return 0 ;;
    esac
    return 1
}

[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

pass=0; fail=0
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

for src in test/errors/*.c; do
    name=$(basename "$src" .c)
    want=$(sed -n '1s|.*/\* expect: \(.*\) \*/.*|\1|p' "$src")
    [ -n "$want" ] || { printf '  FAIL %-18s no "expect:" line\n' "$name"; fail=$((fail+1)); continue; }

    got=$("$ACC" "$src" -o "$tmp/out.bin" -x 2>&1)
    if [ $? -eq 0 ]; then
        printf '  FAIL %-18s acc accepted it\n' "$name"
        fail=$((fail+1)); continue
    fi
    if sanitizer_tripped "$got"; then
        printf '  FAIL %-18s the sanitizer tripped\n%s\n' "$name" \
            "$(printf '%s' "$got" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    # A line and no column says the column is not pinned down, and not that
    # there is none: an error at a line is at a column in it, printed as
    # gcc prints one, `file:line:column: error: text`. The column is taken
    # out of what acc said before the text is looked for.
    if [[ "$want" =~ ^[0-9]+:\ (.*)$ ]]; then
        if ! [[ "$got" =~ ^[^:]+:[0-9]+:[1-9][0-9]*:\ error:\  ]]; then
            printf '  FAIL %-18s no column in %s\n' "$name" "$got"
            fail=$((fail+1)); continue
        fi
        seen=$(printf '%s' "$got" | sed -E '1s/^([^:]+:[0-9]+):[0-9]+:/\1:/')
    else
        seen=$got
    fi
    if [[ "$seen" != *"$want"* ]]; then
        printf '  FAIL %-18s want %s\n                        got  %s\n' "$name" "$want" "$got"
        fail=$((fail+1)); continue
    fi

    # And again with -errors, as a program running acc would: the same
    # error in the file, as `file:line:column: error: text`, and 100 for
    # failure. An expectation with no column is held to the rest of it. The
    # one error with no line, a link's, is `acc:0:0`; any other has both.
    rm -f "$tmp/err.txt"
    "$ACC" "$src" -o "$tmp/out.bin" -x -errors "$tmp/err.txt" > /dev/null 2>&1
    rc=$?
    file=$(cat "$tmp/err.txt" 2>/dev/null)
    if [[ "$want" =~ ^([0-9]+:[0-9]+):\ (.*)$ ]]; then
        infile="$src:${BASH_REMATCH[1]}: ${BASH_REMATCH[2]}"
        fseen=$file
    elif [[ "$want" =~ ^([0-9]+):\ (.*)$ ]]; then
        infile="$src:${BASH_REMATCH[1]}: ${BASH_REMATCH[2]}"
        fseen=$(printf '%s' "$file" | sed -E '1s/^([^:]+:[0-9]+):[0-9]+:/\1:/')
    else
        infile="$want"
        fseen=$file
    fi
    if [ "$rc" -ne 100 ]; then
        printf '  FAIL %-18s -errors: returned %d, not 100\n' "$name" "$rc"
        fail=$((fail+1))
    elif [ "$(printf '%s\n' "$file" | wc -l)" -ne 1 ] \
         || ! [[ "$file" =~ ^(acc:0:0|[^:]+:[1-9][0-9]*:[1-9][0-9]*):\ error:\  ]] \
         || [[ "$fseen" != *"$infile"* ]]; then
        printf '  FAIL %-18s -errors: want %s\n                        file %s\n' \
            "$name" "$infile" "$file"
        fail=$((fail+1))
    else
        printf '  ok   %-18s %s\n' "$name" "$want"
        pass=$((pass+1))
    fi
done

# -errors on a compile that works: 0, and no file left behind, even one
# a failed compile wrote before it.
printf 'int main(void) { return 0; }\n' > "$tmp/ok.c"
echo stale > "$tmp/err.txt"
"$ACC" "$tmp/ok.c" -o "$tmp/ok.bin" -errors "$tmp/err.txt" > /dev/null 2>&1
rc=$?
if [ "$rc" -eq 0 ] && [ ! -e "$tmp/err.txt" ]; then
    printf '  ok   %-18s %s\n' "-errors, success" "returns 0 and removes the file"
    pass=$((pass+1))
else
    printf '  FAIL %-18s returned %d, file %s\n' "-errors, success" "$rc" \
        "$([ -e "$tmp/err.txt" ] && echo left || echo removed)"
    fail=$((fail+1))
fi

# Without -errors nothing changes: a failure is still 1.
"$ACC" test/errors/030_missing_semi.c -o "$tmp/out.bin" > /dev/null 2>&1
rc=$?
if [ "$rc" -eq 1 ]; then
    printf '  ok   %-18s %s\n' "no -errors" "a failure still returns 1"
    pass=$((pass+1))
else
    printf '  FAIL %-18s a failure returned %d, not 1\n' "no -errors" "$rc"
    fail=$((fail+1))
fi

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

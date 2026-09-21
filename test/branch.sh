#!/bin/bash
# What a comparison costs when it is a condition and not a value.
#
# `a < b` leaves a one or a zero, and that is what `x = a < b` wants. What
# `if (a < b)` wants is the branch, and the flags the subtract left already
# say which way it goes -- so acc rewinds the bytes that were making the
# value and jumps on them instead.
#
# The saving is stated here as a size: the same comparison, in a condition
# and in a value, with nothing else between the two programs. It is about
# twenty-five bytes on this target -- the two loads of a constant, the
# reload and the second subtract -- and the check asks for twenty, which is
# enough to fail loudly if the rewind stops happening and not so tight that
# it argues about an instruction.
#
# What the comparisons MEAN is not this file's job: test/cases has that,
# against agondev, including the signed overflow that makes the eZ80's
# answer two instructions rather than one.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# The bytes of code in an object, which is the three at offset 7.
text_of() {
    printf '%s' "$2" > "$tmp/x.c"
    if ! "$ACC" -c "$tmp/x.c" -o "$tmp/x.o" >/dev/null 2>&1; then
        echo "-1"; return
    fi
    od -An -tu1 -j7 -N3 "$tmp/x.o" | awk '{ print $1 + $2 * 256 + $3 * 65536 }'
}

# cheaper <name> <type> <comparison>
cheaper() {
    local what=$1 type=$2 cmp=$3 cond value

    cond=$(text_of c "$type pick($type a, $type b) { if ($cmp) return a;
                                                      return b; }
")
    value=$(text_of v "$type pick($type a, $type b) { int t = $cmp;
                                                      if (t) return a;
                                                      return b; }
")
    if [ "$cond" -lt 0 ] || [ "$value" -lt 0 ]; then
        printf '  FAIL %-28s it would not compile\n' "$what"
        fail=$((fail + 1))
    elif [ $((value - cond)) -ge 20 ]; then
        printf '  ok   %-28s %d against %d\n' "$what" "$cond" "$value"
        pass=$((pass + 1))
    else
        printf '  FAIL %-28s %d against %d, and the condition should be at least 20 less\n' \
            "$what" "$cond" "$value"
        fail=$((fail + 1))
    fi
}

for op in '<' '>' '<=' '>=' '==' '!='; do
    cheaper "signed a $op b"   "int"      "a $op b"
    cheaper "unsigned a $op b" "unsigned" "a $op b"
done

# The same in the other shapes a condition comes in. The second of each pair
# never finishes if it is run, which is beside the point: nothing runs them,
# and what is being weighed is a loop whose test is a comparison against one
# whose test is a value that was made from one.
for shape in 'while' 'for'; do
    case $shape in
      while) cond='int pick(int a, int b) { while (a < b) a++; return a; }'
             value='int pick(int a, int b) { int t = a < b; while (t) a++; return a; }' ;;
      for)   cond='int pick(int a, int b) { for (; a < b; a++) ; return a; }'
             value='int pick(int a, int b) { int t = a < b; for (; t; a++) ; return a; }' ;;
    esac
    c=$(text_of c "$cond
")
    v=$(text_of v "$value
")
    if [ $((v - c)) -ge 20 ]; then
        printf '  ok   %-28s %d against %d\n' "a $shape condition" "$c" "$v"
        pass=$((pass + 1))
    else
        printf '  FAIL %-28s %d against %d\n' "a $shape condition" "$c" "$v"
        fail=$((fail + 1))
    fi
done

# And that nothing is rewound when the comparison is not the last thing
# emitted: the value is wanted here, and taking the bytes that make it away
# would be taking the answer away.
kept=$(text_of k 'int side(int n);
int pick(int a, int b) { if (side(a < b)) return a;
                         return b; }
')
plain=$(text_of p 'int side(int n);
int pick(int a, int b) { if (side(a)) return a;
                         return b; }
')
if [ "$kept" -gt "$plain" ]; then
    printf '  ok   %-28s %d against %d\n' "one whose value is wanted" "$kept" "$plain"
    pass=$((pass + 1))
else
    printf '  FAIL %-28s %d against %d: the value was not made\n' \
        "one whose value is wanted" "$kept" "$plain"
    fail=$((fail + 1))
fi

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

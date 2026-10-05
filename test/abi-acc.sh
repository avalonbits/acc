#!/bin/bash
# Does acc follow the calling convention it says it does?
#
# test/abi.sh pins what agondev's convention *is*, by reading the code its
# compiler generates. This is the other half: it reads the code acc generates
# and checks it against the same table. Neither is a substitute for the other,
# and the differential tests cannot do this job at all -- acc links with
# nothing, so a convention it gets consistently wrong agrees with itself
# everywhere and only fails the day acc output meets agondev output.
#
# The probes are chosen so the answer is a single instruction: a function that
# returns its second parameter compiles to one load, and the slot the first
# parameter occupies is the difference between that load's displacement and 6.
set -uo pipefail

cd "$(dirname "$0")/.."
[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

# compile <source> -- leaves the image at $tmp/p.bin
compile() {
    printf '%s\nint main(void) { return 42; }\n' "$1" > "$tmp/p.c"
    bin/acc "$tmp/p.c" -o "$tmp/p.bin" >/dev/null 2>&1
}

check() {
    if [ "$2" = "$3" ]; then
        printf '  ok   %-34s %s\n' "$1" "$2"
        pass=$((pass+1))
    else
        printf '  FAIL %-34s got %s, want %s\n' "$1" "$2" "$3"
        fail=$((fail+1))
    fi
}

# The displacement of the first `ld hl, (ix+d)` with a positive d, which for
# these probes is where the function reads its last parameter.
param_at() {
    python3 - "$tmp/p.bin" <<'PY'
import re, sys
data = open(sys.argv[1], 'rb').read()
for m in re.finditer(rb'\xdd\x27(.)', data):
    d = m.group(1)[0]
    if 0 < d < 64:
        print(d)
        break
else:
    print("none")
PY
}

echo "  --- argument slots, by where the parameter after one lands"
for probe in "char:9" "short:9" "int:9" "unsigned int:9" "long:12" \
             "long long:15"; do
    type=${probe%:*}; want=${probe#*:}
    compile "int f($type a, int b) { return b; }"
    check "an argument of $type" "$(param_at)" "$want"
done

echo "  --- where a result comes back"
# A one-byte result is left in A: the last thing before the epilogue is ld a,l.
compile "char f(int n) { return n; }"
got=$(python3 - "$tmp/p.bin" <<'PY'
import re, sys
data = open(sys.argv[1], 'rb').read()
print("A" if re.search(rb'\x7d(?:\xdd\xf9)?\xdd\xe1\xc9', data) else "not A")
PY
)
check "a 1-byte result" "$got" "A"

# A four-byte result is HL with its high byte in E, loaded from adjacent bytes.
compile "long f(void) { return 305419896; }"
got=$(python3 - "$tmp/p.bin" <<'PY'
import re, sys
data = open(sys.argv[1], 'rb').read()
m = re.search(rb'\xdd\x27(.)\xdd\x5e(.)\xdd\xf9\xdd\xe1\xc9', data, re.S)
if not m:
    print("not HL:E")
else:
    lo, hi = m.group(1)[0], m.group(2)[0]
    print("HL:E+%d" % ((hi - lo) & 0xff))
PY
)
check "a 4-byte result" "$got" "HL:E+3"

# An eight-byte result is HL, DE and BC, from three slots in a row.
compile "long long f(void) { return 0x1122334455667788LL; }"
got=$(python3 - "$tmp/p.bin" <<'PY'
import re, sys
data = open(sys.argv[1], 'rb').read()
m = re.search(rb'\xdd\x27(.)\xdd\x17(.)\xdd\x07(.)\xdd\xf9\xdd\xe1\xc9', data, re.S)
if not m:
    print("not HL:DE:BC")
else:
    lo, mid, hi = (g[0] for g in m.groups())
    print("HL:DE+%d:BC+%d" % ((mid - lo) & 0xff, (hi - lo) & 0xff))
PY
)
check "an 8-byte result" "$got" "HL:DE+3:BC+6"

# Everything else comes back in HL, so there is no ld a,l before the epilogue.
for type in short int "unsigned int"; do
    compile "$type f(int n) { return n; }"
    got=$(python3 - "$tmp/p.bin" <<'PY'
import re, sys
data = open(sys.argv[1], 'rb').read()
print("A" if re.search(rb'\x7d(?:\xdd\xf9)?\xdd\xe1\xc9', data) else "HL")
PY
)
    check "a result of $type" "$got" "HL"
done

printf '  %d properties checked, %d failed\n' "$((pass+fail))" "$fail"
[ "$fail" -eq 0 ]

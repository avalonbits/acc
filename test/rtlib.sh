#!/bin/bash
# acc's runtime, in the library: lib/rt/*.s, assembled by zap into libc.a.
#
# The code generator calls a routine by name, and the link takes it from
# the library, one object a routine or a few that share code. So:
#
#   - every name the compiler can call (rt_names, in src/runtime.c) is a
#     routine lib/rt exports, and everything lib/rt exports is in the
#     library's index -- a name either side of that is a link that fails,
#     or a program that cannot be built, and only on the code path that
#     calls it;
#   - a program carries the runtime it calls and not the rest: one that
#     multiplies an int has the multiply and not the floating point.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/libc.a}
[ -x "$ACC" ] && [ -f "$LIB" ] || { echo "run make first" >&2; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

# What the compiler calls, what lib/rt exports, and what the library lists.
python3 - "$LIB" <<'PY' || fail=1
import glob, re, sys

src = open('src/runtime.c').read()
table = src[src.index('rt_names[RT_COUNT]'):]
table = table[:table.index('};')]
calls = {'acc_rt_' + n for n in re.findall(r'"(\w+)"', table)}

exports = set()
for p in glob.glob('lib/rt/*.s'):
    for m in re.finditer(r'^\s*XDEF\s+([\w.]+)', open(p).read(), re.M):
        exports.add(m.group(1))
c_exports = {e[1:] for e in exports if e.startswith('_')}

d = open(sys.argv[1], 'rb').read()
g3 = lambda o: d[o] | d[o + 1] << 8 | d[o + 2] << 16
assert d[:4] == b'ACR\x01', 'not an acc library'
nmembers, ndefs, slen = g3(7), g3(10), g3(13)
strings = 16 + nmembers * 9 + ndefs * 6
def name(off):
    end = d.index(b'\0', strings + off)
    return d[strings + off:end].decode()
index = {name(g3(16 + nmembers * 9 + i * 6)) for i in range(ndefs)}
index = {n[1:] if n.startswith('_') else '@' + n for n in index}

bad = 0
for n in sorted(calls - c_exports):
    print('  FAIL the compiler calls %s, and lib/rt has no such routine' % n)
    bad = 1
for n in sorted(c_exports - index):
    print('  FAIL lib/rt exports %s, and the library does not list it' % n)
    bad = 1
if not bad:
    print('  ok   all %d routines the compiler calls are in lib/rt, and all %d '
          'it exports are in the library' % (len(calls), len(c_exports)))
sys.exit(bad)
PY

# A program carries what it calls.
cat > "$tmp/mul.c" <<'EOF'
volatile int a = 6, b = 7;
int main(void) { return a * b; }
EOF
if "$ACC" -c "$tmp/mul.c" -o "$tmp/mul.o" >/dev/null 2>&1 &&
   "$ACC" "$tmp/mul.o" -o "$tmp/mul.bin" -map "$tmp/mul.map" >/dev/null 2>&1; then
    have=$(awk '{ print $3 }' "$tmp/mul.map" | grep '^acc_rt_' | sort | tr '\n' ' ')
    case $have in
      *acc_rt_mul*acc_rt_*f*|*acc_rt_fadd*|*acc_rt_ldivs*)
        echo "  FAIL a program that multiplies carries more: $have"; fail=1 ;;
      *acc_rt_mul*)
        echo "  ok   a program that multiplies carries $have" ;;
      *)
        echo "  FAIL a program that multiplies carries no multiply: $have"; fail=1 ;;
    esac
else
    echo "  FAIL a program that multiplies does not build"; fail=1
fi

exit $fail

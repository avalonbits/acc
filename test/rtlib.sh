#!/bin/bash
# acc's runtime: lib/rt/*.s, assembled by zap into rt.a, the library beside
# libc.a that a link reads first.
#
# The code generator calls a routine by name, and the link takes it from
# the library, one object a routine or a few that share code. So:
#
#   - every name the compiler can call (rt_names, in src/runtime.c) is a
#     routine lib/rt exports, and everything lib/rt exports is in rt.a's
#     index -- a name either side of that is a link that fails,
#     or a program that cannot be built, and only on the code path that
#     calls it;
#   - a program carries the runtime it calls and not the rest: one that
#     multiplies an int has the multiply and not the floating point;
#   - what the C library has in assembly (lib/*.s: the string functions) is
#     in libc.a, and not in rt.a with the runtime.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
LIB=${LIB:-bin/rt.a}
[ -x "$ACC" ] && [ -f "$LIB" ] || { echo "run make first" >&2; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0

# What the compiler calls, what lib/rt exports, and what the library lists.
python3 - "$LIB" "$(dirname "$LIB")/libc.a" <<'PY' || fail=1
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

def listed(path):              # the names a library's index lists
    d = open(path, 'rb').read()
    g3 = lambda o: d[o] | d[o + 1] << 8 | d[o + 2] << 16
    assert d[:4] == b'ACR\x01', 'not an acc library'
    nmembers, ndefs = g3(7), g3(10)
    strings = 16 + nmembers * 9 + ndefs * 6
    def name(off):
        end = d.index(b'\0', strings + off)
        return d[strings + off:end].decode()
    found = {name(g3(16 + nmembers * 9 + i * 6)) for i in range(ndefs)}
    return {n[1:] if n.startswith('_') else '@' + n for n in found}
index = listed(sys.argv[1])
libc = listed(sys.argv[2])

asm = set()
for p in glob.glob('lib/*.s'):
    for m in re.finditer(r'^\s*XDEF\s+_([\w.]+)', open(p).read(), re.M):
        asm.add(m.group(1))

bad = 0
for n in sorted(calls - c_exports):
    print('  FAIL the compiler calls %s, and lib/rt has no such routine' % n)
    bad = 1
for n in sorted(c_exports - index):
    print('  FAIL lib/rt exports %s, and the library does not list it' % n)
    bad = 1
for n in sorted(asm - libc):
    print('  FAIL lib/*.s defines %s, and libc.a does not list it' % n)
    bad = 1
for n in sorted(asm & index):
    print('  FAIL %s is the C library\'s, and rt.a lists it' % n)
    bad = 1
if not asm:
    print('  FAIL lib/*.s defines nothing')
    bad = 1
if not bad:
    print('  ok   all %d routines the compiler calls are in lib/rt, and all %d '
          'it exports are in rt.a' % (len(calls), len(c_exports)))
    print('  ok   the %d functions lib/*.s defines are in libc.a and not in '
          'rt.a' % len(asm))
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

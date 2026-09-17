#!/bin/bash
# What is agondev's C ABI, really?
#
# acc has to match it exactly. Not as a courtesy -- acc exists so that code
# can be compiled on the machine, and a library someone else wrote with
# agondev has to keep working, which means acc's calling convention is not a
# free choice. It is whatever agondev does.
#
# And agondev's behaviour is the only specification there is. UM0077 is the
# eZ80 CPU manual: registers, ADL mode, interrupts, traps, addressing modes,
# the instruction set, opcode maps. It contains no calling convention at all
# -- the words callee, caller and linker appear nowhere in it -- and Zilog's
# compiler documentation is not in this tree. gen.c says as much already, in
# so many words: "the frame agondev's __frameset builds".
#
# So this pins it. It compiles C whose generated code states each answer in a
# single instruction -- a function that returns its fourth parameter compiles
# to a load from that parameter's slot, so the slot is the offset in the load
# -- and checks each one. A toolchain update that moved any of it would
# otherwise turn up as a program crashing on the Agon.
#
# This checks what agondev does, not yet what acc does. Asserting acc's own
# output against the same table is the obvious next step and wants acc to
# emit calls with more shapes than it does today.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang

[ -x "$CC" ] || { echo "  skip abi                no agondev (set AGONDEV)"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

cat > "$tmp/probe.c" <<'PROBE'
typedef struct { int a; int b; } Pair;

int  sink(int x);
int  callee3(int a, int b, int c);

/* Where each argument lives. Each of these is one load from one slot. */
int arg0(int a, int b, int c, int d, int e, int f) { (void)b;(void)c;(void)d;(void)e;(void)f; return a; }
int arg1(int a, int b, int c, int d, int e, int f) { (void)a;(void)c;(void)d;(void)e;(void)f; return b; }
int arg2(int a, int b, int c, int d, int e, int f) { (void)a;(void)b;(void)d;(void)e;(void)f; return c; }
int arg5(int a, int b, int c, int d, int e, int f) { (void)a;(void)b;(void)c;(void)d;(void)e; return f; }

/* What each type is worth in bytes, so the slot widths below are stated
 * against something instead of asserted alone: a long is 4 bytes and takes a
 * 6-byte slot, a char is 1 byte and takes 3. */
int size_char (void) { return (int) sizeof(char); }
int size_short(void) { return (int) sizeof(short); }
int size_int  (void) { return (int) sizeof(int); }
int size_long (void) { return (int) sizeof(long); }
int size_ptr  (void) { return (int) sizeof(void *); }

/* How wide a slot each type takes: the offset of the parameter after it. */
int after_char (char a,  int b) { (void)a; return b; }
int after_short(short a, int b) { (void)a; return b; }
int after_int  (int a,   int b) { (void)a; return b; }
int after_ptr  (void *a, int b) { (void)a; return b; }
int after_long (long a,  int b) { (void)a; return b; }

/* Where a result comes back. */
char  ret_char (void) { return (char)  0x5a; }
short ret_short(void) { return (short) 0x1234; }
int   ret_int  (void) { return 0x123456; }
long  ret_long (void) { return 0x12345678L; }

/* A struct result, and a struct argument. */
Pair ret_struct(int x)   { Pair p; p.a = x; p.b = x + 1; return p; }
int  take_struct(Pair p) { return p.b; }

/* A call site: the order the arguments are pushed, and who removes them. */
int call3(void) { return callee3(1, 2, 3); }

/* Values wanted on both sides of a call. Whatever is reloaded afterwards was
 * not preserved by it. */
int live_across(int x) { return sink(x) + x; }
int iy_across(Pair *p) { int t = sink(p->a); return t + p->b; }
PROBE

"$CC" -mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc \
      -isystem "$AGONDEV/include" -target ez80-none-elf \
      -Oz -Wa,-march=ez80+full -fno-threadsafe-statics \
      -S "$tmp/probe.c" -o "$tmp/probe.s" 2>"$tmp/err" || {
    echo "  FAIL abi                the probe would not compile"
    sed 's/^/         /' "$tmp/err"
    exit 1
}

exec python3 - "$tmp/probe.s" <<'PYEOF'
import re, sys

funcs, cur = {}, None
for line in open(sys.argv[1]):
    text = line.strip()
    m = re.match(r'^_([A-Za-z_]\w*):$', text)
    if m:
        cur = m.group(1)
        funcs[cur] = []
        continue
    if cur and text and not text.startswith('.'):
        funcs[cur].append(text)

groups, order = {}, []


def check(group, ok, what, observed, expected):
    if group not in groups:
        groups[group] = []
        order.append(group)
    groups[group].append((ok, what, observed, expected))


def body(name):
    return funcs.get(name, [])


def slot(name):
    """The (ix + N) a one-load function reads, or None."""
    for text in body(name):
        m = re.match(r'^ld\s+hl,\s*\(ix \+ (\d+)\)$', text)
        if m:
            return int(m.group(1))
    return None


# funcs is keyed off the `_name:` pattern, so an empty table is itself the
# failure: it would mean the symbols are not spelled that way.
check('symbol names', 'arg0' in funcs,
      'a C function foo is the symbol _foo',
      'symbols found: %s' % (sorted(funcs)[:3] or 'none'),
      'the C function arg0 emitted as _arg0')

for name, want in (('arg0', 6), ('arg1', 9), ('arg2', 12), ('arg5', 21)):
    check('argument slots', slot(name) == want,
          'argument %s is at (ix + %d)' % (name[-1], want),
          '(ix + %s)' % slot(name), '(ix + %d)' % want)

def constant(name):
    """The immediate a one-load function returns, or None."""
    for text in body(name):
        m = re.match(r'^ld\s+hl,\s*(\d+)$', text)
        if m:
            return int(m.group(1))
    return None


for name, want in (('size_char', 1), ('size_short', 2), ('size_int', 3),
                   ('size_long', 4), ('size_ptr', 3)):
    check('type sizes', constant(name) == want,
          'sizeof(%s) is %d' % (name[5:], want),
          'sizeof is %s' % constant(name), '%d' % want)

# A slot is the type's size rounded up to a multiple of 3, which is not the
# same number: a long is 4 bytes in a 6-byte slot.
for name, size, width in (('char', 1, 3), ('short', 2, 3), ('int', 3, 3),
                          ('ptr', 3, 3), ('long', 4, 6)):
    want = 6 + width
    check('slot widths', slot('after_' + name) == want,
          '%s is %d byte%s and takes a %d-byte argument slot'
          % (name, size, '' if size == 1 else 's', width),
          'next parameter at (ix + %s)' % slot('after_' + name),
          'next parameter at (ix + %d)' % want)

check('return values',
      any(re.match(r'^ld\s+a,\s*90$', t) for t in body('ret_char')),
      'a 1-byte result comes back in A', body('ret_char'), 'ld a, 90')
check('return values',
      any(re.match(r'^ld(\.sis)?\s+hl,\s*4660$', t) for t in body('ret_short')),
      'a 2-byte result comes back in HL', body('ret_short'), 'ld hl, 4660')
check('return values',
      any(re.match(r'^ld\s+hl,\s*1193046$', t) for t in body('ret_int')),
      'a 3-byte result comes back in HL', body('ret_int'), 'ld hl, 1193046')
# 0x12345678: low three bytes in HL, high byte in E.
check('return values',
      any(re.match(r'^ld\s+hl,\s*3430008$', t) for t in body('ret_long')) and
      any(re.match(r'^ld\s+e,\s*18$', t) for t in body('ret_long')),
      'a 4-byte result comes back in HL with its high byte in E',
      body('ret_long'), 'ld hl, 3430008 and ld e, 18')

rs = body('ret_struct')
check('struct results',
      any(re.match(r'^ld\s+iy,\s*\(ix \+ 6\)$', t) for t in rs) and
      any(re.match(r'^ld\s+hl,\s*\(ix \+ 9\)$', t) for t in rs) and
      any(re.match(r'^lea\s+hl,\s*iy \+ 0$', t) for t in rs),
      'a struct result is written through a hidden first argument and its '
      'pointer returned in HL', rs,
      'destination from (ix + 6), declared argument at (ix + 9), pointer in HL')
check('struct results', slot('take_struct') == 9,
      'a struct argument is flattened into consecutive slots',
      '.b at (ix + %s)' % slot('take_struct'),
      '.a at (ix + 6) and .b at (ix + 9)')

# callee3(1, 2, 3): find which register each constant went into, then read the
# order they are pushed in.
holds, pushed = {}, []
for text in body('call3'):
    m = re.match(r'^ld\s+(hl|de|bc),\s*(\d+)$', text)
    if m:
        holds[m.group(1)] = int(m.group(2))
        continue
    m = re.match(r'^push\s+(hl|de|bc)$', text)
    if m:
        pushed.append(holds.get(m.group(1)))
want_order = [3, 2, 1]
check('push order', pushed == want_order,
      'arguments are pushed right to left, so the first is at the lowest '
      'address', 'pushed in the order %s' % pushed,
      'pushed in the order %s' % want_order)

lines = body('call3')
idx = next((i for i, t in enumerate(lines) if re.match(r'^call\s+_callee3$', t)),
           None)
pops = 0
if idx is not None:
    for text in lines[idx + 1:]:
        if re.match(r'^pop\s+(hl|de|bc)$', text):
            pops += 1
        else:
            break
want_pops = 3
check('caller cleanup', pops == want_pops, 'the caller removes the arguments',
      '%d pops after the call' % pops, '%d pops, one per argument' % want_pops)

# Nothing but IX survives a call, which shows up as the value being read from
# the frame again afterwards instead of being kept.
for name, reg in (('live_across', 'de'), ('iy_across', 'iy')):
    lines = body(name)
    idx = next((i for i, t in enumerate(lines)
                if 'call' in t and '_sink' in t), None)
    check('registers', idx is not None and any(
              re.match(r'^ld\s+%s,\s*\(ix \+ \d+\)$' % reg, t)
              for t in lines[idx + 1:]),
          '%s does not survive a call' % reg.upper(), lines,
          'a reload of %s from the frame after the call' % reg.upper())

check('registers',
      any('__frameset' in t for t in body('live_across')) and
      any(re.match(r'^pop\s+ix$', t) for t in body('live_across')),
      'IX is saved and restored by the callee', body('live_across'),
      'a __frameset call and a matching pop ix')

total = failed = 0
for group in order:
    results = groups[group]
    total += len(results)
    bad = [r for r in results if not r[0]]
    failed += len(bad)
    if not bad:
        print('  ok   %-18s %d' % (group, len(results)))
        continue
    for _, what, observed, expected in bad:
        print('  FAIL %-18s %s' % (group, what))
        print('       expected: %s' % expected)
        print('       observed: %s' % observed)

print('  %d properties checked, %d failed' % (total, failed))
sys.exit(1 if failed else 0)
PYEOF

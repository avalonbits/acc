#!/bin/bash
# The entry stub, against the assembly it was made from.
#
# gen.c carries the stub and the argument routine as bytes, because acc has to
# emit them on a machine with no assembler. The source of truth is
# src/rt/startup.s: those bytes were produced by assembling it, and a change to
# either that is not made to the other is a silent divergence -- the comments
# in gen.c would describe code the compiler does not emit, which is worse than
# having no assembly at all.
#
# So this assembles startup.s and compares it with the arrays, byte for byte.
# The holes are the exception: gen.c leaves an address it does not know yet at
# zero and fills it in when it emits the stub, so a byte that is zero in the
# array and not in the assembly is allowed only where the assembly has a call
# or a load of an address gen.c is known to patch.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
AS=$AGONDEV/bin/ez80-none-elf-as
OBJDUMP=$AGONDEV/bin/ez80-none-elf-objdump
[ -x "$AS" ] || { echo "  [no agondev: the startup check is skipped]"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT

"$AS" -march=ez80+full src/rt/startup.s -o "$tmp/startup.o" || exit 1
"$OBJDUMP" -d "$tmp/startup.o" > "$tmp/dis" || exit 1

python3 - "$tmp/dis" <<'PY'
import re, sys

dis = open(sys.argv[1]).read()
img, syms = {}, {}
for line in dis.splitlines():
    m = re.match(r'^([0-9a-f]+) <(.+)>:', line)
    if m:
        syms[m.group(2)] = int(m.group(1), 16)
    m = re.match(r'\s+([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)', line)
    if m:
        at = int(m.group(1), 16)
        for i, b in enumerate(m.group(2).split()):
            img[at + i] = int(b, 16)

src = open('src/gen.c').read()

def array(name):
    """The byte array gen.c carries, with its comments taken out."""
    text = src[src.index('unsigned char %s[] = {' % name):]
    text = text[text.index('{') + 1:text.index('};')]
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    out = []
    for tok in text.replace('\n', ' ').split(','):
        tok = tok.strip()
        if tok:
            out.append(16 if tok == 'ARGV_MAX' else int(tok, 0))
    return out

def defines(*names):
    return [int(re.search(r'#define %s\s+(\w+)' % n, src).group(1), 0)
            for n in names]

def call_table(name):
    block = src[src.index('static const struct { int at, to; } %s' % name):]

    return [int(x, 0)
            for x in re.findall(r'\{ (0x[0-9a-f]+),', block[:block.index('};')])]

def holes(name):
    """Where gen.c says it fills an address in, as offsets into the array."""
    if name == 'startup_exit':
        return defines('STUB_CLEAR_AT', 'STUB_ARGS_AT', 'STUB_MAIN_AT',
                       'STUB_HOOK_CALL_AT')
    if name == 'startup_print':
        return (defines('STUB_CLEAR_AT', 'STUB_ARGS_AT', 'STUB_MAIN_AT',
                        'STUB_HOOK_CALL_AT')
                + call_table('print_calls'))

    return (call_table('args_calls')
            + defines('ARGS_ARGV_AT', 'ARGS_NAME_AT', 'ARGS_ARGV2_AT'))

end = max(img) + 1
spans = {
    'startup_exit':  (syms['_acc_startup_exit'], syms['_acc_startup_print']),
    'startup_print': (syms['_acc_startup_print'], syms['_acc_args']),
    'args_code':     (syms['_acc_args'], end),
}

bad = 0
for name, (lo, hi) in spans.items():
    want = [img[i] for i in range(lo, hi)]
    got = array(name)
    if len(want) != len(got):
        print('  FAIL %-14s the assembly is %d bytes, gen.c carries %d'
              % (name, len(want), len(got)))
        bad += 1
        continue
    allowed = set()
    holed = 0
    for h in holes(name):
        allowed |= {h, h + 1, h + 2}

        # A hole is three zero bytes in the array: gen.c writes the address
        # over them once it knows it. An offset that is one byte out points
        # at the opcode instead, so the array has the opcode there -- and
        # the patch then writes the address over the instruction. Which the
        # comparison below cannot see, because the opcode in the array is
        # the opcode in the assembly.
        if got[h:h + 3] != [0, 0, 0]:
            print('  FAIL %-14s the hole at %d is 0x%02x 0x%02x 0x%02x in '
                  'gen.c, not three zeros' % (name, h, *got[h:h + 3]))
            bad += 1
            holed = 1
    if holed:
        continue
    for i, (a, b) in enumerate(zip(want, got)):
        if a == b:
            continue
        if b == 0 and i in allowed:
            continue                    # a hole gen.c fills in
        print('  FAIL %-14s byte %d is 0x%02x in the assembly and 0x%02x in '
              'gen.c' % (name, i, a, b))
        bad += 1
        break
    else:
        print('  ok   %-14s %d bytes' % (name, len(got)))

sys.exit(1 if bad else 0)
PY

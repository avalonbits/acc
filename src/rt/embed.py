#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later
"""Turn assembled runtime helpers into a C table acc can emit.

The helpers are written as assembly and read as assembly. Nothing here is
transcribed by hand: this assembles the source, reads the symbols and the
bytes out of the object, and writes the header gen.c includes. Getting a byte
wrong in a table of opcodes is not something review catches, so the table is
not written by a person.

The routines share code -- the four signed and unsigned division entries are
one loop with four ways in -- so they are emitted as one blob with a table of
entry points, rather than one at a time. A program that uses any of them
carries all of them; at a few hundred bytes against the Agon's 448 KB that is
the cheaper trade than four copies of a division loop.

Sharing means internal calls, which means relocations, so those are extracted
too and acc applies them when it drops the blob in. Anything that is not a
call within the blob is rejected here: there is nothing else for acc to
resolve against.
"""
import os
import re
import subprocess
import sys


# The entries a program names itself, as C spells them, rather than calls
# the code generator writes: functions of the C library that C has no way to
# express, so that the library cannot have them as C.
C_NAMES = {'_setjmp', '_longjmp'}


def run(*args):
    return subprocess.run(args, capture_output=True, text=True, check=True).stdout


def main(asm, out, tools):
    obj = out + '.o'
    run(tools + '/ez80-none-elf-as', '-march=ez80+full', asm, '-o', obj)

    text = subprocess.run([tools + '/ez80-none-elf-objcopy', '-O', 'binary',
                           '--only-section=.text', obj, '/dev/stdout'],
                          capture_output=True, check=True).stdout

    # Internal references: each is a 24-bit address of somewhere in .text.
    fixups = []
    for line in run(tools + '/ez80-none-elf-readelf', '-r', obj).splitlines():
        m = re.match(r'^([0-9a-f]{8})\s+\S+\s+(\S+)\s+\S+\s+(\S+)\s*\+\s*(\S+)', line)
        if not m:
            continue
        at, kind, sym, addend = m.group(1), m.group(2), m.group(3), m.group(4)
        if kind != 'R_Z80_24' or sym != '.text':
            sys.exit('%s: a helper refers to %s, which acc has nothing to resolve '
                     'against' % (asm, sym))
        fixups.append((int(at, 16), int(addend, 16)))

    # Global symbols only: the entry points, in address order.
    # And the three places the blob may be cut, each of which only what is
    # below it is emitted for: acc_rt_machine after the prologue every
    # function calls, acc_rt_ops before the arithmetic, and acc_rt_split
    # before the long long routines.
    entries = []
    pieces = []
    cuts = {'acc_rt_machine': len(text), 'acc_rt_ops': len(text),
            'acc_rt_split': len(text)}
    for line in run(tools + '/ez80-none-elf-nm', obj).splitlines():
        parts = line.split()
        if len(parts) != 3:
            continue                    # undefined or absolute; not an entry
        addr, kind, name = parts
        if kind == 't' and not name.startswith('.'):
            pieces.append(int(addr, 16))    # shared code: a unit of its own
        if kind == 'T' and name.startswith('_acc_rt_'):
            entries.append((int(addr, 16), name[1:]))
        elif kind == 'T' and name in C_NAMES:
            entries.append((int(addr, 16), name[1:]))
        if kind == 'T' and name in cuts:
            cuts[name] = int(addr, 16)
    entries.sort()
    if cuts['acc_rt_ops'] > cuts['acc_rt_split']:
        sys.exit('%s: acc_rt_ops comes after acc_rt_split' % asm)
    if cuts['acc_rt_machine'] > cuts['acc_rt_ops']:
        sys.exit('%s: acc_rt_machine comes after acc_rt_ops' % asm)
    for at, to in fixups:
        for name, cut in cuts.items():
            if at < cut and to >= cut:
                sys.exit('%s: a helper above %s calls one below it, which '
                         'may not have been emitted' % (asm, name))

    groups = group_units(text, [a for a, _ in entries], pieces, fixups, tools,
                         asm)

    with open(out, 'w') as f:
        f.write('/* Generated from %s by %s. Do not edit.\n'
                ' *\n'
                ' * Each entry is one helper, whole and position independent.\n'
                ' *\n'
                ' * The bytes are the runtime acc copies into every program it\n'
                ' * compiles, and carry its license: LGPL-2.1-or-later with the\n'
                ' * linking exception written out at the top of %s, so that\n'
                ' * a program does not take on acc\'s license by being compiled by it.\n'
                ' */\n' % (asm, __file__.split('/')[-1], asm))
        f.write('#ifndef ACC_RT_HELPERS_H\n#define ACC_RT_HELPERS_H\n\n')
        f.write('static const unsigned char rt_code[] = {\n')
        for i in range(0, len(text), 12):
            f.write('    ' + ' '.join('0x%02x,' % b for b in text[i:i + 12]) + '\n')
        f.write('};\n\n')
        f.write('/* Where each entry point sits within rt_code. */\n')
        f.write('enum {\n')
        for _, name in entries:
            f.write('    RT_%s,\n' % name.upper().replace('ACC_RT_', ''))
        f.write('    RT_COUNT\n};\n\n')
        f.write('static const short rt_entry[RT_COUNT] = {\n')
        for addr, name in entries:
            f.write('    %d,   /* %s */\n' % (addr, name.replace('acc_rt_', '')))
        f.write('};\n\n')
        f.write('/* What each one is called. An object that uses a helper and\n'
                ' * does not carry the blob names it by this, and the link\n'
                ' * resolves it against the copy it lays down once. The\n'
                ' * assembly spells them with the leading underscore its\n'
                ' * toolchain puts on a C name; an object of acc\'s own does\n'
                ' * not, so they are written here the way C spells them. */\n')
        f.write('static const char *const rt_name[RT_COUNT] = {\n')
        for _, name in entries:
            f.write('    "%s",\n' % name)
        f.write('};\n\n')
        f.write('/* Calls from one routine to another: the address at `at` is the\n'
                ' * blob\'s base plus `to`. */\n')
        f.write('typedef struct { short at, to; } RtFix;\n\n')
        f.write('/* Where the blob may be cut. What is below a cut is\n'
                ' * emitted only when a program uses one of the helpers\n'
                ' * below it, so that a program which only prints does not\n'
                ' * carry the arithmetic. */\n')
        f.write('#define RT_MACHINE %d\n' % cuts['acc_rt_machine'])
        f.write('#define RT_OPS %d\n' % cuts['acc_rt_ops'])
        f.write('#define RT_SPLIT %d\n\n' % cuts['acc_rt_split'])
        # The pieces the blob is laid down in: see group_units.
        starts, entry_group, needs = groups
        ng = len(starts)
        nbytes = (ng + 7) // 8
        f.write('/* The groups the blob is laid down in, a program carrying only\n'
                ' * the ones it reaches: each is a run of the blob that nothing\n'
                ' * outside it jumps into relatively and that falls into nothing\n'
                ' * after it, from rt_group_start[g] to the next one\'s start. A\n'
                ' * group needs the ones its calls and addresses name, and those\n'
                ' * theirs: rt_group_needs is that, closed, as a set of bits. */\n')
        f.write('#define RT_NGROUPS %d\n' % ng)
        f.write('#define RT_NEED_BYTES %d\n\n' % nbytes)
        f.write('static const short rt_group_start[RT_NGROUPS + 1] = {\n')
        for st in starts + [len(text)]:
            f.write('    %d,\n' % st)
        f.write('};\n\n')
        f.write('static const unsigned char rt_entry_group[RT_COUNT] = {\n')
        for g in entry_group:
            f.write('    %d,\n' % g)
        f.write('};\n\n')
        f.write('static const unsigned char rt_group_needs[RT_NGROUPS][RT_NEED_BYTES] = {\n')
        for g in range(ng):
            bits = [0] * nbytes
            for h in needs[g]:
                bits[h // 8] |= 1 << (h % 8)
            f.write('    { ' + ', '.join('0x%02x' % b for b in bits) + ' },\n')
        f.write('};\n\n')
        f.write('#define RT_NFIX %d\n\n' % len(fixups))
        if fixups:
            f.write('static const RtFix rt_fix[RT_NFIX] = {\n')
            for at, to in sorted(fixups):
                f.write('    { %d, %d },\n' % (at, to))
            f.write('};\n\n')
            # And the group each end of each is in, so that placing them
            # is a lookup rather than a walk of the groups.
            gstart = starts + [len(text)]

            def group_at(a):
                g = 0
                while gstart[g + 1] <= a:
                    g += 1
                return g
            for which, pick in (('at', 0), ('to', 1)):
                f.write('static const unsigned char rt_fix_group_%s[RT_NFIX] = {\n'
                        % which)
                for fx in sorted(fixups):
                    f.write('    %d,\n' % group_at(fx[pick]))
                f.write('};\n\n')
        f.write('#endif /* ACC_RT_HELPERS_H */\n')

    print('[%s: %d bytes, %d entry points, %d internal calls]'
          % (out, len(text), len(entries), len(fixups)))


# Instructions after which nothing falls through into what follows.
ENDS = re.compile(r'^(ret|reti|retn|jp 0x[0-9a-f]+|jr 0x[0-9a-f]+|jp \((hl|ix|iy)\))$')
RELATIVE = re.compile(r'^(jr|djnz)\b.*?(0x[0-9a-f]+)$')


def group_units(text, entry_at, pieces, fixups, tools, asm):
    """The blob cut into groups a program can carry separately.

    Each entry point starts a unit, which runs to the next, and so does
    each piece of shared code with a label of its own -- the division loop
    the four ways of dividing call, the test of BC for zero. A unit that can
    fall into the next is one group with it; so is one that a relative jump
    goes between, with every unit in between, since the distance is in the
    jump. What is left is a run of groups that each stand alone but for
    their calls and absolute addresses, which are relocations and can go
    wherever the group they name is put.

    Read from a disassembly, so bytes that are data are read as
    instructions too: that can only join more units than need be, never
    fewer, except by ending a unit on something that looks like a return --
    and a unit that ends in data does not fall through."""
    starts = sorted(set([0] + entry_at + pieces))
    n = len(starts)
    bounds = starts + [len(text)]

    def unit_of(at):
        lo, hi = 0, n
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if starts[mid] <= at:
                lo = mid
            else:
                hi = mid
        return lo

    tmp = asm + '.bin'
    open(tmp, 'wb').write(text)
    dis = run(tools + '/ez80-none-elf-objdump', '-D', '-b', 'binary',
              '-m', 'ez80-adl', tmp)
    os.remove(tmp)
    ins = []
    for line in dis.splitlines():
        m = re.match(r'^\s*([0-9a-f]+):\t([0-9a-f ]+)\t(.*)$', line)
        if m:
            ins.append((int(m.group(1), 16), len(m.group(2).split()),
                        ' '.join(m.group(3).split()).replace(', ', ',')))

    parent = list(range(n))

    def find(u):
        while parent[u] != u:
            parent[u] = parent[parent[u]]
            u = parent[u]
        return u

    def join(a, b):
        for u in range(min(a, b), max(a, b)):
            parent[find(u + 1)] = find(u)

    last = {}
    for at, size, text_ in ins:
        u = unit_of(at)
        if at + size > bounds[u + 1]:
            join(u, u + 1)                      # an instruction across the line
        m = RELATIVE.match(text_)
        if m:
            join(u, unit_of(int(m.group(2), 16)))
        last[u] = text_
    for u in range(n - 1):
        if not ENDS.match(last.get(u, '')):
            join(u, u + 1)                      # falls into the next

    # Contiguous by construction: every join takes in everything between.
    roots = []
    group_of_unit = []
    for u in range(n):
        r = find(u)
        if not roots or roots[-1] != r:
            roots.append(r)
        group_of_unit.append(len(roots) - 1)
    ng = len(roots)
    gstarts = [starts[group_of_unit.index(g)] for g in range(ng)]

    direct = [set([g]) for g in range(ng)]
    for at, to in fixups:
        direct[group_of_unit[unit_of(at)]].add(group_of_unit[unit_of(to)])
    needs = []
    for g in range(ng):
        seen, todo = set(), [g]
        while todo:
            h = todo.pop()
            if h not in seen:
                seen.add(h)
                todo.extend(direct[h])
        needs.append(sorted(seen))

    entry_group = [group_of_unit[unit_of(a)] for a in entry_at]

    return gstarts, entry_group, needs


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2], sys.argv[3])

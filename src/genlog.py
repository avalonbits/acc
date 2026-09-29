#!/usr/bin/env python3
"""opt-acc's log of the calls the parser makes into the code generator.

Reads the prototypes in src/gen.h and writes src/genlog_calls.h, which
genlog.c and ssa.c include with one of these defined:

  GENLOG_RENAME    `#define name genlog_name` for every call logged, which
                   gen.h includes first in the parser's files (ACC_FRONT),
                   so that their calls go through the wrappers;
  GENLOG_OPS       the enum of the calls, GL_name, their names, and which
                   of their arguments are types;
  GENLOG_WRAPPERS  each wrapper: the call written to the log, then made;
  GENLOG_REPLAY    each call made again from its record (see genlog.c).

Which calls are logged is the parser's side of gen.h: every prototype there
but those listed in NOT_LOGGED, which the parser does not make inside a
function or which are not code generation at all.

    python3 src/genlog.py src/gen.h src/genlog_calls.h
"""
import re
import sys

NOT_LOGGED = {
    'gen_init', 'inline_has', 'prescan_begin', 'prescan_param',
    'prescan_body', 'prescan_claim', 'gen_iy_can',
    'gen_finish', 'gen_nfixups', 'gen_fixup_sym',
    'gen_no_address', 'gen_nexterns', 'rt_name_all', 'gen_rt_first',
    'gen_fixup_at', 'gen_externs_helpers', 'gen_extern_at',
    'gen_extern_sym', 'gen_startup', 'gen_nlate', 'gen_late_sym',
    'gen_nbss_fixups', 'gen_bss_fixup_at', 'gen_bss_len', 'gen_bss_offset',
}

# Scalars: stored in the record as a long.
SCALAR = {'int', 'Type', 'NameRef', 'unsigned char', 'long', 'uint32_t'}

# Queries whose pointers are written only when they answer yes, and only
# read then: nothing is kept of what they held going in, and the replay
# checks what they gave back only when the answer was yes.
OUT_WHEN_YES = {'vconst_top', 'vconst_wide'}


def parse_params(text):
    """(type, name) for each parameter in a prototype's parentheses."""
    text = text.strip()
    if text == 'void':
        return []
    params = []
    for param in text.split(','):
        param = ' '.join(param.split())
        match = re.match(r'(.*?)(\w+)$', param)
        params.append((match.group(1).strip(), match.group(2)))
    return params


def read_calls(header):
    """(return type, name, params) for each call in gen.h that is logged."""
    source = open(header).read()
    source = re.sub(r'/\*.*?\*/', '', source, flags=re.S)
    source = re.sub(r'#.*', '', source)
    calls = []
    for proto in re.findall(r'^[A-Za-z][^;{}()]*\([^;{}]*\)\s*;', source,
                            flags=re.M):
        proto = ' '.join(proto.split())
        match = re.match(r'(.*?)\b(\w+)\s*\((.*)\)\s*;$', proto)
        ret, name, args = match.group(1).strip(), match.group(2), match.group(3)
        if name not in NOT_LOGGED:
            calls.append((ret, name, parse_params(args)))

    # Two of out.h's, which the parser calls itself: a rewind of what it
    # wrote, and a seek to write earlier in the image.
    calls.append(('void', 'out_rewind', [('int', 'here')]))
    calls.append(('void', 'out_seek', [('int', 'here')]))
    return calls


def write_ops(out, calls):
    out.append('#ifdef GENLOG_OPS')
    out.append('enum {\n    GL_RAW = 1,')
    for ret, name, params in calls:
        out.append('    GL_%s,' % name)
    out.append('    GL_COUNT\n};')
    out.append('/* Which arguments of each are types, a bit each: see gl_stats. */')
    out.append('static const unsigned char gl_type_args[GL_COUNT] = {')
    out.append('    0, 0,')
    for ret, name, params in calls:
        mask = 0
        for at, (ptype, pname) in enumerate(params):
            if ptype == 'Type':
                mask |= 1 << at
        out.append('    %d,' % mask)
    out.append('};')
    out.append('static const char *const gl_names[GL_COUNT] = {')
    out.append('    NULL, "(raw bytes)",')
    for ret, name, params in calls:
        out.append('    "%s",' % name)
    out.append('};')
    out.append('#endif\n')


def write_wrappers(out, calls):
    out.append('#ifdef GENLOG_WRAPPERS')
    for ret, name, params in calls:
        decl = ', '.join('%s%s' % (ptype, pname) if ptype.endswith('*')
                         else '%s %s' % (ptype, pname)
                         for ptype, pname in params) or 'void'
        out.append('%s genlog_%s(%s)\n{' % (ret, name, decl))
        out.append('    GenRec *rec = gl_begin(GL_%s);' % name)
        passed = []
        pointers = []
        for at, (ptype, pname) in enumerate(params):
            passed.append(pname)
            if ptype in SCALAR:
                out.append('    if (rec) rec->arg[%d] = (long) %s;' % (at, pname))
            elif ptype == 'float':
                out.append('    if (rec) memcpy(&rec->arg[%d], &%s, sizeof %s);'
                           % (at, pname, pname))
            elif ptype == 'GenMark *':
                out.append('    if (rec) rec->arg[%d] = gl_mark_id(%s, GL_%s);'
                           % (at, pname, name))
            elif ptype == 'const char *' and name == 'gen_data':
                out.append('    if (rec) rec->arg[%d] = gl_keep(%s, len);' % (at, pname))
            elif ptype == 'const char *':
                out.append('    if (rec) rec->arg[%d] = (long) (intptr_t) %s;'
                           % (at, pname))
            elif ptype.endswith('*'):
                # Read as well as written, maybe: what it held going in is
                # kept too, and the replay starts it from that.
                if name not in OUT_WHEN_YES:
                    out.append('    if (rec) rec->arg[%d] = (long) *%s;' % (at, pname))
                pointers.append(pname)
            else:
                raise SystemExit('genlog.py: %s: no rule for %s' % (name, ptype))
        if len(params) > 6 or len(pointers) > 4:
            raise SystemExit('genlog.py: %s has too many arguments' % name)
        made = '%s(%s)' % (name, ', '.join(passed))
        if ret == 'void':
            out.append('    %s;' % made)
        else:
            out.append('    %s ret = %s;' % (ret, made))
        for at, pname in enumerate(pointers):
            if name in OUT_WHEN_YES:
                out.append('    if (rec && ret) rec->out[%d] = (long) *%s;' % (at, pname))
            else:
                out.append('    if (rec) rec->out[%d] = (long) *%s;' % (at, pname))
        if ret != 'void':
            if ret.endswith('*'):
                out.append('    if (rec) rec->ret = 0;')
            else:
                out.append('    if (rec) rec->ret = (long) ret;')
        out.append('    gl_end(rec);')
        if ret != 'void':
            out.append('\n    return ret;')
        out.append('}\n')
    out.append('#endif\n')


def write_replay(out, calls):
    out.append('#ifdef GENLOG_REPLAY')
    for ret, name, params in calls:
        out.append('case GL_%s: {' % name)
        passed = []
        nback = 0
        for at, (ptype, pname) in enumerate(params):
            if ptype in SCALAR:
                passed.append('(%s) rec->arg[%d]' % (ptype, at))
            elif ptype == 'float':
                out.append('    float %s;\n\n    memcpy(&%s, &rec->arg[%d], sizeof %s);'
                           % (pname, pname, at, pname))
                passed.append(pname)
            elif ptype == 'GenMark *':
                passed.append('gl_replay_mark(rec->arg[%d])' % at)
            elif ptype == 'const char *' and name == 'gen_data':
                passed.append('gl_kept(rec->arg[%d])' % at)
            elif ptype == 'const char *':
                passed.append('(const char *) (intptr_t) rec->arg[%d]' % at)
            elif ptype.endswith('*'):
                base = ptype[:-1].strip()
                out.append('    %s back%d = (%s) rec->arg[%d];' % (base, nback, base, at))
                passed.append('&back%d' % nback)
                nback += 1
        made = '%s(%s)' % (name, ', '.join(passed))
        if ret == 'void':
            out.append('    %s;' % made)
        elif ret.endswith('*'):
            out.append('    (void) %s;' % made)
        else:
            out.append('    gl_check_ret(rec, (long) %s);' % made)
        for at in range(nback):
            if name in OUT_WHEN_YES:
                out.append('    if (rec->ret)\n        gl_check_out(rec, %d, (long) back%d);'
                           % (at, at))
            else:
                out.append('    gl_check_out(rec, %d, (long) back%d);' % (at, at))
        out.append('    break;\n}')
    out.append('#endif')


def main(header, target):
    calls = read_calls(header)
    out = ['/* Generated by src/genlog.py from src/gen.h. Do not edit. */\n']
    out.append('#ifdef GENLOG_RENAME')
    for ret, name, params in calls:
        out.append('#define %s genlog_%s' % (name, name))
    out.append('#endif\n')
    write_ops(out, calls)
    write_wrappers(out, calls)
    write_replay(out, calls)
    open(target, 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])

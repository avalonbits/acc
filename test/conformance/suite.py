#!/usr/bin/env python3
# The conformance suite's bookkeeping: the manifest, what a refused test
# needs, the comparison of a run against the manifest, and the scoreboard.
# test/conformance/import.sh and test/conformance.sh do the compiling and
# the running and hand their results to this.
#
#   suite.py manifest <source> <revision> <work> > manifest.tsv
#   suite.py compare  <manifest> <observed>          strict both ways
#   suite.py update   <manifest> <observed>          take a run as the manifest
#   suite.py board    <manifest>                     the scoreboard, by clause
#   suite.py reclassify <manifest> <tests>           name what `unknown` rows need
#
# <work> holds what the import found, a line per test in each file, the
# test's name first:
#   filter.txt  `name ok` or `name no <clang's reason>`
#   ref.txt     `name <result>` for agondev's build: six hex digits, hang,
#               crash, or `compile ...`/`link ...` with the reason
#   census.txt  `name<TAB><constructs>`, from census.py
#   acc.txt     what acc did, in the shape observe writes it, below
#
# An observation -- acc.txt, or what test/conformance.sh saw -- is a line
# per test: `name pass`, `name fail <result>`, or `name needs <feature>
# <acc's message>`.
#
# The manifest is tab-separated, a row per test:
#   name  status  detail  clauses  census
# where status is pass, fail, needs or excluded, and the detail says what
# failed, what is needed, or why it was excluded.

import re
import sys
from collections import OrderedDict

sys.path.insert(0, __import__('os').path.dirname(__file__))
from census import clauses          # noqa: E402

GNU = re.compile(r'__attribute|__builtin_|__asm|\basm\b|__inline|__restrict|'
                 r'__extension__|__typeof|\btypeof\b|vector_size|__label__|'
                 r'__alignof|__FLT_|__DBL_|__INT\w*_TYPE__|__SIZEOF_|'
                 r'__UINT\w*_TYPE__|__signed__|__const\b|__volatile\b|'
                 r'__complex__|__real__|__imag__|\blink_error\b|\bmempcpy\b|'
                 r'__FUNCTION__|__PRETTY_FUNCTION__')


def needs(text, census_line, message):
    """What a test acc refuses is waiting on, named after the feature, so
    that building one moves a known set of rows."""
    code = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
    code = re.sub(r'//[^\n]*', '', code)
    if 'old-style-definition' in census_line:
        return 'k&r-definitions'
    if re.search(r'\blong\s+double\b', code):
        return 'long-double'
    if re.search(r'_Complex|__complex', code):
        return 'complex'
    if "parameter's array size is evaluated" in message:
        return 'array-size-side-effect'

    # The measuring device's own doing: a `define` in sources.txt maps
    # __builtin_strcmp to strcmp, and 921007-1 defines a static strcmp of
    # its own that takes nothing, and then spells the call as the builtin.
    m = re.search(r"'(\w+)' takes \d+ arguments?, and this call", message)
    if m and '__builtin_' + m.group(1) in code:
        return 'builtin-mapping'

    # A call with arguments to a function defined with none: undefined
    # (6.5.2.2p6), and acc may refuse it. 20051012-1.
    if m and re.search(r"takes 0 arguments, and this call gives it", message):
        return 'undefined-call'
    if GNU.search(code) or GNU.search(message):
        return 'gnu-extensions'

    # gcc's predefined macros -- __SIZE_MAX__, __CHAR16_TYPE__,
    # __GNUC_STDC_INLINE__ -- which a program that names them expects from
    # gcc and no C99 compiler has to give.
    if re.search(r"'__[A-Z][A-Z0-9_]*__' is not declared|#error __GNUC", message) \
       or re.search(r'\b__[A-Z][A-Z0-9_]*_TYPE__\b', code):
        return 'gnu-extensions'

    return 'unknown'


def source_text(tests, name):
    """A test's text and the text of what it #includes in quotes, which is
    part of what it is made of: pr71626-2 is pr71626-1 run again."""
    import os
    with open('%s/%s.c' % (tests, name), errors='replace') as f:
        text = f.read()
    for inc in re.findall(r'#\s*include\s*"([^"]+)"', text):
        path = os.path.join(tests, inc)
        if os.path.exists(path):
            with open(path, errors='replace') as f:
                text += f.read()
    return text


def read_pairs(path, split=None):
    rows = OrderedDict()
    with open(path) as f:
        for line in f:
            line = line.rstrip('\n')
            if not line:
                continue
            name, _, rest = line.partition('\t' if split == 'tab' else ' ')
            rows[name] = rest
    return rows


def manifest(source, revision, work, tests):
    filt = read_pairs(work + '/filter.txt')
    ref = read_pairs(work + '/ref.txt')
    census = read_pairs(work + '/census.txt', 'tab')
    acc = read_pairs(work + '/acc.txt')

    print('# The conformance suite: a row per test in %s at %s.' % (source,
                                                                    revision))
    print('# Written by test/conformance/import.sh; test/conformance.sh '
          'checks a run against it.')
    print('# name\tstatus\tdetail\tclauses\tcensus')
    for name in filt:
        found = census.get(name, '')
        cl = ' '.join(clauses(found.split()))
        verdict = filt[name]
        if verdict != 'ok':
            status, detail = 'excluded', 'not C99: ' + verdict[3:]
        elif ref.get(name, 'missing') != '00':
            status, detail = 'excluded', 'agondev: ' + ref.get(name, 'missing')
        else:
            status, _, detail = acc.get(name, 'needs unknown not run').partition(' ')
            if status == 'needs':
                feature, _, message = detail.partition(' ')
                if feature == '?':
                    feature = needs(source_text(tests, name), found, message)
                detail = feature + ' ' + message if feature == 'unknown' \
                    else feature
        detail = detail.replace('\t', ' ')[:120]
        print('\t'.join([name, status, detail, cl, found]))


def load_manifest(path):
    rows = OrderedDict()
    with open(path) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            name, status, detail, cl, found = (line.rstrip('\n').split('\t')
                                               + [''] * 5)[:5]
            rows[name] = dict(status=status, detail=detail, clauses=cl,
                              census=found)
    return rows


def expected(row):
    """What a run has to see for a row to hold: the status, and for a fail
    the result, and for needs the feature."""
    if row['status'] == 'fail':
        return 'fail ' + row['detail']
    if row['status'] == 'needs':
        return 'needs ' + row['detail'].split(' ')[0]
    return row['status']


def observed(line):
    status, _, rest = line.partition(' ')
    if status == 'fail':
        return 'fail ' + rest
    if status == 'needs':
        return 'needs ' + rest.split(' ')[0]
    return status


def compare(path, obs_path):
    rows = load_manifest(path)
    obs = read_pairs(obs_path)
    bad = 0
    for name, row in rows.items():
        if row['status'] == 'excluded':
            continue
        want = expected(row)
        got = observed(obs.get(name, 'missing'))
        if got.startswith('needs ?'):
            got = 'needs ' + row['detail'].split(' ')[0] \
                if row['status'] == 'needs' else 'needs (refused)'
        if want == got:
            continue
        bad += 1
        if row['status'] == 'pass':
            why = 'a regression'
        elif got == 'pass':
            why = 'passes now: update the manifest'
        else:
            why = 'changed: update the manifest if this is right'
        print('  FAIL %-24s manifest says %s, run says %s -- %s'
              % (name, want, got, why))
    ran = sum(1 for r in rows.values() if r['status'] != 'excluded')
    print('  %d of %d held' % (ran - bad, ran))
    return bad


def update(path, obs_path):
    with open(path) as f:
        head = [l for l in f if l.startswith('#')]
    rows = load_manifest(path)
    obs = read_pairs(obs_path)
    out = list(head)
    for name, row in rows.items():
        if row['status'] != 'excluded' and name in obs:
            status, _, detail = obs[name].partition(' ')
            if status == 'needs' and detail.startswith('?'):
                detail = row['detail'] if row['status'] == 'needs' \
                    else 'unknown' + detail[1:]
            row['status'], row['detail'] = status, detail.replace('\t', ' ')[:120]
        out.append('\t'.join([name, row['status'], row['detail'],
                              row['clauses'], row['census']]) + '\n')
    with open(path, 'w') as f:
        f.writelines(out)


def reclassify(path, tests):
    """Rows waiting on `unknown`, looked at again with what needs() knows
    now: their message is kept in the row for this."""
    with open(path) as f:
        head = [l for l in f if l.startswith('#')]
    rows = load_manifest(path)
    out = list(head)
    for name, row in rows.items():
        if row['status'] == 'needs' and row['detail'].startswith('unknown'):
            message = row['detail'][len('unknown '):]
            feature = needs(source_text(tests, name), row['census'], message)
            if feature != 'unknown':
                row['detail'] = feature
        out.append('\t'.join([name, row['status'], row['detail'],
                              row['clauses'], row['census']]) + '\n')
    with open(path, 'w') as f:
        f.writelines(out)


def board(path):
    rows = load_manifest(path)
    by_clause = {}
    for row in rows.values():
        kind = row['status']
        if kind == 'needs':
            kind = 'needs ' + row['detail'].split(' ')[0]
        for c in row['clauses'].split():
            by_clause.setdefault(c, {}).setdefault(kind, 0)
            by_clause[c][kind] += 1

    def key(c):
        return [int(x) if x.isdigit() else x for x in c.split('.')]

    total = {}
    for row in rows.values():
        total[row['status']] = total.get(row['status'], 0) + 1
    print('  %d tests: %s' % (len(rows), ', '.join(
        '%d %s' % (n, s) for s, n in sorted(total.items()))))
    print()
    print('  %-10s %6s %6s %6s %6s %9s  %s' % ('clause', 'tests', 'pass',
                                               'fail', 'needs', 'excluded',
                                               'waiting on'))
    holes = []
    for c in sorted(by_clause, key=key):
        k = by_clause[c]
        n = sum(k.values())
        need = sum(v for s, v in k.items() if s.startswith('needs'))
        waiting = ', '.join('%s %d' % (s[6:], v) for s, v in sorted(k.items())
                            if s.startswith('needs'))
        print('  %-10s %6d %6d %6d %6d %9d  %s' % (
            c, n, k.get('pass', 0), k.get('fail', 0), need,
            k.get('excluded', 0), waiting))
        if not k.get('pass'):
            holes.append(c)
    if holes:
        print()
        print('  nothing passes for: ' + ', '.join(holes))


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'manifest':
        manifest(*sys.argv[2:6])
    elif cmd == 'compare':
        sys.exit(1 if compare(sys.argv[2], sys.argv[3]) else 0)
    elif cmd == 'update':
        update(sys.argv[2], sys.argv[3])
    elif cmd == 'board':
        board(sys.argv[2])
    elif cmd == 'reclassify':
        reclassify(sys.argv[2], sys.argv[3])
    elif cmd == 'needs':
        with open(sys.argv[2], errors='replace') as f:
            text = f.read()
        print(needs(text, sys.argv[3], sys.argv[4]))
    else:
        sys.exit(__doc__ or 'usage: see the top of suite.py')


if __name__ == '__main__':
    main()

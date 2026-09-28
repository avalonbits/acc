#!/bin/bash
# Stack frames past the size the eZ80 addresses cheaply, in acc as agondev
# builds it.
#
# `(IX + d)` takes a signed byte, so a local more than 128 bytes into a frame
# cannot be reached with one instruction: the compiler computes its address
# -- `ld bc, -139; lea hl, ix + 0; add hl, bc` -- on every access to every
# local past the line, and each is bytes of acc.bin, which on the Agon is
# heap. Nothing in the C says so; the usual cause is a buffer added to a
# function, or a cold helper with one inlined into a caller.
#
# The fix is not to make the buffer static: that moves it into the image's
# bss, out of the heap, where on the stack it is in the room the stack keeps
# anyway. Give it a function of its own instead (see copy_member in archive.c).
#
# A frame may pass 128 only when it holds nothing but its buffer, or is on
# the list below with the size it has now: the list only shrinks.
#
# Needs agondev. Skips (77) without it.
set -uo pipefail
cd "$(dirname "$0")/.."

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
[ -x "$CC" ] || { echo "  [no agondev: frame check skipped]"; exit 77; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
for f in $(sed -n 's/^SRCS = //p; /^       src/p' Makefile.agon | tr -d '\\'); do
    # shellcheck disable=SC2046
    "$CC" $(make -s -f Makefile.agon cflags) \
        -S "$f" -o "$tmp/$(basename "$f" .c).s" || exit 1
done

exec python3 - "$tmp" <<'PY'
import glob, re, sys

# Functions over 128 bytes that are a buffer given a function of its own,
# and how many addresses each computes: the buffer's, and the spill slots
# clang puts past it -- which no order of the declarations moves. At most
# what they compute now.
BUFFER_ONLY = {'copy_member': 6, 'do_include': 2}

# The frames still over 128, at their size now. lex_file_marks, error_pos
# and acc_error are a buffer and a few locals; given their buffers a
# function of their own, as copy_member was, acc.bin came out 11 bytes
# bigger, so they stay.
ALLOW = {
    'lex_file_marks': 1039,
    'decl_direct': 356,
    'error_pos': 268,
    'acc_error': 268,
}

bad = 0
seen = {}
computed = {}
for path in sorted(glob.glob(sys.argv[1] + '/*.s')):
    fn, size = None, 0
    for line in open(path):
        m = re.match(r'^_([A-Za-z0-9_]+):', line)
        if m:
            fn = m.group(1)
            continue
        if fn and re.match(r'\s+lea\s+hl, ix \+ 0', line):
            computed[fn] = computed.get(fn, 0) + 1
        m = re.match(r'\s+ld\s+hl, -(\d+)', line)
        if m:
            size = int(m.group(1))
            continue
        if re.match(r'\s+call\s+__frameset$', line) and fn and size > 128:
            seen[fn] = size
for fn, size in sorted(seen.items(), key=lambda x: -x[1]):
    if fn in BUFFER_ONLY:
        # Its buffers' addresses, and nothing else of its reached that way.
        if computed.get(fn, 0) > BUFFER_ONLY[fn]:
            print(f'  FAIL frames: {fn} is past 128 and computes '
                  f'{computed[fn]} addresses, more than its {BUFFER_ONLY[fn]}')
            bad += 1
        continue
    if fn not in ALLOW:
        print(f'  FAIL frames: {fn} has a {size}-byte frame, past 128')
        bad += 1
    elif size > ALLOW[fn]:
        print(f'  FAIL frames: {fn} has a {size}-byte frame, more than its {ALLOW[fn]}')
        bad += 1
for fn in ALLOW:
    if fn not in seen:
        print(f'  FAIL frames: {fn} is no longer past 128 -- take it off the list')
        bad += 1
if not bad:
    print(f'  frames: none past 128 but {len(ALLOW)} on the list and {len(BUFFER_ONLY)} buffer')
sys.exit(1 if bad else 0)
PY

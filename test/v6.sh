#!/bin/bash
# ACC version 6 objects that acc does not write itself: the relocation kinds,
# the second table of them, and alignment, which are for assembly -- zap
# writes such objects for C to call. test/objv6.py writes them from the
# format as src/obj.c describes it, and a C program checks every value the
# linker made against the addresses it can see for itself, linked from the
# objects as they are and from an archive of them, which takes items rather
# than objects. Then what the format forbids, each of which acc must refuse.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0
ok()  { pass=$((pass + 1)); }
bad() { printf '  FAIL %-40s %s\n' "$1" "$2"; fail=$((fail + 1)); }

PYTHONPATH=test python3 - "$tmp" <<'PYEOF'
import sys
from objv6 import Obj
tmp = sys.argv[1]

# The table, and what can be asked of its address a byte at a time. Machine
# code by hand: ld hl, 0 / ld l, n / ret returns n in HL, which is an int.
def byte_fn(o, name, kind, target, addend=None, slot=0):
    at = len(o.text)
    o.item(at)
    o.define(name, at, func=True)
    o.text += bytes([0x21, 0, 0, 0, 0x2e, slot, 0xc9])
    o.reloc(at + 5, kind, target, addend)

o = Obj()
o.item(0, align=8)                                  # a page-aligned table
o.define('_table', 0)
o.text += bytes([1, 2, 3, 4])
byte_fn(o, '_low_of_table', 'LOW8', 'text', slot=0)  # its offset, in the slot
byte_fn(o, '_high_past_page', 'HIGH8', '_table', addend=0x1ff)
byte_fn(o, '_upper_of_table', 'UPPER8', '_table', addend=0)
byte_fn(o, '_low_before', 'LOW8', '_table', slot=0xff)  # -1, wrapped
at = len(o.text)
o.item(at)
o.define('_table_word', at)                          # dw _table
o.text += bytes([0, 0])
o.reloc(at, 'ABS16', '_table')
at = len(o.text)
o.item(at)
o.define('_table_at', at)                            # dl _table + 2
o.text += bytes([2, 0, 0])
o.reloc(at, 'ABS24', '_table')
o.bss_len = 16
o.bss_align = 8
o.define('_buf', 0, bss=True)
o.write(tmp + '/tab.o')

# jr to an assembly-only name in another object: no underscore, so C cannot
# name it, and it links all the same.
o = Obj()
o.item(0)
o.define('_jumper', 0, func=True)
o.text += bytes([0x18, 0])
o.reloc(1, 'PCREL8', 'target')
o.write(tmp + '/jr.o')

o = Obj()
o.item(0)
o.define('target', 0, func=True)
o.text += bytes([0x21, 42, 0, 0, 0xc9])
o.write(tmp + '/target.o')

# And a jr that cannot reach: 200 bytes between it and where it goes.
o = Obj()
o.item(0)
o.define('_far', 0, func=True)
o.text += bytes([0x18, 0]) + bytes(200)
o.reloc(1, 'PCREL8', 'target')
o.write(tmp + '/far.o')

# What the format forbids, each on its own.
def broken(name, fix):
    o = Obj()
    o.item(0)
    o.define('_x', 0, func=True)
    o.text += bytes(8)
    fix(o)
    o.write(tmp + '/' + name + '.o')

n3 = lambda v: (v & 0xffffff).to_bytes(3, 'little')
broken('kind', lambda o: o.reloc(0, 6, 'text'))
broken('high_no_addend', lambda o: setattr(o, 'raw',
       ([n3(0) + n3(0 | 2 << 20)], [])))
broken('order', lambda o: setattr(o, 'raw',
       ([n3(4) + n3(0), n3(0) + n3(0)], [])))
broken('both', lambda o: setattr(o, 'raw',
       ([n3(0) + n3(0)], [n3(0) + n3(1 << 20) + n3(0)])))
o = Obj()
o.item(0)
o.item(3, align=4)                                   # at 3, to be on 16
o.define('_x', 0, func=True)
o.text += bytes(8)
o.write(tmp + '/misaligned.o')
PYEOF

cat > "$tmp/main.c" <<'CEOF'
extern unsigned char table[4];
extern unsigned short table_word;
extern unsigned char *table_at;
extern unsigned char buf[16];
int low_of_table(void);
int high_past_page(void);
int upper_of_table(void);
int low_before(void);
int jumper(void);

int main(void) {
    unsigned t = (unsigned) table;
    int r = 0;

    if ((t & 0xff) == 0 && table[2] == 3) r++;          /* aligned, and there */
    if (low_of_table() == (t & 0xff)) r++;
    if (high_past_page() == (((t + 0x1ff) >> 8) & 0xff)) r++;
    if (upper_of_table() == ((t >> 16) & 0xff)) r++;
    if (low_before() == ((t - 1) & 0xff)) r++;
    if (table_word == (t & 0xffff) && table_at == table + 2) r++;
    if (((unsigned) buf & 0xff) == 0) r++;               /* the bss, aligned */
    if (jumper() == 42) r++;

    return r + 34;              /* 8 checks */
}
CEOF
"$ACC" -c "$tmp/main.c" -o "$tmp/main.o" >/dev/null 2>&1 || bad "main.c" "did not compile"

# Linked from the objects, and from an archive of them.
"$ACC" -a "$tmp/asm.a" "$tmp/tab.o" "$tmp/jr.o" "$tmp/target.o" >/dev/null 2>&1
for how in objects archive; do
    if [ $how = objects ]; then
        err=$("$ACC" "$tmp/main.o" "$tmp/tab.o" "$tmp/jr.o" "$tmp/target.o" -o "$tmp/$how.bin" -x 2>&1)
    else
        err=$("$ACC" "$tmp/main.o" "$tmp/asm.a" -o "$tmp/$how.bin" -x 2>&1)
    fi
    if [ $? -ne 0 ]; then
        bad "the kinds, linked from $how" "$(printf '%s' "$err" | head -1)"
    elif emu_available >/dev/null 2>&1; then
        test/agon.sh "$tmp/$how.bin" >/dev/null 2>&1; got=$?
        [ $got -eq 42 ] && ok || bad "the kinds, linked from $how" "returned $got"
    else
        ok
    fi
done

# Refused, with what is wrong said.
refuses() {
    local what=$1 want=$2; shift 2
    if out=$("$ACC" "$@" -o "$tmp/r.bin" -x 2>&1); then
        bad "$what" "it was linked"
    elif ! printf '%s' "$out" | grep -q -- "$want"; then
        bad "$what" "$(printf '%s' "$out" | head -1)"
    else
        ok
    fi
}
refuses "a kind acc does not know" "of kind 6" "$tmp/main.o" "$tmp/kind.o"
refuses "HIGH8 without an addend of its own" "HIGH8 or UPPER8" "$tmp/main.o" "$tmp/high_no_addend.o"
refuses "relocations out of order" "out of order" "$tmp/main.o" "$tmp/order.o"
refuses "a slot in both tables" "two relocations at" "$tmp/main.o" "$tmp/both.o"
refuses "an item that cannot be aligned" "cannot be where it is" "$tmp/main.o" "$tmp/misaligned.o"
refuses "a jr that cannot reach" "relative jump" "$tmp/main.o" "$tmp/tab.o" "$tmp/jr.o" "$tmp/far.o" "$tmp/target.o"

# acc's own objects keep the order too, for a function big enough that its
# long jumps are merged into the table of relocations in their hundreds:
# four hundred returns, most too far from the end to shrink, with a global
# read in every fifth for them to be merged among. (The merge once went out
# of order when the table moved down in memory as it grew. Whether it moves
# is the allocator's to say, so that is tested in test/test_out.c, which
# makes it move; this is the whole path, object to image.)
awk 'BEGIN {
    print "int g = 1;";
    print "static int f(int a) {";
    for (k = 1; k <= 400; k++)
        if (k % 5) printf "    if (a == %d) return %d;\n", k, k % 7;
        else       printf "    if (a == %d) return g + %d;\n", k, k % 7;
    print "    return 99;";
    print "}";
    print "int main(void) { return f(5) == 6 && f(399) == 0 && f(1000) == 99 ? 42 : 1; }";
}' > "$tmp/jumps.c"
if "$ACC" -c "$tmp/jumps.c" -o "$tmp/jumps.o" >/dev/null 2>&1 \
   && err=$("$ACC" "$tmp/jumps.o" -o "$tmp/jumps.bin" -x 2>&1); then
    if emu_available >/dev/null 2>&1; then
        test/agon.sh "$tmp/jumps.bin" >/dev/null 2>&1; got=$?
        [ $got -eq 42 ] && ok || bad "a table grown while jumps merge" "returned $got"
    else
        ok
    fi
else
    bad "a table grown while jumps merge" "$(printf '%s' "${err:-it did not compile}" | head -1)"
fi

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

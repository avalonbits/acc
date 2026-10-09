#!/bin/bash
# volatile: every access the program makes to a volatile object is made --
# none left out because nothing uses its value, none taken from a register
# that held it, none merged with another -- by acc and by opt-acc, in each
# of the ways opt-acc makes code. No program can tell from what it
# computes: nothing on the Agon writes memory behind its back in a test.
# So the accesses are counted in the object, disassembled.
#
#   test/volatile.sh                    # bin/acc and bin/opt-acc
set -u
cd "$(dirname "$0")/.."
export ASAN_OPTIONS=detect_leaks=0

ACC=${ACC:-bin/acc}
OPT=${OPT:-bin/opt-acc}
AGONDEV=${AGONDEV:-$HOME/agondev}
OBJDUMP=$AGONDEV/bin/ez80-none-elf-objdump
[ -x "$OBJDUMP" ] || { echo "no objdump at $OBJDUMP -- skipped"; exit 77; }
[ -x "$ACC" ] && [ -x "$OPT" ] || { echo "$ACC or $OPT missing -- run make"; exit 2; }

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0

FULL="OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_HOMES=2 OPTACC_IY=1 OPTACC_NATIVE=1 OPTACC_LEAF=1 OPTACC_INLINE=1 OPTACC_PEEP=1 OPTACC_MIR=1"
WAYS=("acc|$ACC|"
      "opt-acc|$OPT|"
      "opt-acc full|$OPT|$FULL"
      "machine IR forced|$OPT|$FULL OPTACC_PICK=0"
      "leaf forced|$OPT|OPTACC_SSA=1 OPTACC_REGS=1 OPTACC_IY=1 OPTACC_NATIVE=1 OPTACC_LEAF=1 OPTACC_PICK=0")

# The object's code, disassembled, one instruction a line.
disasm() {
    python3 - "$1" "$tmp/t.bin" <<'PY'
import sys
d = open(sys.argv[1], 'rb').read()
n3 = lambda at: d[at] | d[at + 1] << 8 | d[at + 2] << 16
at = 31 + n3(13) * 7 + n3(16) * 6 + n3(28) * 9 + n3(19) * 12 + n3(25) * 3 + n3(22)
open(sys.argv[2], 'wb').write(d[at:at + n3(7)])
PY
    "$OBJDUMP" -D -b binary -m ez80-adl "$tmp/t.bin" | sed -n 's/^ *[0-9a-f]*:\t[0-9a-f ]*\t//p'
}

# accesses <name> <count> <regex> <source>: in every way, exactly <count>
# instructions match the extended regex.
accesses() {
    local what=$1 want=$2 re=$3 way label cc envs got

    printf '%s\n' "$4" > "$tmp/c.c"
    for way in "${WAYS[@]}"; do
        IFS='|' read -r label cc envs <<< "$way"
        rm -f "$tmp/c.o"
        if ! env $envs "$cc" -c "$tmp/c.c" -o "$tmp/c.o" >/dev/null 2>&1; then
            printf '  FAIL %-44s %s: could not compile it\n' "$what" "$label"
            fail=$((fail + 1)); continue
        fi
        got=$(disasm "$tmp/c.o" | grep -Ec "$re")
        if [ "$got" = "$want" ]; then
            pass=$((pass + 1))
        else
            printf '  FAIL %-44s %s: %s accesses, want %s\n' "$what" "$label" "$got" "$want"
            fail=$((fail + 1))
        fi
    done
}

# decided <name> <ssa|first|none> <yes|no> <source>: what opt-acc, everything
# on, does with f -- tries its SSA form (`ssa`) or keeps the first pass's
# code (`first`) -- and whether the peephole pass looks at it. A function
# touching volatile keeps the first pass's code where the SSA form's
# backends would get it wrong (a volatile read thrown away); a volatile
# local stays in its slot, the rest of the function made as ever; and the
# peephole pass, which cannot tell such an access from another, leaves any
# such function alone.
decided() {
    local what=$1 want_ssa=$2 want_peep=$3 out got_ssa=first got_peep=no

    printf '%s\n' "$4" > "$tmp/c.c"
    rm -f "$tmp/c.o"
    out=$(env $FULL OPTACC_SSA_STATS=1 OPTACC_PEEP_STATS=1 "$OPT" -c "$tmp/c.c" -o "$tmp/c.o" 2>&1)
    printf '%s\n' "$out" | grep -q '^ssa f ' && got_ssa=ssa
    printf '%s\n' "$out" | grep -q '^peep ' && got_peep=yes
    if [ "$got_ssa" = "$want_ssa" ] && [ "$got_peep" = "$want_peep" ]; then
        pass=$((pass + 1))
    else
        printf '  FAIL %-44s want %s and peephole %s, got %s and %s\n' "$what" \
            "$want_ssa" "$want_peep" "$got_ssa" "$got_peep"
        fail=$((fail + 1))
    fi
}

decided "nothing volatile: SSA and the peephole" ssa yes \
    'struct s { int a; int m; }; int f(struct s *p) { return p->m; }'
decided "a volatile member read: SSA, no peephole" ssa no \
    'struct s { int a; volatile int m; }; int f(struct s *p) { return p->m; }'
decided "a volatile bit-field read: SSA, no peephole" ssa no \
    'struct s { int a; volatile unsigned b : 3; }; int f(struct s *p) { return p->b; }'
decided "an anonymous volatile struct's member: likewise" ssa no \
    'struct s { int a; volatile struct { int x; }; }; int f(struct s *p) { return p->x; }'
decided "a volatile seed read once, as perf.h's" ssa no \
    'static volatile unsigned long seed = 5; unsigned long f(int n) { unsigned long s = seed; while (n--) s = s * 3 + 1; return s; }'
decided "a volatile local: SSA, the local in its slot" ssa no \
    'int f(void) { volatile int x = 1; return x + x; }'
decided "stores through a volatile pointer: SSA, no peephole" ssa no \
    'void f(volatile int *p, int v) { *p = v; *p = v + 1; }'
decided "a volatile read thrown away: the first pass's code" first no \
    'volatile int g; void f(void) { g; }'
decided "a volatile pointer's read thrown away: likewise" first no \
    'void f(volatile int *p) { (void) *p; }'

# A global read for nothing, and read again after a store.
accesses "a volatile global read, its value unused" 1 'ld (hl|a),\(0x0' \
    'volatile int g; void f(void) { g; }'
accesses "stored and then read twice: three accesses" 3 '\(0x0+\)|,\(hl\)|\(hl\),' \
    'volatile int g; int f(void) { g = 1; return g + g; }'

# Through a pointer: each read and each store.
accesses "*p + *p reads twice" 2 'ld [a-z]+,\((hl|iy\+0)\)' \
    'int f(volatile int *p) { return *p + *p; }'
accesses "*p = 1; *p = 2 stores twice" 2 'ld \((hl|iy\+0)\),' \
    'void f(volatile int *p) { *p = 1; *p = 2; }'
accesses "(void) *p reads" 1 'ld [a-z]+,\((hl|iy\+0)\)' \
    'void f(volatile int *p) { (void) *p; }'
accesses "p[1] + p[1] reads twice" 2 'ld [a-z]+,\((hl|iy\+3)\)' \
    'int f(volatile int *p) { return p[1] + p[1]; }'

# A local: in memory, and every access made.
accesses "a volatile local stored twice and read twice" 4 '\(ix-3\)' \
    'int f(void) { volatile int x = 1; x = 2; return x + x; }'
accesses "a volatile local read for nothing" 2 '\(ix-3\)' \
    'int f(void) { volatile int x = 1; x; return 0; }'

# A member declared volatile, a typedef of one, a cast to one, and a
# pointer that is itself volatile.
accesses "a volatile member read twice" 2 'ld [a-z]+,\((hl|iy\+3)\)' \
    'struct s { int a; volatile int m; }; int f(struct s *p) { return p->m + p->m; }'
accesses "through a typedef of a volatile byte" 2 'ld \((hl|iy\+0)\),' \
    'typedef volatile unsigned char vbyte; void f(vbyte *p) { *p = 0; *p = 0; }'
accesses "through a cast to a volatile pointer" 2 'ld \(hl\),0x0*5' \
    'void f(void) { *(volatile unsigned char *) 0x1234 = 5; *(volatile unsigned char *) 0x1234 = 5; }'
accesses "a volatile pointer read each time it is used" 2 'ld [a-z]+,\(0x0+\)' \
    'int *volatile gp; int f(void) { return *gp + *gp; }'

# volatile reached through a declarator's star, a typedef and a cast: each
# a local whose accesses would otherwise be taken as known.
accesses "a local pointer that is itself volatile" 3 '\(ix-3\)' \
    'int f(int *q) { int *volatile p = q; return *p + *p; }'
accesses "a local of a volatile typedef" 4 '\(ix-3\)' \
    'typedef volatile int vint; int f(void) { vint x = 1; x = 2; return x + x; }'
accesses "a local written and read through a volatile cast" 3 '\(ix-3\)|,\(hl\)|\(hl\),' \
    'int f(void) { int x = 1; *(volatile int *) &x = 2; return *(volatile int *) &x; }'
accesses "an element's address taken through a volatile cast" 3 '\(ix-3\)|,\(hl\)|\(hl\),' \
    'int f(void) { int x = 1; *&((volatile int *) &x)[0] = 2; return *&((volatile int *) &x)[0]; }'

# Each kind of object declared volatile: a parameter, a local array, a local
# struct, a static local and a static at file scope.
accesses "a volatile parameter read twice" 2 '\(ix\+6\)' \
    'int f(volatile int x) { return x + x; }'
accesses "a volatile local array's element" 3 '\(ix-6\)|,\(hl\)|\(hl\),' \
    'int f(void) { volatile int a[2]; a[0] = 1; return a[0] + a[0]; }'
accesses "a volatile local struct's member" 3 '\(ix-3\)|,\(hl\)|\(hl\),' \
    'struct s { int a; }; int f(void) { volatile struct s v; v.a = 1; return v.a + v.a; }'
accesses "a static volatile local" 3 '\(0x0+\)|,\(hl\)|\(hl\),' \
    'int f(void) { static volatile int s; s = 1; return s + s; }'
accesses "a volatile global read twice, nothing stored" 2 '\(0x0+\)|,\(hl\)|\(hl\),' \
    'volatile int g; int f(void) { return g + g; }'
accesses "a static volatile at file scope" 3 '\(0x0+\)|,\(hl\)|\(hl\),' \
    'static volatile int s; int f(void) { s = 1; return s + s; }'

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

#!/bin/bash
# Moslets: programs MOS runs from /mos, loaded at 0x0B0000 with the 32 KB
# up to 0x0B8000 for everything -- built with -moslet by acc and opt-acc on
# the host, and by acc on the Agon, and each run from /mos.
#
# The program allocates, writes and reads the heap, and prints its
# argument and whether its stack and its heap are in the moslet's 32 KB.
# One whose stack started at 0x0C0000, in MOS's own memory, printed its
# answer and took the machine down on the way back: every run after the
# first checks that the one before it came back.
#
#   test/moslet.sh [acc.bin]        # default bin/acc.bin
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC_BIN=${1:-bin/acc.bin}

emu_available || exit 77
[ -x bin/acc ] && [ -f bin/libc.a ] || { echo "run make first" >&2; exit 2; }

sd=$(emu_card); host=$(mktemp -d); trap '[ -n "${KEEP:-}" ] && echo "kept $host $sd" || rm -rf "$sd" "$host"' EXIT
pass=0; fail=0

cat > "$host/ml.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>

static const char *where(void *spot)
{
    unsigned at = (unsigned) spot;

    return at >= 0xB0000 && at < 0xB8000 ? "in" : "out";
}

int main(int argc, char **argv)
{
    char *p = malloc(100);
    int i, s = 0;

    for (i = 0; i < 100; i++)
        p[i] = (char) i;
    for (i = 0; i < 100; i++)
        s += p[i];
    printf("moslet %s %d stack %s heap %s\n", argc > 1 ? argv[1] : "-", s,
           where(&s), where(p));
    return 0;
}
EOF
want="%s 4950 stack in heap in"

ok()  { printf '  ok   %s\n' "$1"; pass=$((pass + 1)); }
bad() { printf '  FAIL %-40s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# Built on the host by each compiler, and the image's first bytes: a jump
# to 0x0B0045, past MOS's header, and the header's "MOS" at 0x40.
mkdir -p "$sd/mos"
for cc in acc opt-acc; do
    [ -x "bin/$cc" ] || continue
    if ! bin/$cc "$host/ml.c" -moslet -o "$sd/mos/m$cc.bin" >/dev/null 2>&1; then
        bad "$cc -moslet" "does not build"
        continue
    fi
    head=$(od -An -tx1 -N4 "$sd/mos/m$cc.bin" | tr -d ' \n')
    magic=$(dd if="$sd/mos/m$cc.bin" bs=1 skip=64 count=3 2>/dev/null)
    if [ "$head" = "c345000b" ] && [ "$magic" = MOS ]; then
        ok "$cc -moslet jumps to 0x0B0045"
    else
        bad "$cc -moslet jumps to 0x0B0045" "$head $magic"
    fi
done
[ -f "$sd/mos/mopt-acc.bin" ] && mv "$sd/mos/mopt-acc.bin" "$sd/mos/mopt.bin"

# And acc on the Agon, building one into /mos itself.
agon=0
if [ -f "$ACC_BIN" ]; then
    agon=1
    cp "$ACC_BIN" "$sd/bin/acc.bin"
    mkdir -p "$sd/lib/acc"
    cp bin/libc.a bin/rt.a "$sd/lib/acc/"
    cp -r include "$sd/lib/acc/include"
    cp "$host/ml.c" "$sd/ml.c"
fi

# Too big for a moslet: said so, where the same program is fine at 0x040000.
printf 'static char big[40000];\nint main(void) { return big[1]; }\n' > "$host/big.c"
if bin/acc "$host/big.c" -moslet -o "$host/big.bin" 2>&1 | grep -q 'has 32768 for both'; then
    ok "too big for a moslet, said so"
else
    bad "too big for a moslet, said so" "no error"
fi
if bin/acc "$host/big.c" -o "$host/big.bin" >/dev/null 2>&1; then
    ok "and fine as a program"
else
    bad "and fine as a program" "refused"
fi

{
    printf 'echo RUN macc\r\ntry macc acc\r\n'
    [ -f "$sd/mos/mopt.bin" ] && printf 'echo RUN mopt\r\ntry mopt opt\r\n'
    if [ "$agon" = 1 ]; then
        printf 'echo RUN agon\r\ntry acc ml.c -moslet -o /mos/magon.bin\r\n'
        printf 'try magon agon\r\n'
    fi
} > "$sd/autoexec.txt"
ACC_EMU_PROMPT=1 ACC_EMU_TIMEOUT=180 emu_run "$sd" -z -u > "$host/console.txt" 2>&1
tr -d '\r' < "$host/console.txt" > "$host/out.txt"

for run in acc opt agon; do
    [ "$run" = opt ] && [ ! -f "$sd/mos/mopt.bin" ] && continue
    [ "$run" = agon ] && [ "$agon" = 0 ] && { echo "  [no $ACC_BIN: the Agon build is skipped]"; continue; }
    line=$(printf "moslet $want" "$run")
    if grep -qxF "$line" "$host/out.txt"; then
        ok "the $run moslet runs from /mos"
    else
        bad "the $run moslet runs from /mos" \
            "$(grep -a '^moslet' "$host/out.txt" | head -1)"
    fi
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

#!/bin/bash
# Compiles a C program on the Agon, with acc running on the Agon, and runs it.
#
# This is the whole point of the project in one test. acc.bin is put on an
# emulated sdcard together with libagon.a, MOS boots, acc compiles and links a
# source file there, and the binary it produced is then run.
#
# Everything else in the suite runs acc on the host. Only this says whether it
# works on the machine -- and the three bugs it has caught so far were all
# invisible to a host build: a shift undefined at 24 bits, namespace flags that
# do not fit in a token, and a loop agondev miscompiles.
set -uo pipefail
cd "$(dirname "$0")/.."

EMU=${ACC_EMU:-$HOME/code/fab-agon-emulator}
BIN=$EMU/target/release/agon-cli-emulator
MOS=${ACC_MOS:-$EMU/sdcard/MOS.bin}
AGONDEV=${AGONDEV:-$HOME/agondev}
TIMEOUT=${ACC_EMU_TIMEOUT:-300}

[ -x "$BIN" ]               || { echo "  skip no emulator (set ACC_EMU)"; exit 0; }
[ -f "$MOS" ]               || { echo "  skip no MOS.bin (set ACC_MOS)"; exit 0; }
[ -f "$AGONDEV/lib/libagon.a" ] || { echo "  skip no libagon.a (set AGONDEV)"; exit 0; }
[ -f bin/acc.bin ]          || { echo "  skip bin/acc.bin missing (make -f Makefile.agon)"; exit 0; }

sd=$(mktemp -d); trap 'rm -rf "$sd"' EXIT
mkdir -p "$sd/bin" "$sd/lib"
cp "$MOS" "$sd/MOS.bin"
cp bin/acc.bin "$sd/bin/"
cp "$AGONDEV/lib/libagon.a" "$sd/lib/"
DELAY="06 03 21 00 00 00 2b 7c b5 20 fb 10 f5"
python3 test/moshdr.py exit_ok.bin "$sd/bin/exit_ok.bin" $DELAY af d3 00 c9

cat > "$sd/t.c" <<'CEOF'
int printf(const char *, ...);
int fact(int n) { int r = 1; while (n > 1) { r = r * n; n = n - 1; } return r; }
int main(void) {
    int i;
    for (i = 1; i < 9; i++) printf("%d! = %d\r\n", i, fact(i));
    return 0;
}
CEOF

# Compile and link on the machine, then run what came out.
printf 'acc -nostdlib -o t.bin t.c -L/lib -lagon\r\nt\r\nexit_ok\r\n' > "$sd/autoexec.txt"

out=$(cd "$EMU" && timeout "$TIMEOUT" "$BIN" --sdcard "$sd" -z -u 2>/dev/null)
status=$?
[ $status -eq 124 ] && { echo "  FAIL timed out after ${TIMEOUT}s"; exit 1; }

got=$(printf '%s\n' "$out" \
  | sed -e '/Tom.s Fake VDP/d' -e '/unknown packet VDU/d' \
        -e '/Agon Console8 MOS Version/d' -e '/Emulator shutdown triggered/d' \
        -e '/warning:/d' \
  | tr -cd '\11\12\15\40-\176' | tr -d '\r' | sed '/^[[:space:]]*$/d')

want='1! = 1
2! = 2
3! = 6
4! = 24
5! = 120
6! = 720
7! = 5040
8! = 40320'

if [ ! -f "$sd/t.bin" ]; then
    echo "  FAIL acc produced no binary on the Agon"
    printf '%s\n' "$out" | tail -6 | sed 's/^/         /'
    exit 1
fi

if [ "$got" = "$want" ]; then
    echo "  ok   acc compiled and linked on the Agon ($(stat -c%s "$sd/t.bin") bytes), and it ran"
else
    echo "  FAIL output from the Agon-built program differs"
    diff <(printf '%s\n' "$want") <(printf '%s\n' "$got") | sed 's/^/         /' | head -20
    exit 1
fi

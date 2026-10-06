#!/bin/bash
# The runtime's long routines that take registers -- lib/rt/lr*.s, a long
# in E:UHL and A:UBC as agondev's take one -- run on the Agon against acc's
# long operators, which call the routines that take addresses: the same
# answers, and BC, D and IY kept. test/rtlong/glue.s makes them callable
# from C; test/rtlong/check.c walks the operands and answers 42 where all
# agree, printing any that do not.
#
# Needs zap (bin/zap, which make builds) and the emulator. Skips (77)
# without the emulator.
set -u

cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] && [ -x bin/zap ] || { echo "run make first" >&2; exit 2; }
emu_available >/dev/null 2>&1 || { echo "  [no emulator: the long routines are not run]"; exit 77; }
export ASAN_OPTIONS=detect_leaks=0

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
bin/zap test/rtlong/glue.s "$tmp/glue.o" -f acc >/dev/null || exit 2
"$ACC" -c test/rtlong/check.c -o "$tmp/check.o" -Iinclude >/dev/null || exit 2
"$ACC" "$tmp/check.o" "$tmp/glue.o" -o "$tmp/check.bin" -x >/dev/null || exit 2

sd=$(emu_card); trap 'rm -rf "$tmp" "$sd"' EXIT
cp "$tmp/check.bin" "$sd/bin/p.bin"
printf 'bin/p\r\n' > "$sd/autoexec.txt"
out=$(ACC_EMU_TIMEOUT=180 emu_run "$sd" -z -u 2>&1); rc=$?

if [ "$rc" -eq 42 ]; then
    echo "  ok   the long routines that take registers agree with acc's operators"
    exit 0
fi
echo "  FAIL the long routines that take registers: answered $rc"
printf '%s\n' "$out" | tr -d '\r' | grep -a '^op ' | head -20 | sed 's/^/         /'
exit 1

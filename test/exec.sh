#!/bin/bash
# Compiles each program in test/exec with acc, runs it on the Agon, and
# compares what it printed against the expectation in its own header.
#
# A code generator cannot be reviewed into correctness. Every bug in this
# backend so far -- a displacement added twice, a dereference clobbering the
# register the destination address was in, a compare that destroyed the value
# a switch was still testing -- produced code that assembled, linked and
# disassembled plausibly, and was caught by running it.
#
# The expectation is the lines between "/* expect:" and "*/" at the top of the
# source, so a test is one file with nothing to keep in step.
set -uo pipefail
cd "$(dirname "$0")/.."

pass=0; fail=0; skipped=0
for src in test/exec/*.c; do
    name=$(basename "$src" .c)
    want=$(sed -n '/^\/\* expect:$/,/^\*\/$/p' "$src" | sed '1d;$d')

    if ! out=$(test/accld.sh "$src" /tmp/acc_exec.bin 2>&1); then
        case $? in
          77) skipped=$((skipped+1)); printf '  skip %s (no toolchain)\n' "$name"; continue ;;
          *)  fail=$((fail+1)); printf '  FAIL %s (build)\n%s\n' "$name" "$(printf '%s' "$out" | sed 's/^/         /')"; continue ;;
        esac
    fi

    got=$(test/agon.sh /tmp/acc_exec.bin 2>&1)
    case $? in
      77) skipped=$((skipped+1)); printf '  skip %s (no emulator)\n' "$name"; continue ;;
      124) fail=$((fail+1)); printf '  FAIL %s (timed out)\n' "$name"; continue ;;
    esac

    # The programs print CRLF, because that is what the Agon's console wants.
    got=$(printf '%s' "$got" | tr -d '\r' | sed '/^[[:space:]]*$/d')
    want=$(printf '%s' "$want" | sed '/^[[:space:]]*$/d')

    if [ "$got" = "$want" ]; then
        pass=$((pass+1)); printf '  ok   %s\n' "$name"
    else
        fail=$((fail+1)); printf '  FAIL %s\n' "$name"
        diff <(printf '%s\n' "$want") <(printf '%s\n' "$got") \
            | sed 's/^/         /' | head -20
    fi
done
rm -f /tmp/acc_exec.bin /tmp/acc_exec.o

printf '  %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skipped"
[ "$fail" -eq 0 ]

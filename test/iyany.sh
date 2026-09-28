#!/bin/bash
# Every case in test/cases with a local of each function kept in IY.
#
# A local declared register lives in IY (see iy_local in src/vstack.c), and
# the programs that declare one are few. ACC_IY_ANY, which only the host's
# acc reads, gives IY to the first local of every function that could have
# one, register or not, and test/run.sh then holds each case to agondev as
# it always does -- every load, store, step, call and backend use of IY in
# the corpus, with a local in IY around it.
#
# A local whose address is taken cannot be in IY, and nothing stops the
# first one being such a local here: those cases stop at "internal: the
# address of the local in IY", which is expected and counted, not failed.
# Anything else that fails is a bug.
#
# About twenty-five minutes: it is not in `make test`. Run it after changing
# how locals, calls or IY are compiled.
set -uo pipefail
cd "$(dirname "$0")/.."

log=$(mktemp); trap 'rm -f "$log"' EXIT
ACC_IY_ANY=1 test/run.sh > "$log" 2>&1
taken=$(grep -c 'the address of the local in IY' "$log")
bad=$(grep -A1 '  FAIL' "$log" | grep -v '^--' | paste - - \
      | grep -vc 'the address of the local in IY')
tail -1 "$log"
echo "  $taken with the address of the first local taken, $bad other failures"
if [ "$bad" -ne 0 ]; then
    grep -A3 '  FAIL' "$log" | grep -v 'the address of the local in IY' | head -30
    exit 1
fi

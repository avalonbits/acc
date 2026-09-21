#!/bin/bash
# What `make` leaves behind, against what the tests then run.
#
# Every scripted test defaults to $(BIN)/acc-asan and none of them build it,
# so if the ordinary build does not either, `make && ACC=bin/acc-asan
# test/relax.sh` runs whatever the last full `make test` left there. That is
# a compiler without the change under test, and it reports green. The way it
# bites hardest is on a new test: break the line it covers, watch the test
# pass anyway, and conclude the test does not cover it.
#
# Asked of make rather than of the clock: -n is a dry run and -B makes it
# name everything it would build, so nothing is compiled and no file is
# touched to find out. Doing it by timestamp would mean touching a source
# in the middle of a test run, which is its own way to corrupt one.
set -uo pipefail

cd "$(dirname "$0")/.."

pass=0; fail=0

check() {
    if [ "$2" = "$3" ]; then
        printf '  ok   %-34s %s\n' "$1" "$3"; pass=$((pass + 1))
    else
        printf '  FAIL %-34s %s, wanted %s\n' "$1" "$2" "$3"; fail=$((fail + 1))
    fi
}

plan=$(make -Bn all 2>/dev/null)

# The compilers the tests are pointed at, and the library they link against.
for want in bin/acc-asan bin/acc bin/libc.a; do
    case $plan in
      *"$want"*) got=built ;;
      *)         got=missing ;;
    esac
    check "\`all\` builds $want" "$got" built
done

# And nothing the Makefile hands to a test is built by `test` alone: a
# binary that only the test target knows how to make is the same trap with
# a different name.
for b in $(grep -oE 'ACC=\$\(BIN\)/[a-z-]+' Makefile | sed 's|.*/||' | sort -u); do
    case $plan in
      *"bin/$b"*) got=built ;;
      *)          got=missing ;;
    esac
    check "a test runs bin/$b, and \`all\`" "$got" built
done

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

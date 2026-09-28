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

# And that the Agon build is told about the same headers the host build is.
#
# Its objects listed only src/acc.h, so a changed header rebuilt the host
# compiler and not the Agon one, and target.sh reported programs where the
# builds disagreed -- correctly, about a compiler that had not been
# recompiled. Compared as lists rather than by building anything, because
# finding out by timestamp means touching a source in the middle of a run.
host_hdrs=$(sed -n 's/^HDR *= *//p' Makefile | tr ' ' '\n' | grep -v '^\\*$' | sort -u)
agon_hdrs=$(sed -n 's/^HDRS *= *//p' Makefile.agon | tr ' ' '\n' | grep -v '^\\*$' | sort -u)
check "both builds list the same headers" \
      "$(printf '%s' "$agon_hdrs" | md5sum)" "$(printf '%s' "$host_hdrs" | md5sum)"

case $(sed -n 's/^\$(OBJ)\/%.o: *//p' Makefile.agon) in
  *'$(HDRS)'*) got=yes ;;
  *)           got=no ;;
esac
check "the Agon objects depend on them" "$got" yes

# A member taken out of the library. Every prerequisite left is as old as it
# was, so the library was not made again and kept the member that had gone,
# and its object stayed in bin/lib for test/lib.sh to archive. Done on a
# copy of the build in a directory of its own, with LIBSRC given on the
# command line, so the tree and the real bin are not touched.
if [ -x bin/acc ] && [ -f bin/libc.a ]; then
    tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
    mkdir "$tmp/bin"
    cp -a bin/acc bin/zap bin/libc.a bin/lib "$tmp/bin/"
    members=$(make -s --no-print-directory -f Makefile -f - print-LIBSRC \
              <<<'print-%: ; @echo $($*)')
    asm=$(make -s --no-print-directory -f Makefile -f - print-LIBASM \
          <<<'print-%: ; @echo $($*)')      # the library's assembly, with it
    fewer=$(printf '%s\n' $members | grep -v '^lib/timer\.c$' | tr '\n' ' ')
    mk() { make -s --no-print-directory BIN="$tmp/bin" "$@" "$tmp/bin/libc.a" 2>&1; }

    mk >/dev/null
    check "an unchanged library is left alone" "$(mk)" ""
    touch "$tmp/bin/lib/gone.o"
    case $(mk LIBSRC="$fewer") in
      *"from $(( $(wc -w <<<"$members $asm") - 1 )) objects"*) got=remade ;;
      *)                                                  got="left alone" ;;
    esac
    check "a member taken out remakes it" "$got" remade
    [ -e "$tmp/bin/lib/timer.o" ] && got=kept || got=removed
    check "and the member's object goes" "$got" removed
    [ -e "$tmp/bin/lib/gone.o" ] && got=kept || got=removed
    check "as does any other that is not one" "$got" removed
fi

printf '  %d passed, %d failed\n' "$pass" "$fail"
[ "$fail" -eq 0 ]

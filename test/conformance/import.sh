#!/bin/bash
# Imports the conformance suite's tests from the sources in
# test/conformance/sources.txt, each at its pinned revision, and writes a
# manifest for each: test/conformance/<source>.tsv. See
# docs/c99-conformance-plan.md for what each step is for.
#
#   test/conformance/import.sh [--from <checkout>] [--only <source>]
#
# For each source:
#   1. its tests fetched into test/conformance/cache/<source>, which git
#      ignores -- a shallow, sparse fetch of the pinned revision, or with
#      --from a copy out of a checkout already at it;
#   2. each asked of agondev's clang with the source's filter flags: one
#      clang refuses is not strict C99, and is excluded with its reason;
#   3. each that is, built by agondev and run on the emulator: one that does
#      not return 0 there assumes something this machine does not give it,
#      and is excluded too -- its answer is the reference, and a test with
#      no right answer here has none;
#   4. a census of every test, excluded or not (census.py);
#   5. what acc does with the rest (observe.sh);
# and then the manifest (suite.py). Run once per import, or when a source's
# pin or flags change; test/conformance.sh checks acc against the manifest.
#
# Needs agondev, git and the emulator.
set -uo pipefail
cd "$(dirname "$0")/../.."
. test/emu.sh
. test/conformance/batch.sh
. test/conformance/observe.sh

from= only=
while [ $# -gt 0 ]; do
    case $1 in
      --from) from=$2; shift 2 ;;
      --only) only=$2; shift 2 ;;
      *) echo "usage: $0 [--from <checkout>] [--only <source>]" >&2; exit 2 ;;
    esac
done

AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
LD=$AGONDEV/bin/ez80-none-elf-ld
[ -x "$CC" ] || { echo "no agondev at $AGONDEV" >&2; exit 2; }
emu_available || { echo "no emulator" >&2; exit 2; }
make -s >/dev/null || exit 2

CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc
        -isystem $AGONDEV/include -target ez80-none-elf -Oz -Wa,-march=ez80+full"

import_source() {
    local name=$1 url=$2 rev=$3 dir=$4 filter=$5 defines=$6 also=$7 expect=$8
    local kinds=$9 split=${10}
    local cache=test/conformance/cache/$name work tests have d paths

    echo "[$name at ${rev:0:12}]"

    # 1. The tests, at the pinned revision, and the directories beside them
    # that some of them include from: their C sources and headers, and what
    # the tests have to print, when the source says.
    tests=$cache/$dir
    paths=
    for d in $dir $also; do
        paths="$paths /$d/*.c /$d/*.h"
    done
    [ -n "$expect" ] && paths="$paths /$dir/*.c$expect"
    [ -n "$split" ] && paths="/$dir/*"          # all of it, for the script
    have=$(git -C "$cache" rev-parse HEAD 2>/dev/null || true)
    if [ "$have" != "$rev" ] && [ ! -f "$cache/.from-$rev" ]; then
        rm -rf "$cache"
        if [ -n "$from" ]; then
            [ "$(git -C "$from" rev-parse HEAD)" = "$rev" ] ||
                { echo "$from is not at $rev" >&2; return 2; }
            for d in $dir $also; do
                mkdir -p "$cache/$d"
                cp "$from/$d"/*.[ch] "$cache/$d/" 2>/dev/null
            done
            [ -n "$split" ] && cp "$from/$dir"/* "$cache/$dir/" 2>/dev/null
            [ -n "$expect" ] && cp "$from/$dir"/*.c"$expect" "$tests/"
            touch "$cache/.from-$rev"
        else
            git init -q "$cache"
            git -C "$cache" remote add origin "$url"
            git -C "$cache" sparse-checkout set --no-cone $paths || return 2
            git -C "$cache" fetch -q --depth 1 --filter=blob:none origin "$rev" ||
                return 2
            git -C "$cache" checkout -q FETCH_HEAD || return 2
        fi
    fi

    # A source whose files are not its tests as they stand: its script
    # writes the tests, into split/ beside them.
    if [ -n "$split" ]; then
        python3 "test/conformance/$split" "$tests" "$cache/split" || return 2
        tests=$cache/split
    fi

    work=$(mktemp -d)
    ls "$tests" | sed -n 's/\.c$//p' | sort > "$work/all"
    [ -n "$expect" ] && echo "  what each prints is checked against its .c$expect"

    # What kind each test is, when not all of them are run: gcc.dg's
    # directives say, through dg.py.
    : > "$work/kinds.txt"
    if [ "$kinds" = dg ]; then
        (cd "$tests" && python3 "$OLDPWD/test/conformance/dg.py" \
            $(sed 's/$/.c/' "$work/all")) > "$work/kinds.txt"
        echo "  $(cut -f2 "$work/kinds.txt" | sort | uniq -c | awk '{ printf "%s %s, ", $1, $2 }' | sed 's/, $//')"
    fi
    echo "  $(wc -l < "$work/all") tests"

    # 2. Strict C99, as clang says. A test that has to be refused is one
    # when clang refuses it too, at one of the lines the test says: an
    # error in C99, and not only under the test's own options.
    export IMPORT_CC=$CC IMPORT_CFLAGS=$CFLAGS IMPORT_FILTER=$filter
    export IMPORT_TESTS=$tests IMPORT_WORK=$work
    xargs -P 8 -n 1 bash -c '
        n=$1 kind=run flags= lines=
        row=$(grep -m1 "^$n	" "$IMPORT_WORK/kinds.txt")
        if [ -n "$row" ]; then
            kind=$(printf "%s" "$row" | cut -f2)
            flags=$(printf "%s" "$row" | cut -f3)
            lines=$(printf "%s" "$row" | cut -f4)
        fi
        err=$($IMPORT_CC $IMPORT_CFLAGS $IMPORT_FILTER $flags -c "$IMPORT_TESTS/$n.c" \
              -o "$IMPORT_WORK/$n.f.o" 2>&1)
        refused=$?
        first=$(printf "%s" "$err" | grep -m1 "error:")
        if [ "$kind" = other ]; then
            echo "$n no not a test that is run or compiled"
        elif [ "$kind" = reject ]; then
            at=$(printf "%s" "$first" | sed -n "s/^[^:]*:\([0-9]*\):.*/\1/p")
            if [ "$refused" -eq 0 ]; then
                echo "$n no an error only under its own options: clang takes it"
            elif printf " %s " "$lines" | grep -q " ${at:-x} "; then
                echo "$n ok"
            else
                echo "$n no clang refuses it at line ${at:-?}, which it does not name"
            fi
        elif [ "$refused" -eq 0 ]; then
            echo "$n ok"
        else
            echo "$n no $(printf "%s" "$first" | sed "s/.*error: //")"
        fi > "$IMPORT_WORK/$n.filter"' _ < "$work/all"
    cat "$work"/*.filter | sort > "$work/filter.txt"
    echo "  $(grep -c ' ok$' "$work/filter.txt") strict C99"

    # 3. Right on this machine, as agondev's build says.
    for f in start shim; do
        "$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full \
            "test/conformance/refkit/$f.s" -o "$work/$f.o" || return 2
    done
    # Only the tests that are run have an answer to check; one that has to
    # compile, or to be refused, has had its say from clang.
    awk -F'\t' '$2 != "run" { print $1 }' "$work/kinds.txt" | sort > "$work/notrun"
    awk '$2 == "ok" { print $1 }' "$work/filter.txt" > "$work/c99all"
    comm -23 "$work/c99all" "$work/notrun" > "$work/c99"
    export IMPORT_LD=$LD IMPORT_DEFINES=$defines IMPORT_LIB=$AGONDEV/lib
    xargs -P 8 -n 1 bash -c '
        n=$1 w=$IMPORT_WORK
        flags=$(grep -m1 "^$n	" "$w/kinds.txt" | cut -f3)
        if ! err=$($IMPORT_CC $IMPORT_CFLAGS -w $IMPORT_DEFINES $flags \
                   -c "$IMPORT_TESTS/$n.c" -o "$w/$n.r.o" 2>&1); then
            echo "$n compile $(printf "%s" "$err" | grep -m1 "error:" | sed "s/.*error: //")"
        elif ! err=$($IMPORT_LD --oformat binary -Ttext=0x40000 -e _start \
                   --defsym __stack=0xB0000 --defsym ___heaptop=0xAC000 \
                   --defsym ___heapbot=_end -o "$w/$n.r.bin" "$w/start.o" \
                   "$w/shim.o" "$w/$n.r.o" -L"$IMPORT_LIB" -lagon 2>&1); then
            echo "$n link $(printf "%s" "$err" | grep -m1 -o "undefined reference to .*")"
        fi > "$w/$n.ref"' _ < "$work/c99"
    : > "$work/reflist"
    : > "$work/ref.txt"
    while read -r n; do
        if [ -s "$work/$n.ref" ]; then
            cat "$work/$n.ref" >> "$work/ref.txt"
        else
            echo "$n $work/$n.r.bin" >> "$work/reflist"
        fi
    done < "$work/c99"
    mkdir "$work/refprinted"
    batch_run "$work/reflist" "$work/refran" "$work/refprinted" || return 2
    comm -12 "$work/c99all" "$work/notrun" | sed 's/$/ 00/' >> "$work/ref.txt"
    while read -r n r; do
        [ "$r" = 00 ] && ! printed_right "$tests" "$n" "$expect" "$work/refprinted" &&
            r=output
        echo "$n $r"
    done < "$work/refran" >> "$work/ref.txt"
    echo "  $(grep -c ' 00$' "$work/ref.txt") right under agondev"

    # 4. What each is made of.
    (cd "$tests" && python3 "$OLDPWD/test/conformance/census.py" \
        $(sed 's/$/.c/' "$work/all")) | sort > "$work/census.txt"

    # 5. And what acc does with the ones that hold.
    awk '$2 == "00" { print $1 }' "$work/ref.txt" | sort > "$work/hold"
    observe "$tests" "$work/hold" "$defines" "$work/acc.txt" "$expect" \
        "$( [ "$kinds" = dg ] && echo "$work/kinds.txt")" || return 2
    echo "  $(grep -c ' pass' "$work/acc.txt") pass under acc"

    python3 test/conformance/suite.py manifest "$name" "$rev" "$work" "$tests" \
        > "test/conformance/$name.tsv"
    rm -rf "$work"
    echo "  wrote test/conformance/$name.tsv"
}

# sources.txt: a `source` line, then its `filter` and `define` lines.
name=
flush() {
    if [ -n "$name" ] && { [ -z "$only" ] || [ "$name" = "$only" ]; }; then
        import_source "$name" "$url" "$rev" "$dir" "$filter" "$defines" "$also" \
            "$expect" "$kinds" "$split"
    fi
}
while read -r word rest; do
    case $word in
      source)
        flush || exit $?
        read -r name url rev dir <<< "$rest"
        filter= defines= also= expect= kinds= split= ;;
      kinds) kinds=$rest ;;
      split) split=$rest ;;
      also) also="$also $rest" ;;
      expect) expect=$rest ;;
      filter) filter=$rest ;;
      define) defines=$rest ;;
    esac
done < <(grep -v '^\s*#' test/conformance/sources.txt)
flush

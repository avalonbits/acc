#!/bin/bash
# The C99 conformance suite: every test test/conformance/import.sh took in,
# compiled and run by acc and held to its row in the manifest,
# test/conformance/<source>.tsv. See docs/c99-conformance-plan.md.
#
#   test/conformance.sh             run, check, and print the scoreboard
#   test/conformance.sh --check     run and check, as `make test` does
#   test/conformance.sh --update    run, and take what it saw as the manifest
#   test/conformance.sh --board     only the scoreboard, from the manifests
#
# Strict both ways. A test the manifest has passing that does not is a
# regression; a test it has failing, or waiting on a feature, that now
# passes fails the run too, until the manifest is updated -- which is how
# landing a feature shows up, as rows moving to pass. The scoreboard is by
# clause of the standard: how many tests cover each, how many pass, and
# what the others are waiting on.
#
# Skips (77) when the tests have not been imported, since they are fetched
# and not kept in the tree.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh
. test/conformance/batch.sh
. test/conformance/observe.sh

mode=${1:-run}
status=0

while read -r word name url rev dir; do
    [ "$word" = source ] || continue
    manifest=test/conformance/$name.tsv
    tests=test/conformance/cache/$name/$dir
    awk -v n="$name" '$1 == "source" { on = ($2 == n) }
                      on && $1 == "split" { found = 1 } END { exit !found }' \
        test/conformance/sources.txt && tests=test/conformance/cache/$name/split
    [ -f "$manifest" ] || { echo "  [no $manifest: run test/conformance/import.sh]"; exit 77; }

    echo "[$name at ${rev:0:12}]"
    if [ "$mode" != --board ]; then
        if [ ! -d "$tests" ]; then
            echo "  [its tests are not imported: run test/conformance/import.sh]"
            exit 77
        fi
        emu_available || exit 77
        make -s >/dev/null || exit 2
        defines=$(awk -v n="$name" '$1 == "source" { on = ($2 == n) }
                                    on && $1 == "define" { $1 = ""; print }' \
                  test/conformance/sources.txt)
        expect=$(awk -v n="$name" '$1 == "source" { on = ($2 == n) }
                                   on && $1 == "expect" { print $2 }' \
                 test/conformance/sources.txt)
        work=$(mktemp -d)
        awk -F'\t' '!/^#/ && $2 != "excluded" { print $1 }' "$manifest" > "$work/names"
        kinds=
        if awk -v n="$name" '$1 == "source" { on = ($2 == n) }
                             on && $1 == "kinds" && $2 == "dg" { found = 1 }
                             END { exit !found }' test/conformance/sources.txt; then
            kinds=$work/kinds.txt
            (cd "$tests" && python3 "$OLDPWD/test/conformance/dg.py" \
                $(sed 's/$/.c/' "$work/names")) > "$kinds"
        fi
        observe "$tests" "$work/names" "$defines" "$work/seen" "$expect" "$kinds" ||
            exit 2
        if [ "$mode" = --update ]; then
            python3 test/conformance/suite.py update "$manifest" "$work/seen"
            echo "  updated $manifest"
        else
            python3 test/conformance/suite.py compare "$manifest" "$work/seen" ||
                status=1
        fi
        rm -rf "$work"
    fi
    [ "$mode" = --check ] && continue
    echo
    python3 test/conformance/suite.py board "$manifest"
done < test/conformance/sources.txt

exit $status

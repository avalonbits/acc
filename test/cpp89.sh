#!/bin/bash
# The preprocessor against Decus CPP's C89 conformance suite.
#
# Decus CPP is Martin Minow's 1984 preprocessor, still maintained, and it
# ships 181 tests written against ANSI X3.159-1989 section 3.8. They are in
# the public domain, and they cover ground acc's own macro.sh and include.sh
# do not: argument prescan and the three different arguments a parameter has
# depending on whether `#`, `##` or nothing is in front of it; what a `//`
# comment does to the `/*` inside it; a backslash that joins two lines in the
# middle of a name; the operators an `#if` has to evaluate and the ones it
# has to leave alone.
#
# What is here is those tests, ported. Each is a program that answers 0 when
# the preprocessor did its job, so the port renames its main and adds one
# that turns that into acc's 42. test/cpp89/NOT-PORTED says which of the 181
# are not here and why -- trigraphs, __VA_OPT__, and the ones that read the
# preprocessor's output rather than running a program, which acc has no flag
# to show.
#
# Every ported program was checked against gcc before it was trusted: the
# original answers 0 and the port answers 42, or it is not here.
set -uo pipefail
cd "$(dirname "$0")/.."

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make"; exit 2; }

export ASAN_OPTIONS=detect_leaks=0

# A sanitizer report is a failure even when the answer is right, as it is in
# run.sh and for the same reason: the preprocessor allocates for every
# expansion, and reading freed memory can still give 42.
sanitizer_tripped() {
    case $1 in
      *"AddressSanitizer"*|*"runtime error:"*|*"UndefinedBehaviorSanitizer"*)
          return 0 ;;
    esac
    return 1
}

tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
pass=0; fail=0; skip=0

for src in test/cpp89/*.c test/cpp89/*/t.c; do
    [ -e "$src" ] || continue
    dir=$(dirname "$src")
    name=$(basename "$dir")
    inc=(-I "$dir")
    if [ "$name" = cpp89 ]; then
        name=$(basename "$src" .c)          # a test that is one file
    else
        # A test with headers has a directory of its own, and any directory
        # inside that is one an #include is meant to be looked for in --
        # which for a few of these is the thing being tested.
        for sub in "$dir"/*/; do
            [ -d "$sub" ] && inc+=(-I "$sub")
        done
    fi

    if ! err=$("$ACC" "$src" "${inc[@]}" -o "$tmp/t.bin" -x 2>&1); then
        printf '  FAIL %-48s %s\n' "$name" \
            "$(printf '%s' "$err" | head -1)"
        fail=$((fail+1)); continue
    fi
    if sanitizer_tripped "$err"; then
        printf '  FAIL %-48s the sanitizer tripped\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        fail=$((fail+1)); continue
    fi

    test/agon.sh "$tmp/t.bin" >/dev/null 2>&1; got=$?
    if [ $got -eq 77 ]; then
        skip=$((skip+1)); continue              # compiled, at least
    fi
    if [ $got -eq 42 ]; then
        pass=$((pass+1))
    else
        printf '  FAIL %-48s answered %d, not 42\n' "$name" "$got"
        fail=$((fail+1))
    fi
done

if [ $skip -gt 0 ]; then
    printf '  %d passed, %d failed, %d compiled but not run (no emulator)\n' \
        "$pass" "$fail" "$skip"
else
    printf '  %d passed, %d failed\n' "$pass" "$fail"
fi
[ "$fail" -eq 0 ]

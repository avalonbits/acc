#!/bin/bash
# Compiles each program in test/cases twice -- once with acc, once with
# agondev -- runs both on the Agon, and requires the same answer.
#
# A program reports through IO port 0, so the answer is one byte. Cases are
# written to land well inside that.
#
# The cases run RUN_JOBS at a time, each on its own card, and are reported in
# their own order once all of them are done.
#
# RUN_MODES names a file of ways to drive them, one a line -- a label, the
# compiler (in bin/), and the environment it runs with, `|` between -- and
# then every case under every way is one job of the same pool, reported a
# way at a time: a way does not wait for the slowest case of the one
# before it. test/modes is make test's.
#
# agondev's answer is kept in bin/answers (ACC_ANSWERS names another place),
# under a hash of its program and of the emulator and MOS that ran it, so the
# same reference is run once and not again by every compiler `make test`
# drives through here. Only a 42 is kept -- the one answer a case can pass
# with -- so a reference that ever said anything else runs again every time.
set -uo pipefail
cd "$(dirname "$0")/.."
. test/emu.sh

# Which compiler to drive. `make test` points this at the sanitized build, so
# a run that gets the right answer by reading freed memory still fails.
ACC=${ACC:-bin/acc}
ANSWERS=${ACC_ANSWERS:-bin/answers}

# acc allocates and never frees: it is a one-shot process and giving the
# memory back on the way out would cost code on a target where code is the
# scarce thing. That is deliberate, so leak checking is off; everything else
# ASan and UBSan look for is on.
export ASAN_OPTIONS=detect_leaks=0

# A sanitizer report on stderr is a failure even when the answer is right,
# which is exactly the case that hid the symbol-table bug.
sanitizer_tripped() {
    case $1 in
      *"AddressSanitizer"*|*"runtime error:"*|*"UndefinedBehaviorSanitizer"*) return 0 ;;
    esac
    return 1
}

# one_case <source> <dir>: prints what the case has to say, and leaves pass,
# fail or skip in <dir>/verdict.
one_case() {
    local src=$1 dir=$2 name err want got key

    name=$(basename "$src" .c)
    echo fail > "$dir/verdict"

    if ! err=$("$ACC" "$src" -o "$dir/acc.bin" -x 2>&1); then
        printf '  FAIL %-18s acc could not compile it\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        return
    fi
    if sanitizer_tripped "$err"; then
        printf '  FAIL %-18s the sanitizer tripped\n%s\n' "$name" \
            "$(printf '%s' "$err" | sed 's/^/         /')"
        return
    fi

    if ! err=$(test/oracle.sh "$src" "$dir/ref.bin" 2>&1); then
        case $? in
          77) printf '  skip %-18s no agondev\n' "$name"; echo skip > "$dir/verdict" ;;
          *)  printf '  FAIL %-18s the reference build failed\n%s\n' "$name" \
                  "$(printf '%s' "$err" | sed 's/^/         /')" ;;
        esac
        return
    fi

    if [ -n "$NO_EMU" ]; then
        printf '  skip %-18s no emulator\n' "$name"; echo skip > "$dir/verdict"
        return
    fi

    key=$({ cat "$dir/ref.bin"; printf '\n%s\n%s\n' "$EMU_BIN" "$EMU_MOS"; } \
          | sha256sum | cut -c1-64)
    if [ -f "$ANSWERS/$key" ]; then
        want=$(cat "$ANSWERS/$key")
    else
        test/agon.sh "$dir/ref.bin" >/dev/null 2>&1; want=$?
        if [ "$want" -eq 42 ]; then
            echo 42 > "$ANSWERS/$key.$BASHPID" && mv "$ANSWERS/$key.$BASHPID" "$ANSWERS/$key"
        fi
    fi
    test/agon.sh "$dir/acc.bin" >/dev/null 2>&1; got=$?

    # Every case is written to come out at 42. Agreeing with agondev is not
    # enough on its own: a branch whose condition is false in both compilers
    # agrees perfectly and tests nothing, which is how two of these shipped
    # with arithmetic I had got wrong in the expected values. The answer being
    # 42 is what says the branches fired.
    if [ "$got" -eq "$want" ] && [ "$got" -ne 42 ]; then
        printf '  FAIL %-18s both say %d, but a case has to come out at 42\n' \
            "$name" "$got"
        return
    fi

    if [ "$got" -eq "$want" ]; then
        printf '  ok   %-18s %3d\n' "$name" "$got"
        echo pass > "$dir/verdict"
    else
        printf '  FAIL %-18s acc says %d, agondev says %d\n' "$name" "$got" "$want"
    fi
}

modes=$(mktemp); tmp=$(mktemp -d); trap 'rm -rf "$tmp" "$modes"' EXIT
if [ -n "${RUN_MODES:-}" ]; then
    grep -v '^#' "$RUN_MODES" | grep -v '^$' > "$modes"
else
    printf '|%s|\n' "$ACC" > "$modes"
fi
while IFS='|' read -r label compiler envs; do
    case $compiler in */*) ;; *) compiler=bin/$compiler ;; esac
    [ -x "$compiler" ] || { echo "$compiler missing -- run make"; exit 2; }
done < "$modes"

NO_EMU=
emu_available >/dev/null 2>&1 || NO_EMU=1
mkdir -p "$ANSWERS"

export ANSWERS NO_EMU EMU_BIN EMU_MOS
export -f one_case sanitizer_tripped

# Each job: the case, its directory, the compiler and the environment.
way=0
while IFS='|' read -r label compiler envs; do
    case $compiler in */*) ;; *) compiler=bin/$compiler ;; esac
    way=$((way+1))
    count=0
    for src in ${CASES:-test/cases/*.c}; do
        count=$((count+1))
        printf '%s\n%s\n%s\n%s\n' "$src" "$tmp/$way/$count" "$compiler" "$envs"
    done
done < "$modes" | xargs -d '\n' -P "${RUN_JOBS:-8}" -n 4 bash -c '
    mkdir -p "$2" && export ACC="$3" && { [ -z "$4" ] || export $4; } &&
        one_case "$1" "$2" > "$2/out"' _

failed=0
way=0
while IFS='|' read -r label compiler envs; do
    way=$((way+1))
    [ -n "$label" ] && printf '[%s]\n' "$label"
    pass=0; fail=0; skip=0
    for dir in $(ls "$tmp/$way" | sort -n); do
        cat "$tmp/$way/$dir/out"
        case $(cat "$tmp/$way/$dir/verdict") in
          pass) pass=$((pass+1)) ;;
          skip) skip=$((skip+1)) ;;
          *)    fail=$((fail+1)) ;;
        esac
    done
    printf '  %d passed, %d failed, %d skipped\n' "$pass" "$fail" "$skip"
    failed=$((failed+fail))
done < "$modes"
[ "$failed" -eq 0 ]

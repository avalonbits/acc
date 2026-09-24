# What acc does with a source's tests. Sourced after test/emu.sh and
# test/conformance/batch.sh.
#
#   observe <tests dir> <names> <defines> <out> [<expect>]
#
# <names> has a test's name on each line. Each is compiled and linked by acc
# as any program is, with <defines> -- the source's `define` line -- and its
# headers, and the ones that build are run, a batch to a boot. <out> gets a
# line per test, as test/conformance/suite.py reads an observation:
#
#   name pass                    it ran and its status was 0
#   name fail <result>           it ran and returned something else, or hung
#                                (hang) or crashed the machine (crash), or
#                                (output) printed something other than what
#                                <expect> says it should -- see sources.txt
#   name needs ? <message>       acc refused it, or its link; suite.py works
#                                out what it needs from the test and the message

# Whether test <name> printed what <expect> says it should. A source with
# no expected outputs is judged by what its tests return alone -- gcc's
# print, and say nothing by it -- and in one that has them, a test with no
# file of them should print nothing.
printed_right() {
    local tests=$1 name=$2 expect=$3 printed=$4 want

    [ -z "$expect" ] && return 0
    want=$tests/$name.c$expect
    if [ ! -f "$want" ]; then
        [ ! -s "$printed/$name.out" ]
        return
    fi
    cmp -s "$want" "$printed/$name.out"
}

observe() {
    local tests=$1 names=$2 defines=$3 out=$4 expect=${5:-} work

    work=$(mktemp -d)
    export OBSERVE_TESTS=$tests OBSERVE_WORK=$work OBSERVE_DEFINES=$defines
    xargs -P "${OBSERVE_JOBS:-8}" -n 1 bash -c '
        n=$1 d=$OBSERVE_WORK
        if ! err=$(bin/acc -c "$OBSERVE_TESTS/$n.c" -o "$d/$n.o" -Iinclude \
                   $OBSERVE_DEFINES 2>&1 >/dev/null); then
            echo "$n needs ? $(printf "%s" "$err" | grep -m1 "error:" \
                               | sed "s/.*error: //")" > "$d/$n.out"
        elif ! err=$(bin/acc "$d/$n.o" bin/libc.a -o "$d/$n.bin" 2>&1 >/dev/null); then
            echo "$n needs ? $(printf "%s" "$err" | grep -m1 "error:" \
                               | sed "s/.*error: //")" > "$d/$n.out"
        fi' _ < "$names"

    : > "$work/list"
    while read -r n; do
        [ -f "$work/$n.out" ] || echo "$n $work/$n.bin" >> "$work/list"
    done < "$names"
    mkdir "$work/printed"
    batch_run "$work/list" "$work/ran" "$work/printed" || { rm -rf "$work"; return 2; }

    : > "$out"
    while read -r n; do
        if [ -f "$work/$n.out" ]; then
            cat "$work/$n.out" >> "$out"
            continue
        fi
        r=$(awk -v n="$n" '$1 == n { print $2 }' "$work/ran")
        [ "$r" = 00 ] && ! printed_right "$tests" "$n" "$expect" "$work/printed" &&
            r=output
        if [ "$r" = 00 ]; then
            echo "$n pass" >> "$out"
        else
            echo "$n fail ${r:-missing}" >> "$out"
        fi
    done < "$names"
    rm -rf "$work"
}

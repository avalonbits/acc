# Running many programs on the emulator in few boots. Sourced by
# test/conformance/import.sh and test/conformance.sh, after test/emu.sh.
#
#   batch_run <list> <results> [<outputs>]
#
# <list> has a line per program, `name path-to-image`. Each image is one
# built to print its result as six hex digits and return to MOS: acc's
# startup without -x does, and so does test/conformance/refkit/start.s for
# agondev's. <results> gets a line per program, `name status`: the status
# is the low byte of the result, as two hex digits -- what an exit status
# is, and all acc's -x sends; the rest of the register is whatever main
# left there -- or `hang` or `crash`. With <outputs>, a directory, what each
# program printed before its result goes in <outputs>/<name>.out.
#
# The programs go on one card, `echo @n` in front of each so that the
# console says which printed what, a chunk to a boot. MOS 3 stops the
# autoexec at a program that returns anything but 0, so a failing one ends
# its boot; the next boot starts after it. A program that hangs ends its
# boot at the timeout, and the next starts after that one.

BATCH_CHUNK=${BATCH_CHUNK:-150}
BATCH_JOBS=${BATCH_JOBS:-8}           # emulators at once: a program takes
                                      # about two seconds to load and run
BATCH_TIMEOUT=${BATCH_TIMEOUT:-1800}   # a whole boot, as a last resort
BATCH_HANG=${BATCH_HANG:-300}          # one program, in seconds: c-testsuite's 00040
                                      # takes three minutes of acc's code

# Whether a boot is over short of its `stop`: MOS's prompt, which it shows
# once the autoexec has stopped at a program that failed; a second banner,
# which is a program that crashed the machine and MOS starting again; or
# no program begun for BATCH_HANG seconds, which is one that hung.
batch_watch() {
    local cap=$1 marks now

    grep -q '^/ \*' "$cap" && return 0
    [ "$(grep -c 'MOS Version' "$cap")" -gt 1 ] && return 0
    marks=$(grep -c '^@[0-9]' "$cap")
    now=$SECONDS
    if [ "$marks" != "${batch_marks:-}" ]; then
        batch_marks=$marks
        batch_since=$now
    fi
    [ $((now - ${batch_since:-$now})) -ge "$BATCH_HANG" ]
}

# The list split among BATCH_JOBS emulators, and their results put back in
# the list's order.
batch_run() {
    local list=$1 results=$2 outputs=${3:-} parts j

    parts=$(mktemp -d)
    split -n "r/$BATCH_JOBS" -d "$list" "$parts/list."
    for j in "$parts"/list.*; do
        [ -s "$j" ] && batch_serial "$j" "$j.out" "$outputs" &
    done
    wait
    cat "$parts"/list.*.out 2>/dev/null > "$parts/all"
    awk 'NR == FNR { r[$1] = $2; next } { print $1, ($1 in r) ? r[$1] : "missing" }' \
        "$parts/all" "$list" > "$results"
    rm -rf "$parts"
}

batch_serial() {
    local list=$1 results=$2 outputs=$3 work stop sd n total from i name path

    work=$(mktemp -d)
    stop=$work/stop.bin
    echo 'int main(void) { return 0; }' > "$work/stop.c"
    bin/acc "$work/stop.c" -o "$stop" -x >/dev/null || return 2
    : > "$results"
    total=$(wc -l < "$list")
    from=1
    while [ "$from" -le "$total" ]; do
        sd=$(emu_card)
        cp "$stop" "$sd/bin/stop.bin"
        : > "$sd/autoexec.txt"
        : > "$work/names"
        n=0
        while read -r name path; do
            n=$((n + 1))
            cp "$path" "$sd/c$n.bin"
            printf 'echo @%d\r\nc%d\r\n' "$n" "$n" >> "$sd/autoexec.txt"
            echo "$name" >> "$work/names"
        done < <(sed -n "${from},$((from + BATCH_CHUNK - 1))p" "$list")
        printf 'stop\r\n' >> "$sd/autoexec.txt"

        batch_marks= batch_since=
        ACC_EMU_WATCH=batch_watch ACC_EMU_TIMEOUT=$BATCH_TIMEOUT \
            emu_run "$sd" -z -u > "$work/console" 2>&1
        rm -rf "$sd"

        # Which printed what, and how far the boot got: the last marker it
        # reached. A marker with no result after it is a program that hung,
        # unless it is the last of the chunk and the boot ended cleanly.
        # A program that crashes the machine starts MOS again, and the
        # autoexec with it: the second banner ends what this boot says.
        awk -v names="$work/names" -v outputs="$outputs" '
            BEGIN { while ((getline l < names) > 0) name[++k] = l }
            { sub(/\r$/, "") }
            /^Agon .*MOS Version/ && ++boots > 1 {
                if (at && !(at in result)) result[at] = "crash"
                exit
            }
            /^@[0-9]+$/ {
                at = substr($0, 2) + 0; last = at
                if (outputs != "") {
                    file = outputs "/" name[at] ".out"
                    printf "" > file
                }
                next
            }
            !at || (at in result) { next }
            # The result is the last thing a program prints, six hex
            # digits, and may follow what it printed on the same line;
            # what it printed before them is its output.
            /[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f]$/ {
                result[at] = substr($0, length($0) - 1)
                $0 = substr($0, 1, length($0) - 6)
                if (outputs != "" && $0 != "")
                    printf "%s", $0 > file
                next
            }
            outputs != "" { print > file }
            END {
                for (i = 1; i <= last; i++)
                    print name[i], (i in result) ? result[i] : "hang"
            }' "$work/console" >> "$results"
        i=$(awk '/^Agon .*MOS Version/ && ++boots > 1 { exit }
                 /^@[0-9]+\r?$/ { gsub(/[@\r]/, ""); last = $0 }
                 END { print last + 0 }' "$work/console")
        [ "$i" -eq 0 ] && { echo "batch: the card did not boot" >&2; rm -rf "$work"; return 2; }
        from=$((from + i))
    done
    rm -rf "$work"
}

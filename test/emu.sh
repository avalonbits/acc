# The Agon emulator, driven the way it has to be driven. Sourced, not run.
#
#   . test/emu.sh
#   sd=$(emu_card)          # a fresh card, seeded with MOS's own files
#   ... put files and autoexec.txt on it ...
#   out=$(emu_run "$sd" -z) # console output, stdout and stderr both
#
# Every part of this is load-bearing, and the cost of getting it wrong is not
# an error message -- it is a program that appears to hang:
#
#   * A release build, run from its own directory, because it resolves
#     ./firmware/... relative to the current directory.
#   * The card seeded with mos/, MOS.bin and firmware.bin. Without them it
#     boots and then misbehaves.
#   * The command in autoexec.txt, never typed into stdin. Typing it in looks
#     like it works -- small programs run and print -- and then past a few tens
#     of KB of input silently stops running the command at all. In a sister
#     project that cost a multi-day hunt for a compiler defect that did not
#     exist.
#   * stdin held open with a fifo, because the emulator exits the moment stdin
#     reaches EOF. Unthrottled it may finish the work first and look fine;
#     throttled it quits before MOS has booted.
#   * Both streams captured: console output arrives on stderr as well as
#     stdout.
#
# When something on the Agon looks like it is hanging, run a known-good program
# on comparable input through this same harness before building any theory. If
# that hangs too, the harness or the environment is the problem.

# Which build. ACC_EMU names one; otherwise the one that counts cycles if it
# is there, and the 1.2.4 release if it is not. The counting build is the
# release directory with an agon-cli-emulator built from fab-agon-emulator
# 2037657, which reports the cycles between a write to IO port 0x40 and one
# to 0x41: test/bench.sh takes its count from that when there is one, and it
# runs programs no slower than the release does.
if [ -n "${ACC_EMU:-}" ]; then
    EMU=$ACC_EMU
elif [ -x "$HOME/fab-agon-emulator-2037657/agon-cli-emulator" ]; then
    EMU=$HOME/fab-agon-emulator-2037657
else
    EMU=$HOME/fab-agon-emulator-1.2.4
fi
EMU_BIN=$EMU/agon-cli-emulator

# Which MOS. 3.0.2 -- the release the library is written against, and what
# the emulator's platform firmware is -- unless ACC_EMU_MOS names another,
# or the build has no platform firmware and boots its own default.
EMU_MOS=${ACC_EMU_MOS:-$EMU/firmware/mos_platform.bin}

emu_available() {
    [ -x "$EMU_BIN" ] || { echo "no emulator at $EMU_BIN (set ACC_EMU)" >&2; return 1; }
    [ -f "$EMU/sdcard/MOS.bin" ] || { echo "no MOS.bin under $EMU/sdcard" >&2; return 1; }
    return 0
}

emu_card() {
    local sd
    sd=$(mktemp -d)
    mkdir -p "$sd/bin"
    cp -r "$EMU/sdcard/mos" "$sd/" 2>/dev/null
    cp "$EMU/sdcard/MOS.bin" "$EMU/sdcard/firmware.bin" "$sd/" 2>/dev/null
    echo "$sd"
}

# emu_run <card> [emulator flags...]
emu_run() {
    local sd=$1; shift
    local fifo cap hold emu rc

    fifo=$(mktemp -u); cap=$(mktemp)
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" & hold=$!

    local mos=() mos_bin=${ACC_EMU_MOS:-$EMU_MOS}
    [ -f "$mos_bin" ] && mos=(--mos "$mos_bin")
    (cd "$EMU" && exec timeout "${ACC_EMU_TIMEOUT:-300}" ./agon-cli-emulator \
        --sdcard "$sd" "${mos[@]}" "$@" < "$fifo" > "$cap" 2>&1) & emu=$!

    # With ACC_EMU_PROMPT set, stopped as soon as MOS prints its prompt,
    # which is when autoexec.txt has run: a program that prints its answer
    # cannot also stop the emulator through port 0, since what it printed
    # is still on its way to the VDP when the emulator goes. Without this
    # such a program runs out the whole timeout.
    if [ -n "${ACC_EMU_PROMPT:-}" ]; then
        while kill -0 "$emu" 2>/dev/null; do
            if grep -q '^/ \*' "$cap"; then
                kill "$emu" 2>/dev/null
                wait "$emu" 2>/dev/null
                emu=
                break
            fi
            sleep 0.1
        done
    fi
    if [ -n "$emu" ]; then
        wait "$emu"; rc=$?
    else
        rc=0
    fi

    kill "$hold" 2>/dev/null; wait "$hold" 2>/dev/null
    rm -f "$fifo"
    cat "$cap"; rm -f "$cap"

    return $rc
}

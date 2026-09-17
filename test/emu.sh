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

EMU=${ACC_EMU:-$HOME/fab-agon-emulator-1.2.4}
EMU_BIN=$EMU/agon-cli-emulator

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
    local fifo cap hold rc

    fifo=$(mktemp -u); cap=$(mktemp)
    mkfifo "$fifo"
    tail -f /dev/null > "$fifo" & hold=$!

    (cd "$EMU" && timeout "${ACC_EMU_TIMEOUT:-300}" ./agon-cli-emulator \
        --sdcard "$sd" "$@" < "$fifo" > "$cap" 2>&1)
    rc=$?

    kill "$hold" 2>/dev/null; wait "$hold" 2>/dev/null
    rm -f "$fifo"
    cat "$cap"; rm -f "$cap"

    return $rc
}

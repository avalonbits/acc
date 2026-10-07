#!/bin/bash
# The real programs, against agondev -Oz: how big acc makes them and how
# fast they run, each built both ways -- acc, vi, AED, zap and ez80asm.
#
#   test/apps.sh                 # ACC=bin/opt-acc for opt-acc's builds
#
# acc itself, at a fixed revision (ACC_REV), as test/size.sh takes it,
# compiling that revision's test/bench inputs on the Agon; Tom's vi for the
# Agon, Busybox vi ported (VI_REV, fetched into test/perf/cache as size.sh
# fetches it), substituting through a file and writing it out; and AED, the
# editor, from a checkout at a tag (AED, AED_REV), running its own
# benchmark -- test/bench/bench.c, searching, walking lines, copying a range
# and typing with undo -- a fixed number of times; and zap, the assembler,
# from a checkout at a tag (ZAP, ZAP_REV) as test/perf.sh takes it,
# assembling BBC BASIC for the Agon -- perf.sh's zap-basic; and ez80asm, the
# Agon's other assembler, at a tag (EZ80ASM_REV, fetched into test/perf/cache
# as vi is), assembling the same BBC BASIC with -m, its smaller tables. None
# of them is committed here: vi, AED and zap are GPL, and ez80asm, MIT, is
# fetched as vi is.
#
# ez80asm is written for agondev. Its two assembly helpers are GNU as, made
# zap's for acc's build; and acc's build renames its remove(), which
# agondev's libc lacks and acc's has, and FILE's handle, which agondev's
# calls fhandle and acc's fh.
#
# `code` is the program's own objects, what each compiler made of its C, as
# size.sh counts it. `cycles` is the emulator's count from the command
# being typed to its prompt coming back, between two small programs that
# start and stop it (IO ports 0x40 and 0x41), as test/perf.sh times zap.
# Library calls are in it: acc's libc in the one build, agondev's in the
# other, which is what a program built either way runs with. Each run's
# output -- the objects acc wrote, the file vi wrote, the counts AED
# reports, the binary zap assembled -- has to be the same from both
# builds, or its time is no measure.
#
# AED calls hub's client library: agondev's build links the libhub.a AED
# carries, acc's the one hub's Makefile assembles for acc (HUB_ACC). The
# benchmark never finds hub running; the calls are there to link.
#
# The ratios are acc's over agondev's: lower is better. Their geometric
# means are printed last.
set -u
cd "$(dirname "$0")/.."
. test/emu.sh

ACC=${ACC:-bin/acc}
[ -x "$ACC" ] || { echo "$ACC missing -- run make" >&2; exit 2; }
AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
SIZE=$AGONDEV/bin/ez80-none-elf-size
[ -x "$CC" ] || { echo "no agondev at $AGONDEV" >&2; exit 2; }
emu_available || exit 77
CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc
        -isystem $AGONDEV/include -target ez80-none-elf -DAGONDEV
        -Wa,-march=ez80+full -fno-threadsafe-statics -w -Oz"
LDFLAGS="-defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000
         -defsym=_has_exit_handler=0 -T $AGONDEV/config/linker.conf --oformat binary"

ACC_REV=${ACC_REV:-c99-first-milestone}
VI_URL=https://github.com/tomm/toms-agon-experiments.git
VI_REV=${VI_REV:-b2789b763c8041a0487ab3cfbbdfbe10d27c87c3}
AED=${AED:-$HOME/code/aed}
AED_REV=${AED_REV:-v1.6.3}
ZAP=${ZAP:-$HOME/code/zap}
ZAP_REV=${ZAP_REV:-v1.1.0}
BASIC=test/corpus/Z_PRG_Agon-bbc-basic-v/tests
CACHE=test/perf/cache

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export ASAN_OPTIONS=detect_leaks=0
names=

EZ80ASM_URL=https://github.com/AgonPlatform/agon-ez80asm.git
EZ80ASM_REV=${EZ80ASM_REV:-v2.3}

# The two that start and stop the count.
for port in 40 41; do
    printf '#include <ez80f92.h>\nint main(void) { io_out(0x%s, 0); return 0; }\n' \
        "$port" > "$work/t$port.c"
    { bin/acc -c "$work/t$port.c" -o "$work/t$port.o" -Iinclude &&
          bin/acc "$work/t$port.o" bin/libc.a -o "$work/t$port.bin"; } >/dev/null 2>&1 || exit 2
done

# acc, its sources and its inputs at ACC_REV.
if git rev-parse -q --verify "$ACC_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$work/acc/src" "$work/acc/in" "$work/acc/include"
    git archive "$ACC_REV" src | tar -x -C "$work/acc/src" --strip-components=1
    git archive "$ACC_REV" test/bench | tar -x -C "$work/acc/in" --strip-components=2
    git archive "$ACC_REV" include | tar -x -C "$work/acc/include" --strip-components=1
    printf '#define ACC_BUILD 0\n' > "$work/acc/src/acc_build.h"
    names="$names acc"
else
    echo "note: no revision $ACC_REV -- acc is left out" >&2
fi

# vi, as size.sh fetches and adjusts it.
if [ ! -d "$CACHE/vi/.git" ]; then
    rm -rf "$CACHE/vi"
    git init -q "$CACHE/vi" && git -C "$CACHE/vi" remote add origin "$VI_URL" &&
        git -C "$CACHE/vi" sparse-checkout set vi/src >/dev/null 2>&1 &&
        git -C "$CACHE/vi" fetch -q --depth 1 --filter=blob:none origin "$VI_REV" &&
        git -C "$CACHE/vi" checkout -q FETCH_HEAD ||
        { echo "note: vi could not be fetched -- left out" >&2; rm -rf "$CACHE/vi"; }
fi
if [ -d "$CACHE/vi/vi/src" ]; then
    mkdir -p "$work/vi/src"
    cp "$CACHE/vi/vi/src"/* "$work/vi/src/"
    python3 - "$work/vi/src" <<'PY'
import glob, re, sys
for p in glob.glob(sys.argv[1] + '/*.[ch]'):
    s = open(p).read()
    s = re.sub(r'\bsystem\(', 'vi_system(', s)
    s = s.replace('((col % tabstop) ?: tabstop)',
                  '((col % tabstop) ? (col % tabstop) : tabstop)')
    s = re.sub(r'\bcmdcnt \?: 1', 'cmdcnt ? cmdcnt : 1', s)
    open(p, 'w').write(s)
PY
    names="$names vi"
fi

# AED's benchmark: its modules, flattened as its test/bench/build.sh stages
# them, with bench.c's main for main.c's and the stub for hub.
HUB_ACC=${HUB_ACC:-$HOME/code/hub/build/lib/acc/libhub.a}
if [ ! -f "$HUB_ACC" ]; then
    echo "note: no acc libhub.a at $HUB_ACC (make -C ~/code/hub build/lib/acc/libhub.a) -- AED is left out" >&2
elif git -C "$AED" rev-parse -q --verify "$AED_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$work/aed/src" "$work/aed/tree"
    git -C "$AED" archive "$AED_REV" src test/bench third_party | tar -x -C "$work/aed/tree"
    for f in "$work/aed/tree/src"/*.[ch] "$work/aed/tree/src/core"/*.[ch] \
             "$work/aed/tree/src/ui"/*.[ch]; do
        [ "$(basename "$f")" = main.c ] || cp "$f" "$work/aed/src/"
    done
    # 192 KB of buffer, not 256: acc's image is the bigger, and 256 KB and
    # the line index left its malloc short below the stack. Both builds
    # are given the same, which holds the corpus several times over.
    sed 's/^#define MEM_KB .*/#define MEM_KB      192/' \
        "$work/aed/tree/test/bench/bench.c" > "$work/aed/src/bench.c"
    bash "$work/aed/tree/test/bench/mkcorpus.sh" "$work/aed/bench.txt" 2000 >/dev/null
    mkdir -p "$work/aed/inc/hub"
    cp "$work/aed/tree/third_party"/hub-*/include/hub/hub.h "$work/aed/inc/hub/"
    cp "$work/aed/tree/third_party"/hub-*/lib/libhub.a "$work/aed/libhub-Oz.a"
    names="$names aed"
else
    echo "note: no AED at $AED ($AED_REV) -- left out" >&2
fi

# zap, and the BBC BASIC it assembles, as perf.sh takes them.
if git -C "$ZAP" rev-parse -q --verify "$ZAP_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$work/zap/src" "$work/basic"
    git -C "$ZAP" archive "$ZAP_REV" src | tar -x -C "$work/zap/src" --strip-components=1
    rm -f "$work/zap/src/zmalloc.c"     # zap's measuring shim, a link
    git -C "$ZAP" archive "$ZAP_REV" "$BASIC" | tar -x -C "$work/basic" --strip-components=4
    names="$names zap"
else
    echo "note: no zap at $ZAP ($ZAP_REV) -- left out, and ez80asm, which assembles its BASIC" >&2
fi

# ez80asm, at its tag, as vi is fetched.
ezcache=$CACHE/ez80asm-$EZ80ASM_REV
if [ -d "$work/basic" ] && [ ! -d "$ezcache/.git" ]; then
    rm -rf "$ezcache"
    git init -q "$ezcache" && git -C "$ezcache" remote add origin "$EZ80ASM_URL" &&
        git -C "$ezcache" sparse-checkout set src >/dev/null 2>&1 &&
        git -C "$ezcache" fetch -q --depth 1 --filter=blob:none origin \
            "refs/tags/$EZ80ASM_REV" &&
        git -C "$ezcache" checkout -q FETCH_HEAD ||
        { echo "note: ez80asm could not be fetched -- left out" >&2; rm -rf "$ezcache"; }
fi
if [ -d "$work/basic" ] && [ -d "$ezcache/src" ]; then
    mkdir -p "$work/ez80asm/src"
    cp "$ezcache/src"/*.[ch] "$ezcache/src"/*.asm "$work/ez80asm/src/"
    names="$names ez80asm"
fi

# build <name> <acc|Oz>: objects into $work/<name>/<how>/, the image as
# $work/<name>/<how>.bin. An .asm among the sources is agondev's GNU as:
# made zap's for acc's build.
build() {
    local name=$1 how=$2 dir=$work/$1 f o objs= inc defs=

    inc="-I$dir/src"
    [ "$name" = aed ] && inc="$inc -I$dir/inc"
    [ "$name" = ez80asm ] && defs="-Dfhandle=fh -Dremove=asm_remove"
    mkdir -p "$dir/$how"
    for f in "$dir/src"/*.c; do
        o="$dir/$how/$(basename "$f" .c).o"
        if [ "$how" = acc ]; then
            "$ACC" -c "$f" -o "$o" -Iinclude $inc -DAGONDEV $defs >/dev/null 2>&1
        else
            $CC $CFLAGS $inc -c "$f" -o "$o" 2>/dev/null
        fi || { echo "$name: $(basename "$f") did not compile ($how)" >&2; return 1; }
        objs="$objs $o"
    done
    for f in "$dir/src"/*.asm; do
        [ -f "$f" ] || continue
        o="$dir/$how/$(basename "$f" .asm).o"
        if [ "$how" = acc ]; then
            sed -e 's/^\.section[[:space:]].*/\tSEGMENT CODE/' \
                -e 's/^\.global[[:space:]]*\(.*\)/\tXDEF\t\1/' "$f" > "${o%.o}.s" &&
                bin/zap "${o%.o}.s" "$o" -f acc >/dev/null 2>&1
        else
            "$AGONDEV/bin/ez80-none-elf-as" -march=ez80+full -I "$AGONDEV/include" \
                "$f" -o "$o" 2>/dev/null
        fi || { echo "$name: $(basename "$f") did not assemble ($how)" >&2; return 1; }
        objs="$objs $o"
    done
    if [ "$how" = acc ]; then
        "$ACC" $objs $([ "$name" = aed ] && echo "$HUB_ACC") bin/libc.a \
            -o "$dir/$how.bin" >/dev/null 2>&1
    else
        "$AGONDEV/bin/ez80-none-elf-ld" $LDFLAGS -o "$dir/$how.bin" $objs \
            $([ "$name" = aed ] && echo "$dir/libhub-Oz.a") -L"$AGONDEV/lib" -lagon >/dev/null 2>&1
    fi || { echo "$name: the $how build did not link" >&2; return 1; }
}

# code <name> <how>: the bytes of the program's own objects.
code() {
    if [ "$2" = acc ]; then
        python3 test/perf/objsize.py acc-text "$work/$1"/acc/*.o
    else
        "$SIZE" "$work/$1/$2"/*.o | awk 'NR > 1 { t += $1 + $2 } END { print t }'
    fi
}

apps_done() {
    grep -q 'Debug OUT(0x41)' "$1"
}

# run <name> <how>: prints `cycles check`, the check a digest of what the
# run left on the card, or nothing.
run() {
    local name=$1 how=$2 dir=$work/$1 sd out cycles check f

    sd=$(emu_card)
    cp "$work/t40.bin" "$sd/bin/tstart.bin"
    cp "$work/t41.bin" "$sd/bin/tstop.bin"
    case $name in
      acc)
        cp "$dir/$how.bin" "$sd/bin/acc.bin"
        mkdir -p "$sd/lib/acc" "$sd/in"
        cp -r "$dir/include" "$sd/lib/acc/include"
        cp "$dir/in"/*.c "$sd/in/"
        {
            printf 'tstart\r\n'
            for f in "$dir/in"/*.c; do
                f=$(basename "$f" .c)
                printf 'acc -c in/%s.c -o in/%s.o\r\n' "$f" "$f"
            done
            printf 'tstop\r\n'
        } > "$sd/autoexec.txt" ;;
      vi)
        cp "$dir/$how.bin" "$sd/bin/vi.bin"
        cat "$work/acc/src/parse.c" "$work/acc/src/lex.c" | head -c 40000 > "$sd/text.txt"
        # The file first; the commands after it are run last first.
        printf 'tstart\r\nvi text.txt +wq +%%s/e/E/g +%%s/int/INT/g\r\ntstop\r\n' \
            > "$sd/autoexec.txt" ;;
      zap)
        cp "$dir/$how.bin" "$sd/bin/zap.bin"
        cp "$work/basic"/* "$sd/"
        printf 'tstart\r\nzap bbcbasicvez.s out.bin\r\ntstop\r\n' > "$sd/autoexec.txt" ;;
      ez80asm)
        cp "$dir/$how.bin" "$sd/bin/ez80asm.bin"
        cp "$work/basic"/* "$sd/"
        printf 'tstart\r\nez80asm bbcbasicvez.s out.bin -c -m\r\ntstop\r\n' > "$sd/autoexec.txt" ;;
      aed)
        cp "$dir/$how.bin" "$sd/bin/aedbench.bin"
        cp "$dir/bench.txt" "$sd/bench.txt"
        printf 'tstart\r\naedbench /bench.txt 2\r\ntstop\r\n' > "$sd/autoexec.txt" ;;
    esac
    out=$(ACC_EMU_WATCH=apps_done ACC_EMU_TIMEOUT=${APPS_TIMEOUT:-300} emu_run "$sd" -z -u 2>&1)
    cycles=$(printf '%s' "$out" | sed -n 's/.*Debug OUT(0x41): \([0-9]*\) CPU cycles.*/\1/p' | head -1)
    case $name in
      acc) check=$(cat "$sd/in"/*.o 2>/dev/null | md5sum | cut -c1-8)
           [ -n "$(ls "$sd/in"/*.o 2>/dev/null)" ] || check= ;;
      vi)  check=$([ -f "$sd/text.txt" ] && md5sum < "$sd/text.txt" | cut -c1-8) ;;
      zap|ez80asm)
           check=$([ -f "$sd/out.bin" ] && md5sum < "$sd/out.bin" | cut -c1-8) ;;
      aed) # Each case's line but the hundredths it took.
           check=$([ -f "$sd/bench.out" ] && tr -d '\r' < "$sd/bench.out" |
                   awk 'NF == 3 && $3 ~ /^x/ { print $1, $3; next } { print }' |
                   md5sum | cut -c1-8) ;;
    esac
    [ -n "${APPS_KEEP:-}" ] && { rm -rf "$APPS_KEEP/$name.$how"; cp -r "$sd" "$APPS_KEEP/$name.$how"; printf '%s\n' "$out" > "$APPS_KEEP/$name.$how/console"; }
    rm -rf "$sd"
    [ -n "$cycles" ] && [ -n "$check" ] && echo "$cycles $check"
}

# Each build and run its own job, on its own card.
one() {
    local name=$1 how=$2 r

    build "$name" "$how" 2> "$work/$name.$how.err" || return
    r=$(run "$name" "$how")
    if [ -z "$r" ]; then
        echo "$name: the $how build did not run to the end" > "$work/$name.$how.err"
        return
    fi
    echo "$r $(code "$name" "$how")" > "$work/$name.$how.res"
}

# APPS_ONLY: the names of the ones to measure, of acc, vi, aed, zap and
# ez80asm.
if [ -n "${APPS_ONLY:-}" ]; then
    only=
    for name in $names; do
        case " $APPS_ONLY " in *" $name "*) only="$only $name" ;; esac
    done
    names=$only
fi

export ACC CC CFLAGS LDFLAGS SIZE AGONDEV EMU_BIN EMU_MOS work HUB_ACC
export -f build code run one apps_done
for name in $names; do
    printf '%s\nacc\n%s\nOz\n' "$name" "$name"
done | xargs -d '\n' -P "${APPS_JOBS:-6}" -n 2 bash -c '. test/emu.sh; one "$1" "$2"' _

printf '%-6s %8s %8s %6s   %12s %12s %6s\n' program "acc code" "-Oz code" ratio \
    "acc cycles" "-Oz cycles" ratio
status=0
logs=
for name in $names; do
    for how in acc Oz; do
        if [ ! -f "$work/$name.$how.res" ]; then
            cat "$work/$name.$how.err" >&2
            status=1; continue 2
        fi
        read -r c k sz < "$work/$name.$how.res"
        eval "cyc_$how=$c chk_$how=$k size_$how=$sz"
    done
    if [ "$chk_acc" != "$chk_Oz" ]; then
        echo "$name: the builds left different output -- acc $chk_acc, -Oz $chk_Oz" >&2
        status=1; continue
    fi
    printf '%-6s %8d %8d %6s   %12d %12d %6s\n' "$name" "$size_acc" "$size_Oz" \
        "$(awk -v a="$size_acc" -v b="$size_Oz" 'BEGIN { printf "%.3f", a / b }')" \
        "$cyc_acc" "$cyc_Oz" "$(awk -v a="$cyc_acc" -v b="$cyc_Oz" 'BEGIN { printf "%.3f", a / b }')"
    logs="$logs $size_acc/$size_Oz/$cyc_acc/$cyc_Oz"
done
[ -n "$logs" ] && echo "$logs" | tr ' ' '\n' | awk -F/ 'NF == 4 {
    s += log($1 / $2); c += log($3 / $4); n++ }
    END { if (n) printf "%-6s %8s %8s %6.4f   %12s %12s %6.4f\n", "(mean)", "", "", exp(s / n), "", "", exp(c / n) }'
exit $status

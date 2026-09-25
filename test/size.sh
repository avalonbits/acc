#!/bin/bash
# How big the code acc generates is, against agondev's, over real programs.
#
#   test/size.sh              every program, a line each
#   test/size.sh -f <name>    and that one's functions, side by side
#
# The programs: each of test/perf's, one file each; zap, the assembler acc
# exists to build on the machine, taken from a checkout at a tag as
# test/bench.sh takes it (ZAP, ZAP_REV); and Tom's vi for the Agon, Busybox
# vi ported, fetched at a pinned commit into test/perf/cache (VI_REV). None
# of zap or vi is committed here: both are GPL.
#
# Each is built three ways -- acc, and agondev at -Oz and at -O2, with
# agondev's own flags, startup and link -- and two sizes are given for each.
# `code` is the program's own objects: what each compiler made of its C,
# functions, their constants and initialised data. `image` is the whole
# linked program, library and startup included, which is what goes on the
# card -- and for a small program is mostly the library, printf above all,
# so it compares the libraries more than the compilers. The ratios are
# acc's over agondev -Oz's: lower is better, 1.00 is even.
#
# vi needs two things changed for acc that are vi's and not C99's to change:
# it defines a `system` of its own, a name <stdlib.h> gives C99's -- renamed
# vi_system -- and it writes GNU's `x ?: y` six times, written `x ? x : y`.
# Both compilers are given the changed source, so they compile the same C.
#
# With -f, the program's functions as each compiler made them, the
# furthest apart first: acc's from the map -map writes beside each object,
# agondev's from its nm, through test/perf/objsize.py. A function one
# compiler has and the other does not -- inlined by agondev, say -- is
# listed with a dash for the other.
set -u
cd "$(dirname "$0")/.."

focus=
[ "${1:-}" = -f ] && { focus=$2; shift 2; }

[ -x bin/acc ] || { echo "bin/acc missing -- run make" >&2; exit 2; }
AGONDEV=${AGONDEV:-$HOME/agondev}
CC=$AGONDEV/bin/ez80-none-elf-clang
NM=$AGONDEV/bin/ez80-none-elf-nm
[ -x "$CC" ] || { echo "no agondev at $AGONDEV" >&2; exit 2; }
SIZE=$AGONDEV/bin/ez80-none-elf-size
# agondev's own flags and link, from its config/makefile.inc: what a program
# built for the Agon with agondev gets.
CFLAGS="-mllvm -z80-gas-style -mllvm -z80-print-zero-offset -nostdinc
        -isystem $AGONDEV/include -target ez80-none-elf -DAGONDEV
        -Wa,-march=ez80+full -fno-threadsafe-statics -w"
LDFLAGS="-defsym=RAM_START=0x40000 -defsym=RAM_SIZE=0x70000
         -defsym=_has_exit_handler=0 -T $AGONDEV/config/linker.conf --oformat binary"

ZAP=${ZAP:-$HOME/code/zap}
ZAP_REV=${ZAP_REV:-v1.1.0}
VI_URL=https://github.com/tomm/toms-agon-experiments.git
VI_REV=${VI_REV:-b2789b763c8041a0487ab3cfbbdfbe10d27c87c3}
CACHE=test/perf/cache

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export ASAN_OPTIONS=detect_leaks=0

# The programs, as `name directory-of-its-sources`, one line each.
: > "$work/programs"
for src in test/perf/*.c; do
    name=$(basename "$src" .c)
    mkdir -p "$work/src/$name"
    cp "$src" test/perf/perf.h "$work/src/$name/"
    echo "$name $work/src/$name" >> "$work/programs"
done

if git -C "$ZAP" rev-parse -q --verify "$ZAP_REV^{commit}" >/dev/null 2>&1; then
    mkdir -p "$work/src/zap"
    git -C "$ZAP" archive "$ZAP_REV" src | tar -x -C "$work/src/zap" --strip-components=1
    rm -f "$work/src/zap/zmalloc.c"     # zap's own measuring shim, a link
    echo "zap $work/src/zap" >> "$work/programs"
else
    echo "note: no zap at $ZAP ($ZAP_REV) -- left out" >&2
fi

if [ ! -d "$CACHE/vi/.git" ]; then
    rm -rf "$CACHE/vi"
    git init -q "$CACHE/vi" && git -C "$CACHE/vi" remote add origin "$VI_URL" &&
        git -C "$CACHE/vi" sparse-checkout set vi/src >/dev/null 2>&1 &&
        git -C "$CACHE/vi" fetch -q --depth 1 --filter=blob:none origin "$VI_REV" &&
        git -C "$CACHE/vi" checkout -q FETCH_HEAD ||
        { echo "note: vi could not be fetched -- left out" >&2; rm -rf "$CACHE/vi"; }
fi
if [ -d "$CACHE/vi/vi/src" ]; then
    mkdir -p "$work/src/vi"
    cp "$CACHE/vi/vi/src"/* "$work/src/vi/"
    python3 - "$work/src/vi" <<'PY'
import glob, re, sys
for p in glob.glob(sys.argv[1] + '/*.[ch]'):
    s = open(p).read()
    s = re.sub(r'\bsystem\(', 'vi_system(', s)
    s = s.replace('((col % tabstop) ?: tabstop)',
                  '((col % tabstop) ? (col % tabstop) : tabstop)')
    s = re.sub(r'\bcmdcnt \?: 1', 'cmdcnt ? cmdcnt : 1', s)
    open(p, 'w').write(s)
PY
    echo "vi $work/src/vi" >> "$work/programs"
fi

# build <directory> <acc|Oz|O2>: objects into <directory>/<how>/, and the
# image as <directory>/<how>.bin.
build() {
    local dir=$1 how=$2 f objs=

    mkdir -p "$dir/$how"
    for f in "$dir"/*.c; do
        local o="$dir/$how/$(basename "$f" .c).o"

        if [ "$how" = acc ]; then
            bin/acc -c "$f" -o "$o" -Iinclude -I"$dir" -map "$o.map" \
                >/dev/null 2>&1 || return 1
        else
            $CC $CFLAGS -$how -I"$dir" -c "$f" -o "$o" 2>/dev/null || return 1
        fi
        objs="$objs $o"
    done
    if [ "$how" = acc ]; then
        bin/acc $objs bin/libc.a -o "$dir/$how.bin" >/dev/null 2>&1
    else
        "$AGONDEV/bin/ez80-none-elf-ld" $LDFLAGS -o "$dir/$how.bin" $objs \
            -L"$AGONDEV/lib" -lagon >/dev/null 2>&1
    fi
}

# code <directory> <how>: the bytes of the program's own objects.
code() {
    if [ "$2" = acc ]; then
        python3 test/perf/objsize.py acc-text "$1"/acc/*.o
    else
        "$SIZE" "$1/$2"/*.o | awk 'NR > 1 { t += $1 + $2 } END { print t }'
    fi
}

ratio() {
    awk -v a="$1" -v b="$2" 'BEGIN { printf "%.2f", a / b }'
}

printf '%-10s %8s %8s %8s %6s   %8s %8s %8s %6s\n' program \
    "acc code" "-Oz" "-O2" ratio "acc image" "-Oz" "-O2" ratio
status=0
logs=
while read -r name dir; do
    for how in acc Oz O2; do
        if ! build "$dir" "$how"; then
            echo "$name: the $how build failed" >&2
            status=1; continue 2
        fi
    done
    ca=$(code "$dir" acc) cz=$(code "$dir" Oz) co=$(code "$dir" O2)
    a=$(stat -c%s "$dir/acc.bin") z=$(stat -c%s "$dir/Oz.bin") o=$(stat -c%s "$dir/O2.bin")
    printf '%-10s %8d %8d %8d %6s   %8d %8d %8d %6s\n' "$name" \
        "$ca" "$cz" "$co" "$(ratio "$ca" "$cz")" "$a" "$z" "$o" "$(ratio "$a" "$z")"
    logs="$logs $ca/$cz/$a/$z"

    if [ "$name" = "$focus" ]; then
        awk '$1 != "-" { print $1, $3 }' "$dir"/acc/*.o.map > "$work/f.acc"
        python3 test/perf/objsize.py elf "$NM" "$dir"/Oz/*.o > "$work/f.Oz"
        python3 - "$work/f.acc" "$work/f.Oz" > "$work/focus" <<'PY'
import sys
acc = dict(l.rsplit(None, 1) for l in open(sys.argv[1]))
oz = dict(l.rsplit(None, 1) for l in open(sys.argv[2]))
rows = []
for name in set(acc) | set(oz):
    a, z = acc.get(name), oz.get(name)
    rows.append((int(a or 0) - int(z or 0), name, a or '-', z or '-'))
print('  %-28s %8s %8s %8s' % ('function', 'acc', '-Oz', 'more'))
for diff, name, a, z in sorted(rows, reverse=True):
    print('  %-28s %8s %8s %8d' % (name[:28], a, z, diff))
PY
    fi
done < "$work/programs"

# The geometric mean of each ratio, so that no one program outweighs the
# rest.
[ -n "$logs" ] && echo "$logs" | tr ' ' '\n' | awk -F/ 'NF == 4 {
        c += log($1 / $2); s += log($3 / $4); n++
    } END { if (n) printf "%-10s %26s %6.2f   %26s %6.2f\n", "(mean)", "",
                          exp(c / n), "", exp(s / n) }'

if [ -n "$focus" ]; then
    if [ -s "$work/focus" ]; then
        echo
        echo "$focus, a function at a time:"
        cat "$work/focus"
    else
        echo "no program called $focus" >&2
        status=1
    fi
fi

exit $status

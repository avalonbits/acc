#!/bin/bash
# The packed error messages of agondev's build, said as the source says them.
#
# src/msgpack.py packs the words of every error message into single bytes
# for agondev's build, and src/fmt.c expands them as it reads a format.
# Here the sources are packed as that build packs them, a host acc is built
# from them with fmt.c's printf in place of the C library's, and
# test/errors.sh -- every message of its cases, word for word -- is run
# against it. And the packing has to save something.
set -uo pipefail
cd "$(dirname "$0")/.."

CC=${CC:-cc}
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
SRCS=$(sed -n 's/^SRCS = //p; /^       src/p' Makefile.agon | tr -d '\\')

# shellcheck disable=SC2086
python3 src/msgpack.py "$tmp/pp" $SRCS || { echo "  FAIL msgpack"; exit 1; }
# The messages' bytes, as written and as packed, from the packer itself.
# shellcheck disable=SC2086
read -r before after < <(python3 - $SRCS <<'PY'
import importlib.util, sys
spec = importlib.util.spec_from_file_location('mp', 'src/msgpack.py')
mp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mp)
texts = [t for p in sys.argv[1:] for _, _, t in mp.formats(open(p).read())]
words = mp.choose(texts)
code = {w: mp.FIRST + i for i, w in enumerate(words)}
print(sum(len(t) + 1 for t in texts),
      sum(len(mp.pack(t, code)) + 1 for t in texts) + sum(len(w) + 1 for w in words))
PY
)
if [ "$after" -ge "$before" ]; then
    echo "  FAIL msgpack saved nothing ($before bytes of messages, $after after)"
    exit 1
fi

# shellcheck disable=SC2046
"$CC" -O1 -fsigned-char -DACC_LIBC="\"$PWD/bin/libc.a\"" -DACC_INCLUDE_DIR="\"$PWD/include\"" -DACC_MSG_PACKED -DACC_FMT_WRAP -I"$tmp/pp" -Isrc \
    -o "$tmp/acc" "$tmp"/pp/*.c || { echo "  FAIL the packed acc does not build"; exit 1; }
out=$(ACC="$tmp/acc" test/errors.sh 2>&1)
if printf '%s\n' "$out" | tail -1 | grep -q ' 0 failed'; then
    echo "  msgpack: $(printf '%s\n' "$out" | tail -1 | sed 's/^ *//'), packed; messages $before -> $after bytes with the words"
else
    echo "  FAIL the packed messages"
    printf '%s\n' "$out" | grep FAIL | head -5
    exit 1
fi

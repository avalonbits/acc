#!/bin/bash
# The reference, docs/ref/*.md, made into bin/acc.dat for ref (make doc):
# every function a documented header declares is a key ref finds, so that
# `ref acc <name>` never comes back empty for a name the library has; so is
# every option `acc -h` lists; and the contents page, index.md, links every
# page.
#
# A page whose title is a header's name, `# `string.h`` for
# include/string.h, documents that header. Its functions are found by
# reading the header's declarations, so a function added to a header
# without a heading of its own in the page is a failure here. A page with
# any other title, such as options.md, is about acc itself.
set -uo pipefail
cd "$(dirname "$0")/.."

DOCS=${DOCS:-docs/ref}
DAT=${DAT:-bin/acc.dat}
REFTOOL=${REFTOOL:-bin/reftool}
[ -x "$REFTOOL" ] && [ -f "$DAT" ] || { echo "  no $DAT: make doc needs ref (REF_SRC)"; exit 77; }

pass=0; fail=0

ok() { pass=$((pass + 1)); }
bad() { printf '  FAIL %-34s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# Each of the names in $2, a line each, is a key ref finds exactly: reftool
# answers a line a query, and kind 1 is an exact key. $1 says whose.
keys() {
    local what=$1 names=$2 found name answer

    found=$(printf '%s\n' "$names" | "$REFTOOL" "$DAT" lookup)
    while read -r name && read -r answer <&3; do
        case $answer in
        "1 "*) ok ;;
        *) bad "$what: $name is a key" "lookup: $answer" ;;
        esac
    done <<<"$names" 3<<<"$found"
}

# The functions a header declares: its comments and preprocessor lines
# taken out, then the name before the parameters of each declaration that
# is not a typedef.
functions() {
    python3 - "$1" <<'PY'
import re, sys

text = open(sys.argv[1]).read()
text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
text = text.replace('\\\n', ' ')                # a #define's lines made one
text = re.sub(r'//[^\n]*', ' ', text)
text = re.sub(r'^\s*#[^\n]*', ' ', text, flags=re.M)
text = re.sub(r'__attribute__\s*\(\(.*?\)\)', ' ', text)
for decl in text.split(';'):
    decl = ' '.join(decl.split())
    if not decl or decl.startswith('typedef') or decl.endswith('}'):
        continue
    # A function answering a pointer to one, as signal is, is named
    # inside the first parentheses; `__` names are the library's own.
    name = (re.search(r'\(\s*\*\s*([A-Za-z_]\w*)\s*\(', decl)
            or re.search(r'\b([A-Za-z_]\w*)\s*\(', decl))
    if name and not name.group(1).startswith('__'):
        print(name.group(1))
PY
}

for page in "$DOCS"/*.md; do
    [ "$page" = "$DOCS/index.md" ] && continue
    base=$(basename "$page")
    if grep -q "]($base)" "$DOCS/index.md"; then
        ok
    else
        bad "index.md links $base" "no link to it"
    fi

    header=$(sed -n '1s/^# `\(.*\)`$/\1/p' "$page")
    [ -n "$header" ] || continue        # about acc, not a header
    if [ ! -f "include/$header" ]; then
        bad "$base names a header" "include/$header is not there"
        continue
    fi

    names=$(functions "include/$header")
    [ -n "$names" ] || continue         # macros and types alone
    keys "$header" "$names"
done

# The book's title, and its pages in the order index.md lists them, which
# is the order md2ref reads them in: index.md first, then by name, so the
# names begin with their place.
title=$("$REFTOOL" "$DAT" info | sed -n 's/^title //p')
if [ "$title" = "Agon C Compiler API reference" ]; then
    ok
else
    bad "the book's title" "it is: $title"
fi
listed=$(grep -o '](0*[0-9]*-[^)#]*\.md)' "$DOCS/index.md" | tr -d '()]' | awk '!seen[$0]++')
named=$(cd "$DOCS" && ls [0-9]*.md)
if [ "$listed" = "$named" ]; then
    ok
else
    bad "pages in index.md's order" "$(diff <(echo "$listed") <(echo "$named") | head -3 | tr '\n' ' ')"
fi

# The options, as the usage acc prints names them: the word an option line
# starts with, and any other option the line names after a `;`.
ACC=${ACC:-bin/acc}
if [ -x "$ACC" ]; then
    options=$("$ACC" -h 2>&1 | grep -E '^  [-@]' \
              | grep -oE '(^  |[ ;] )(-[A-Za-z]+|@)' | sed 's/^[ ;]*//' | sort -u)
    if [ "$(printf '%s\n' "$options" | grep -c .)" -lt 10 ]; then
        bad "acc -h lists the options" "found: $options"
    else
        keys "acc -h" "$options"
    fi
fi

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

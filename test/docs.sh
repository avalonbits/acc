#!/bin/bash
# The reference, docs/ref/*.md, made into bin/acc.dat for ref (make doc):
# every function a documented header declares is a key ref finds, so that
# `ref acc <name>` never comes back empty for a name the library has; and
# the contents page, index.md, links every page.
#
# A page documents the header its title names, `# `string.h`` for
# include/string.h. Its functions are found by reading the header's
# declarations, so a function added to a header without a heading of its
# own in the page is a failure here.
set -uo pipefail
cd "$(dirname "$0")/.."

DAT=${DAT:-bin/acc.dat}
REFTOOL=${REFTOOL:-bin/reftool}
[ -x "$REFTOOL" ] && [ -f "$DAT" ] || { echo "  no $DAT: make doc needs ref (REF_SRC)"; exit 77; }

pass=0; fail=0

ok() { pass=$((pass + 1)); }
bad() { printf '  FAIL %-34s %s\n' "$1" "$2"; fail=$((fail + 1)); }

# The functions a header declares: its comments and preprocessor lines
# taken out, then the first name before a parenthesis in each declaration
# that is not a typedef.
functions() {
    python3 - "$1" <<'PY'
import re, sys

text = open(sys.argv[1]).read()
text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
text = re.sub(r'//[^\n]*', ' ', text)
text = re.sub(r'^\s*#[^\n]*', ' ', text, flags=re.M)
text = re.sub(r'__attribute__\s*\(\(.*?\)\)', ' ', text)
for decl in text.split(';'):
    decl = ' '.join(decl.split())
    if not decl or decl.startswith('typedef') or decl.endswith('}'):
        continue
    name = re.search(r'\b([A-Za-z_]\w*)\s*\(', decl)
    if name:
        print(name.group(1))
PY
}

for page in docs/ref/*.md; do
    [ "$page" = docs/ref/index.md ] && continue
    base=$(basename "$page")
    if grep -q "]($base)" docs/ref/index.md; then
        ok
    else
        bad "index.md links $base" "no link to it"
    fi

    header=$(sed -n '1s/^# `\(.*\)`$/\1/p' "$page")
    if [ -z "$header" ] || [ ! -f "include/$header" ]; then
        bad "$base names its header" "title: $(head -1 "$page")"
        continue
    fi

    # reftool answers a line a query: kind 1 is a key found exactly.
    names=$(functions "include/$header")
    found=$(printf '%s\n' "$names" | "$REFTOOL" "$DAT" lookup)
    while read -r name && read -r answer <&3; do
        case $answer in
        "1 "*) ok ;;
        *) bad "$header: $name is a key" "lookup: $answer" ;;
        esac
    done <<<"$names" 3<<<"$found"
done

echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]

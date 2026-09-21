/* A static function declared before the ones defined ahead of it.
 *
 * acc drops the functions in a file that nothing in the file wants, and
 * works out what wants what by walking edges recorded as each call is
 * written. Looking an edge's target up among the file's own functions was a
 * binary search over them in the order they were defined -- but what it
 * searched for was the symbol, and a symbol's number is the order it was
 * first named. A forward declaration names one function before a later line
 * defines another, and from there the two orders disagree.
 *
 * The search then found nothing, took the target for a function outside the
 * file, and dropped it -- while the call to it stayed. So: `target` is
 * declared at the top, so that its symbol is the lowest of them, and defined
 * below eight functions whose symbols are all higher. That is enough of them
 * for the search to walk away from where it sits rather than land on it by
 * luck, which is why there are eight and not one.
 *
 * `caller` calls it after it is defined, which is the other half: a call
 * written before the definition marks the target wanted outright and never
 * reaches the search at all.
 */
static int target(int x);

static int f1(int x) { return x + 1; }
static int f2(int x) { return f1(x) + 1; }
static int f3(int x) { return f2(x) + 1; }
static int f4(int x) { return f3(x) + 1; }
static int f5(int x) { return f4(x) + 1; }
static int f6(int x) { return f5(x) + 1; }
static int f7(int x) { return f6(x) + 1; }
static int f8(int x) { return f7(x) + 1; }

static int target(int x) { return f8(x) + 1; }

/* After the definition, so the edge goes through the lookup. */
static int caller(int x) { return target(x); }

int main(void)
{
    if (caller(0) != 9) return 1;
    if (caller(33) != 42) return 2;

    return 42;
}

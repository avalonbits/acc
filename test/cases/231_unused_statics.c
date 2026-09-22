/* A file with functions of its own that nothing in it calls.
 *
 * What the answer says is only that the ones that are called still work.
 * The reason this case exists is what it makes target.sh compare: acc takes
 * an uncalled `static` back out of the image at the end, and whether it
 * does depended on memory it never wrote. The table beside the symbols,
 * which holds each function's parameters and flags, was grown with realloc
 * and left as it came; a slot that happened to have the "something holds
 * this one's address" bit set made an uncalled function look wanted.
 *
 * On a host that bit is always clear, because a fresh page from the system
 * is zeros, so every test here passed and dead.sh agreed. On the Agon,
 * where the allocator hands back memory that has been used before, it was
 * whatever was there -- and the Agon build kept functions the host build
 * dropped. Two builds of one compiler, the same source, different images.
 *
 * Nothing in test/cases had an uncalled static in it, which is why
 * target.sh could not see it. This one does.
 */
static int never_called(int x) { return x * 3 + 1; }
static int nor_this(int x) { return never_called(x) + 2; }

static int doubled(int x) { return x + x; }
static int plus_two(int x) { return x + 2; }

static int table[4] = { 1, 2, 3, 4 };

/* Called through a pointer rather than by name, so it stays whatever else
 * goes -- the other half of the same decision. */
static int by_pointer(int x) { return x + 10; }

int main(void) {
    int r = 0;
    int (*f)(int) = by_pointer;

    if (doubled(10) == 20) r++;
    if (plus_two(5) == 7) r++;
    if (f(1) == 11) r++;
    if (table[3] == 4) r++;

    return r + 38;              /* 4 checks */
}

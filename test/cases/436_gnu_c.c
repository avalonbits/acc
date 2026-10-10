/* GNU C, as agondev's clang is: __GNUC__ and its version, __VERSION__ a
 * string, __builtin_ffs and __builtin_popcount from the library with no
 * header, and __builtin_unreachable on a path that is never taken. */
static int pick(int which)
{
    if (which == 1)
        return 7;
    if (which == 2)
        return 9;
    __builtin_unreachable();

    return 0;
}

int main(void)
{
    static const char version[] = __VERSION__;
    volatile int none = 0, one = 1, mid = 0x100, neg = -8, top = 0x800000, odd = 0x50;

#if !defined(__GNUC__) || __GNUC__ != 4 || __GNUC_MINOR__ != 2 || __GNUC_PATCHLEVEL__ != 1
    return 1;
#endif
    if (sizeof version < 2)
        return 2;
    if (__builtin_ffs(none) != 0 || __builtin_ffs(one) != 1 || __builtin_ffs(mid) != 9
        || __builtin_ffs(neg) != 4 || __builtin_ffs(top) != 24 || __builtin_ffs(odd) != 5)
        return 3;
    if (pick(1) + pick(2) != 16)
        return 4;
    if (__builtin_popcount(none) != 0 || __builtin_popcount(odd) != 2
        || __builtin_popcount(0xffffffu) != 24 || __builtin_popcount(top) != 1)
        return 5;

    return 42;
}

/* The storage classes and qualifiers that change nothing acc generates but
 * that C programs are full of: auto, register, volatile, restrict, inline --
 * and qualifiers wherever C allows them among the specifiers. */
static inline int twice(int x) {
    return 2 * x;
}

inline static int thrice(register int x) {
    return 3 * x;
}

int copy(char *restrict to, const char *restrict from, int n) {
    for (register int i = 0; i < n; i++)
        to[i] = from[i];

    return n;
}

volatile int ticks;
unsigned const int limit = 7;
int const volatile *const volatile watched = &ticks;

int main(void) {
    int r = 0;
    auto int a = 3;
    register int b = 4;
    volatile unsigned char flag = 1;
    char buf[4];
    long unsigned const volatile wide = 100000;
    unsigned volatile short half = 2;

    if (twice(a) + thrice(b) == 18) r++;
    if (copy(buf, "abc", 4) == 4 && buf[2] == 'c') r++;
    ticks = 5;
    if (*watched == 5 && limit == 7) r++;
    flag++;
    if (flag == 2 && wide == 100000 && half == 2) r++;
    if (sizeof(volatile int) == 3 && sizeof(const unsigned char) == 1) r++;

    return r + 37;          /* 5 checks */
}

/* _Bool: one byte that holds 0 or 1, whatever it is given -- an int past a
 * byte, a long, a float, a pointer -- and stepping that stays 0 or 1. */
struct flags { _Bool on; char tag; };

_Bool g = 256;                      /* 1, not the low byte 0 */
_Bool none;

_Bool odd(int x) {
    return x & 1;
}

_Bool nonnull(char *p) {
    return p;
}

int count(_Bool a, _Bool b) {
    return a + b;
}

int main(void) {
    int r = 0;
    _Bool b = 2;
    _Bool c = 0.5;
    long big = 0x10000000;
    _Bool d = big;
    unsigned char narrow;
    struct flags f;
    char x;

    if (g == 1 && none == 0 && sizeof(_Bool) == 1) r++;
    if (b == 1 && c == 1 && d == 1) r++;
    b = b + 1;                      /* 2, as a _Bool */
    if (b == 1) r++;
    b--;
    b--;                            /* -1, as a _Bool */
    if (b == 1) r++;
    b = 0;
    b++;
    b++;
    if (b == 1) r++;
    narrow = 2;
    b = narrow & 2;
    if (b == 1 && odd(7) && !odd(8)) r++;
    if (nonnull(&x) && !nonnull(0)) r++;
    f.on = 3;
    f.tag = 'z';
    if (f.on == 1 && f.tag == 'z') r++;
    if (count(4, 5) == 2 && (_Bool) 0x100 == 1 && (_Bool) 0 == 0) r++;

    return r + 33;          /* 9 checks */
}

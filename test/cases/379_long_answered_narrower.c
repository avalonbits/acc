/* A long answered by a function of a narrower type -- a short, a signed
 * char -- is its low bytes as that type has them, not the long: gcc's
 * pr51023, whose short is 0x272A, not 0x4272A. */
short foo(long x) { return x; }
signed char bar(long x) { return x; }
int main(void) { long a = 0x4272AL; if (foo(a) == a) return 1; if (foo(a) != (short) 0x272A) return 2; if (bar(0x1234fe) != -2) return 3; return 42; }

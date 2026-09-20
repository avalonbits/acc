/* A wide operator's right operand is read where the program keeps it,
 * rather than copied into scratch first -- so this checks that the operand
 * is still what it was afterwards.
 *
 * The routines that do write through that address are named in gen.c: the
 * divisions leave the remainder where the divisor was, the float subtract
 * turns the right operand's sign over, and the float comparison rewrites
 * both. If one of the others starts doing it, or one of those three stops
 * being named, the value checked here changes under it. */
/* The operand has to be a local or a parameter for the routine to be given
 * its address: a global is read into scratch on the way, and scratch is
 * what the operators were always free to overwrite. */
long use_long(long a) {
    long lr = 5L;
    long sum = 0;

    sum += a + lr;  if (lr != 5L) return -1;
    sum += a - lr;  if (lr != 5L) return -2;
    sum += a * lr;  if (lr != 5L) return -3;
    sum += a / lr;  if (lr != 5L) return -4;
    sum += a % lr;  if (lr != 5L) return -5;
    sum += a & lr;  if (lr != 5L) return -6;
    sum += a | lr;  if (lr != 5L) return -7;
    sum += a ^ lr;  if (lr != 5L) return -8;
    sum += a << 2;  if (lr != 5L) return -9;
    sum += a >> 1;  if (lr != 5L) return -10;
    if (a < lr || a == lr || a > lr || a <= lr || a >= lr || a != lr)
        sum += 0;
    if (lr != 5L)
        return -11;

    return sum;
}

unsigned long use_ulong(unsigned long a) {
    unsigned long ur = 3UL;
    unsigned long sum = 0;

    sum += a / ur;  if (ur != 3UL) return 1;
    sum += a % ur;  if (ur != 3UL) return 2;
    sum += a >> 1;  if (ur != 3UL) return 3;
    if (a > ur)
        sum += 1;

    return ur == 3UL ? sum : 4;
}

long long use_llong(long long a) {
    long long llr = 7LL;
    long long sum = 0;

    sum += a + llr;  if (llr != 7LL) return -1;
    sum += a - llr;  if (llr != 7LL) return -2;
    sum += a * llr;  if (llr != 7LL) return -3;
    sum += a / llr;  if (llr != 7LL) return -4;
    sum += a % llr;  if (llr != 7LL) return -5;
    sum += a & llr;  if (llr != 7LL) return -6;
    sum += a ^ llr;  if (llr != 7LL) return -7;
    sum += a << 3;   if (llr != 7LL) return -8;
    if (a < llr || a >= llr)
        sum += 0;
    if (llr != 7LL)
        return -9;

    return sum;
}

int use_float(float a) {
    float fr = 2.0f;
    float sum = 0.0f;

    sum = sum + (a + fr);  if (fr != 2.0f) return 1;
    sum = sum + (a - fr);  if (fr != 2.0f) return 2;
    sum = sum + (a * fr);  if (fr != 2.0f) return 3;
    sum = sum + (a / fr);  if (fr != 2.0f) return 4;
    if (a > fr || a == fr)
        sum = sum + 1.0f;
    if (fr != 2.0f)
        return 5;

    return (int) sum;
}

int main(void) {
    long l = use_long(20L);
    unsigned long u = use_ulong(20UL);
    long long q = use_llong(20LL);
    int f = use_float(6.0f);

    if (l != 20 + 5 + 20 - 5 + 20 * 5 + 4 + 0 + (20 & 5) + (20 | 5) + (20 ^ 5)
             + (20 << 2) + (20 >> 1))
        return 1;
    if (u != 6 + 2 + 10 + 1)
        return 2;
    if (q != 27 + 13 + 140 + 2 + 6 + (20 & 7) + (20 ^ 7) + 160)
        return 3;
    if (f != 6 + 2 + 6 - 2 + 6 * 2 + 3 + 1)
        return 4;
    return 42;
}
